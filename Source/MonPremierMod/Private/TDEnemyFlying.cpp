#include "TDEnemyFlying.h"
#include "TDCreatureSpawner.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TDLaserFence.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

ATDEnemyFlying::ATDEnemyFlying()
{
    PrimaryActorTick.bCanEverTick = true;

    // Collision sphere (remplace la capsule de ACharacter)
    CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
    CollisionSphere->InitSphereRadius(30.0f);
    CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CollisionSphere->SetCollisionObjectType(ECC_Pawn);
    CollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    CollisionSphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);
    CollisionSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
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
    VisibleMesh->SetCastShadow(false);  // PERF: pas d'ombre par ennemi volant

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

    // Charger materiau hologramme (ShieldDome = translucide)
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> HoloMatFinder(TEXT("/MonPremierMod/Materials/M_ShieldDome.M_ShieldDome"));
    if (HoloMatFinder.Succeeded())
    {
        HologramBaseMaterial = HoloMatFinder.Object;
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

    // Sauvegarder TOUS les materiaux originaux du mesh
    if (VisibleMesh)
    {
        OriginalMaterialCount = VisibleMesh->GetNumMaterials();
        for (int32 i = 0; i < OriginalMaterialCount; i++)
        {
            OriginalMaterials.Add(VisibleMesh->GetMaterial(i));
        }
        if (OriginalMaterialCount > 0)
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
    }

    // Creer le materiau hologramme (cyan translucide) pour quand on traverse des objets
    if (HologramBaseMaterial)
    {
        HologramMaterial = UMaterialInstanceDynamic::Create(HologramBaseMaterial, this);
        if (HologramMaterial)
        {
            HologramMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.0f, 0.8f, 1.0f, 0.3f));
            HologramMaterial->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.0f, 0.8f, 1.0f, 0.3f));
            HologramMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.3f);
        }
    }
    
    UE_LOG(LogTemp, Verbose, TEXT("TDEnemyFlying %s: HologramBaseMat=%d HologramMat=%d OrigMatCount=%d VisibleMesh=%d"),
        *GetName(), HologramBaseMaterial != nullptr, HologramMaterial != nullptr, OriginalMaterialCount, VisibleMesh != nullptr);

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

    // === HOLOGRAMME: detecter si on est a l'interieur d'un objet/ennemi ===
    if (VisibleMesh && HologramMaterial)
    {
        bool bTouchingSomething = false;
        
        // Throttle: scan fallback seulement toutes les 0.2s (overlap query = chaque frame car pas cher)
        HologramScanTimer += DeltaTime;
        bool bDoFallbackScan = (HologramScanTimer >= 0.2f);
        if (bDoFallbackScan) HologramScanTimer = 0.0f;
        
        // Methode 1: Overlap query avec TOUS les types d'objets (rapide, chaque frame)
        {
            TArray<FOverlapResult> GhostOverlaps;
            FCollisionQueryParams OvParams;
            OvParams.AddIgnoredActor(this);
            FCollisionObjectQueryParams ObjParams;
            ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
            ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);
            ObjParams.AddObjectTypesToQuery(ECC_Pawn);
            ObjParams.AddObjectTypesToQuery(ECC_PhysicsBody);
            ObjParams.AddObjectTypesToQuery(ECC_Vehicle);
            ObjParams.AddObjectTypesToQuery(ECC_Destructible);
            // Ajouter les channels custom Satisfactory (ECC_GameTraceChannel1 a 6)
            ObjParams.AddObjectTypesToQuery(ECollisionChannel::ECC_GameTraceChannel1);
            ObjParams.AddObjectTypesToQuery(ECollisionChannel::ECC_GameTraceChannel2);
            ObjParams.AddObjectTypesToQuery(ECollisionChannel::ECC_GameTraceChannel3);
            ObjParams.AddObjectTypesToQuery(ECollisionChannel::ECC_GameTraceChannel4);
            ObjParams.AddObjectTypesToQuery(ECollisionChannel::ECC_GameTraceChannel5);
            ObjParams.AddObjectTypesToQuery(ECollisionChannel::ECC_GameTraceChannel6);
            
            bTouchingSomething = GetWorld()->OverlapMultiByObjectType(
                GhostOverlaps, GetActorLocation(), FQuat::Identity,
                ObjParams, FCollisionShape::MakeSphere(10.0f), OvParams);
        }
        
        // Methode 2 (fallback, throttle 0.2s): overlap Pawn pour detecter autres ennemis proches
        if (!bTouchingSomething && bDoFallbackScan)
        {
            TArray<FOverlapResult> PawnOverlaps;
            FCollisionQueryParams PawnParams;
            PawnParams.AddIgnoredActor(this);
            if (GetWorld()->OverlapMultiByChannel(PawnOverlaps, GetActorLocation(), FQuat::Identity,
                ECC_Pawn, FCollisionShape::MakeSphere(150.0f), PawnParams))
            {
                bTouchingSomething = true;
            }
        }
        
        // Gerer le timer hologramme (persistence 3 secondes)
        if (bTouchingSomething)
        {
            HologramTimer = HologramDuration; // Reset timer a 3s
        }
        else if (HologramTimer > 0.0f)
        {
            HologramTimer -= DeltaTime;
        }
        
        bool bShouldBeHologram = (HologramTimer > 0.0f);
        
        // Activer hologramme
        if (bShouldBeHologram && !bIsHologramActive)
        {
            bIsHologramActive = true;
            // Forcer hologramme sur TOUS les slots materiaux
            for (int32 i = 0; i < VisibleMesh->GetNumMaterials(); i++)
            {
                VisibleMesh->SetMaterial(i, HologramMaterial);
            }
            UE_LOG(LogTemp, Verbose, TEXT("TDEnemyFlying %s: HOLOGRAMME ACTIVE"), *GetName());
        }
        // Desactiver hologramme (timer expire)
        else if (!bShouldBeHologram && bIsHologramActive)
        {
            bIsHologramActive = false;
            // Restaurer les materiaux originaux
            for (int32 i = 0; i < OriginalMaterials.Num() && i < VisibleMesh->GetNumMaterials(); i++)
            {
                if (bLaserActive && bBlinkState && BlackMaterial && i == 0)
                    VisibleMesh->SetMaterial(i, BlackMaterial);
                else
                    VisibleMesh->SetMaterial(i, OriginalMaterials[i]);
            }
            UE_LOG(LogTemp, Verbose, TEXT("TDEnemyFlying %s: HOLOGRAMME DESACTIVE"), *GetName());
        }
        // Forcer le hologramme chaque frame tant qu'il est actif (au cas ou le blink l'ecrase)
        else if (bShouldBeHologram && bIsHologramActive)
        {
            for (int32 i = 0; i < VisibleMesh->GetNumMaterials(); i++)
            {
                if (VisibleMesh->GetMaterial(i) != HologramMaterial)
                {
                    VisibleMesh->SetMaterial(i, HologramMaterial);
                }
            }
        }
    }

    // Toujours orienter l'oeil vers la cible
    RotateEyeTowardsTarget(DeltaTime);

    FVector MyLoc = GetActorLocation();

    // === VERIFICATION CIBLE VALIDE ===
    if (TargetBuilding && (!IsValid(TargetBuilding) || TargetBuilding->IsPendingKillPending()))
    {
        UE_LOG(LogTemp, Verbose, TEXT("TDEnemyFlying %s: cible detruite (par autre), reset!"), *GetName());
        StopLaser();
        TargetBuilding = nullptr;
        Waypoints.Empty();
        CurrentWaypointIndex = 0;
    }
    
    // === PERF: Cache spawner (1 seule fois) ===
    if (!CachedSpawner.IsValid())
    {
        for (TActorIterator<ATDCreatureSpawner> SpIt(GetWorld()); SpIt; ++SpIt) { CachedSpawner = *SpIt; break; }
    }
    ATDCreatureSpawner* SpawnerPtr = Cast<ATDCreatureSpawner>(CachedSpawner.Get());
    
    // Verifier si la cible est marquee comme detruite (via cache spawner, pas GetAllActorsOfClass)
    if (TargetBuilding && SpawnerPtr && SpawnerPtr->IsBuildingDestroyed(TargetBuilding))
    {
        StopLaser();
        TargetBuilding = nullptr;
    }

    // === PERF: Raycasts plafond/sol + targeting = THROTTLE toutes les 0.5s ===
    TargetScanTimer += DeltaTime;
    bool bDoFullScan = (TargetScanTimer >= TargetScanInterval);
    if (bDoFullScan) TargetScanTimer = 0.0f;
    
    // Raycasts plafond/sol seulement pendant le scan (pas chaque frame)
    float MyCeilingZ = MyLoc.Z + 10000.0f;
    float MyFloorZ = MyLoc.Z - 10000.0f;
    bool bUnderCeiling = false;
    bool bAboveFloor = false;
    
    if (bDoFullScan)
    {
        FCollisionQueryParams EnvParams;
        EnvParams.AddIgnoredActor(this);
        FHitResult CeilHit;
        bUnderCeiling = GetWorld()->LineTraceSingleByChannel(
            CeilHit, MyLoc, MyLoc + FVector(0, 0, 500.0f), ECC_WorldStatic, EnvParams);
        if (bUnderCeiling) MyCeilingZ = CeilHit.ImpactPoint.Z;
        
        FHitResult FloorHit;
        bAboveFloor = GetWorld()->LineTraceSingleByChannel(
            FloorHit, MyLoc, MyLoc - FVector(0, 0, 500.0f), ECC_WorldStatic, EnvParams);
        if (bAboveFloor) MyFloorZ = FloorHit.ImpactPoint.Z;
        
        // Si la cible est au-dessus du plafond OU en-dessous du sol -> abandonner
        if (TargetBuilding && IsValid(TargetBuilding))
        {
            float TargetZ = TargetBuilding->GetActorLocation().Z;
            if ((bUnderCeiling && TargetZ > MyCeilingZ + 100.0f) ||
                (bAboveFloor && TargetZ < MyFloorZ - 100.0f))
            {
                UE_LOG(LogTemp, Verbose, TEXT("TDEnemyFlying %s: cible inaccessible (plafond/sol), abandon!"), *GetName());
                StopLaser();
                TargetBuilding = nullptr;
            }
        }
    }

    // === SYSTEME DE CIBLAGE: seulement toutes les 0.5s (au lieu de chaque frame) ===
    TArray<FOverlapResult> Overlaps;
    if (bDoFullScan)
    {
        FCollisionQueryParams DetectParams;
        DetectParams.AddIgnoredActor(this);
        FCollisionShape DetectSphere = FCollisionShape::MakeSphere(1000.0f);
        GetWorld()->OverlapMultiByChannel(Overlaps, MyLoc, FQuat::Identity, ECC_WorldStatic, DetectSphere, DetectParams);
    }

    // Listes de cibles par accessibilite et priorite
    AActor* BestAccessibleTurret = nullptr;   float BestATDist = MAX_FLT;
    AActor* BestAccessibleMachine = nullptr;  float BestAMDist = MAX_FLT;
    AActor* BestAccessibleStructure = nullptr; float BestASDist = MAX_FLT;
    AActor* BestAccessibleInfra = nullptr;     float BestAIDist = MAX_FLT;
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

        // Ignorer les pylones laser fence (cibles uniquement quand bloque)
        if (OvClass.Contains(TEXT("LaserFence"))) continue;

        // Ignorer les batiments deja detruits
        if (SpawnerPtr && SpawnerPtr->IsBuildingDestroyed(OvActor)) continue;

        // Ignorer poutres et escaliers (non-destructibles)
        if (OvClass.Contains(TEXT("Beam")) || OvClass.Contains(TEXT("Stair")))
            continue;
        
        // Ignorer objets non-attaquables et decoratifs
        if (OvClass.Contains(TEXT("SpaceElevator")) || OvClass.Contains(TEXT("Ladder")) ||
            OvClass.Contains(TEXT("Walkway")) ||
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
        else if (OvClass.Contains(TEXT("ConveyorBelt")) || OvClass.Contains(TEXT("ConveyorLift")) ||
                 OvClass.Contains(TEXT("ConveyorAttachment")) || OvClass.Contains(TEXT("ConveyorPole")) ||
                 OvClass.Contains(TEXT("PowerLine")) || OvClass.Contains(TEXT("Wire")) ||
                 OvClass.Contains(TEXT("PowerPole")) || OvClass.Contains(TEXT("PowerTower")) ||
                 OvClass.Contains(TEXT("RailroadTrack")) || OvClass.Contains(TEXT("PillarBase")) ||
                 OvClass.Contains(TEXT("Pipeline")) || OvClass.Contains(TEXT("Pipe")))
            Prio = 4; // infrastructure (basse priorite mais destructible)

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
            else if (Prio == 4 && Dist < BestAIDist) { BestAIDist = Dist; BestAccessibleInfra = OvActor; }
        }
        else if (Prio <= 2) // Seulement tracker les murs qui bloquent des cibles de valeur
        {
            // Le mur qui bloque cette machine
            AActor* BlockingActor = LOSHit.GetActor();
            if (BlockingActor && IsValid(BlockingActor) && Dist < BestBMDist)
            {
                FString BlockerClass = BlockingActor->GetClass()->GetName();
                // Si bloque par une fence -> chercher breche d'abord, sinon cibler pylone
                if (BlockerClass.Contains(TEXT("LaserFence")))
                {
                    // 1) Breche existante? -> ne pas attaquer le pylone, voler vers la breche
                    FVector BreachLoc = ATDLaserFence::FindNearestBreach(MyLoc, 5000.0f);
                    if (!BreachLoc.IsZero())
                    {
                        // Breche trouvee: skip, le flying ira naturellement contourner
                        // (pas de BestBlockingWall = pas d'attaque pylone)
                    }
                    else
                    {
                        // 2) Pas de breche -> cibler le pylone le plus proche
                        ATDLaserFence* NearestPylon = nullptr;
                        float BestPD = MAX_FLT;
                        for (ATDLaserFence* P : ATDLaserFence::AllPylons)
                        {
                            if (!P || !IsValid(P) || !P->bHasPower) continue;
                            float PD = FVector::Dist(MyLoc, P->GetActorLocation());
                            if (PD < BestPD) { BestPD = PD; NearestPylon = P; }
                        }
                        if (NearestPylon)
                        {
                            BestBMDist = Dist;
                            BestBlockedMachine = OvActor;
                            BestBlockingWall = NearestPylon;
                        }
                    }
                }
                else if (BlockerClass.StartsWith(TEXT("Build_")))
                {
                    BestBMDist = Dist;
                    BestBlockedMachine = OvActor;
                    BestBlockingWall = BlockingActor;
                }
            }
        }
    }

    // === DECISION DE CIBLAGE PAR COUCHE (seulement si scan actif) ===
    if (bDoFullScan)
    {
    AActor* SmartTarget = nullptr;
    bool bTargetIsWall = false;

    // Couche 1: cibles accessibles (turrets > machines > murs bloquants > structures)
    if (BestAccessibleTurret)
        SmartTarget = BestAccessibleTurret;
    else if (BestAccessibleMachine)
        SmartTarget = BestAccessibleMachine;
    else if (BestBlockingWall)
    {
        SmartTarget = BestBlockingWall;
        bTargetIsWall = true;
    }
    else if (BestAccessibleStructure)
        SmartTarget = BestAccessibleStructure;
    else if (BestAccessibleInfra)
        SmartTarget = BestAccessibleInfra;

    // Appliquer la decision
    if (SmartTarget)
    {
        if (TargetBuilding != SmartTarget)
        {
            SetTarget(SmartTarget);
            
            // Recalculer fly path dynamique depuis position actuelle
            if (SpawnerPtr)
            {
                TArray<FVector> NewPath = SpawnerPtr->GetFlyPathFor(MyLoc, SmartTarget->GetActorLocation());
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
                UE_LOG(LogTemp, Verbose, TEXT("TDEnemyFlying %s: mur %s bloque %s, attaque le mur!"),
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
    } // fin bDoFullScan pour ciblage intelligent

    // === COMPORTEMENT COURANT (chaque frame): attaque ou vol vers cible existante ===
    if (!bDoFullScan && TargetBuilding && IsValid(TargetBuilding))
    {
        float DistanceToTarget = FVector::Dist(MyLoc, TargetBuilding->GetActorLocation());

        if (DistanceToTarget <= AttackRange)
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
    else if (!TargetBuilding || !IsValid(TargetBuilding))
    {
        // Plus de cible - couper le laser, chercher seulement pendant les scans
        StopLaser();
        
        if (bDoFullScan)
        {
            // Recherche fallback via TActorIterator (seulement toutes les 0.5s)
            UE_LOG(LogTemp, Verbose, TEXT("TDEnemyFlying %s: cible detruite, recherche..."), *GetName());

            AActor* BestTurret = nullptr;    float BestTurretDist = MAX_FLT;
            AActor* BestMachine = nullptr;   float BestMachineDist = MAX_FLT;
            AActor* BestStructure = nullptr; float BestStructureDist = MAX_FLT;

            for (TActorIterator<AActor> It(GetWorld()); It; ++It)
            {
                AActor* Actor = *It;
                if (!Actor || !IsValid(Actor)) continue;

                float Dist = FVector::Dist(MyLoc, Actor->GetActorLocation());
                FString ClassName = Actor->GetClass()->GetName();

                bool bIsTDBld = ClassName.StartsWith(TEXT("TD")) && !ClassName.StartsWith(TEXT("TDEnemy")) && !ClassName.StartsWith(TEXT("TDCreature")) && !ClassName.StartsWith(TEXT("TDWorld")) && !ClassName.StartsWith(TEXT("TDDropship"));
                if (!ClassName.StartsWith(TEXT("Build_")) && !bIsTDBld) continue;

                // Ignorer les pylones laser fence
                if (ClassName.Contains(TEXT("LaserFence"))) continue;

                if (ClassName.Contains(TEXT("Beam")) || ClassName.Contains(TEXT("Stair")))
                    continue;
                
                if (ClassName.Contains(TEXT("Wall")) || ClassName.Contains(TEXT("Foundation")) ||
                    ClassName.Contains(TEXT("Ramp")) || ClassName.Contains(TEXT("Fence")) ||
                    ClassName.Contains(TEXT("Roof")) || ClassName.Contains(TEXT("Frame")) ||
                    ClassName.Contains(TEXT("Pillar")) || ClassName.Contains(TEXT("Quarter")))
                    continue;
                
                if (ClassName.Contains(TEXT("SpaceElevator")) || ClassName.Contains(TEXT("Ladder")) ||
                    ClassName.Contains(TEXT("Walkway")) ||
                    ClassName.Contains(TEXT("Catwalk")) || ClassName.Contains(TEXT("Sign")) ||
                    ClassName.Contains(TEXT("Light")) || ClassName.Contains(TEXT("HubTerminal")) ||
                    ClassName.Contains(TEXT("WorkBench")) || ClassName.Contains(TEXT("TradingPost")) ||
                    ClassName.Contains(TEXT("Tetromino")) || ClassName.Contains(TEXT("Potty")) ||
                    ClassName.Contains(TEXT("SnowDispenser")) || ClassName.Contains(TEXT("Decoration")) ||
                    ClassName.Contains(TEXT("Calendar")) || ClassName.Contains(TEXT("Fireworks")) ||
                    ClassName.Contains(TEXT("GolfCart")) || ClassName.Contains(TEXT("CandyCane")))
                    continue;

                if (bIsTDBld)
                {
                    if (Dist < BestTurretDist) { BestTurretDist = Dist; BestTurret = Actor; }
                }
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
}

void ATDEnemyFlying::FlyTowardsTarget(float DeltaTime)
{
    if (!TargetBuilding || !IsValid(TargetBuilding)) return;

    FVector MyLocation = GetActorLocation();
    FVector TargetLocation = TargetBuilding->GetActorLocation();

    // === MODE WAYPOINTS 3D: suivre le chemin pre-calcule ===
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
        }
        else
        {
            bFollowingWaypoints = false;
        }
    }
    
    // === Calcul hauteur cible ===
    float TargetZ;
    if (bFollowingWaypoints)
    {
        // En mode waypoint: voler DIRECTEMENT vers le waypoint (pas de raycast)
        TargetZ = EffectiveTarget.Z;
    }
    else
    {
        // Mode direct vers la cible: voler a hauteur cible + offset
        FVector ToTarget2DCheck = FVector(TargetLocation.X - MyLocation.X, TargetLocation.Y - MyLocation.Y, 0.0f);
        float HDist = ToTarget2DCheck.Size();
        
        if (HDist < AttackRange * 0.8f)
            TargetZ = TargetLocation.Z + FlyHeightOffset;
        else
            TargetZ = TargetLocation.Z + 500.0f;
    }

    // === Mouvement direct vers la cible (INTANGIBLE - traverse tout) ===
    FVector ToTarget2D = FVector(EffectiveTarget.X - MyLocation.X, EffectiveTarget.Y - MyLocation.Y, 0.0f);
    FVector HorizontalDir = ToTarget2D.GetSafeNormal();
    if (HorizontalDir.IsNearlyZero()) HorizontalDir = FVector::ForwardVector;

    float ZDiff = TargetZ - MyLocation.Z;
    float VerticalSpeed = FMath::Clamp(ZDiff * 3.0f, -FlySpeed, FlySpeed);

    // === Bob sinusoidal ===
    float BobOffset = FMath::Sin(BobTimer) * BobAmplitude * DeltaTime;

    // === Mouvement final (pas de sweep, pas de collision, pas de separation) ===
    FVector Movement = HorizontalDir * FlySpeed * DeltaTime
        + FVector(0, 0, VerticalSpeed * DeltaTime)
        + FVector(0, 0, BobOffset);

    // SetActorLocation SANS sweep (false) = traverse tout
    SetActorLocation(MyLocation + Movement, false);

    // Orienter dans la direction de deplacement
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

        // Utiliser le spawner cache (pas de GetAllActorsOfClass chaque tick)
        ATDCreatureSpawner* DmgSpawner = Cast<ATDCreatureSpawner>(CachedSpawner.Get());
        if (DmgSpawner)
        {
            DmgSpawner->DamageBuilding(TargetBuilding, DamageThisTick);
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

    // Faire clignoter le mesh en noir pendant le tir (SAUF si hologramme actif)
    if (VisibleMesh && BlackMaterial && OriginalMaterial && !bIsHologramActive)
    {
        float Pulse = FMath::Sin(GetWorld()->GetTimeSeconds() * 8.0f);
        bool bNewBlinkState = (Pulse > 0.0f);
        if (bNewBlinkState != bBlinkState)
        {
            bBlinkState = bNewBlinkState;
            VisibleMesh->SetMaterial(0, bBlinkState ? BlackMaterial : OriginalMaterial);
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
    // Restaurer le materiau original (SAUF si hologramme actif)
    if (VisibleMesh && OriginalMaterial && !bIsHologramActive)
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
        UE_LOG(LogTemp, Verbose, TEXT("TDEnemyFlying %s cible: %s"), *GetName(), *NewTarget->GetName());
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
