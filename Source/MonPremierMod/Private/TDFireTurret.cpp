#include "TDFireTurret.h"
#include "TDEnemy.h"
#include "TDEnemyFlying.h"
#include "TDEnemyRam.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "NiagaraFunctionLibrary.h"
#include "Resources/FGItemDescriptor.h"
#include "FGInventoryComponent.h"

ATDFireTurret::ATDFireTurret(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = true;
    mFactoryTickFunction.bCanEverTick = true;

    FireInterval = 1.0f / FireRate;

    // Creer la base (meme mesh que LaserTurret)
    BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
    BaseMesh->SetupAttachment(RootComponent);
    BaseMesh->SetRelativeLocation(FVector(0, 0, -70));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> BaseTurretMesh(TEXT("/MonPremierMod/Meshes/Turrets/LaserTurret/Base/baseTurret.baseTurret"));
    if (BaseTurretMesh.Succeeded())
    {
        BaseMesh->SetStaticMesh(BaseTurretMesh.Object);
        BaseMesh->SetRelativeScale3D(FVector(0.8f, 0.8f, 0.6f));
        UE_LOG(LogTemp, Warning, TEXT("TDFireTurret: Base mesh charge!"));
    }

    // Charger materiau base
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BaseMat(TEXT("/MonPremierMod/Meshes/Turrets/LaserTurret/Base/Material_001.Material_001"));
    if (BaseMat.Succeeded())
    {
        BaseMesh->SetMaterial(0, BaseMat.Object);
    }

    // Creer le pivot central pour la rotation
    HeadPivot = CreateDefaultSubobject<USceneComponent>(TEXT("HeadPivot"));
    HeadPivot->SetupAttachment(BaseMesh);
    HeadPivot->SetRelativeLocation(FVector(0, 0, 110));

    // Creer la tete de tourelle (nouveau mesh FireTurret)
    TurretHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretHead"));
    TurretHead->SetupAttachment(HeadPivot);
    TurretHead->SetRelativeLocation(FVector(-25, 0, 0));
    TurretHead->SetRelativeRotation(FRotator(0, 0, 0));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> HeadTurretMesh(TEXT("/MonPremierMod/Meshes/Turrets/FireTurret/Head/Meshy_AI_Sci_fi_turret_head_wi_0210071244_texture.Meshy_AI_Sci_fi_turret_head_wi_0210071244_texture"));
    if (HeadTurretMesh.Succeeded())
    {
        TurretHead->SetStaticMesh(HeadTurretMesh.Object);
        TurretHead->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.65f));
        UE_LOG(LogTemp, Warning, TEXT("TDFireTurret: Head mesh charge!"));
    }

    // Creer le composant audio pour les tirs avec attenuation 3D
    FireAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("FireAudio"));
    FireAudioComponent->SetupAttachment(HeadPivot);
    FireAudioComponent->bAutoActivate = false;
    FireAudioComponent->bAllowSpatialization = true;
    FireAudioComponent->bOverrideAttenuation = true;
    FireAudioComponent->AttenuationOverrides.bAttenuate = true;
    FireAudioComponent->AttenuationOverrides.bSpatialize = true;
    FireAudioComponent->AttenuationOverrides.FalloffDistance = 2000.0f;
    FireAudioComponent->AttenuationOverrides.AttenuationShapeExtents = FVector(200.0f, 0.0f, 0.0f);

    // Creer le composant audio dedie au son de tir (mitrailleuse)
    ShootAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("ShootAudio"));
    ShootAudioComponent->SetupAttachment(HeadPivot);
    ShootAudioComponent->bAutoActivate = false;
    ShootAudioComponent->bAllowSpatialization = true;
    ShootAudioComponent->bOverrideAttenuation = true;
    ShootAudioComponent->AttenuationOverrides.bAttenuate = true;
    ShootAudioComponent->AttenuationOverrides.bSpatialize = true;
    ShootAudioComponent->AttenuationOverrides.FalloffDistance = 2000.0f;
    ShootAudioComponent->AttenuationOverrides.AttenuationShapeExtents = FVector(200.0f, 0.0f, 0.0f);

    // Creer le composant audio pour sons one-shot (LockOn) avec attenuation 3D
    OneShotAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("OneShotAudio"));
    OneShotAudioComponent->SetupAttachment(HeadPivot);
    OneShotAudioComponent->bAutoActivate = false;
    OneShotAudioComponent->bAllowSpatialization = true;
    OneShotAudioComponent->bOverrideAttenuation = true;
    OneShotAudioComponent->AttenuationOverrides.bAttenuate = true;
    OneShotAudioComponent->AttenuationOverrides.bSpatialize = true;
    OneShotAudioComponent->AttenuationOverrides.FalloffDistance = 2000.0f;
    OneShotAudioComponent->AttenuationOverrides.AttenuationShapeExtents = FVector(200.0f, 0.0f, 0.0f);

    // Creer le composant muzzle flash
    MuzzleFlashComponent = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("MuzzleFlash"));
    MuzzleFlashComponent->SetupAttachment(HeadPivot);
    MuzzleFlashComponent->bAutoActivate = false;

    // Creer 3 tracers de balle (effet mitrailleuse)
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

    for (int32 i = 0; i < 10; i++)
    {
        FName Name = *FString::Printf(TEXT("BulletTracer%d"), i);
        BulletTracers[i] = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        BulletTracers[i]->SetupAttachment(GetRootComponent());
        BulletTracers[i]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        BulletTracers[i]->SetVisibility(false);
        BulletTracers[i]->CastShadow = false;

        if (CylinderMesh.Succeeded())
        {
            BulletTracers[i]->SetStaticMesh(CylinderMesh.Object);
        }
    }

    // Creer l'effet de flamme Niagara au bout du canon
    FlameEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("FlameEffect"));
    FlameEffect->SetupAttachment(HeadPivot);
    FlameEffect->SetRelativeLocation(FVector(-75.0f, 0.0f, 20.0f));
    FlameEffect->bAutoActivate = false;
    FlameEffect->SetAutoDestroy(false);

    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> FlameFX(TEXT("/MonPremierMod/Particles/NS_FireTurret.NS_FireTurret"));
    if (FlameFX.Succeeded())
    {
        FlameEffect->SetAsset(FlameFX.Object);
        FlameNiagaraAsset = FlameFX.Object;
        UE_LOG(LogTemp, Warning, TEXT("TDFireTurret: NS_FireTurret Niagara LOADED!"));
    }

    // Charger le son de tir FireTurret
    static ConstructorHelpers::FObjectFinder<USoundBase> FireSoundObj(TEXT("/MonPremierMod/Audios/Turret/FireTurretShoot.FireTurretShoot"));
    if (FireSoundObj.Succeeded())
    {
        BulletFireSound = FireSoundObj.Object;
        UE_LOG(LogTemp, Warning, TEXT("TDFireTurret: BulletFireSound LOADED!"));
    }

    static ConstructorHelpers::FObjectFinder<USoundBase> LockOnSoundObj(TEXT("/MonPremierMod/Audios/Turret/LockOnSound.LockOnSound"));
    if (LockOnSoundObj.Succeeded())
    {
        LockOnSound = LockOnSoundObj.Object;
        UE_LOG(LogTemp, Warning, TEXT("TDFireTurret: LockOnSound LOADED!"));
    }

    static ConstructorHelpers::FObjectFinder<USoundBase> IdleSoundObj(TEXT("/MonPremierMod/Audios/Turret/IdleSound.IdleSound"));
    if (IdleSoundObj.Succeeded())
    {
        IdleSound = IdleSoundObj.Object;
        UE_LOG(LogTemp, Warning, TEXT("TDFireTurret: IdleSound LOADED!"));
    }
}

void ATDFireTurret::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning, TEXT("TDFireTurret spawned at %s | Ammo: %d/%d"), *GetActorLocation().ToString(), CurrentAmmo, MaxAmmo);

    // Charger la classe Desc_CartridgeStandard pour les munitions
    AmmoItemClass = LoadClass<UFGItemDescriptor>(nullptr, TEXT("/Game/FactoryGame/Resource/Parts/CartridgeStandard/Desc_CartridgeStandard.Desc_CartridgeStandard_C"));
    if (AmmoItemClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("TDFireTurret: Desc_CartridgeStandard LOADED!"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("TDFireTurret: FAILED to load Desc_CartridgeStandard!"));
    }

    // Creer le materiau gold pour les tracers
    UMaterialInterface* BaseMat = BulletTracers[0] ? BulletTracers[0]->GetMaterial(0) : nullptr;
    if (BaseMat)
    {
        TracerMaterial = UMaterialInstanceDynamic::Create(BaseMat, this);
    }
    if (TracerMaterial)
    {
        TracerMaterial->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.85f, 0.65f, 0.13f, 1.0f));
        TracerMaterial->SetScalarParameterValue(TEXT("Metallic"), 1.0f);
        TracerMaterial->SetScalarParameterValue(TEXT("Roughness"), 0.3f);
        for (int32 i = 0; i < 10; i++)
        {
            if (BulletTracers[i])
            {
                BulletTracers[i]->SetMaterial(0, TracerMaterial);
            }
        }
    }

    // Forcer le tick actif (AFGBuildable peut le desactiver)
    PrimaryActorTick.SetTickFunctionEnable(true);
    PrimaryActorTick.bCanEverTick = true;
    SetActorTickEnabled(true);
}

void ATDFireTurret::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Verifier les munitions
    bHasAmmo = (CurrentAmmo > 0);

    if (!bHasAmmo)
    {
        StopFiring();
        bIsActive = false;
        CurrentTarget = nullptr;
        bIsLockedOn = false;

        // Tete vers le bas quand plus de munitions
        if (HeadPivot)
        {
            FRotator CurrentRot = HeadPivot->GetRelativeRotation();
            FRotator TargetRot = FRotator(55.0f, CurrentRot.Yaw, 0.0f);
            FRotator NewRot = FMath::RInterpTo(CurrentRot, TargetRot, DeltaTime, 3.0f);
            HeadPivot->SetRelativeRotation(NewRot);
        }
        return;
    }

    // Sauvegarder cible precedente pour detecter changement
    AActor* PreviousTarget = CurrentTarget;

    // PERF: Throttle target scan toutes les 0.3s
    TargetScanTimer += DeltaTime;
    bool bDoScan = (TargetScanTimer >= TargetScanInterval);
    if (bDoScan) TargetScanTimer = 0.0f;

    // Chercher une cible si on n'en a pas ou si elle est morte/hors portee
    if (!CurrentTarget || !IsValid(CurrentTarget))
    {
        if (bDoScan) CurrentTarget = FindBestTarget();
    }
    else
    {
        float DistanceToTarget = FVector::Dist(GetActorLocation(), CurrentTarget->GetActorLocation());
        if (DistanceToTarget > Range || IsEnemyDead(CurrentTarget))
        {
            if (bDoScan) CurrentTarget = FindBestTarget();
            else CurrentTarget = nullptr;
        }
    }

    // Reset lock-on si la cible a change
    if (CurrentTarget != PreviousTarget)
    {
        bIsLockedOn = false;
        bIsFiring = false;
    }

    if (CurrentTarget)
    {
        bIsActive = true;

        // Gerer le delai de verrouillage avant tir
        if (!bIsLockedOn)
        {
            bIsLockedOn = true;
            LockOnTimer = 0.0f;
            if (LockOnSound && OneShotAudioComponent)
            {
                OneShotAudioComponent->SetSound(LockOnSound);
                OneShotAudioComponent->SetVolumeMultiplier(0.25f);
                OneShotAudioComponent->Play();
            }
        }

        LockOnTimer += DeltaTime;
        RotateTowardsTarget(DeltaTime);

        // Tirer seulement apres le delai de verrouillage
        if (LockOnTimer >= LockOnDelay)
        {
            FireTimer += DeltaTime;
            if (FireTimer >= FireInterval)
            {
                FireBullet();
                FireTimer = 0.0f;
            }
        }
    }
    else
    {
        bIsActive = false;
        bIsLockedOn = false;
        LockOnTimer = 0.0f;
        StopFiring();

        // Animation idle quand pas de cible
        UpdateIdleAnimation(DeltaTime);
    }
}

void ATDFireTurret::Factory_Tick(float dt)
{
    Super::Factory_Tick(dt);
    // Logique dans Tick() car Factory_Tick tourne en parallele (worker thread)
}

AActor* ATDFireTurret::FindBestTarget()
{
    AActor* BestTarget = nullptr;
    float BestDistance = Range;

    auto TryCandidate = [&](AActor* Candidate)
    {
        if (!Candidate || !IsValid(Candidate)) return;
        if (IsEnemyDead(Candidate)) return;

        float Distance = FVector::Dist(GetActorLocation(), Candidate->GetActorLocation());
        if (Distance >= BestDistance) return;

        FHitResult HitResult;
        FCollisionQueryParams Params;
        Params.AddIgnoredActor(this);

        FVector Start = TurretHead->GetComponentLocation();
        FVector End = Candidate->GetActorLocation();

        bool bHit = GetWorld()->LineTraceSingleByChannel(
            HitResult, Start, End, ECC_Visibility, Params);

        if (!bHit || HitResult.GetActor() == Candidate)
        {
            BestDistance = Distance;
            BestTarget = Candidate;
        }
    };

    for (TActorIterator<ATDEnemy> It(GetWorld()); It; ++It)
    {
        TryCandidate(*It);
    }

    for (TActorIterator<ATDEnemyFlying> It(GetWorld()); It; ++It)
    {
        TryCandidate(*It);
    }

    // Chercher les beliers
    for (TActorIterator<ATDEnemyRam> It(GetWorld()); It; ++It)
    {
        TryCandidate(*It);
    }

    return BestTarget;
}

bool ATDFireTurret::IsEnemyDead(AActor* Enemy)
{
    if (!Enemy || !IsValid(Enemy)) return true;

    if (ATDEnemy* Ground = Cast<ATDEnemy>(Enemy))
        return Ground->bIsDead;
    if (ATDEnemyFlying* Flying = Cast<ATDEnemyFlying>(Enemy))
        return Flying->bIsDead;
    if (ATDEnemyRam* Ram = Cast<ATDEnemyRam>(Enemy))
        return Ram->bIsDead;

    return true;
}

void ATDFireTurret::DealDamageToEnemy(AActor* Enemy, float DmgAmount)
{
    if (!Enemy || !IsValid(Enemy)) return;

    if (ATDEnemy* Ground = Cast<ATDEnemy>(Enemy))
    {
        Ground->TakeDamageCustom(DmgAmount);
    }
    else if (ATDEnemyFlying* Flying = Cast<ATDEnemyFlying>(Enemy))
    {
        Flying->TakeDamageCustom(DmgAmount);
    }
    else if (ATDEnemyRam* Ram = Cast<ATDEnemyRam>(Enemy))
    {
        Ram->TakeDamageCustom(DmgAmount);
    }
}

void ATDFireTurret::RotateTowardsTarget(float DeltaTime)
{
    if (!CurrentTarget || !HeadPivot) return;

    FVector TargetLocation = CurrentTarget->GetActorLocation();
    FVector PivotLocation = HeadPivot->GetComponentLocation();

    FVector FullDirection = TargetLocation - PivotLocation;
    float DistanceXY = FVector(FullDirection.X, FullDirection.Y, 0).Size();

    float TargetPitch = FMath::RadiansToDegrees(FMath::Atan2(FullDirection.Z, DistanceXY));

    const float MaxPitchAngle = 60.0f;
    if (FMath::Abs(TargetPitch) > MaxPitchAngle)
    {
        CurrentTarget = nullptr;
        StopFiring();
        FRotator CurrentRot = HeadPivot->GetRelativeRotation();
        FRotator TargetRot = FRotator(0.0f, 0.0f, 0.0f);
        FRotator NewRot = FMath::RInterpTo(CurrentRot, TargetRot, GetWorld()->GetDeltaSeconds(), 2.0f);
        HeadPivot->SetRelativeRotation(FRotator(NewRot.Pitch, NewRot.Yaw, 0));
        return;
    }

    FVector HorizontalDirection = FullDirection;
    HorizontalDirection.Z = 0;
    HorizontalDirection.Normalize();

    // Calculer Yaw cible en world space puis convertir en relative
    float WorldYaw = FMath::RadiansToDegrees(FMath::Atan2(HorizontalDirection.Y, HorizontalDirection.X)) + 180.0f;
    float ActorYaw = GetActorRotation().Yaw;
    float TargetYaw = WorldYaw - ActorYaw;

    FRotator CurrentRotation = HeadPivot->GetRelativeRotation();

    float LockSpeed = 15.0f;
    FRotator TargetRotation = FRotator(-TargetPitch, TargetYaw, 0);
    FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, LockSpeed);

    HeadPivot->SetRelativeRotation(NewRotation);
}

void ATDFireTurret::FireBullet()
{
    if (!CurrentTarget || CurrentAmmo <= 0) return;

    // Verifier ligne de vue
    FVector Start = HeadPivot->GetComponentLocation() - HeadPivot->GetForwardVector() * 50.0f + FVector(0, 0, 12.0f);
    FVector End = CurrentTarget->GetActorLocation();

    FHitResult HitResult;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    Params.AddIgnoredActor(CurrentTarget);

    bool bHitObstacle = GetWorld()->LineTraceSingleByChannel(
        HitResult, Start, End, ECC_Visibility, Params);

    if (bHitObstacle)
    {
        CurrentTarget = nullptr;
        StopFiring();
        bIsLockedOn = false;
        return;
    }

    // Consommer une munition
    CurrentAmmo--;

    // Infliger des degats
    DealDamageToEnemy(CurrentTarget, DamagePerBullet);

    // Visuel: 3 petites bullets courtes (effet mitrailleuse)
    {
        FVector Direction = End - Start;
        float Distance = Direction.Size();
        FVector DirNorm = Direction.GetSafeNormal();
        FRotator TracerRot = FRotationMatrix::MakeFromZ(Direction).Rotator();

        float BulletLen = Distance * 0.04f;
        float Spacing = 0.85f / 10.0f;

        for (int32 i = 0; i < 10; i++)
        {
            if (BulletTracers[i])
            {
                float Offset = 0.05f + i * Spacing;
                FVector BulletCenter = Start + DirNorm * (Distance * Offset + BulletLen * 0.5f);
                BulletTracers[i]->SetWorldLocation(BulletCenter);
                BulletTracers[i]->SetWorldRotation(TracerRot);
                BulletTracers[i]->SetWorldScale3D(FVector(0.012f, 0.012f, BulletLen / 100.0f));
                BulletTracers[i]->SetVisibility(true);
            }
        }

        GetWorldTimerManager().ClearTimer(TracerTimerHandle);
        GetWorldTimerManager().SetTimer(TracerTimerHandle, this, &ATDFireTurret::HideBulletTracers, 0.06f, false);
    }

    // Activer l'effet de flamme (reste actif tant qu'on tire)
    if (FlameEffect && !FlameEffect->IsActive())
    {
        FlameEffect->Activate(true);
    }
    // Reset le timer: si pas de tir pendant 0.3s, on coupe la flamme
    GetWorldTimerManager().ClearTimer(FlameTimerHandle);
    GetWorldTimerManager().SetTimer(FlameTimerHandle, this, &ATDFireTurret::StopFlameEffect, 0.3f, false);

    bIsFiring = true;

    // Son de tir (demarre une seule fois, arrete dans StopFiring)
    if (BulletFireSound && ShootAudioComponent && !ShootAudioComponent->IsPlaying())
    {
        ShootAudioComponent->SetSound(BulletFireSound);
        ShootAudioComponent->Play();
    }

    // Muzzle flash
    if (MuzzleFlashComponent)
    {
        MuzzleFlashComponent->Activate(true);
    }

    // PERF: SpawnEmitterAtLocation supprime (5x/sec = tres couteux)

    UE_LOG(LogTemp, Verbose, TEXT("TDFireTurret hit %s for %.1f damage | Ammo: %d/%d"),
        *CurrentTarget->GetName(), DamagePerBullet, CurrentAmmo, MaxAmmo);
}

void ATDFireTurret::StopFiring()
{
    if (bIsFiring && FireAudioComponent)
    {
        FireAudioComponent->Stop();
    }

    // Stopper le son de tir
    if (ShootAudioComponent && ShootAudioComponent->IsPlaying())
    {
        ShootAudioComponent->Stop();
    }

    if (MuzzleFlashComponent)
    {
        MuzzleFlashComponent->Deactivate();
    }

    // Couper la flamme quand on arrete de tirer
    if (FlameEffect)
    {
        FlameEffect->Deactivate();
    }
    GetWorldTimerManager().ClearTimer(FlameTimerHandle);

    // Stopper tout son residuel
    if (OneShotAudioComponent && OneShotAudioComponent->IsPlaying())
    {
        OneShotAudioComponent->Stop();
    }

    bIsFiring = false;
    FireTimer = 0.0f;
}

void ATDFireTurret::TakeDamageCustom(float DamageAmount)
{
    Health -= DamageAmount;

    UE_LOG(LogTemp, Warning, TEXT("TDFireTurret took %.1f damage, health: %.1f/%.1f"),
        DamageAmount, Health, MaxHealth);

    if (Health <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("TDFireTurret destroyed!"));
        Destroy();
    }
}

void ATDFireTurret::Upgrade()
{
    if (Level >= 3)
    {
        UE_LOG(LogTemp, Warning, TEXT("TDFireTurret already at max level!"));
        return;
    }

    Level++;

    DamagePerBullet *= 1.5f;
    Range *= 1.2f;
    MaxHealth *= 1.3f;
    Health = MaxHealth;
    FireRate *= 1.3f;
    FireInterval = 1.0f / FireRate;
    MaxAmmo = FMath::RoundToInt(MaxAmmo * 1.5f);

    UE_LOG(LogTemp, Warning, TEXT("TDFireTurret upgraded to level %d! DmgPerBullet: %.1f, Range: %.1f, FireRate: %.1f"),
        Level, DamagePerBullet, Range, FireRate);
}

void ATDFireTurret::Reload(int32 AmmoAmount)
{
    int32 AmmoNeeded = MaxAmmo - CurrentAmmo;
    int32 AmmoToAdd = FMath::Min(AmmoAmount, AmmoNeeded);
    CurrentAmmo += AmmoToAdd;

    UE_LOG(LogTemp, Warning, TEXT("TDFireTurret reloaded +%d ammo | Now: %d/%d"), AmmoToAdd, CurrentAmmo, MaxAmmo);
}

void ATDFireTurret::HideBulletTracers()
{
    for (int32 i = 0; i < 10; i++)
    {
        if (BulletTracers[i])
        {
            BulletTracers[i]->SetVisibility(false);
        }
    }
}

void ATDFireTurret::StopFlameEffect()
{
    if (FlameEffect)
    {
        FlameEffect->Deactivate();
    }
}

// IFGUseableInterface
bool ATDFireTurret::IsUseable_Implementation() const
{
    return true;
}

FText ATDFireTurret::GetLookAtDecription_Implementation(AFGCharacterPlayer* byCharacter, const FUseState& state) const
{
    if (CurrentAmmo >= MaxAmmo)
    {
        return FText::FromString(FString::Printf(TEXT("Fire Turret - Ammo: %d/%d (FULL)"), CurrentAmmo, MaxAmmo));
    }
    return FText::FromString(FString::Printf(TEXT("Fire Turret - Ammo: %d/%d\nPress [E] to reload"), CurrentAmmo, MaxAmmo));
}

void ATDFireTurret::OnUse_Implementation(AFGCharacterPlayer* byCharacter, const FUseState& state)
{
    if (!byCharacter || !AmmoItemClass) return;
    if (CurrentAmmo >= MaxAmmo) return;

    UFGInventoryComponent* PlayerInv = byCharacter->GetInventory();
    if (!PlayerInv) return;

    int32 AmmoNeeded = MaxAmmo - CurrentAmmo;
    int32 AmmoAvailable = PlayerInv->GetNumItems(AmmoItemClass);

    if (AmmoAvailable <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("TDFireTurret: Player has no cartridges!"));
        return;
    }

    int32 AmmoToConsume = FMath::Min(AmmoNeeded, AmmoAvailable);
    PlayerInv->Remove(AmmoItemClass, AmmoToConsume);
    Reload(AmmoToConsume);

    UE_LOG(LogTemp, Warning, TEXT("TDFireTurret: Consumed %d cartridges, ammo now %d/%d"), AmmoToConsume, CurrentAmmo, MaxAmmo);
}

void ATDFireTurret::UpdateIdleAnimation(float DeltaTime)
{
    if (!HeadPivot) return;

    if (IdleSound && FireAudioComponent && !FireAudioComponent->IsPlaying())
    {
        FireAudioComponent->SetSound(IdleSound);
        FireAudioComponent->SetVolumeMultiplier(0.025f);
        FireAudioComponent->bAllowSpatialization = true;
        FireAudioComponent->Play();
    }

    IdleTimer += DeltaTime;

    if (IdleTimer > 4.0f)
    {
        IdleTimer = 0.0f;
        IdleLookDirection *= -1.0f;
    }

    float TargetYawOffset = IdleLookDirection * 30.0f;
    IdleYawOffset = FMath::FInterpTo(IdleYawOffset, TargetYawOffset, DeltaTime, 0.5f);

    FRotator CurrentRot = HeadPivot->GetRelativeRotation();
    float IdleSpeed = 1.5f;
    float NewYaw = FMath::FInterpTo(CurrentRot.Yaw, IdleYawOffset, DeltaTime, IdleSpeed);
    float NewPitch = FMath::FInterpTo(CurrentRot.Pitch, 0.0f, DeltaTime, IdleSpeed);

    HeadPivot->SetRelativeRotation(FRotator(NewPitch, NewYaw, 0));
}
