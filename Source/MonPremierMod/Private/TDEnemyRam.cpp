#include "TDEnemyRam.h"
#include "TDCreatureSpawner.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

ATDEnemyRam::ATDEnemyRam()
{
    PrimaryActorTick.bCanEverTick = true;

    // Collision sphere
    CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
    CollisionSphere->InitSphereRadius(60.0f);
    CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    CollisionSphere->SetCollisionObjectType(ECC_Pawn);
    CollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    CollisionSphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    CollisionSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
    CollisionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    CollisionSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    CollisionSphere->SetGenerateOverlapEvents(true);
    CollisionSphere->SetSimulatePhysics(false);
    SetRootComponent(CollisionSphere);

    // Mesh visible - Belier
    VisibleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisibleMesh"));
    VisibleMesh->SetupAttachment(RootComponent);
    VisibleMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
    VisibleMesh->SetRelativeScale3D(FVector(0.7f, 0.7f, 0.7f));
    VisibleMesh->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
    VisibleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    VisibleMesh->SetCastShadow(true);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> RamMesh(TEXT("/MonPremierMod/Meshes/Ennemy/c_0155/C_0155.C_0155"));
    if (RamMesh.Succeeded())
    {
        VisibleMesh->SetStaticMesh(RamMesh.Object);
    }

    // Charger le systeme Niagara de flammes (meme que le volant)
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> FlameSystemObj(TEXT("/MonPremierMod/Particles/NS_StylizedFireEnnemy.NS_StylizedFireEnnemy"));
    if (FlameSystemObj.Succeeded())
    {
        FlameNiagaraSystem = FlameSystemObj.Object;
    }

    // Propulseur (sous/arriere du belier, actif pendant la charge)
    ThrusterEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ThrusterEffect"));
    ThrusterEffect->SetupAttachment(RootComponent);
    ThrusterEffect->SetRelativeLocation(FVector(-80.0f, 0.0f, -20.0f));
    ThrusterEffect->SetRelativeScale3D(FVector(1.5f, 1.5f, 1.5f));
    ThrusterEffect->bAutoActivate = false;

    // Etoiles de stun (3 etoiles tournantes)
    static ConstructorHelpers::FObjectFinder<UStaticMesh> StunMesh(TEXT("/MonPremierMod/Meshes/Items/StuntEffet/StuntEffet.StuntEffet"));

    StunStar1 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StunStar1"));
    StunStar1->SetupAttachment(RootComponent);
    StunStar1->SetRelativeScale3D(FVector(0.3f, 0.3f, 0.3f));
    StunStar1->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    StunStar1->SetCastShadow(false);
    StunStar1->SetVisibility(false);

    StunStar2 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StunStar2"));
    StunStar2->SetupAttachment(RootComponent);
    StunStar2->SetRelativeScale3D(FVector(0.3f, 0.3f, 0.3f));
    StunStar2->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    StunStar2->SetCastShadow(false);
    StunStar2->SetVisibility(false);

    StunStar3 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StunStar3"));
    StunStar3->SetupAttachment(RootComponent);
    StunStar3->SetRelativeScale3D(FVector(0.3f, 0.3f, 0.3f));
    StunStar3->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    StunStar3->SetCastShadow(false);
    StunStar3->SetVisibility(false);

    if (StunMesh.Succeeded())
    {
        StunStar1->SetStaticMesh(StunMesh.Object);
        StunStar2->SetStaticMesh(StunMesh.Object);
        StunStar3->SetStaticMesh(StunMesh.Object);
    }

    // Audio
    OneShotAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("OneShotAudio"));
    OneShotAudioComponent->SetupAttachment(RootComponent);
    OneShotAudioComponent->bAutoActivate = false;
    OneShotAudioComponent->bAllowSpatialization = true;
    OneShotAudioComponent->bOverrideAttenuation = true;

    HoverAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("HoverAudio"));
    HoverAudioComponent->SetupAttachment(RootComponent);
    HoverAudioComponent->bAutoActivate = false;
    HoverAudioComponent->bAllowSpatialization = true;
    HoverAudioComponent->bOverrideAttenuation = true;

    // Charger les sons
    static ConstructorHelpers::FObjectFinder<USoundWave> ChargeSoundObj(TEXT("/MonPremierMod/Audios/Ennemy/CuteBot.CuteBot"));
    if (ChargeSoundObj.Succeeded()) ChargeSound = ChargeSoundObj.Object;

    static ConstructorHelpers::FObjectFinder<USoundWave> StunSoundObj(TEXT("/MonPremierMod/Audios/Ennemy/robotic-oops.robotic-oops"));
    if (StunSoundObj.Succeeded()) StunSound = StunSoundObj.Object;

    static ConstructorHelpers::FObjectFinder<USoundWave> TargetLockSoundObj(TEXT("/MonPremierMod/Audios/Ennemy/Roboth.Roboth"));
    if (TargetLockSoundObj.Succeeded()) TargetLockSound = TargetLockSoundObj.Object;
}

void ATDEnemyRam::BeginPlay()
{
    Super::BeginPlay();

    OriginalMoveSpeed = MoveSpeed;
    
    // Ground trace au spawn pour coller au sol immediatement
    FVector SpawnLoc = GetActorLocation();
    FHitResult GroundHit;
    FCollisionQueryParams TraceParams;
    TraceParams.AddIgnoredActor(this);
    if (GetWorld()->LineTraceSingleByChannel(GroundHit, SpawnLoc + FVector(0,0,500), SpawnLoc - FVector(0,0,5000), ECC_WorldStatic, TraceParams))
    {
        BaseHeight = GroundHit.ImpactPoint.Z + 50.0f;
        SpawnLoc.Z = BaseHeight;
        SetActorLocation(SpawnLoc);
    }
    else
    {
        BaseHeight = SpawnLoc.Z;
    }

    // Activer le propulseur en mode idle (desactive, on l'active a la charge)
    if (ThrusterEffect && FlameNiagaraSystem)
    {
        ThrusterEffect->SetAsset(FlameNiagaraSystem);
    }

    // Custom Depth pour outline
    if (VisibleMesh)
    {
        VisibleMesh->SetRenderCustomDepth(true);
        VisibleMesh->SetCustomDepthStencilValue(1);
    }

    UE_LOG(LogTemp, Warning, TEXT("TDEnemyRam: Spawn! HP=%.0f, ChargeDmg=%.0f, Speed=%.0f"), Health, ChargeDamage, MoveSpeed);
}

void ATDEnemyRam::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bIsDead || !GetWorld()) return;

    // === ATTERRISSAGE: calculer chemin apres premier contact sol ===
    if (!bHasLanded)
    {
        // Le Ram trace le sol chaque frame - on attend le premier trace reussi
        FHitResult LandCheck;
        FCollisionQueryParams LandParams;
        LandParams.AddIgnoredActor(this);
        FVector Loc = GetActorLocation();
        if (GetWorld()->LineTraceSingleByChannel(LandCheck, Loc + FVector(0,0,200), Loc - FVector(0,0,500), ECC_WorldStatic, LandParams))
        {
            // Sol detecte sous nous - on est pose
            bHasLanded = true;
            UE_LOG(LogTemp, Warning, TEXT("TDEnemyRam %s: ATTERRI a %s, demande chemin..."), *GetName(), *Loc.ToString());
            
            if (TargetBuilding && IsValid(TargetBuilding))
            {
                ATDCreatureSpawner* Spawner = nullptr;
                for (TActorIterator<ATDCreatureSpawner> SpIt(GetWorld()); SpIt; ++SpIt) { Spawner = *SpIt; break; }
                if (Spawner)
                {
                    TArray<FVector> Path = Spawner->GetGroundPathFor(Loc, TargetBuilding->GetActorLocation());
                    if (Path.Num() > 0)
                    {
                        Waypoints = Path;
                        CurrentWaypointIndex = 0;
                        UE_LOG(LogTemp, Warning, TEXT("  -> %d waypoints recus"), Path.Num());
                    }
                }
            }
        }
    }

    // Slow timer
    if (SlowTimer > 0.0f)
    {
        SlowTimer -= DeltaTime;
        if (SlowTimer <= 0.0f)
        {
            SpeedMultiplier = 1.0f;
            MoveSpeed = OriginalMoveSpeed;
        }
    }

    // Timer sans attaque -> teleport si VRAIMENT bloque (pas de cible ET pas d'attaque)
    if (TargetBuilding && IsValid(TargetBuilding))
    {
        // A une cible -> reset si en approche/preparation (progresse)
        if (CurrentState == ERamState::Approaching || CurrentState == ERamState::Preparing)
        {
            NoAttackTimer = 0.0f;
        }
        else
        {
            NoAttackTimer += DeltaTime;
        }
    }
    else
    {
        NoAttackTimer += DeltaTime;
    }
    
    if (NoAttackTimer >= TeleportTimeout && CurrentState != ERamState::Charging && CurrentState != ERamState::Falling && CurrentState != ERamState::Approaching && CurrentState != ERamState::Preparing)
    {
        TeleportToBase();
        NoAttackTimer = 0.0f;
    }

    // Coller au sol via ground trace (sauf pendant la charge et la chute)
    if (CurrentState != ERamState::Charging && CurrentState != ERamState::Falling)
    {
        FVector Loc = GetActorLocation();
        
        // Ground trace pour trouver le sol (ignorer les ennemis)
        FHitResult GroundHit;
        FVector TraceStart = Loc + FVector(0, 0, 200.0f);
        FVector TraceEnd = Loc - FVector(0, 0, 2000.0f);
        FCollisionQueryParams TraceParams;
        TraceParams.AddIgnoredActor(this);
        TraceParams.bIgnoreBlocks = false;
        // Ignorer tous les Pawns (ennemis au sol, volants, rams)
        for (TActorIterator<APawn> PIt(GetWorld()); PIt; ++PIt)
        {
            TraceParams.AddIgnoredActor(*PIt);
        }
        
        if (GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_WorldStatic, TraceParams))
        {
            BaseHeight = GroundHit.ImpactPoint.Z + 50.0f;  // 50u au-dessus du sol (ras du sol)
        }
        
        // Petit bob subtil (5u seulement)
        BobTimer += DeltaTime;
        float BobOffset = FMath::Sin(BobTimer * BobFrequency * 2.0f * PI) * BobAmplitude;
        Loc.Z = BaseHeight + BobOffset;
        SetActorLocation(Loc);
    }

    // Invalider les cibles detruites AVANT la machine a etats + vider waypoints
    if (TargetBuilding && !IsValid(TargetBuilding))
    {
        TargetBuilding = nullptr;
        Waypoints.Empty();
        CurrentWaypointIndex = 0;
    }
    if (PrimaryTarget && !IsValid(PrimaryTarget))
    {
        PrimaryTarget = nullptr;
    }
    
    // Invalider les cibles marquees comme detruites (structures)
    {
        TArray<AActor*> Spawners;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ATDCreatureSpawner::StaticClass(), Spawners);
        if (Spawners.Num() > 0)
        {
            ATDCreatureSpawner* Spawner = Cast<ATDCreatureSpawner>(Spawners[0]);
            if (Spawner)
            {
                if (TargetBuilding && Spawner->IsBuildingDestroyed(TargetBuilding))
                {
                    TargetBuilding = nullptr;
                }
                if (PrimaryTarget && Spawner->IsBuildingDestroyed(PrimaryTarget))
                {
                    PrimaryTarget = nullptr;
                }
            }
        }
    }

    // Machine a etats
    switch (CurrentState)
    {
    case ERamState::Roaming:
        UpdateRoaming(DeltaTime);
        break;
    case ERamState::Approaching:
        UpdateApproaching(DeltaTime);
        break;
    case ERamState::Preparing:
        UpdatePreparing(DeltaTime);
        break;
    case ERamState::Charging:
        UpdateCharging(DeltaTime);
        break;
    case ERamState::Stunned:
        UpdateStunned(DeltaTime);
        break;
    case ERamState::Falling:
        UpdateFalling(DeltaTime);
        break;
    }
}

// === ROAMING: cherche un batiment dans 50m ===
void ATDEnemyRam::UpdateRoaming(float DeltaTime)
{
    // Si on etait en mode wall-breaking et le mur est detruit -> retour a la cible prio
    if (bIsBreakingWall && (!TargetBuilding || !IsValid(TargetBuilding)))
    {
        bIsBreakingWall = false;
        if (PrimaryTarget && IsValid(PrimaryTarget))
        {
            TargetBuilding = PrimaryTarget;
            UE_LOG(LogTemp, Warning, TEXT("TDEnemyRam %s: mur detruit, retour cible prioritaire %s"), *GetName(), *PrimaryTarget->GetName());
        }
    }

    // Chercher un batiment dans la range
    AActor* Building = FindBuildingInRange();
    if (Building)
    {
        // Sauvegarder la cible prio
        if (!PrimaryTarget || !IsValid(PrimaryTarget))
            PrimaryTarget = Building;

        // Consulter le Base Analyzer global (pre-calcule une fois par vague)
        AActor* BlockingWall = nullptr;
        {
            for (TActorIterator<ATDCreatureSpawner> It(GetWorld()); It; ++It)
            {
                ATDCreatureSpawner* Spawner = *It;
                if (!Spawner) continue;
                
                FAttackPath Path = Spawner->GetBestAttackPath(GetActorLocation(), false);
                if (Path.Target && IsValid(Path.Target))
                {
                    // Utiliser la cible du plan global
                    Building = Path.Target;
                    
                    if (Path.bIsEnclosed && Path.WallToBreak && IsValid(Path.WallToBreak))
                    {
                        BlockingWall = Path.WallToBreak;
                    }
                }
                break;
            }
        }

        if (BlockingWall && IsValid(BlockingWall))
        {
            // Un mur bloque! Charger ce mur en priorite
            if (Building && IsValid(Building)) PrimaryTarget = Building;
            TargetBuilding = BlockingWall;
            bIsBreakingWall = true;
            UE_LOG(LogTemp, Warning, TEXT("TDEnemyRam %s: mur bloque, je charge!"), *GetName());
        }
        else if (Building && IsValid(Building))
        {
            TargetBuilding = Building;
        }

        if (TargetBuilding && IsValid(TargetBuilding))
        {
            CurrentState = ERamState::Approaching;
            // Son de verrouillage de cible (3D)
            if (TargetLockSound && OneShotAudioComponent)
            {
                OneShotAudioComponent->SetSound(TargetLockSound);
                OneShotAudioComponent->Play();
            }
            UE_LOG(LogTemp, Warning, TEXT("TDEnemyRam %s: Cible -> %s, Approaching"), *GetName(), *TargetBuilding->GetName());
        }
        return;
    }

    // Pas de cible proche: suivre les waypoints ou aller vers la cible assignee
    if (TargetBuilding && IsValid(TargetBuilding))
    {
        FVector MyLoc = GetActorLocation();
        FVector MoveTarget;
        
        // Suivre les waypoints pre-calcules si disponibles
        if (CurrentWaypointIndex < Waypoints.Num())
        {
            FVector WP = Waypoints[CurrentWaypointIndex];
            if (FVector::Dist2D(MyLoc, WP) < 250.0f)
            {
                CurrentWaypointIndex++;
            }
            MoveTarget = (CurrentWaypointIndex < Waypoints.Num()) ? Waypoints[CurrentWaypointIndex] : TargetBuilding->GetActorLocation();
        }
        else
        {
            MoveTarget = TargetBuilding->GetActorLocation();
        }
        
        FVector Direction = (MoveTarget - MyLoc).GetSafeNormal2D();

        FVector NewLoc = MyLoc + Direction * MoveSpeed * SpeedMultiplier * DeltaTime;
        NewLoc.Z = MyLoc.Z;
        SetActorLocation(NewLoc);
        BaseHeight = NewLoc.Z;

        FRotator LookRot = Direction.Rotation();
        SetActorRotation(FMath::RInterpTo(GetActorRotation(), LookRot, DeltaTime, 3.0f));
    }
}

// === APPROACHING: se placer a 20m de la cible ===
void ATDEnemyRam::UpdateApproaching(float DeltaTime)
{
    if (!TargetBuilding || !IsValid(TargetBuilding))
    {
        CurrentState = ERamState::Roaming;
        TargetBuilding = nullptr;
        return;
    }

    FVector MyLoc = GetActorLocation();
    FVector TargetLoc = TargetBuilding->GetActorLocation();
    float DistToTarget = FVector::Dist2D(MyLoc, TargetLoc);

    // Si on est a 20m ou plus: on est pret a se positionner
    if (DistToTarget <= MinChargeDistance + 200.0f && DistToTarget >= MinChargeDistance - 200.0f)
    {
        // On est a bonne distance, commencer la preparation
        PrepareTimer = 0.0f;
        CurrentState = ERamState::Preparing;
        UE_LOG(LogTemp, Warning, TEXT("TDEnemyRam %s: A %.0fm, preparation charge!"), *GetName(), DistToTarget / 100.0f);
        return;
    }

    // Se deplacer vers la position de charge (a 20m de la cible)
    FVector Direction;
    if (DistToTarget > MinChargeDistance)
    {
        // Trop loin: avancer vers la cible
        Direction = (TargetLoc - MyLoc).GetSafeNormal2D();
    }
    else
    {
        // Trop pres: reculer
        Direction = (MyLoc - TargetLoc).GetSafeNormal2D();
    }

    FVector NewLoc = MyLoc + Direction * MoveSpeed * SpeedMultiplier * DeltaTime;
    NewLoc.Z = MyLoc.Z;
    SetActorLocation(NewLoc);
    BaseHeight = NewLoc.Z;

    // Tourner vers la cible
    FRotator LookRot = (TargetLoc - MyLoc).GetSafeNormal2D().Rotation();
    SetActorRotation(FMath::RInterpTo(GetActorRotation(), LookRot, DeltaTime, 3.0f));
}

// === PREPARING: idle 3 secondes, comme un taureau ===
void ATDEnemyRam::UpdatePreparing(float DeltaTime)
{
    if (!TargetBuilding || !IsValid(TargetBuilding))
    {
        CurrentState = ERamState::Roaming;
        TargetBuilding = nullptr;
        return;
    }

    PrepareTimer += DeltaTime;

    // Tourner vers la cible pendant la preparation
    FVector MyLoc = GetActorLocation();
    FVector TargetLoc = TargetBuilding->GetActorLocation();
    FRotator LookRot = (TargetLoc - MyLoc).GetSafeNormal2D().Rotation();
    SetActorRotation(FMath::RInterpTo(GetActorRotation(), LookRot, DeltaTime, 5.0f));

    // Petit mouvement de recul pendant la preparation (comme un taureau qui gratte le sol)
    if (PrepareTimer < PrepareTime * 0.5f)
    {
        // Premiere moitie: leger recul
        FVector BackDir = (MyLoc - TargetLoc).GetSafeNormal2D();
        FVector NewLoc = MyLoc + BackDir * 30.0f * DeltaTime;
        NewLoc.Z = MyLoc.Z;
        SetActorLocation(NewLoc);
        BaseHeight = NewLoc.Z;
    }

    if (PrepareTimer >= PrepareTime)
    {
        // Verifier que la cible existe toujours avant de charger
        if (!TargetBuilding || !IsValid(TargetBuilding))
        {
            CurrentState = ERamState::Roaming;
            TargetBuilding = nullptr;
            return;
        }
        
        // Raycast vers la cible pour verifier la ligne de vue
        FHitResult LOSHit;
        FCollisionQueryParams LOSParams;
        LOSParams.AddIgnoredActor(this);
        // Ignorer les Pawns
        for (TActorIterator<APawn> PIt(GetWorld()); PIt; ++PIt)
        {
            LOSParams.AddIgnoredActor(*PIt);
        }
        
        FVector RayStart = MyLoc + FVector(0, 0, 30.0f);
        FVector RayEnd = TargetLoc + FVector(0, 0, 30.0f);
        bool bLOSBlocked = GetWorld()->LineTraceSingleByChannel(LOSHit, RayStart, RayEnd, ECC_WorldStatic, LOSParams);
        
        if (bLOSBlocked)
        {
            AActor* Obstacle = LOSHit.GetActor();
            if (Obstacle && IsValid(Obstacle) && Obstacle != TargetBuilding)
            {
                // FILTRE STRICT: seulement charger sur des batiments, PAS sur le terrain
                FString ObsClass = Obstacle->GetClass()->GetName();
                bool bIsObsTD = ObsClass.StartsWith(TEXT("TD")) && !ObsClass.StartsWith(TEXT("TDEnemy")) && !ObsClass.StartsWith(TEXT("TDCreature")) && !ObsClass.StartsWith(TEXT("TDWorld")) && !ObsClass.StartsWith(TEXT("TDDropship"));
                if (ObsClass.StartsWith(TEXT("Build_")) || bIsObsTD)
                {
                    PrimaryTarget = TargetBuilding;
                    TargetBuilding = Obstacle;
                    bIsBreakingWall = true;
                    TargetLoc = Obstacle->GetActorLocation();
                    UE_LOG(LogTemp, Warning, TEXT("TDEnemyRam %s: obstacle batiment %s! Charge dessus."), *GetName(), *Obstacle->GetName());
                }
            }
        }
        
        // CHARGE!
        ChargeDirection = (TargetLoc - MyLoc).GetSafeNormal2D();
        ChargeStartLocation = MyLoc;
        MaxChargeDistance = FVector::Dist2D(MyLoc, TargetLoc) + 500.0f;
        ChargeDistanceTraveled = 0.0f;
        CurrentState = ERamState::Charging;

        // Son de charge "ahhhhh" (3D)
        if (ChargeSound && OneShotAudioComponent)
        {
            OneShotAudioComponent->SetSound(ChargeSound);
            OneShotAudioComponent->Play();
        }

        // Activer propulseur
        if (ThrusterEffect)
        {
            ThrusterEffect->Activate(true);
        }

        UE_LOG(LogTemp, Warning, TEXT("TDEnemyRam %s: CHARGE! Direction=%s"), *GetName(), *ChargeDirection.ToString());
    }
}

// === CHARGING: fonce vers la cible a grande vitesse ===
void ATDEnemyRam::UpdateCharging(float DeltaTime)
{
    // Verifier que la cible existe toujours PENDANT la charge
    if (TargetBuilding && !IsValid(TargetBuilding))
    {
        TargetBuilding = nullptr;
        // Cible detruite pendant la charge -> arreter et stun
        if (ThrusterEffect) ThrusterEffect->Deactivate();
        StunTimer = 0.0f;
        CurrentState = ERamState::Stunned;
        ShowStunStars(true);
        if (StunSound && OneShotAudioComponent)
        {
            OneShotAudioComponent->SetSound(StunSound);
            OneShotAudioComponent->Play();
        }
        UE_LOG(LogTemp, Warning, TEXT("TDEnemyRam %s: cible detruite pendant charge, stun!"), *GetName());
        return;
    }

    float ActualChargeSpeed = ChargeSpeed * SpeedMultiplier;
    FVector MyLoc = GetActorLocation();

    // Orienter vers la direction de charge
    if (!ChargeDirection.IsNearlyZero())
    {
        FRotator ChargeRot = ChargeDirection.Rotation();
        SetActorRotation(ChargeRot);
    }

    // Avancer dans la direction de charge (coller au sol pendant la charge)
    FVector NewLoc = MyLoc + ChargeDirection * ActualChargeSpeed * DeltaTime;
    
    // Ground trace pendant la charge aussi (ignorer ennemis)
    FHitResult ChargeGroundHit;
    FVector CTraceStart = NewLoc + FVector(0, 0, 200.0f);
    FVector CTraceEnd = NewLoc - FVector(0, 0, 2000.0f);
    FCollisionQueryParams CTraceParams;
    CTraceParams.AddIgnoredActor(this);
    for (TActorIterator<APawn> PIt(GetWorld()); PIt; ++PIt)
    {
        CTraceParams.AddIgnoredActor(*PIt);
    }
    if (GetWorld()->LineTraceSingleByChannel(ChargeGroundHit, CTraceStart, CTraceEnd, ECC_WorldStatic, CTraceParams))
    {
        BaseHeight = ChargeGroundHit.ImpactPoint.Z + 50.0f;
    }
    NewLoc.Z = BaseHeight;
    SetActorLocation(NewLoc);

    ChargeDistanceTraveled += ActualChargeSpeed * DeltaTime;

    // Raycast frontal pour detecter les murs/fondations sur le chemin
    {
        FHitResult WallHit;
        FVector RayStart = MyLoc + FVector(0, 0, 30.0f);
        FVector RayEnd = NewLoc + ChargeDirection * 100.0f + FVector(0, 0, 30.0f);
        FCollisionQueryParams WallTraceParams;
        WallTraceParams.AddIgnoredActor(this);
        // Ignorer les Pawns (ennemis)
        for (TActorIterator<APawn> PIt(GetWorld()); PIt; ++PIt)
        {
            WallTraceParams.AddIgnoredActor(*PIt);
        }
        
        if (GetWorld()->LineTraceSingleByChannel(WallHit, RayStart, RayEnd, ECC_WorldStatic, WallTraceParams))
        {
            AActor* WallActor = WallHit.GetActor();
            if (WallActor && IsValid(WallActor))
            {
                FString WallClass = WallActor->GetClass()->GetName();
                bool bIsWallTD = WallClass.StartsWith(TEXT("TD")) && !WallClass.StartsWith(TEXT("TDEnemy")) && !WallClass.StartsWith(TEXT("TDCreature")) && !WallClass.StartsWith(TEXT("TDWorld")) && !WallClass.StartsWith(TEXT("TDDropship"));
                // STRICT: seulement Build_* et TD* (pas terrain/rochers/falaises)
                if (WallClass.StartsWith(TEXT("Build_")) || bIsWallTD)
                {
                    // Infliger degats au mur
                    float FinalDamage = ChargeDamage * (SpeedMultiplier < 1.0f ? (1.0f - SlowDamageReduction) : 1.0f);
                    for (TActorIterator<ATDCreatureSpawner> SpIt(GetWorld()); SpIt; ++SpIt)
                    {
                        ATDCreatureSpawner* Spawner = *SpIt;
                        if (Spawner)
                        {
                            Spawner->DamageBuilding(WallActor, FinalDamage);
                            Spawner->MarkBuildingAttacked(WallActor);
                            break;
                        }
                    }
                    NoAttackTimer = 0.0f;
                    
                    if (ThrusterEffect) ThrusterEffect->Deactivate();
                    StunTimer = 0.0f;
                    CurrentState = ERamState::Stunned;
                    ShowStunStars(true);
                    TargetBuilding = nullptr;
                    if (StunSound && OneShotAudioComponent)
                    {
                        OneShotAudioComponent->SetSound(StunSound);
                        OneShotAudioComponent->Play();
                    }
                    UE_LOG(LogTemp, Warning, TEXT("TDEnemyRam %s: IMPACT MUR %s! Degats: %.0f"), *GetName(), *WallActor->GetName(), FinalDamage);
                    return;
                }
            }
        }
    }

    // Verifier collision avec la cible (ou n'importe quel batiment)
    bool bHitTarget = false;
    AActor* HitBuilding = nullptr;

    if (TargetBuilding && IsValid(TargetBuilding))
    {
        float DistToTarget = FVector::Dist2D(NewLoc, TargetBuilding->GetActorLocation());
        if (DistToTarget < 300.0f) // 3m = contact
        {
            bHitTarget = true;
            HitBuilding = TargetBuilding;
        }
    }

    // Aussi verifier collision avec n'importe quel batiment sur le chemin
    if (!bHitTarget)
    {
        for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        {
            AActor* Actor = *It;
            if (!Actor || !IsValid(Actor) || Actor == this) continue;

            FString ClassName = Actor->GetClass()->GetName();
            bool bIsTDB = ClassName.StartsWith(TEXT("TD")) && !ClassName.StartsWith(TEXT("TDEnemy")) && !ClassName.StartsWith(TEXT("TDCreature")) && !ClassName.StartsWith(TEXT("TDWorld")) && !ClassName.StartsWith(TEXT("TDDropship"));
            if (ClassName.StartsWith(TEXT("Build_")) || bIsTDB)
            {
                float Dist = FVector::Dist2D(NewLoc, Actor->GetActorLocation());
                if (Dist < 300.0f)
                {
                    bHitTarget = true;
                    HitBuilding = Actor;
                    break;
                }
            }
        }
    }

    if (bHitTarget && HitBuilding)
    {
        // IMPACT!
        float FinalDamage = ChargeDamage;

        // Si slow au moment de l'impact: -30% degats
        if (SpeedMultiplier < 1.0f)
        {
            FinalDamage *= (1.0f - SlowDamageReduction);
            UE_LOG(LogTemp, Warning, TEXT("TDEnemyRam %s: IMPACT SLOW! Degats reduits: %.0f (x%.1f)"), *GetName(), FinalDamage, SpeedMultiplier);
            NoAttackTimer = 0.0f;
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("TDEnemyRam %s: IMPACT! Degats: %.0f"), *GetName(), FinalDamage);
            NoAttackTimer = 0.0f;
        }

        // Infliger les degats via le spawner
        for (TActorIterator<ATDCreatureSpawner> It(GetWorld()); It; ++It)
        {
            ATDCreatureSpawner* Spawner = *It;
            if (Spawner)
            {
                Spawner->DamageBuilding(HitBuilding, FinalDamage);
                Spawner->MarkBuildingAttacked(HitBuilding);
                break;
            }
        }

        // Desactiver propulseur
        if (ThrusterEffect)
        {
            ThrusterEffect->Deactivate();
        }

        // Passer en stun
        StunTimer = 0.0f;
        CurrentState = ERamState::Stunned;
        ShowStunStars(true);
        TargetBuilding = nullptr;
        // Son de stun "oops" (3D)
        if (StunSound && OneShotAudioComponent)
        {
            OneShotAudioComponent->SetSound(StunSound);
            OneShotAudioComponent->Play();
        }
        UE_LOG(LogTemp, Warning, TEXT("TDEnemyRam %s: Stunned pour %.0fs!"), *GetName(), StunDuration);
        return;
    }

    // Si on a parcouru trop de distance sans toucher: rate, stun quand meme
    if (ChargeDistanceTraveled >= MaxChargeDistance)
    {
        UE_LOG(LogTemp, Warning, TEXT("TDEnemyRam %s: Charge ratee! Stun."), *GetName());

        if (ThrusterEffect)
        {
            ThrusterEffect->Deactivate();
        }

        StunTimer = 0.0f;
        CurrentState = ERamState::Stunned;
        ShowStunStars(true);
        TargetBuilding = nullptr;
        // Son de stun "oops" (3D)
        if (StunSound && OneShotAudioComponent)
        {
            OneShotAudioComponent->SetSound(StunSound);
            OneShotAudioComponent->Play();
        }
    }
}

// === STUNNED: etourdi, etoiles tournantes ===
void ATDEnemyRam::UpdateStunned(float DeltaTime)
{
    StunTimer += DeltaTime;

    // Animer les etoiles
    UpdateStunStars(DeltaTime);

    if (StunTimer >= StunDuration)
    {
        // Fin du stun
        ShowStunStars(false);
        
        // Si on cassait un mur et qu'on a la cible prio -> retour
        if (bIsBreakingWall && PrimaryTarget && IsValid(PrimaryTarget))
        {
            bIsBreakingWall = false;
            TargetBuilding = PrimaryTarget;
            UE_LOG(LogTemp, Warning, TEXT("TDEnemyRam %s: Fin stun, retour cible prio %s"), *GetName(), *PrimaryTarget->GetName());
        }
        
        CurrentState = ERamState::Roaming;
        UE_LOG(LogTemp, Warning, TEXT("TDEnemyRam %s: Fin du stun, retour Roaming"), *GetName());
    }
}

// === STUN STARS: 3 etoiles tournantes au-dessus de la tete ===
void ATDEnemyRam::UpdateStunStars(float DeltaTime)
{
    StunStarAngle += DeltaTime * 180.0f; // 180 deg/s
    if (StunStarAngle > 360.0f) StunStarAngle -= 360.0f;

    float StarRadius = 80.0f;
    float StarHeight = 150.0f;

    for (int32 i = 0; i < 3; i++)
    {
        float Angle = StunStarAngle + (120.0f * i); // 120 deg entre chaque etoile
        float Rad = FMath::DegreesToRadians(Angle);

        FVector StarPos;
        StarPos.X = FMath::Cos(Rad) * StarRadius;
        StarPos.Y = FMath::Sin(Rad) * StarRadius;
        StarPos.Z = StarHeight;

        UStaticMeshComponent* Star = (i == 0) ? StunStar1 : (i == 1) ? StunStar2 : StunStar3;
        if (Star)
        {
            Star->SetRelativeLocation(StarPos);
            // Faire tourner l'etoile sur elle-meme aussi
            Star->SetRelativeRotation(FRotator(0.0f, Angle * 2.0f, 0.0f));
        }
    }
}

void ATDEnemyRam::ShowStunStars(bool bShow)
{
    if (StunStar1) StunStar1->SetVisibility(bShow);
    if (StunStar2) StunStar2->SetVisibility(bShow);
    if (StunStar3) StunStar3->SetVisibility(bShow);
}

// === Chercher un batiment dans la range de detection (priorite: turrets > machines > structures) ===
AActor* ATDEnemyRam::FindBuildingInRange()
{
    if (!GetWorld()) return nullptr;
    
    // Obtenir le spawner pour verifier les batiments detruits
    ATDCreatureSpawner* Spawner = nullptr;
    for (TActorIterator<ATDCreatureSpawner> SpIt(GetWorld()); SpIt; ++SpIt)
    {
        Spawner = *SpIt;
        break;
    }
    
    FVector MyLoc = GetActorLocation();
    AActor* BestTurret = nullptr;    float BestTurretDist = DetectionRange;
    AActor* BestMachine = nullptr;   float BestMachineDist = DetectionRange;
    AActor* BestStructure = nullptr; float BestStructureDist = DetectionRange;

    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        AActor* Actor = *It;
        if (!Actor || !IsValid(Actor)) continue;
        
        // Ignorer les batiments marques comme detruits
        if (Spawner && Spawner->IsBuildingDestroyed(Actor)) continue;

        // Ignorer les batiments trop hauts ou trop bas (max 400u = 4m de difference)
        float HeightDiff = FMath::Abs(Actor->GetActorLocation().Z - MyLoc.Z);
        if (HeightDiff > 400.0f) continue;

        float Dist = FVector::Dist(MyLoc, Actor->GetActorLocation());
        if (Dist >= DetectionRange) continue;

        FString ClassName = Actor->GetClass()->GetName();
        
        // FILTRE STRICT: seulement Build_* et TD* (pas terrain/decorations)
        bool bIsTDBuilding = ClassName.StartsWith(TEXT("TD")) && !ClassName.StartsWith(TEXT("TDEnemy")) && !ClassName.StartsWith(TEXT("TDCreature")) && !ClassName.StartsWith(TEXT("TDWorld")) && !ClassName.StartsWith(TEXT("TDDropship"));
        if (!ClassName.StartsWith(TEXT("Build_")) && !bIsTDBuilding) continue;

        // Ignorer transport/infrastructure
        if (ClassName.Contains(TEXT("ConveyorBelt")) || ClassName.Contains(TEXT("ConveyorLift")) ||
            ClassName.Contains(TEXT("ConveyorAttachment")) ||
            ClassName.Contains(TEXT("PowerLine")) || ClassName.Contains(TEXT("Wire")) ||
            ClassName.Contains(TEXT("PowerPole")) || ClassName.Contains(TEXT("PowerTower")) ||
            ClassName.Contains(TEXT("RailroadTrack")) || ClassName.Contains(TEXT("PillarBase")) ||
            ClassName.Contains(TEXT("Beam")) || ClassName.Contains(TEXT("Stair")) ||
            ClassName.Contains(TEXT("Pipeline")) || ClassName.Contains(TEXT("Pipe")))
            continue;
        
        // Ignorer objets non-attaquables et decoratifs
        if (ClassName.Contains(TEXT("SpaceElevator")) || ClassName.Contains(TEXT("Ladder")) ||
            ClassName.Contains(TEXT("ConveyorPole")) || ClassName.Contains(TEXT("Walkway")) ||
            ClassName.Contains(TEXT("Catwalk")) || ClassName.Contains(TEXT("Sign")) ||
            ClassName.Contains(TEXT("Light")) || ClassName.Contains(TEXT("HubTerminal")) ||
            ClassName.Contains(TEXT("WorkBench")) || ClassName.Contains(TEXT("TradingPost")) ||
            ClassName.Contains(TEXT("Tetromino")) || ClassName.Contains(TEXT("Potty")) ||
            ClassName.Contains(TEXT("SnowDispenser")) || ClassName.Contains(TEXT("Decoration")) ||
            ClassName.Contains(TEXT("Calendar")) || ClassName.Contains(TEXT("Fireworks")) ||
            ClassName.Contains(TEXT("GolfCart")) || ClassName.Contains(TEXT("CandyCane")))
            continue;

        // Prio 1: Tourelles/defenses TD
        if (bIsTDBuilding)
        {
            if (Dist < BestTurretDist) { BestTurretDist = Dist; BestTurret = Actor; }
        }
        // Prio 2: Machines de production
        else if (ClassName.Contains(TEXT("Constructor")) ||
                 ClassName.Contains(TEXT("Assembler")) ||
                 ClassName.Contains(TEXT("Manufacturer")) ||
                 ClassName.Contains(TEXT("Smelter")) ||
                 ClassName.Contains(TEXT("Foundry")) ||
                 ClassName.Contains(TEXT("Refinery")) ||
                 ClassName.Contains(TEXT("Generator")) ||
                 ClassName.Contains(TEXT("Miner")) ||
                 ClassName.Contains(TEXT("Packager")) ||
                 ClassName.Contains(TEXT("Blender")))
        {
            if (Dist < BestMachineDist) { BestMachineDist = Dist; BestMachine = Actor; }
        }
        // Prio 3: Tout autre Build_* (murs, fondations, etc.)
        else
        {
            if (Dist < BestStructureDist) { BestStructureDist = Dist; BestStructure = Actor; }
        }
    }

    if (BestTurret) return BestTurret;
    if (BestMachine) return BestMachine;
    return BestStructure;
}

// === DEGATS / MORT ===
float ATDEnemyRam::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
    AController* EventInstigator, AActor* DamageCauser)
{
    if (bIsDead) return 0.0f;
    float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    Health -= ActualDamage;
    if (Health <= 0.0f) Die();
    return ActualDamage;
}

void ATDEnemyRam::TakeDamageCustom(float DamageAmount)
{
    if (bIsDead) return;
    Health -= DamageAmount;
    UE_LOG(LogTemp, Warning, TEXT("TDEnemyRam %s: -%.0f HP (reste %.0f/%.0f)"), *GetName(), DamageAmount, Health, MaxHealth);
    if (Health <= 0.0f) Die();
}

void ATDEnemyRam::Die()
{
    if (bIsDead) return;
    bIsDead = true;

    UE_LOG(LogTemp, Warning, TEXT("TDEnemyRam %s est mort!"), *GetName());

    ShowStunStars(false);

    if (ThrusterEffect)
    {
        ThrusterEffect->Deactivate();
    }

    if (CollisionSphere)
    {
        CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    // Faire tomber et disparaitre
    if (VisibleMesh)
    {
        VisibleMesh->SetSimulatePhysics(true);
    }

    SetLifeSpan(5.0f);
}

void ATDEnemyRam::SetTarget(AActor* NewTarget)
{
    TargetBuilding = NewTarget;
    if (NewTarget)
    {
        UE_LOG(LogTemp, Warning, TEXT("TDEnemyRam %s cible: %s"), *GetName(), *NewTarget->GetName());
    }
}

void ATDEnemyRam::ApplySlow(float Duration, float InSlowFactor)
{
    if (bIsDead) return;

    SpeedMultiplier = FMath::Clamp(InSlowFactor, 0.1f, 1.0f);
    SlowTimer = Duration;
    MoveSpeed = OriginalMoveSpeed * SpeedMultiplier;

    UE_LOG(LogTemp, Warning, TEXT("TDEnemyRam %s: SLOWED x%.1f for %.1fs"), *GetName(), SpeedMultiplier, Duration);
}

void ATDEnemyRam::TeleportToBase()
{
    if (bIsDead || !GetWorld()) return;

    // Chercher le batiment le plus proche
    AActor* TeleportTarget = TargetBuilding;

    if (!TeleportTarget || !IsValid(TeleportTarget))
    {
        float BestDist = MAX_FLT;
        for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        {
            AActor* Actor = *It;
            if (!Actor || !IsValid(Actor)) continue;
            FString ClassName = Actor->GetClass()->GetName();
            bool bIsTD = ClassName.StartsWith(TEXT("TD")) && !ClassName.StartsWith(TEXT("TDEnemy")) && !ClassName.StartsWith(TEXT("TDCreature")) && !ClassName.StartsWith(TEXT("TDWorld")) && !ClassName.StartsWith(TEXT("TDDropship"));
            if (ClassName.StartsWith(TEXT("Build_")) || bIsTD)
            {
                float Dist = FVector::Dist(GetActorLocation(), Actor->GetActorLocation());
                if (Dist < BestDist)
                {
                    BestDist = Dist;
                    TeleportTarget = Actor;
                }
            }
        }
    }

    if (!TeleportTarget || !IsValid(TeleportTarget)) return;

    // Teleporter au-dessus du batiment (500u)
    FVector TargetLoc = TeleportTarget->GetActorLocation();
    FVector TeleportLoc = TargetLoc + FVector(FMath::RandRange(-200.0f, 200.0f), FMath::RandRange(-200.0f, 200.0f), 500.0f);

    SetActorLocation(TeleportLoc);
    TargetBuilding = TeleportTarget;
    BaseHeight = TeleportLoc.Z;
    CurrentState = ERamState::Falling;

    // Desactiver les etoiles de stun au cas ou
    ShowStunStars(false);

    UE_LOG(LogTemp, Warning, TEXT("TDEnemyRam %s: TELEPORT au-dessus de %s!"), *GetName(), *TeleportTarget->GetName());
}

void ATDEnemyRam::UpdateFalling(float DeltaTime)
{
    // Tomber avec gravite simulee
    const float Gravity = 980.0f;
    FVector Loc = GetActorLocation();
    Loc.Z -= Gravity * DeltaTime;

    // Verifier si on touche le sol ou un batiment
    FHitResult HitResult;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    FVector Start = GetActorLocation();
    FVector End = Loc;

    bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_WorldStatic, Params);

    if (bHit)
    {
        // Atterri! Se poser sur la surface
        Loc = HitResult.ImpactPoint + FVector(0, 0, 60.0f); // Hauteur de la collision sphere
        SetActorLocation(Loc);
        BaseHeight = Loc.Z;
        CurrentState = ERamState::Roaming;
        UE_LOG(LogTemp, Warning, TEXT("TDEnemyRam %s: Atterri apres teleport!"), *GetName());
    }
    else
    {
        SetActorLocation(Loc);
        BaseHeight = Loc.Z;
    }
}
