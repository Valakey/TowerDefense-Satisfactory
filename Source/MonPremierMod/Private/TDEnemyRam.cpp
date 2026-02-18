#include "TDEnemyRam.h"
#include "TDCreatureSpawner.h"
#include "TDLaserFence.h"
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
    VisibleMesh->SetCastShadow(false);  // PERF: pas d'ombre par belier

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
    
    // Ground trace au spawn pour coller au sol immediatement (ignorer batiments)
    FVector SpawnLoc = GetActorLocation();
    TArray<FHitResult> SpawnHits;
    FCollisionQueryParams TraceParams;
    TraceParams.AddIgnoredActor(this);
    bool bFoundGround = false;
    if (GetWorld()->LineTraceMultiByChannel(SpawnHits, SpawnLoc + FVector(0,0,500), SpawnLoc - FVector(0,0,5000), ECC_WorldStatic, TraceParams))
    {
        for (const FHitResult& Hit : SpawnHits)
        {
            AActor* HitActor = Hit.GetActor();
            if (HitActor)
            {
                FString HitClass = HitActor->GetClass()->GetName();
                if (HitClass.StartsWith(TEXT("Build_")) || 
                    (HitClass.StartsWith(TEXT("TD")) && !HitClass.StartsWith(TEXT("TDEnemy"))))
                    continue;
            }
            BaseHeight = Hit.ImpactPoint.Z + 50.0f;
            SpawnLoc.Z = BaseHeight;
            SetActorLocation(SpawnLoc);
            bFoundGround = true;
            break;
        }
    }
    if (!bFoundGround)
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

    // === ATTERRISSAGE: attendre le premier contact sol ===
    if (!bHasLanded)
    {
        FHitResult LandCheck;
        FCollisionQueryParams LandParams;
        LandParams.AddIgnoredActor(this);
        FVector Loc = GetActorLocation();
        if (GetWorld()->LineTraceSingleByChannel(LandCheck, Loc + FVector(0,0,200), Loc - FVector(0,0,500), ECC_WorldStatic, LandParams))
        {
            bHasLanded = true;
            UE_LOG(LogTemp, Warning, TEXT("TDEnemyRam %s: ATTERRI a %s (flow field mode)"), *GetName(), *Loc.ToString());
            // Plus besoin de BFS ici: le flow field sera query dans UpdateRoaming/Approaching
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
        
        // Ground trace pour trouver le TERRAIN (ignorer les batiments pour ne pas monter dessus)
        TArray<FHitResult> GroundHits;
        FVector TraceStart = Loc + FVector(0, 0, 200.0f);
        FVector TraceEnd = Loc - FVector(0, 0, 2000.0f);
        FCollisionQueryParams TraceParams;
        TraceParams.AddIgnoredActor(this);
        
        if (GetWorld()->LineTraceMultiByChannel(GroundHits, TraceStart, TraceEnd, ECC_WorldStatic, TraceParams))
        {
            for (const FHitResult& Hit : GroundHits)
            {
                AActor* HitActor = Hit.GetActor();
                if (HitActor)
                {
                    FString HitClass = HitActor->GetClass()->GetName();
                    // Ignorer les batiments (Build_* et TD*) - on veut le terrain uniquement
                    if (HitClass.StartsWith(TEXT("Build_")) || 
                        (HitClass.StartsWith(TEXT("TD")) && !HitClass.StartsWith(TEXT("TDEnemy"))))
                        continue;
                }
                BaseHeight = Hit.ImpactPoint.Z + 50.0f;  // 50u au-dessus du sol (ras du sol)
                break;
            }
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
    
    // === PERF: Cache spawner une seule fois (au lieu de GetAllActorsOfClass chaque frame) ===
    if (!CachedSpawnerPtr.IsValid())
    {
        for (TActorIterator<ATDCreatureSpawner> SpIt(GetWorld()); SpIt; ++SpIt) { CachedSpawnerPtr = *SpIt; break; }
    }
    ATDCreatureSpawner* CachedSpawner = Cast<ATDCreatureSpawner>(CachedSpawnerPtr.Get());
    
    // Invalider les cibles marquees comme detruites (structures)
    if (CachedSpawner)
    {
        if (TargetBuilding && CachedSpawner->IsBuildingDestroyed(TargetBuilding))
        {
            TargetBuilding = nullptr;
        }
        if (PrimaryTarget && CachedSpawner->IsBuildingDestroyed(PrimaryTarget))
        {
            PrimaryTarget = nullptr;
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
    ATDCreatureSpawner* CachedSpawner = Cast<ATDCreatureSpawner>(CachedSpawnerPtr.Get());
    
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
        if (CachedSpawner)
        {
            FAttackPath Path = CachedSpawner->GetBestAttackPath(GetActorLocation(), false);
            if (Path.Target && IsValid(Path.Target))
            {
                Building = Path.Target;
                if (Path.bIsEnclosed && Path.WallToBreak && IsValid(Path.WallToBreak))
                {
                    BlockingWall = Path.WallToBreak;
                }
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

    // === FENCE: d'abord chercher une breche, sinon attaquer pylone ===
    if (!Building && (ATDLaserFence::AllPylons.Num() > 0 || ATDLaserFence::BreachPoints.Num() > 0) && NoAttackTimer > 5.0f)
    {
        FVector RamLoc = GetActorLocation();

        // 1) Chercher une breche existante
        FVector BreachLoc = ATDLaserFence::FindNearestBreach(RamLoc, 5000.0f);
        if (!BreachLoc.IsZero())
        {
            UE_LOG(LogTemp, Warning, TEXT("TDEnemyRam %s: BRECHE trouvee a %s -> contournement!"), *GetName(), *BreachLoc.ToString());
            // Utiliser la breche comme cible temporaire de mouvement
            // On va se deplacer vers la breche via flow field en gardant TargetBuilding
            FVector Dir = (BreachLoc - RamLoc).GetSafeNormal2D();
            FVector NewLoc = RamLoc + Dir * MoveSpeed * SpeedMultiplier * 0.016f;
            NewLoc.Z = RamLoc.Z;
            SetActorLocation(NewLoc);
            SetActorRotation(Dir.Rotation());
            NoAttackTimer = 0.0f;
            return;
        }

        // 2) Pas de breche -> attaquer le pylone le plus proche
        ATDLaserFence* NearestPylon = nullptr;
        float BestPD = 1500.0f;
        for (ATDLaserFence* P : ATDLaserFence::AllPylons)
        {
            if (!P || !IsValid(P) || !P->bHasPower) continue;
            if (P->Barriers.Num() == 0) continue;
            float D = FVector::Dist(RamLoc, P->GetActorLocation());
            if (D < BestPD) { BestPD = D; NearestPylon = P; }
        }
        if (NearestPylon)
        {
            UE_LOG(LogTemp, Warning, TEXT("TDEnemyRam %s: PAS DE BRECHE! Targeting pylon %s"), *GetName(), *NearestPylon->GetName());
            TargetBuilding = NearestPylon;
            CurrentState = ERamState::Approaching;
            NoAttackTimer = 0.0f;
            return;
        }
    }

    // Pas de cible proche: utiliser le flow field pour se deplacer vers la cible
    if (TargetBuilding && IsValid(TargetBuilding))
    {
        FVector MyLoc = GetActorLocation();
        FVector Direction;
        
        // === FLOW FIELD: lookup O(1) (utilise CachedSpawner du Tick) ===
        FVector FlowDir = FVector::ZeroVector;
        if (CachedSpawner)
        {
            FlowDir = CachedSpawner->QueryGroundFlow(MyLoc, TargetBuilding);
        }
        
        if (!FlowDir.IsNearlyZero())
        {
            Direction = FlowDir;
        }
        else
        {
            // Fallback direct vers la cible
            Direction = (TargetBuilding->GetActorLocation() - MyLoc).GetSafeNormal2D();
        }

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
    ATDCreatureSpawner* CachedSpawner = Cast<ATDCreatureSpawner>(CachedSpawnerPtr.Get());
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
    
    // Ground trace pendant la charge aussi (ignorer batiments pour rester au sol)
    TArray<FHitResult> ChargeGroundHits;
    FVector CTraceStart = NewLoc + FVector(0, 0, 200.0f);
    FVector CTraceEnd = NewLoc - FVector(0, 0, 2000.0f);
    FCollisionQueryParams CTraceParams;
    CTraceParams.AddIgnoredActor(this);
    if (GetWorld()->LineTraceMultiByChannel(ChargeGroundHits, CTraceStart, CTraceEnd, ECC_WorldStatic, CTraceParams))
    {
        for (const FHitResult& Hit : ChargeGroundHits)
        {
            AActor* HitActor = Hit.GetActor();
            if (HitActor)
            {
                FString HitClass = HitActor->GetClass()->GetName();
                if (HitClass.StartsWith(TEXT("Build_")) || 
                    (HitClass.StartsWith(TEXT("TD")) && !HitClass.StartsWith(TEXT("TDEnemy"))))
                    continue;
            }
            BaseHeight = Hit.ImpactPoint.Z + 50.0f;
            break;
        }
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
                    // Infliger degats au mur (via CachedSpawner)
                    float FinalDamage = ChargeDamage * (SpeedMultiplier < 1.0f ? (1.0f - SlowDamageReduction) : 1.0f);
                    if (CachedSpawner)
                    {
                        CachedSpawner->DamageBuilding(WallActor, FinalDamage);
                        CachedSpawner->MarkBuildingAttacked(WallActor);
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

    // Verifier collision avec batiments sur le chemin via sphere overlap (au lieu de TActorIterator<AActor>)
    if (!bHitTarget)
    {
        TArray<FOverlapResult> ChargeOverlaps;
        FCollisionQueryParams ChargeOvParams;
        ChargeOvParams.AddIgnoredActor(this);
        if (GetWorld()->OverlapMultiByChannel(ChargeOverlaps, NewLoc, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(300.0f), ChargeOvParams))
        {
            for (const FOverlapResult& Ov : ChargeOverlaps)
            {
                AActor* Actor = Ov.GetActor();
                if (!Actor || !IsValid(Actor)) continue;
                FString ClassName = Actor->GetClass()->GetName();
                bool bIsTDB = ClassName.StartsWith(TEXT("TD")) && !ClassName.StartsWith(TEXT("TDEnemy")) && !ClassName.StartsWith(TEXT("TDCreature")) && !ClassName.StartsWith(TEXT("TDWorld")) && !ClassName.StartsWith(TEXT("TDDropship"));
                if (ClassName.StartsWith(TEXT("Build_")) || bIsTDB)
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

        // Infliger les degats via le spawner (cache)
        if (CachedSpawner)
        {
            CachedSpawner->DamageBuilding(HitBuilding, FinalDamage);
            CachedSpawner->MarkBuildingAttacked(HitBuilding);
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
    
    // Utiliser le spawner cache (pas de TActorIterator a chaque appel)
    ATDCreatureSpawner* Spawner = Cast<ATDCreatureSpawner>(CachedSpawnerPtr.Get());
    
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

        // Ignorer les pylones laser fence (cibles uniquement quand bloque)
        if (ClassName.Contains(TEXT("LaserFence"))) continue;

        // Ignorer poutres et escaliers (non-destructibles)
        if (ClassName.Contains(TEXT("Beam")) || ClassName.Contains(TEXT("Stair")))
            continue;
        
        // Ignorer objets non-attaquables et decoratifs
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
    UE_LOG(LogTemp, Verbose, TEXT("TDEnemyRam %s: -%.0f HP (reste %.0f/%.0f)"), *GetName(), DamageAmount, Health, MaxHealth);
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
