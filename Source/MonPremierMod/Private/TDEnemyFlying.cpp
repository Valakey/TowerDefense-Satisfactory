#include "TDEnemyFlying.h"
#include "TDCreatureSpawner.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

ATDEnemyFlying::ATDEnemyFlying()
{
    PrimaryActorTick.bCanEverTick = true;

    // Collision sphere (remplace la capsule de ACharacter)
    CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
    CollisionSphere->InitSphereRadius(30.0f);
    CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    CollisionSphere->SetCollisionObjectType(ECC_Pawn);
    CollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    CollisionSphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    CollisionSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
    CollisionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    CollisionSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    CollisionSphere->SetGenerateOverlapEvents(true);
    // Pas de simulation physique - on gere le mouvement manuellement
    CollisionSphere->SetSimulatePhysics(false);
    SetRootComponent(CollisionSphere);

    // Mesh visible - Enemy_Flying
    VisibleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisibleMesh"));
    VisibleMesh->SetupAttachment(RootComponent);
    VisibleMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
    VisibleMesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));
    VisibleMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
    VisibleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    VisibleMesh->SetCastShadow(true);

    // Charger le mesh Enemy_Flying
    static ConstructorHelpers::FObjectFinder<UStaticMesh> EnemyMesh(TEXT("/MonPremierMod/Meshes/Ennemy/FlyEnemy/Enemy_Flying.Enemy_Flying"));
    if (EnemyMesh.Succeeded())
    {
        VisibleMesh->SetStaticMesh(EnemyMesh.Object);
        UE_LOG(LogTemp, Warning, TEXT("TDEnemyFlying: Enemy_Flying mesh charge!"));
    }

    // Charger le systeme Niagara de flammes (meme que le vaisseau)
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> FlameSystemObj(TEXT("/MonPremierMod/Particles/NS_StylizedFireEnnemy.NS_StylizedFireEnnemy"));
    if (FlameSystemObj.Succeeded())
    {
        FlameNiagaraSystem = FlameSystemObj.Object;
    }

    // Creer le composant Niagara pour le reacteur (sous l'ennemi)
    ThrusterEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ThrusterEffect"));
    ThrusterEffect->SetupAttachment(RootComponent);
    ThrusterEffect->SetRelativeLocation(FVector(0.0f, 0.0f, -50.0f));  // Sous l'ennemi
    ThrusterEffect->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
    ThrusterEffect->bAutoActivate = false;

    // Creer le laser beam (cube tres fin, noir)
    LaserBeam = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LaserBeam"));
    LaserBeam->SetupAttachment(RootComponent);
    LaserBeam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LaserBeam->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
    LaserBeam->SetCastShadow(false);
    LaserBeam->SetVisibility(false);  // Cache par defaut

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube"));
    if (CubeMesh.Succeeded())
    {
        LaserBeam->SetStaticMesh(CubeMesh.Object);
        LaserBeam->SetWorldScale3D(FVector(2.0f, 0.32f, 0.32f));
    }

    // Audio one-shot (attaque)
    OneShotAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("OneShotAudio"));
    OneShotAudioComponent->SetupAttachment(RootComponent);
    OneShotAudioComponent->bAutoActivate = false;
    OneShotAudioComponent->bAllowSpatialization = true;
    OneShotAudioComponent->bOverrideAttenuation = true;
    OneShotAudioComponent->AttenuationOverrides.bAttenuate = true;
    OneShotAudioComponent->AttenuationOverrides.bSpatialize = true;
    OneShotAudioComponent->AttenuationOverrides.FalloffDistance = 3000.0f;

    // Audio vol (boucle)
    FlyingAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("FlyingAudio"));
    FlyingAudioComponent->SetupAttachment(RootComponent);
    FlyingAudioComponent->bAutoActivate = false;
    FlyingAudioComponent->bAllowSpatialization = true;
    FlyingAudioComponent->bOverrideAttenuation = true;
    FlyingAudioComponent->AttenuationOverrides.bAttenuate = true;
    FlyingAudioComponent->AttenuationOverrides.bSpatialize = true;
    FlyingAudioComponent->AttenuationOverrides.FalloffDistance = 2000.0f;

    // Charger les sons (reutiliser les sons existants)
    static ConstructorHelpers::FObjectFinder<USoundWave> AttackSoundObj(TEXT("/MonPremierMod/Audios/Ennemy/AttackMob.AttackMob"));
    if (AttackSoundObj.Succeeded())
    {
        AttackSound = AttackSoundObj.Object;
    }

    static ConstructorHelpers::FObjectFinder<USoundWave> FlyingSoundObj(TEXT("/MonPremierMod/Audios/Ennemy/deplacementennemy.deplacementennemy"));
    if (FlyingSoundObj.Succeeded())
    {
        FlyingSound = FlyingSoundObj.Object;
    }

    // Pas de controller AI
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void ATDEnemyFlying::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning, TEXT("TDEnemyFlying spawned: %s at %s"), *GetName(), *GetActorLocation().ToString());

    OriginalFlySpeed = FlySpeed;

    // Outline rouge
    if (VisibleMesh)
    {
        VisibleMesh->SetRenderCustomDepth(true);
        VisibleMesh->SetCustomDepthStencilValue(1);
    }

    // Creer materiau noir pour le laser
    if (LaserBeam)
    {
        UMaterialInstanceDynamic* LaserMat = UMaterialInstanceDynamic::Create(LaserBeam->GetMaterial(0), this);
        if (LaserMat)
        {
            LaserMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.02f, 0.0f, 0.05f, 1.0f));  // Noir/violet tres sombre
            LaserBeam->SetMaterial(0, LaserMat);
        }
    }

    // Sauvegarder le materiau original du mesh et creer un materiau noir pour le blink
    if (VisibleMesh && VisibleMesh->GetNumMaterials() > 0)
    {
        OriginalMaterial = VisibleMesh->GetMaterial(0);
        BlackMaterial = UMaterialInstanceDynamic::Create(VisibleMesh->GetMaterial(0), this);
        if (BlackMaterial)
        {
            BlackMaterial->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.01f, 0.0f, 0.02f, 1.0f));
            BlackMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.01f, 0.0f, 0.02f, 1.0f));
            BlackMaterial->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(0.05f, 0.0f, 0.1f, 1.0f));
        }
    }

    // Son de vol en boucle
    if (FlyingAudioComponent && FlyingSound)
    {
        FlyingSound->bLooping = true;
        FlyingAudioComponent->SetSound(FlyingSound);
        FlyingAudioComponent->SetVolumeMultiplier(0.08f);
        FlyingAudioComponent->SetPitchMultiplier(1.3f);  // Pitch plus aigu pour differencier du sol
        FlyingAudioComponent->Play();
    }

    // Randomiser le bob timer pour que les volants ne bougent pas tous en sync
    BobTimer = FMath::RandRange(0.0f, 6.28f);

    // Activer l'effet Niagara du reacteur
    if (ThrusterEffect && FlameNiagaraSystem)
    {
        ThrusterEffect->SetAsset(FlameNiagaraSystem);
        ThrusterEffect->SetWorldScale3D(FVector(1.0f, 1.0f, 1.0f));
        ThrusterEffect->Activate();
        UE_LOG(LogTemp, Warning, TEXT("TDEnemyFlying: Flammes reacteur activees!"));
    }
}

void ATDEnemyFlying::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bIsDead) return;

    // Slow timer
    if (SlowTimer > 0.0f)
    {
        SlowTimer -= DeltaTime;
        if (SlowTimer <= 0.0f)
        {
            SpeedMultiplier = 1.0f;
            FlySpeed = OriginalFlySpeed;
        }
    }

    // Timer d'attaque
    if (AttackTimer > 0.0f)
    {
        AttackTimer -= DeltaTime;
    }

    // Bob timer pour mouvement sinusoidal
    BobTimer += DeltaTime * BobFrequency;

    // Toujours orienter l'oeil vers la cible
    RotateEyeTowardsTarget(DeltaTime);

    FVector MyLoc = GetActorLocation();

    // === VERIFICATION CIBLE VALIDE ===
    if (TargetBuilding && (!IsValid(TargetBuilding) || TargetBuilding->IsPendingKillPending()))
    {
        UE_LOG(LogTemp, Warning, TEXT("TDEnemyFlying %s: cible detruite (par autre), reset!"), *GetName());
        StopLaser();
        TargetBuilding = nullptr;
        Waypoints.Empty();
        CurrentWaypointIndex = 0;
    }
    
    // Verifier si la cible est marquee comme detruite (structures)
    if (TargetBuilding)
    {
        TArray<AActor*> Spawners;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ATDCreatureSpawner::StaticClass(), Spawners);
        if (Spawners.Num() > 0)
        {
            ATDCreatureSpawner* Spawner = Cast<ATDCreatureSpawner>(Spawners[0]);
            if (Spawner && Spawner->IsBuildingDestroyed(TargetBuilding))
            {
                StopLaser();
                TargetBuilding = nullptr;
            }
        }
    }

    // === DETECTION PLAFOND: raycast vers le haut ===
    FCollisionQueryParams CeilParams;
    CeilParams.AddIgnoredActor(this);
    FHitResult CeilHit;
    float MyCeilingZ = MyLoc.Z + 10000.0f;
    bool bUnderCeiling = GetWorld()->LineTraceSingleByChannel(
        CeilHit, MyLoc, MyLoc + FVector(0, 0, 500.0f), ECC_WorldStatic, CeilParams);
    if (bUnderCeiling)
    {
        MyCeilingZ = CeilHit.ImpactPoint.Z;
    }

    // === DETECTION SOL: raycast vers le bas ===
    FCollisionQueryParams FloorParams;
    FloorParams.AddIgnoredActor(this);
    FHitResult FloorHit;
    float MyFloorZ = MyLoc.Z - 10000.0f;
    bool bAboveFloor = GetWorld()->LineTraceSingleByChannel(
        FloorHit, MyLoc, MyLoc - FVector(0, 0, 500.0f), ECC_WorldStatic, FloorParams);
    if (bAboveFloor)
    {
        MyFloorZ = FloorHit.ImpactPoint.Z;
    }

    // Si la cible est au-dessus du plafond OU en-dessous du sol -> abandonner
    if (TargetBuilding && IsValid(TargetBuilding))
    {
        float TargetZ = TargetBuilding->GetActorLocation().Z;
        if ((bUnderCeiling && TargetZ > MyCeilingZ + 100.0f) ||
            (bAboveFloor && TargetZ < MyFloorZ - 100.0f))
        {
            UE_LOG(LogTemp, Warning, TEXT("TDEnemyFlying %s: cible %s inaccessible (plafond/sol), abandon!"), *GetName(), *TargetBuilding->GetName());
            StopLaser();
            TargetBuilding = nullptr;
        }
    }

    // === SYSTEME DE CIBLAGE INTELLIGENT PAR COUCHES ===
    // Scan tous les batiments dans la sphere de detection
    FCollisionQueryParams DetectParams;
    DetectParams.AddIgnoredActor(this);
    FCollisionShape DetectSphere = FCollisionShape::MakeSphere(1000.0f);
    TArray<FOverlapResult> Overlaps;
    GetWorld()->OverlapMultiByChannel(Overlaps, MyLoc, FQuat::Identity, ECC_WorldStatic, DetectSphere, DetectParams);

    // Recuperer le spawner pour verifier les batiments detruits
    ATDCreatureSpawner* CachedSpawner = nullptr;
    for (TActorIterator<ATDCreatureSpawner> SpIt(GetWorld()); SpIt; ++SpIt)
    {
        CachedSpawner = *SpIt;
        break;
    }

    // Listes de cibles par accessibilite et priorite
    AActor* BestAccessibleTurret = nullptr;   float BestATDist = MAX_FLT;
    AActor* BestAccessibleMachine = nullptr;  float BestAMDist = MAX_FLT;
    AActor* BestAccessibleStructure = nullptr; float BestASDist = MAX_FLT;
    AActor* BestBlockingWall = nullptr;        float BestBWScore = -MAX_FLT;
    // Pour le mur: on veut le mur qui bloque la machine la plus proche
    AActor* BestBlockedMachine = nullptr;      float BestBMDist = MAX_FLT;

    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* OvActor = Overlap.GetActor();
        if (!OvActor || !IsValid(OvActor)) continue;

        // Filtrer par hauteur (plafond/sol)
        float ActorZ = OvActor->GetActorLocation().Z;
        if (bUnderCeiling && ActorZ > MyCeilingZ + 100.0f) continue;
        if (bAboveFloor && ActorZ < MyFloorZ - 100.0f) continue;

        FString OvClass = OvActor->GetClass()->GetName();
        bool bIsTDBuilding = OvClass.StartsWith(TEXT("TD")) && !OvClass.StartsWith(TEXT("TDEnemy")) && !OvClass.StartsWith(TEXT("TDCreature")) && !OvClass.StartsWith(TEXT("TDWorld")) && !OvClass.StartsWith(TEXT("TDDropship"));
        if (!OvClass.StartsWith(TEXT("Build_")) && !bIsTDBuilding) continue;

        // Ignorer les batiments deja detruits
        if (CachedSpawner && CachedSpawner->IsBuildingDestroyed(OvActor)) continue;

        // Ignorer transport/infrastructure
        if (OvClass.Contains(TEXT("ConveyorBelt")) || OvClass.Contains(TEXT("ConveyorLift")) ||
            OvClass.Contains(TEXT("ConveyorAttachment")) ||
            OvClass.Contains(TEXT("PowerLine")) || OvClass.Contains(TEXT("Wire")) ||
            OvClass.Contains(TEXT("PowerPole")) || OvClass.Contains(TEXT("PowerTower")) ||
            OvClass.Contains(TEXT("RailroadTrack")) || OvClass.Contains(TEXT("PillarBase")) ||
            OvClass.Contains(TEXT("Beam")) || OvClass.Contains(TEXT("Stair")) ||
            OvClass.Contains(TEXT("Pipeline")) || OvClass.Contains(TEXT("Pipe")))
            continue;
        
        // Ignorer objets non-attaquables et decoratifs
        if (OvClass.Contains(TEXT("SpaceElevator")) || OvClass.Contains(TEXT("Ladder")) ||
            OvClass.Contains(TEXT("ConveyorPole")) || OvClass.Contains(TEXT("Walkway")) ||
            OvClass.Contains(TEXT("Catwalk")) || OvClass.Contains(TEXT("Sign")) ||
            OvClass.Contains(TEXT("Light")) || OvClass.Contains(TEXT("HubTerminal")) ||
            OvClass.Contains(TEXT("WorkBench")) || OvClass.Contains(TEXT("TradingPost")) ||
            OvClass.Contains(TEXT("Tetromino")) || OvClass.Contains(TEXT("Potty")) ||
            OvClass.Contains(TEXT("SnowDispenser")) || OvClass.Contains(TEXT("Decoration")) ||
            OvClass.Contains(TEXT("Calendar")) || OvClass.Contains(TEXT("Fireworks")) ||
            OvClass.Contains(TEXT("GolfCart")) || OvClass.Contains(TEXT("CandyCane")))
            continue;

        float Dist = FVector::Dist(MyLoc, OvActor->GetActorLocation());

        // Determiner la priorite du batiment
        int32 Prio = 3; // structure par defaut
        if (bIsTDBuilding)
            Prio = 1; // turret/defense
        else if (OvClass.Contains(TEXT("Constructor")) || OvClass.Contains(TEXT("Smelter")) ||
                 OvClass.Contains(TEXT("Assembler")) || OvClass.Contains(TEXT("Manufacturer")) ||
                 OvClass.Contains(TEXT("Miner")) || OvClass.Contains(TEXT("Foundry")) ||
                 OvClass.Contains(TEXT("Refinery")) || OvClass.Contains(TEXT("Generator")) ||
                 OvClass.Contains(TEXT("Packager")) || OvClass.Contains(TEXT("Blender")) ||
                 OvClass.Contains(TEXT("Storage")))
            Prio = 2; // machine de production

        // Verifier la ligne de vue (LOS)
        FCollisionQueryParams LOSCheck;
        LOSCheck.AddIgnoredActor(this);
        FHitResult LOSHit;
        bool bBlocked = GetWorld()->LineTraceSingleByChannel(
            LOSHit, MyLoc, OvActor->GetActorLocation(), ECC_WorldStatic, LOSCheck);

        bool bAccessible = !bBlocked || (LOSHit.GetActor() == OvActor);

        if (bAccessible)
        {
            // Cible accessible directement
            if (Prio == 1 && Dist < BestATDist) { BestATDist = Dist; BestAccessibleTurret = OvActor; }
            else if (Prio == 2 && Dist < BestAMDist) { BestAMDist = Dist; BestAccessibleMachine = OvActor; }
            else if (Prio == 3 && Dist < BestASDist) { BestASDist = Dist; BestAccessibleStructure = OvActor; }
        }
        else if (Prio <= 2) // Seulement tracker les murs qui bloquent des cibles de valeur
        {
            // Le mur qui bloque cette machine
            AActor* BlockingActor = LOSHit.GetActor();
            if (BlockingActor && IsValid(BlockingActor) && Dist < BestBMDist)
            {
                FString BlockerClass = BlockingActor->GetClass()->GetName();
                if (BlockerClass.StartsWith(TEXT("Build_")))
                {
                    BestBMDist = Dist;
                    BestBlockedMachine = OvActor;
                    BestBlockingWall = BlockingActor;
                }
            }
        }
    }

    // === DECISION DE CIBLAGE PAR COUCHE ===
    AActor* SmartTarget = nullptr;
    bool bTargetIsWall = false;

    // Couche 1: cibles accessibles (turrets > machines > murs bloquants > structures)
    if (BestAccessibleTurret)
        SmartTarget = BestAccessibleTurret;
    else if (BestAccessibleMachine)
        SmartTarget = BestAccessibleMachine;
    else if (BestBlockingWall)
    {
        // Pas de machine/turret accessible -> attaquer le mur qui bloque la machine la plus proche
        SmartTarget = BestBlockingWall;
        bTargetIsWall = true;
    }
    else if (BestAccessibleStructure)
        SmartTarget = BestAccessibleStructure;

    // Appliquer la decision
    if (SmartTarget)
    {
        if (TargetBuilding != SmartTarget)
        {
            SetTarget(SmartTarget);
            
            // Recalculer fly path dynamique depuis position actuelle
            if (CachedSpawner)
            {
                TArray<FVector> NewPath = CachedSpawner->GetFlyPathFor(MyLoc, SmartTarget->GetActorLocation());
                if (NewPath.Num() > 0)
                {
                    Waypoints = NewPath;
                    CurrentWaypointIndex = 0;
                }
                else
                {
                    Waypoints.Empty();
                    CurrentWaypointIndex = 0;
                }
            }
            
            if (bTargetIsWall && BestBlockedMachine)
            {
                UE_LOG(LogTemp, Warning, TEXT("TDEnemyFlying %s: mur %s bloque %s, attaque le mur!"),
                    *GetName(), *SmartTarget->GetName(), *BestBlockedMachine->GetName());
            }
        }

        float DistToTarget = FVector::Dist(MyLoc, SmartTarget->GetActorLocation());
        if (DistToTarget <= AttackRange)
        {
            StuckTimer = 0.0f;
            AttackTarget(DeltaTime);
        }
        else
        {
            StopLaser();
            FlyTowardsTarget(DeltaTime);
        }
    }
    else if (TargetBuilding && IsValid(TargetBuilding))
    {
        float DistanceToTarget = FVector::Dist(MyLoc, TargetBuilding->GetActorLocation());

        if (DistanceToTarget <= AttackRange)
        {
            StuckTimer = 0.0f;  // On attaque, pas coince
            AttackTarget(DeltaTime);
        }
        else
        {
            StopLaser();
            FlyTowardsTarget(DeltaTime);
        }
    }
    else
    {
        // Plus de cible - couper le laser
        StopLaser();
        // Cible detruite - chercher une nouvelle cible
        UE_LOG(LogTemp, Warning, TEXT("TDEnemyFlying %s: cible detruite, recherche..."), *GetName());

        // Priorite: turrets > machines > structures
        AActor* BestTurret = nullptr;    float BestTurretDist = MAX_FLT;
        AActor* BestMachine = nullptr;   float BestMachineDist = MAX_FLT;
        AActor* BestStructure = nullptr; float BestStructureDist = MAX_FLT;
        FVector MyLoc = GetActorLocation();

        for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        {
            AActor* Actor = *It;
            if (!Actor || !IsValid(Actor)) continue;

            // Ignorer les batiments au-dessus du plafond ou en-dessous du sol
            float FallbackZ = Actor->GetActorLocation().Z;
            if (bUnderCeiling && FallbackZ > MyCeilingZ + 100.0f) continue;
            if (bAboveFloor && FallbackZ < MyFloorZ - 100.0f) continue;

            float Dist = FVector::Dist(MyLoc, Actor->GetActorLocation());
            FString ClassName = Actor->GetClass()->GetName();

            // Filtrer: seuls Build_* et TD* sont des batiments valides
            bool bIsTDBld = ClassName.StartsWith(TEXT("TD")) && !ClassName.StartsWith(TEXT("TDEnemy")) && !ClassName.StartsWith(TEXT("TDCreature")) && !ClassName.StartsWith(TEXT("TDWorld")) && !ClassName.StartsWith(TEXT("TDDropship"));
            if (!ClassName.StartsWith(TEXT("Build_")) && !bIsTDBld) continue;

            // Ignorer transport/infrastructure
            if (ClassName.Contains(TEXT("ConveyorBelt")) || ClassName.Contains(TEXT("ConveyorLift")) ||
                ClassName.Contains(TEXT("ConveyorAttachment")) ||
                ClassName.Contains(TEXT("PowerLine")) || ClassName.Contains(TEXT("Wire")) ||
                ClassName.Contains(TEXT("PowerPole")) || ClassName.Contains(TEXT("PowerTower")) ||
                ClassName.Contains(TEXT("RailroadTrack")) || ClassName.Contains(TEXT("PillarBase")) ||
                ClassName.Contains(TEXT("Beam")) || ClassName.Contains(TEXT("Stair")) ||
                ClassName.Contains(TEXT("Pipeline")) || ClassName.Contains(TEXT("Pipe")))
                continue;
            
            // Ignorer murs/fondations (les volants passent par-dessus)
            if (ClassName.Contains(TEXT("Wall")) || ClassName.Contains(TEXT("Foundation")) ||
                ClassName.Contains(TEXT("Ramp")) || ClassName.Contains(TEXT("Fence")) ||
                ClassName.Contains(TEXT("Roof")) || ClassName.Contains(TEXT("Frame")) ||
                ClassName.Contains(TEXT("Pillar")) || ClassName.Contains(TEXT("Quarter")))
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

            // Prio 1: Tourelles/defenses (TD*)
            if (bIsTDBld)
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
            // Prio 3: Autres structures Build_*
            else
            {
                if (Dist < BestStructureDist) { BestStructureDist = Dist; BestStructure = Actor; }
            }
        }

        AActor* BestTarget = BestTurret;
        if (!BestTarget) BestTarget = BestMachine;
        if (!BestTarget) BestTarget = BestStructure;

        if (BestTarget)
        {
            SetTarget(BestTarget);
        }
    }
}

void ATDEnemyFlying::FlyTowardsTarget(float DeltaTime)
{
    if (!TargetBuilding || !IsValid(TargetBuilding)) return;

    FVector MyLocation = GetActorLocation();
    FVector TargetLocation = TargetBuilding->GetActorLocation();

    // === MODE WAYPOINTS 3D: suivre le chemin pre-calcule (ZERO raycast) ===
    // Les waypoints sont dans des voxels AIR, donc aucune collision possible
    bool bFollowingWaypoints = (CurrentWaypointIndex < Waypoints.Num());
    
    FVector EffectiveTarget = TargetLocation;
    if (bFollowingWaypoints)
    {
        FVector WP = Waypoints[CurrentWaypointIndex];
        if (FVector::Dist(MyLocation, WP) < 300.0f)
        {
            CurrentWaypointIndex++;
        }
        if (CurrentWaypointIndex < Waypoints.Num())
        {
            EffectiveTarget = Waypoints[CurrentWaypointIndex];
            bFollowingWaypoints = true;
        }
        else
        {
            bFollowingWaypoints = false;  // Waypoints finis, mode direct
        }
    }
    
    float TargetZ;
    float FloorZ = MyLocation.Z - 1000.0f;
    bool bHasCeiling = false;
    float CeilingZ = MyLocation.Z + 10000.0f;
    
    if (bFollowingWaypoints)
    {
        // En mode waypoint: voler DIRECTEMENT vers le waypoint
        // Les murs sont detectes dans la grille voxel (raycasts horizontaux Phase 1.5)
        // donc le chemin BFS contourne deja les murs - ZERO raycast runtime
        TargetZ = EffectiveTarget.Z;
    }
    else
    {
        // Mode fallback: raycasts classiques (plus de waypoints)
        FCollisionQueryParams Params;
        Params.AddIgnoredActor(this);
        
        FHitResult GroundHit;
        if (GetWorld()->LineTraceSingleByChannel(
            GroundHit, MyLocation, MyLocation + FVector(0, 0, -5000.0f), ECC_WorldStatic, Params))
        {
            FloorZ = GroundHit.ImpactPoint.Z;
        }
        
        FHitResult CeilingHit;
        bHasCeiling = GetWorld()->LineTraceSingleByChannel(
            CeilingHit, MyLocation, MyLocation + FVector(0, 0, 2000.0f), ECC_WorldStatic, Params);
        if (bHasCeiling) CeilingZ = CeilingHit.ImpactPoint.Z;
        
        FVector ToTarget2DCheck = FVector(TargetLocation.X - MyLocation.X, TargetLocation.Y - MyLocation.Y, 0.0f);
        float HDist = ToTarget2DCheck.Size();
        
        if (HDist < AttackRange * 0.8f)
            TargetZ = TargetLocation.Z + FlyHeightOffset;
        else
            TargetZ = FloorZ + 500.0f;
        
        TargetZ = FMath::Max(TargetZ, FloorZ + 150.0f);
        if (bHasCeiling) TargetZ = FMath::Min(TargetZ, CeilingZ - 80.0f);
        if (bHasCeiling && (CeilingZ - FloorZ) < 200.0f) TargetZ = (FloorZ + CeilingZ) / 2.0f;
    }

    // === Mouvement vers la cible effective ===
    FVector ToTarget2D = FVector(EffectiveTarget.X - MyLocation.X, EffectiveTarget.Y - MyLocation.Y, 0.0f);
    FVector HorizontalDir = ToTarget2D.GetSafeNormal();
    if (HorizontalDir.IsNearlyZero()) HorizontalDir = FVector::ForwardVector;

    // === MEMOIRE DE MUR: contourner au lieu de foncer dans le mur ===
    // Quand on a touche un mur, on blend la direction avec le slide pendant WallAvoidTimer
    if (WallAvoidTimer > 0.0f)
    {
        WallAvoidTimer -= DeltaTime;
        // Slide = direction projetee sur le plan du mur
        FVector SlideDir = FVector::VectorPlaneProject(HorizontalDir, FVector(WallAvoidNormal.X, WallAvoidNormal.Y, 0.0f).GetSafeNormal());
        if (!SlideDir.IsNearlyZero())
        {
            SlideDir.Normalize();
            // Plus le timer est haut, plus on contourne (80% slide -> 20% slide)
            float BlendAlpha = FMath::Clamp(WallAvoidTimer / 2.0f, 0.2f, 0.8f);
            HorizontalDir = FMath::Lerp(HorizontalDir, SlideDir, BlendAlpha).GetSafeNormal();
        }
        else
        {
            // Slide direction nulle = mur pile en face -> ajouter composante laterale
            FVector Lateral = FVector::CrossProduct(FVector::UpVector, WallAvoidNormal).GetSafeNormal();
            HorizontalDir = (HorizontalDir * 0.3f + Lateral * 0.7f).GetSafeNormal();
        }
    }

    float ZDiff = TargetZ - MyLocation.Z;
    float VerticalSpeed = FMath::Clamp(ZDiff * 3.0f, -FlySpeed, FlySpeed);

    // === Separation entre volants ===
    FVector SeparationForce = FVector::ZeroVector;
    for (TActorIterator<ATDEnemyFlying> It(GetWorld()); It; ++It)
    {
        ATDEnemyFlying* Other = *It;
        if (!Other || Other == this || Other->bIsDead) continue;
        float Dist = FVector::Dist(MyLocation, Other->GetActorLocation());
        if (Dist < 350.0f && Dist > 1.0f)
        {
            FVector Away = (MyLocation - Other->GetActorLocation()).GetSafeNormal();
            SeparationForce += Away * (1.0f - Dist / 350.0f);
        }
    }

    // === Bob sinusoidal ===
    float BobOffset = FMath::Sin(BobTimer) * BobAmplitude * DeltaTime;

    // === Mouvement final ===
    FVector Movement = HorizontalDir * FlySpeed * DeltaTime
        + FVector(0, 0, VerticalSpeed * DeltaTime)
        + FVector(0, 0, BobOffset)
        + SeparationForce * 100.0f * DeltaTime;

    FVector NewLocation = MyLocation + Movement;
    FHitResult SweepHit;
    SetActorLocation(NewLocation, true, &SweepHit);

    if (SweepHit.bBlockingHit)
    {
        // Glisser le long de la surface
        FVector SlideMovement = FVector::VectorPlaneProject(Movement, SweepHit.ImpactNormal);
        FHitResult SlideHit;
        SetActorLocation(GetActorLocation() + SlideMovement, true, &SlideHit);
        
        // MEMORISER le mur pour contourner les prochaines frames
        WallAvoidNormal = SweepHit.ImpactNormal;
        WallAvoidTimer = 2.0f;
    }

    // === DETECTION DE BLOCAGE (simplifiee) ===
    FVector PostMoveLocation = GetActorLocation();
    float MovedDist = FVector::Dist(MyLocation, PostMoveLocation);
    if (MovedDist < 3.0f)
    {
        StuckTimer += DeltaTime;
        
        // Sauter les waypoints proches si on est coince dessus
        if (bFollowingWaypoints && CurrentWaypointIndex < Waypoints.Num())
        {
            if (StuckTimer > 0.8f)
            {
                CurrentWaypointIndex++;
                StuckTimer = 0.0f;
            }
        }
        
        // Si coince > 3s sans waypoints: forcer montee + contournement
        if (StuckTimer > 3.0f)
        {
            FVector ForceUp = FVector(WallAvoidNormal.X * 0.5f, WallAvoidNormal.Y * 0.5f, 1.0f).GetSafeNormal();
            SetActorLocation(PostMoveLocation + ForceUp * FlySpeed * DeltaTime * 3.0f, true);
            WallAvoidTimer = 3.0f;  // Contourner longtemps apres montee
            StuckTimer = 0.0f;
            UE_LOG(LogTemp, Verbose, TEXT("TDEnemyFlying %s: coince 3s, force montee+contournement"), *GetName());
        }
    }
    else
    {
        StuckTimer = 0.0f;
    }

    // Orienter l'oeil dans la direction de deplacement horizontale
    if (!HorizontalDir.IsNearlyZero())
    {
        FRotator MoveRotation = HorizontalDir.Rotation();
        SetActorRotation(FMath::RInterpTo(GetActorRotation(), MoveRotation, DeltaTime, 3.0f));
    }
}

void ATDEnemyFlying::RotateEyeTowardsTarget(float DeltaTime)
{
    if (!TargetBuilding || !IsValid(TargetBuilding) || !VisibleMesh) return;

    FVector MyLocation = GetActorLocation();
    FVector TargetLocation = TargetBuilding->GetActorLocation();
    FVector Direction = (TargetLocation - MyLocation).GetSafeNormal();

    if (!Direction.IsNearlyZero())
    {
        // L'oeil regarde la cible - meme direction que le beam
        FRotator TargetRotation = Direction.Rotation();
        FRotator NewRotation = FMath::RInterpTo(GetActorRotation(), TargetRotation, DeltaTime, 5.0f);
        SetActorRotation(NewRotation);
    }
}

void ATDEnemyFlying::AttackTarget(float DeltaTime)
{
    if (!TargetBuilding || !IsValid(TargetBuilding)) return;

    // Degats continus avec timer
    DamageTimer += DeltaTime;

    if (DamageTimer >= DamageInterval)
    {
        float DamageThisTick = AttackDamage * DamageInterval;

        // Trouver le spawner pour utiliser son systeme de PV
        TArray<AActor*> Spawners;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ATDCreatureSpawner::StaticClass(), Spawners);

        if (Spawners.Num() > 0)
        {
            ATDCreatureSpawner* Spawner = Cast<ATDCreatureSpawner>(Spawners[0]);
            if (Spawner)
            {
                Spawner->DamageBuilding(TargetBuilding, DamageThisTick);
            }
        }

        // Life steal: recuperer des HP egaux aux degats infliges (cap a MaxHealth)
        if (Health < MaxHealth)
        {
            Health = FMath::Min(Health + DamageThisTick, MaxHealth);
        }

        DamageTimer = 0.0f;
    }

    // Mettre a jour le visuel du laser
    UpdateLaserVisual();

    // Faire clignoter le mesh en noir pendant le tir
    if (VisibleMesh && BlackMaterial && OriginalMaterial)
    {
        float Pulse = FMath::Sin(GetWorld()->GetTimeSeconds() * 8.0f);
        if (Pulse > 0.0f)
        {
            VisibleMesh->SetMaterial(0, BlackMaterial);
        }
        else
        {
            VisibleMesh->SetMaterial(0, OriginalMaterial);
        }
    }

    // Son d'attaque (jouer une fois quand le laser s'active)
    if (!bLaserActive && OneShotAudioComponent && AttackSound)
    {
        OneShotAudioComponent->SetSound(AttackSound);
        OneShotAudioComponent->SetVolumeMultiplier(0.20f);
        OneShotAudioComponent->Play();
    }
    bLaserActive = true;
}

void ATDEnemyFlying::UpdateLaserVisual()
{
    if (!LaserBeam || !TargetBuilding) return;

    FVector Start = GetActorLocation();
    FVector End = TargetBuilding->GetActorLocation();

    float LaserLength = FVector::Dist(Start, End);
    FVector LaserCenter = (Start + End) / 2.0f;
    FVector LaserDirection = (End - Start).GetSafeNormal();

    LaserBeam->SetWorldLocation(LaserCenter);
    LaserBeam->SetWorldRotation(LaserDirection.Rotation());
    LaserBeam->SetWorldScale3D(FVector(LaserLength / 100.0f, 0.12f, 0.12f));  // Laser x4
    LaserBeam->SetVisibility(true);
}

void ATDEnemyFlying::StopLaser()
{
    if (LaserBeam)
    {
        LaserBeam->SetVisibility(false);
    }
    // Restaurer le materiau original
    if (VisibleMesh && OriginalMaterial)
    {
        VisibleMesh->SetMaterial(0, OriginalMaterial);
    }
    bLaserActive = false;
    DamageTimer = 0.0f;
}

float ATDEnemyFlying::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
    AController* EventInstigator, AActor* DamageCauser)
{
    if (bIsDead) return 0.0f;

    float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

    Health -= ActualDamage;
    UE_LOG(LogTemp, Warning, TEXT("TDEnemyFlying %s prend %.0f degats (HP: %.0f/%.0f)"),
        *GetName(), ActualDamage, Health, MaxHealth);

    if (Health <= 0.0f && !bIsDead)
    {
        Die();
    }

    return ActualDamage;
}

void ATDEnemyFlying::TakeDamageCustom(float DamageAmount)
{
    if (bIsDead) return;

    Health -= DamageAmount;

    if (Health <= 0.0f)
    {
        Die();
    }
}

void ATDEnemyFlying::SetTarget(AActor* NewTarget)
{
    TargetBuilding = NewTarget;
    if (NewTarget)
    {
        UE_LOG(LogTemp, Warning, TEXT("TDEnemyFlying %s cible: %s"), *GetName(), *NewTarget->GetName());
    }
}

void ATDEnemyFlying::Die()
{
    if (bIsDead) return;
    bIsDead = true;

    UE_LOG(LogTemp, Warning, TEXT("TDEnemyFlying %s est mort!"), *GetName());

    // Arreter le laser
    StopLaser();

    // Arreter le son de vol
    if (FlyingAudioComponent && FlyingAudioComponent->IsPlaying())
    {
        FlyingAudioComponent->Stop();
    }

    // Arreter l'effet du reacteur
    if (ThrusterEffect)
    {
        ThrusterEffect->Deactivate();
    }

    // Desactiver les collisions
    if (CollisionSphere)
    {
        CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    // Detruire apres un court delai
    SetLifeSpan(2.0f);
}

void ATDEnemyFlying::ApplySlow(float Duration, float SlowFactor)
{
    if (bIsDead) return;

    SpeedMultiplier = FMath::Clamp(SlowFactor, 0.1f, 1.0f);
    SlowTimer = Duration;
    FlySpeed = OriginalFlySpeed * SpeedMultiplier;

    UE_LOG(LogTemp, Warning, TEXT("TDEnemyFlying %s: SLOWED x%.1f for %.1fs (speed: %.0f)"), *GetName(), SpeedMultiplier, Duration, FlySpeed);
}
