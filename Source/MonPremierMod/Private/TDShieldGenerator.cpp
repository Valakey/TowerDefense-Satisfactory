#include "TDShieldGenerator.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Async/Async.h"

// Static members
const float ATDShieldGenerator::RING_BASE_Z = 30.0f;
const float ATDShieldGenerator::RING_SPACING = 25.0f;
const float ATDShieldGenerator::CHARGE_TIME_PER_RING = 1.0f;
const float ATDShieldGenerator::FIRING_VISUAL_DURATION = 0.5f;
TSet<uint32> ATDShieldGenerator::ShieldedBuildingIDs;

ATDShieldGenerator::ATDShieldGenerator(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = true;
    mFactoryTickFunction.bCanEverTick = true;

    // Base mesh (bobine Tesla) - monte de 150u pour ne pas etre dans le sol
    BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
    BaseMesh->SetupAttachment(RootComponent);
    BaseMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> BaseMeshObj(TEXT("/MonPremierMod/Meshes/Turrets/TeslaTurret/TeslaTurret.TeslaTurret"));
    if (BaseMeshObj.Succeeded()) BaseMesh->SetStaticMesh(BaseMeshObj.Object);

    // 10 anneaux (du bas vers le haut, separes de 5u)
    static ConstructorHelpers::FObjectFinder<UStaticMesh> RingMeshObj(TEXT("/MonPremierMod/Meshes/Turrets/TeslaTurret/Ring/Ring.Ring"));
    for (int32 i = 0; i < NUM_RINGS; i++)
    {
        UStaticMeshComponent* R = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Ring_%d"), i));
        R->SetupAttachment(BaseMesh);
        R->SetRelativeLocation(FVector(0.0f, 0.0f, RING_BASE_Z + i * RING_SPACING));
        R->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
        R->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));
        R->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        R->SetVisibility(false);
        if (RingMeshObj.Succeeded()) R->SetStaticMesh(RingMeshObj.Object);
        Rings.Add(R);
    }

    // Arc electrique (tower -> batiment)
    ArcMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArcMesh"));
    ArcMesh->SetupAttachment(RootComponent);
    ArcMesh->SetAbsolute(true, true, true);
    ArcMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ArcMesh->SetVisibility(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> ArcMeshObj(TEXT("/MonPremierMod/Meshes/Turrets/TeslaTurret/ArcElectric/ArcElectric.ArcElectric"));
    if (ArcMeshObj.Succeeded()) ArcMesh->SetStaticMesh(ArcMeshObj.Object);

    // Dome bouclier (sphere avec M_ShieldDome)
    ShieldDomeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShieldDomeMesh"));
    ShieldDomeMesh->SetupAttachment(RootComponent);
    ShieldDomeMesh->SetAbsolute(true, true, true);
    ShieldDomeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ShieldDomeMesh->SetVisibility(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshObj(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereMeshObj.Succeeded()) ShieldDomeMesh->SetStaticMesh(SphereMeshObj.Object);

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShieldMatObj(TEXT("/MonPremierMod/Materials/M_ShieldDome.M_ShieldDome"));
    if (ShieldMatObj.Succeeded()) ShieldDomeMesh->SetMaterial(0, ShieldMatObj.Object);

    // Sphere de detection
    ShieldSphere = CreateDefaultSubobject<USphereComponent>(TEXT("ShieldSphere"));
    ShieldSphere->SetupAttachment(RootComponent);
    ShieldSphere->SetSphereRadius(ShieldRadius);
    ShieldSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ShieldSphere->SetCollisionResponseToAllChannels(ECR_Overlap);

    // Electricite
    PowerConnection = CreateDefaultSubobject<UFGPowerConnectionComponent>(TEXT("PowerConnection"));
    PowerConnection->SetupAttachment(BaseMesh);
    PowerConnection->SetRelativeLocation(FVector(0, 0, -40.0f));
    PowerInfo = CreateDefaultSubobject<UFGPowerInfoComponent>(TEXT("PowerInfo"));

    // Audio
    ChargeAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("ChargeAudio"));
    ChargeAudioComponent->SetupAttachment(BaseMesh);
    ChargeAudioComponent->bAutoActivate = false;
    ChargeAudioComponent->bAllowSpatialization = true;
    ChargeAudioComponent->bOverrideAttenuation = true;
    ChargeAudioComponent->AttenuationOverrides.bAttenuate = true;
    ChargeAudioComponent->AttenuationOverrides.bSpatialize = true;
    ChargeAudioComponent->AttenuationOverrides.FalloffDistance = 2000.0f;
    ChargeAudioComponent->AttenuationOverrides.AttenuationShapeExtents = FVector(200.0f, 0.0f, 0.0f);

    BeamAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("BeamAudio"));
    BeamAudioComponent->SetupAttachment(BaseMesh);
    BeamAudioComponent->bAutoActivate = false;
    BeamAudioComponent->bAllowSpatialization = true;
    BeamAudioComponent->bOverrideAttenuation = true;
    BeamAudioComponent->AttenuationOverrides.bAttenuate = true;
    BeamAudioComponent->AttenuationOverrides.bSpatialize = true;
    BeamAudioComponent->AttenuationOverrides.FalloffDistance = 2000.0f;
    BeamAudioComponent->AttenuationOverrides.AttenuationShapeExtents = FVector(200.0f, 0.0f, 0.0f);

    // Sons
    static ConstructorHelpers::FObjectFinder<USoundBase> ChargeSoundObj(TEXT("/MonPremierMod/Audios/Turret/ChargeCircle.ChargeCircle"));
    if (ChargeSoundObj.Succeeded()) ChargeSound = ChargeSoundObj.Object;

    static ConstructorHelpers::FObjectFinder<USoundBase> BeamSoundObj(TEXT("/MonPremierMod/Audios/Turret/beamElec.beamElec"));
    if (BeamSoundObj.Succeeded()) BeamSound = BeamSoundObj.Object;
}

void ATDShieldGenerator::BeginPlay()
{
    Super::BeginPlay();

    if (PowerConnection && PowerInfo)
    {
        PowerConnection->SetPowerInfo(PowerInfo);
        PowerInfo->SetTargetConsumption(PowerConsumption);
    }

    ShieldState = ETDShieldState::Idle;
    CurrentChargedRings = 0;
    HideAllRings();
    HideDome();
    if (ArcMesh) ArcMesh->SetVisibility(false);

    UE_LOG(LogTemp, Warning, TEXT("TDShieldGenerator: BeginPlay - Radius: %.0f, Power: %.0f MW"), ShieldRadius, PowerConsumption);
}

void ATDShieldGenerator::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    // Visuals principalement geres dans Factory_Tick (toujours actif)
    // Tick() sert de boost quand le joueur est proche (PrimaryActorTick actif)
    if (bHasPower && CurrentChargedRings > 0)
    {
        float RotSpeed = 180.0f * DeltaTime * FMath::Max(1, CurrentChargedRings);
        for (int32 i = 0; i < CurrentChargedRings && i < Rings.Num(); i++)
        {
            if (Rings[i] && Rings[i]->IsVisible())
            {
                FRotator R = Rings[i]->GetRelativeRotation();
                R.Yaw += RotSpeed;
                R.Pitch = FMath::Sin(GetWorld()->GetTimeSeconds() * (2.0f + i * 0.7f)) * 2.0f;
                R.Roll = 90.0f + FMath::Sin(GetWorld()->GetTimeSeconds() * (3.0f + i * 0.5f)) * 1.5f;
                Rings[i]->SetRelativeRotation(R);
            }
        }
    }
}

void ATDShieldGenerator::Factory_Tick(float dt)
{
    Super::Factory_Tick(dt);

    CheckPowerStatus();

    switch (ShieldState)
    {
    case ETDShieldState::Idle:
        if (bHasPower)
        {
            ShieldState = ETDShieldState::Charging;
            CurrentChargedRings = 0;
            ChargeTimer = 0.0f;
            UE_LOG(LogTemp, Warning, TEXT("TDShieldGenerator %s: Power ON -> CHARGING"), *GetName());
        }
        break;

    case ETDShieldState::Charging:
        if (!bHasPower)
        {
            HideAllRings();
            CurrentChargedRings = 0;
            ShieldState = ETDShieldState::Idle;
            AsyncTask(ENamedThreads::GameThread, [this]() {
                if (IsValid(this) && ChargeAudioComponent && ChargeAudioComponent->IsPlaying()) ChargeAudioComponent->Stop();
            });
            UE_LOG(LogTemp, Warning, TEXT("TDShieldGenerator %s: Power LOST -> IDLE (discharge)"), *GetName());
            break;
        }

        ChargeTimer += dt;
        if (ChargeTimer >= CHARGE_TIME_PER_RING && CurrentChargedRings < NUM_RINGS)
        {
            ChargeTimer = 0.0f;
            ShowRing(CurrentChargedRings);
            CurrentChargedRings++;

            // Son de charge a chaque anneau (dispatch GameThread)
            AsyncTask(ENamedThreads::GameThread, [this]() {
                if (IsValid(this) && ChargeAudioComponent && ChargeSound)
                {
                    ChargeAudioComponent->SetSound(ChargeSound);
                    ChargeAudioComponent->SetVolumeMultiplier(0.3f);
                    ChargeAudioComponent->Play();
                }
            });

            if (CurrentChargedRings >= NUM_RINGS)
            {
                ShieldState = ETDShieldState::Ready;
                UE_LOG(LogTemp, Warning, TEXT("TDShieldGenerator %s: 10/10 rings -> READY"), *GetName());
            }
        }
        break;

    case ETDShieldState::Ready:
        if (!bHasPower)
        {
            HideAllRings();
            CurrentChargedRings = 0;
            ShieldState = ETDShieldState::Idle;
            break;
        }
        // Attend TryProtectBuilding (appele depuis DamageBuilding du spawner)
        break;

    case ETDShieldState::Firing:
        if (!bHasPower)
        {
            StopFiring();
            ShieldState = ETDShieldState::Idle;
            break;
        }

        FiringTimer += dt;

        // Garder l'arc oriente vers le batiment
        if (ProtectedBuilding.IsValid())
        {
            UpdateArcVisual();
        }

        // Fin du visuel -> retour en charge
        if (FiringTimer >= FIRING_VISUAL_DURATION || !ProtectedBuilding.IsValid())
        {
            StopFiring();
            ShieldState = ETDShieldState::Charging;
            CurrentChargedRings = 0;
            ChargeTimer = 0.0f;
        }
        break;
    }

    // Rotation des anneaux visibles (dans Factory_Tick car PrimaryActorTick est desactive par significance)
    if (bHasPower && CurrentChargedRings > 0)
    {
        float RotSpeed = 180.0f * dt * FMath::Max(1, CurrentChargedRings);
        for (int32 i = 0; i < CurrentChargedRings && i < Rings.Num(); i++)
        {
            if (Rings[i] && Rings[i]->IsVisible())
            {
                FRotator R = Rings[i]->GetRelativeRotation();
                R.Yaw += RotSpeed;
                R.Pitch = FMath::Sin(GetWorld()->GetTimeSeconds() * (2.0f + i * 0.7f)) * 2.0f;
                R.Roll = 90.0f + FMath::Sin(GetWorld()->GetTimeSeconds() * (3.0f + i * 0.5f)) * 1.5f;
                Rings[i]->SetRelativeRotation(R);
            }
        }
    }

    // Sync visibilite des anneaux (au cas ou Tick() ne tourne pas)
    for (int32 i = 0; i < Rings.Num(); i++)
    {
        if (!Rings[i]) continue;
        bool bShouldBeVisible = (ShieldState == ETDShieldState::Charging || ShieldState == ETDShieldState::Ready) && i < CurrentChargedRings;
        if (Rings[i]->IsVisible() != bShouldBeVisible)
        {
            Rings[i]->SetVisibility(bShouldBeVisible);
        }
    }

    // Arc visuel pendant le tir
    if (ShieldState == ETDShieldState::Firing && ProtectedBuilding.IsValid())
    {
        UpdateArcVisual();
    }
}

void ATDShieldGenerator::CheckPowerStatus()
{
    bool bPrev = bHasPower;
    bHasPower = false;

    if (PowerInfo && PowerInfo->HasPower()) bHasPower = true;
    else if (PowerConnection && PowerConnection->HasPower()) bHasPower = true;

    if (bHasPower != bPrev)
        UE_LOG(LogTemp, Warning, TEXT("TDShieldGenerator %s: Power -> %s"), *GetName(), bHasPower ? TEXT("ON") : TEXT("OFF"));
}

bool ATDShieldGenerator::TryProtectBuilding(AActor* Building)
{
    if (!Building || !IsValid(Building)) return false;
    if (ShieldState != ETDShieldState::Ready) return false;
    if (!bHasPower) return false;
    if (!IsActorInRange(Building)) return false;

    // 1 shield par batiment a la fois
    uint32 BID = Building->GetUniqueID();
    if (ShieldedBuildingIDs.Contains(BID)) return false;

    // Tirer!
    FireShield(Building);
    return true;
}

void ATDShieldGenerator::FireShield(AActor* Building)
{
    ProtectedBuilding = Building;
    FiringTimer = 0.0f;
    ShieldState = ETDShieldState::Firing;

    // Enregistrer le batiment protege
    ShieldedBuildingIDs.Add(Building->GetUniqueID());

    // Montrer l'arc electrique
    UpdateArcVisual();

    // Montrer le dome autour du batiment
    ShowDome(Building);

    // Cacher les anneaux (decharge)
    HideAllRings();

    // Son de tir (dispatch GameThread)
    AsyncTask(ENamedThreads::GameThread, [this]() {
        if (IsValid(this) && BeamAudioComponent && BeamSound)
        {
            BeamAudioComponent->SetSound(BeamSound);
            BeamAudioComponent->SetVolumeMultiplier(0.5f);
            BeamAudioComponent->Play();
        }
    });

    UE_LOG(LogTemp, Warning, TEXT("TDShieldGenerator %s: FIRE -> shield on %s!"), *GetName(), *Building->GetName());
}

void ATDShieldGenerator::StopFiring()
{
    if (ProtectedBuilding.IsValid())
    {
        ShieldedBuildingIDs.Remove(ProtectedBuilding->GetUniqueID());
    }
    ProtectedBuilding.Reset();

    if (ArcMesh) ArcMesh->SetVisibility(false);
    HideDome();
    HideAllRings();
    CurrentChargedRings = 0;
}

void ATDShieldGenerator::HideAllRings()
{
    for (UStaticMeshComponent* R : Rings)
    {
        if (R) R->SetVisibility(false);
    }
}

void ATDShieldGenerator::ShowRing(int32 Index)
{
    if (Index >= 0 && Index < Rings.Num() && Rings[Index])
    {
        Rings[Index]->SetVisibility(true);
    }
}

void ATDShieldGenerator::UpdateArcVisual()
{
    if (!ArcMesh || !ProtectedBuilding.IsValid()) return;

    // Arc part du sommet de la turret (Z=380)
    FVector TowerTop = GetActorLocation() + FVector(0, 0, 380.0f);

    // Cible = centre du dome (bounds visibles du batiment protege)
    FBox TargetBox(ForceInit);
    for (UActorComponent* Comp : ProtectedBuilding->GetComponents())
    {
        UPrimitiveComponent* PC = Cast<UPrimitiveComponent>(Comp);
        if (!PC) continue;
        if (Cast<USphereComponent>(PC)) continue;
        if (!PC->IsVisible()) continue;
        TargetBox += PC->Bounds.GetBox();
    }
    FVector TargetCenter = TargetBox.IsValid ? TargetBox.GetCenter() : ProtectedBuilding->GetActorLocation();

    FVector Dir = TargetCenter - TowerTop;
    float Distance = Dir.Size();
    FRotator ArcRot = Dir.Rotation();

    // Placer l'arc au milieu entre turret et cible, etire pour couvrir la distance totale
    FVector ArcMidpoint = TowerTop + Dir * 0.5f;
    float ScaleX = FMath::Max(Distance / 100.0f, 0.1f);

    ArcMesh->SetWorldLocation(ArcMidpoint);
    ArcMesh->SetWorldRotation(ArcRot);
    ArcMesh->SetWorldScale3D(FVector(ScaleX, 1.0f, 1.0f));
    ArcMesh->SetVisibility(true);
}

void ATDShieldGenerator::ShowDome(AActor* Target)
{
    if (!ShieldDomeMesh || !Target) return;

    // Calculer les bounds sans composants enormes (ShieldSphere, etc.)
    FBox BoundsBox(ForceInit);
    for (UActorComponent* Comp : Target->GetComponents())
    {
        UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Comp);
        if (!PrimComp) continue;
        // Ignorer les spheres de detection et les composants invisibles geants
        if (Cast<USphereComponent>(PrimComp)) continue;
        if (!PrimComp->IsVisible()) continue;
        BoundsBox += PrimComp->Bounds.GetBox();
    }
    FVector Origin = BoundsBox.GetCenter();
    FVector Extent = BoundsBox.GetExtent();
    float RadiusNeeded = Extent.Size() + 50.0f;

    // Sphere mesh = 50u de rayon (100u diametre), scale = RadiusNeeded / 50
    float DomeScale = FMath::Max(RadiusNeeded / 50.0f, 1.5f);

    ShieldDomeMesh->SetWorldLocation(Origin);
    ShieldDomeMesh->SetWorldRotation(FRotator::ZeroRotator);
    ShieldDomeMesh->SetWorldScale3D(FVector(DomeScale));
    ShieldDomeMesh->SetVisibility(true);
}

void ATDShieldGenerator::HideDome()
{
    if (ShieldDomeMesh) ShieldDomeMesh->SetVisibility(false);
}

bool ATDShieldGenerator::IsActorInRange(AActor* Actor) const
{
    if (!Actor || !IsValid(Actor)) return false;
    return FVector::Dist(GetActorLocation(), Actor->GetActorLocation()) <= ShieldRadius;
}
