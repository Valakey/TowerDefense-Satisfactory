#include "TDTurret.h"
#include "TDEnemy.h"
#include "TDEnemyFlying.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"

ATDTurret::ATDTurret(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = true;
    mFactoryTickFunction.bCanEverTick = true;

    // Creer la base (mesh custom)
    BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
    BaseMesh->SetupAttachment(RootComponent);
    BaseMesh->SetRelativeLocation(FVector(0, 0, -70));  // Baisser au sol
    
    static ConstructorHelpers::FObjectFinder<UStaticMesh> BaseTurretMesh(TEXT("/MonPremierMod/Meshes/Turrets/LaserTurret/Base/baseTurret.baseTurret"));
    if (BaseTurretMesh.Succeeded())
    {
        BaseMesh->SetStaticMesh(BaseTurretMesh.Object);
        BaseMesh->SetRelativeScale3D(FVector(0.8f, 0.8f, 0.6f));  // Base plus basse
        UE_LOG(LogTemp, Warning, TEXT("TDTurret: Base mesh charge!"));
    }
    
    // Charger materiau base
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BaseMat(TEXT("/MonPremierMod/Meshes/Turrets/LaserTurret/Base/Material_001.Material_001"));
    if (BaseMat.Succeeded())
    {
        BaseMesh->SetMaterial(0, BaseMat.Object);
    }

    // Creer le pivot central pour la rotation (point fixe au centre du socle)
    HeadPivot = CreateDefaultSubobject<USceneComponent>(TEXT("HeadPivot"));
    HeadPivot->SetupAttachment(BaseMesh);
    HeadPivot->SetRelativeLocation(FVector(0, 0, 110));  // Centre au-dessus du socle
    
    // Creer la tete de tourelle attachee au pivot
    TurretHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretHead"));
    TurretHead->SetupAttachment(HeadPivot);  // Attacher au pivot, pas au socle
    TurretHead->SetRelativeLocation(FVector(-25, 0, 0));  // Avancee par rapport au pivot
    TurretHead->SetRelativeRotation(FRotator(0, 0, 0));  // Pas de rotation initiale
    
    static ConstructorHelpers::FObjectFinder<UStaticMesh> HeadTurretMesh(TEXT("/MonPremierMod/Meshes/Turrets/LaserTurret/Head/head_turret.head_turret"));
    if (HeadTurretMesh.Succeeded())
    {
        TurretHead->SetStaticMesh(HeadTurretMesh.Object);
        TurretHead->SetRelativeScale3D(FVector(0.7f, 0.7f, 0.7f));  // Head plus petite
        UE_LOG(LogTemp, Warning, TEXT("TDTurret: Head mesh charge!"));
    }
    
    // Charger materiau head
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> HeadMat(TEXT("/MonPremierMod/Meshes/Turrets/LaserTurret/Head/Material_001.Material_001"));
    if (HeadMat.Succeeded())
    {
        TurretHead->SetMaterial(0, HeadMat.Object);
    }

    // Creer le laser (cube tres fin et long)
    LaserBeam = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LaserBeam"));
    LaserBeam->SetupAttachment(TurretHead);
    LaserBeam->SetRelativeLocation(FVector(100, 0, 0));
    
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube"));
    if (CubeMesh.Succeeded())
    {
        LaserBeam->SetStaticMesh(CubeMesh.Object);
        LaserBeam->SetRelativeScale3D(FVector(2.0f, 0.10f, 0.10f));  // Plus gros laser
    }
    
    // IMPORTANT: Desactiver les collisions du laser pour ne pas pousser les ennemis
    LaserBeam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LaserBeam->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
    
    // Materiau rouge pour le laser
    static ConstructorHelpers::FObjectFinder<UMaterial> RedMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
    if (RedMaterial.Succeeded())
    {
        LaserBeam->SetMaterial(0, RedMaterial.Object);
    }
    
    LaserBeam->SetVisibility(false);  // Cache par defaut

    // Creer le composant audio pour le laser avec attenuation 3D
    LaserAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("LaserAudio"));
    LaserAudioComponent->SetupAttachment(HeadPivot);
    LaserAudioComponent->bAutoActivate = false;
    LaserAudioComponent->bAllowSpatialization = true;
    LaserAudioComponent->bOverrideAttenuation = true;
    LaserAudioComponent->AttenuationOverrides.bAttenuate = true;
    LaserAudioComponent->AttenuationOverrides.bSpatialize = true;
    LaserAudioComponent->AttenuationOverrides.FalloffDistance = 2000.0f;  // 20 metres
    LaserAudioComponent->AttenuationOverrides.AttenuationShapeExtents = FVector(200.0f, 0.0f, 0.0f);

    // Creer le composant audio pour sons one-shot (LockOn, Charge) avec attenuation 3D
    OneShotAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("OneShotAudio"));
    OneShotAudioComponent->SetupAttachment(HeadPivot);
    OneShotAudioComponent->bAutoActivate = false;
    OneShotAudioComponent->bAllowSpatialization = true;
    OneShotAudioComponent->bOverrideAttenuation = true;
    OneShotAudioComponent->AttenuationOverrides.bAttenuate = true;
    OneShotAudioComponent->AttenuationOverrides.bSpatialize = true;
    OneShotAudioComponent->AttenuationOverrides.FalloffDistance = 2000.0f;  // 20 metres
    OneShotAudioComponent->AttenuationOverrides.AttenuationShapeExtents = FVector(200.0f, 0.0f, 0.0f);

    // Creer le composant muzzle flash
    MuzzleFlashComponent = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("MuzzleFlash"));
    MuzzleFlashComponent->SetupAttachment(HeadPivot);
    MuzzleFlashComponent->bAutoActivate = false;

    // Composants electricite
    PowerConnection = CreateDefaultSubobject<UFGPowerConnectionComponent>(TEXT("PowerConnection"));
    PowerConnection->SetupAttachment(BaseMesh);
    PowerConnection->SetRelativeLocation(FVector(0, 0, 50.0f));  // Point de connexion visible

    PowerInfo = CreateDefaultSubobject<UFGPowerInfoComponent>(TEXT("PowerInfo"));

    // Charger les sons
    static ConstructorHelpers::FObjectFinder<USoundBase> LaserFireSoundObj(TEXT("/MonPremierMod/Audios/Turret/LaserFireSound.LaserFireSound"));
    if (LaserFireSoundObj.Succeeded())
    {
        LaserFireSound = LaserFireSoundObj.Object;
        UE_LOG(LogTemp, Warning, TEXT("TDTurret: LaserFireSound LOADED!"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("TDTurret: LaserFireSound FAILED to load!"));
    }

    static ConstructorHelpers::FObjectFinder<USoundBase> LockOnSoundObj(TEXT("/MonPremierMod/Audios/Turret/LockOnSound.LockOnSound"));
    if (LockOnSoundObj.Succeeded())
    {
        LockOnSound = LockOnSoundObj.Object;
        UE_LOG(LogTemp, Warning, TEXT("TDTurret: LockOnSound LOADED!"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("TDTurret: LockOnSound FAILED to load!"));
    }

    static ConstructorHelpers::FObjectFinder<USoundBase> LaserChargeSoundObj(TEXT("/MonPremierMod/Audios/Turret/LaserChargeSound.LaserChargeSound"));
    if (LaserChargeSoundObj.Succeeded())
    {
        LaserChargeSound = LaserChargeSoundObj.Object;
        UE_LOG(LogTemp, Warning, TEXT("TDTurret: LaserChargeSound LOADED!"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("TDTurret: LaserChargeSound FAILED to load!"));
    }

    static ConstructorHelpers::FObjectFinder<USoundBase> IdleSoundObj(TEXT("/MonPremierMod/Audios/Turret/IdleSound.IdleSound"));
    if (IdleSoundObj.Succeeded())
    {
        IdleSound = IdleSoundObj.Object;
        UE_LOG(LogTemp, Warning, TEXT("TDTurret: IdleSound LOADED!"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("TDTurret: IdleSound FAILED to load!"));
    }
}

void ATDTurret::BeginPlay()
{
    Super::BeginPlay();
    
    UE_LOG(LogTemp, Warning, TEXT("TDTurret spawned at %s"), *GetActorLocation().ToString());

    // Configurer le systeme electrique
    if (PowerConnection && PowerInfo)
    {
        PowerConnection->SetPowerInfo(PowerInfo);
        PowerInfo->SetTargetConsumption(PowerConsumption);
        UE_LOG(LogTemp, Warning, TEXT("TDTurret: Power system initialized (%.1f MW)"), PowerConsumption);
    }
    
    // Creer un materiau dynamique pour le laser (rouge)
    if (LaserBeam)
    {
        UMaterialInstanceDynamic* LaserMat = UMaterialInstanceDynamic::Create(LaserBeam->GetMaterial(0), this);
        if (LaserMat)
        {
            LaserMat->SetVectorParameterValue(TEXT("Color"), FLinearColor::Red);
            LaserBeam->SetMaterial(0, LaserMat);
        }
    }
    
    // Les materiaux custom sont deja appliques dans le constructeur

    // IMPORTANT: Forcer le tick actif (AFGBuildable peut le desactiver)
    PrimaryActorTick.SetTickFunctionEnable(true);
    PrimaryActorTick.bCanEverTick = true;
    SetActorTickEnabled(true);
    UE_LOG(LogTemp, Warning, TEXT("TDTurret: PrimaryActorTick FORCE ENABLED"));
}

void ATDTurret::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Verifier l'electricite
    CheckPowerStatus();

    // Debug log toutes les 2 secondes
    static float DebugTimer = 0.0f;
    DebugTimer += DeltaTime;
    if (DebugTimer > 2.0f)
    {
        DebugTimer = 0.0f;
        FRotator HeadRot = HeadPivot ? HeadPivot->GetRelativeRotation() : FRotator::ZeroRotator;
        UE_LOG(LogTemp, Warning, TEXT("TDTurret %s: bHasPower=%d | HeadPitch=%.1f HeadYaw=%.1f | PowerInfo=%d PowerConn=%d"),
            *GetName(), bHasPower, HeadRot.Pitch, HeadRot.Yaw,
            PowerInfo ? PowerInfo->HasPower() : -1,
            PowerConnection ? PowerConnection->HasPower() : -1);
    }

    if (!bHasPower)
    {
        StopLaser();
        bIsActive = false;
        CurrentTarget = nullptr;
        bIsLockedOn = false;

        // Tete vers le bas pour montrer que la tourelle est eteinte
        if (HeadPivot)
        {
            FRotator CurrentRot = HeadPivot->GetRelativeRotation();
            FRotator TargetRot = FRotator(55.0f, CurrentRot.Yaw, 0.0f);  // Pitch 55 = tete vers le bas
            FRotator NewRot = FMath::RInterpTo(CurrentRot, TargetRot, DeltaTime, 3.0f);
            HeadPivot->SetRelativeRotation(NewRot);
        }
        return;
    }

    // Sauvegarder cible precedente pour detecter changement
    AActor* PreviousTarget = CurrentTarget;
    
    // Chercher une cible si on n'en a pas ou si elle est morte/hors portee
    if (!CurrentTarget || !IsValid(CurrentTarget))
    {
        CurrentTarget = FindBestTarget();
    }
    else
    {
        // Verifier que la cible est toujours valide et a portee
        float DistanceToTarget = FVector::Dist(GetActorLocation(), CurrentTarget->GetActorLocation());
        if (DistanceToTarget > Range || IsEnemyDead(CurrentTarget))
        {
            CurrentTarget = FindBestTarget();
        }
    }
    
    // Reset lock-on si la cible a change (pour rejouer les sons)
    if (CurrentTarget != PreviousTarget)
    {
        bIsLockedOn = false;
        bLaserActive = false;
    }

    if (CurrentTarget)
    {
        bIsActive = true;
        
        // Gerer le delai de verrouillage avant tir
        if (!bIsLockedOn)
        {
            bIsLockedOn = true;
            LockOnTimer = 0.0f;
            // Jouer son de verrouillage via composant avec attenuation 3D
            if (LockOnSound && OneShotAudioComponent)
            {
                OneShotAudioComponent->SetSound(LockOnSound);
                OneShotAudioComponent->SetVolumeMultiplier(0.25f);
                OneShotAudioComponent->Play();
            }
            // Jouer son de precharge laser via composant avec attenuation 3D
            if (LaserChargeSound && LaserAudioComponent)
            {
                LaserAudioComponent->SetSound(LaserChargeSound);
                LaserAudioComponent->SetVolumeMultiplier(0.25f);
                LaserAudioComponent->Play();
            }
        }
        
        LockOnTimer += DeltaTime;
        RotateTowardsTarget(DeltaTime);
        
        // Tirer seulement apres le delai de verrouillage
        if (LockOnTimer >= LockOnDelay)
        {
            FireLaser(DeltaTime);
        }
        else
        {
            StopLaser();
        }
    }
    else
    {
        bIsActive = false;
        bIsLockedOn = false;
        LockOnTimer = 0.0f;
        StopLaser();
        
        // Animation idle quand pas de cible
        UpdateIdleAnimation(DeltaTime);
    }
}

void ATDTurret::Factory_Tick(float dt)
{
    Super::Factory_Tick(dt);
    // Logique dans Tick() car Factory_Tick tourne en parallele (worker thread)
}

void ATDTurret::CheckPowerStatus()
{
    bool bPreviousPower = bHasPower;
    bHasPower = false;

    // Verifier via PowerInfo directement
    if (PowerInfo && PowerInfo->HasPower())
    {
        bHasPower = true;
    }
    // Verifier via PowerConnection
    else if (PowerConnection && PowerConnection->HasPower())
    {
        bHasPower = true;
    }

    // Log changement d'etat
    if (bHasPower != bPreviousPower)
    {
        UE_LOG(LogTemp, Warning, TEXT("TDTurret %s: Power state changed -> %s"), *GetName(), bHasPower ? TEXT("ON") : TEXT("OFF"));
    }
}

AActor* ATDTurret::FindBestTarget()
{
    AActor* BestTarget = nullptr;
    float BestDistance = Range;
    
    // Lambda pour tester un candidat (evite duplication)
    auto TryCandidate = [&](AActor* Candidate)
    {
        if (!Candidate || !IsValid(Candidate)) return;
        if (IsEnemyDead(Candidate)) return;
        
        float Distance = FVector::Dist(GetActorLocation(), Candidate->GetActorLocation());
        if (Distance >= BestDistance) return;
        
        // Verifier ligne de vue
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
    
    // Chercher les ennemis au sol
    for (TActorIterator<ATDEnemy> It(GetWorld()); It; ++It)
    {
        TryCandidate(*It);
    }
    
    // Chercher les ennemis volants
    for (TActorIterator<ATDEnemyFlying> It(GetWorld()); It; ++It)
    {
        TryCandidate(*It);
    }
    
    return BestTarget;
}

bool ATDTurret::IsEnemyDead(AActor* Enemy)
{
    if (!Enemy || !IsValid(Enemy)) return true;
    
    if (ATDEnemy* Ground = Cast<ATDEnemy>(Enemy))
        return Ground->bIsDead;
    if (ATDEnemyFlying* Flying = Cast<ATDEnemyFlying>(Enemy))
        return Flying->bIsDead;
    
    return true;
}

void ATDTurret::DealDamageToEnemy(AActor* Enemy, float DmgAmount)
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
}

void ATDTurret::RotateTowardsTarget(float DeltaTime)
{
    if (!CurrentTarget || !HeadPivot) return;
    
    FVector TargetLocation = CurrentTarget->GetActorLocation();
    FVector PivotLocation = HeadPivot->GetComponentLocation();
    
    // Direction complete vers la cible (avec hauteur)
    FVector FullDirection = TargetLocation - PivotLocation;
    float DistanceXY = FVector(FullDirection.X, FullDirection.Y, 0).Size();
    
    // Calculer le Pitch cible (angle vertical)
    float TargetPitch = FMath::RadiansToDegrees(FMath::Atan2(FullDirection.Z, DistanceXY));
    
    // Limite de 60 degres max en haut ou en bas
    const float MaxPitchAngle = 60.0f;
    if (FMath::Abs(TargetPitch) > MaxPitchAngle)
    {
        // Ennemi hors angle de tir - perdre l'aggro et reset rotation
        CurrentTarget = nullptr;
        StopLaser();
        // Reset rotation vers position neutre
        FRotator CurrentRot = HeadPivot->GetRelativeRotation();
        FRotator TargetRot = FRotator(0.0f, 0.0f, 0.0f);
        FRotator NewRot = FMath::RInterpTo(CurrentRot, TargetRot, GetWorld()->GetDeltaSeconds(), 2.0f);
        float ResetYaw = NewRot.Yaw;
        float ResetPitch = NewRot.Pitch;
        HeadPivot->SetRelativeRotation(FRotator(ResetPitch, ResetYaw, 0));
        return;
    }
    
    // Calculer direction horizontale pour le Yaw
    FVector HorizontalDirection = FullDirection;
    HorizontalDirection.Z = 0;
    HorizontalDirection.Normalize();
    
    // Calculer Yaw cible en world space puis convertir en relative (soustraire rotation acteur)
    float WorldYaw = FMath::RadiansToDegrees(FMath::Atan2(HorizontalDirection.Y, HorizontalDirection.X)) + 180.0f;
    float ActorYaw = GetActorRotation().Yaw;
    float TargetYaw = WorldYaw - ActorYaw;
    
    // Rotation actuelle du pivot
    FRotator CurrentRotation = HeadPivot->GetRelativeRotation();
    
    // Interpoler vers le Yaw et Pitch cibles (rotation RAPIDE au verrouillage)
    // Utiliser RInterpTo pour eviter les rotations 360 degres
    float LockSpeed = 15.0f;  // Vitesse rapide pour verrouillage
    FRotator TargetRotation = FRotator(-TargetPitch, TargetYaw, 0);  // Inverser Pitch
    FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, LockSpeed);
    
    // Appliquer rotation au PIVOT (Yaw horizontal + Pitch vertical)
    HeadPivot->SetRelativeRotation(NewRotation);
}

void ATDTurret::FireLaser(float DeltaTime)
{
    if (!CurrentTarget) return;
    
    // Verifier ligne de vue avant de tirer (obstacle entre tourelle et cible?)
    FVector Start = HeadPivot->GetComponentLocation();
    FVector End = CurrentTarget->GetActorLocation();
    
    FHitResult HitResult;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    Params.AddIgnoredActor(CurrentTarget);
    
    bool bHitObstacle = GetWorld()->LineTraceSingleByChannel(
        HitResult,
        Start,
        End,
        ECC_Visibility,
        Params
    );
    
    // Si obstacle detecte, perdre la cible
    if (bHitObstacle)
    {
        CurrentTarget = nullptr;
        StopLaser();
        bIsLockedOn = false;
        bLaserActive = false;
        return;
    }
    
    // Mettre a jour le timer de degats
    DamageTimer += DeltaTime;
    
    // Infliger des degats periodiquement
    if (DamageTimer >= DamageInterval)
    {
        float DamageThisTick = DamagePerSecond * DamageInterval;
        DealDamageToEnemy(CurrentTarget, DamageThisTick);
        DamageTimer = 0.0f;
        
        UE_LOG(LogTemp, Verbose, TEXT("TDTurret hit %s for %.1f damage"), 
            *CurrentTarget->GetName(), DamageThisTick);
    }
    
    // Mettre a jour le visuel du laser
    LaserEndPoint = CurrentTarget->GetActorLocation();
    UpdateLaserVisual();
}

void ATDTurret::UpdateLaserVisual()
{
    if (!LaserBeam) return;
    
    // Point de depart du laser - pointe du canon
    FVector Start = HeadPivot->GetComponentLocation() - HeadPivot->GetForwardVector() * 75.0f + FVector(0, 0, -4.0f);
    FVector End = LaserEndPoint;
    
    // Calculer la longueur et la position du laser
    float LaserLength = FVector::Dist(Start, End);
    FVector LaserCenter = (Start + End) / 2.0f;
    FVector LaserDirection = (End - Start).GetSafeNormal();
    
    // Positionner et orienter le laser
    LaserBeam->SetWorldLocation(LaserCenter);
    LaserBeam->SetWorldRotation(LaserDirection.Rotation());
    LaserBeam->SetWorldScale3D(FVector(LaserLength / 100.0f, 0.02f, 0.02f));  // Laser tres fin
    
    LaserBeam->SetVisibility(true);
    
    // Demarrer le son du laser si pas deja actif (3D spatialise, volume 0.5)
    if (!bLaserActive)
    {
        if (LaserAudioComponent && LaserFireSound)
        {
            LaserAudioComponent->SetSound(LaserFireSound);
            LaserAudioComponent->SetVolumeMultiplier(0.25f);
            LaserAudioComponent->bAllowSpatialization = true;
            LaserAudioComponent->Play();
        }
        // Activer muzzle flash
        if (MuzzleFlashComponent)
        {
            MuzzleFlashComponent->Activate(true);
        }
    }
    bLaserActive = true;
    
    // Spawn particules d'impact sur la cible
    if (ImpactParticle)
    {
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactParticle, End, FRotator::ZeroRotator, FVector(0.5f), true);
    }
    
    // Debug: dessiner une ligne rouge
    DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, 0.0f, 0, 2.0f);
}

void ATDTurret::StopLaser()
{
    if (LaserBeam)
    {
        LaserBeam->SetVisibility(false);
    }
    
    // Arreter le son du laser
    if (bLaserActive && LaserAudioComponent)
    {
        LaserAudioComponent->Stop();
    }
    
    // Desactiver muzzle flash
    if (MuzzleFlashComponent)
    {
        MuzzleFlashComponent->Deactivate();
    }
    
    bLaserActive = false;
    DamageTimer = 0.0f;
}

void ATDTurret::TakeDamageCustom(float DamageAmount)
{
    Health -= DamageAmount;
    
    UE_LOG(LogTemp, Warning, TEXT("TDTurret took %.1f damage, health: %.1f/%.1f"), 
        DamageAmount, Health, MaxHealth);
    
    if (Health <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("TDTurret destroyed!"));
        Destroy();
    }
}

void ATDTurret::Upgrade()
{
    if (Level >= 3)
    {
        UE_LOG(LogTemp, Warning, TEXT("TDTurret already at max level!"));
        return;
    }
    
    Level++;
    
    // Augmenter les stats
    DamagePerSecond *= 1.5f;
    Range *= 1.2f;
    MaxHealth *= 1.3f;
    Health = MaxHealth;
    PowerConsumption *= 1.2f;
    
    UE_LOG(LogTemp, Warning, TEXT("TDTurret upgraded to level %d! DPS: %.1f, Range: %.1f"), 
        Level, DamagePerSecond, Range);
}

void ATDTurret::UpdateIdleAnimation(float DeltaTime)
{
    if (!HeadPivot) return;
    
    // Jouer son idle a faible volume (0.05) et 3D spatialise
    if (IdleSound && LaserAudioComponent && !LaserAudioComponent->IsPlaying())
    {
        LaserAudioComponent->SetSound(IdleSound);
        LaserAudioComponent->SetVolumeMultiplier(0.025f);
        LaserAudioComponent->bAllowSpatialization = true;
        LaserAudioComponent->Play();
    }
    
    // Timer pour changer de direction de temps en temps
    IdleTimer += DeltaTime;
    
    // Changer de direction toutes les 3-5 secondes
    if (IdleTimer > 4.0f)
    {
        IdleTimer = 0.0f;
        IdleLookDirection *= -1.0f;  // Inverser direction
    }
    
    // Calculer l'offset cible (regarder a 30 degres de chaque cote)
    float TargetYawOffset = IdleLookDirection * 30.0f;
    
    // Interpoler smooth vers la position cible
    IdleYawOffset = FMath::FInterpTo(IdleYawOffset, TargetYawOffset, DeltaTime, 0.5f);
    
    // Appliquer la rotation idle (smooth)
    FRotator CurrentRot = HeadPivot->GetRelativeRotation();
    float IdleSpeed = 1.5f;  // Vitesse lente pour idle
    float NewYaw = FMath::FInterpTo(CurrentRot.Yaw, IdleYawOffset, DeltaTime, IdleSpeed);
    float NewPitch = FMath::FInterpTo(CurrentRot.Pitch, 0.0f, DeltaTime, IdleSpeed);
    
    HeadPivot->SetRelativeRotation(FRotator(NewPitch, NewYaw, 0));
}
