#include "TDEnemy.h"
#include "TDCreatureSpawner.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "Components/PointLightComponent.h"

ATDEnemy::ATDEnemy()
{
    PrimaryActorTick.bCanEverTick = true;

    // Configuration de la capsule de collision
    GetCapsuleComponent()->InitCapsuleSize(60.0f, 80.0f);
    
    // IMPORTANT: Configurer les collisions pour recevoir les degats des armes du joueur
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    GetCapsuleComponent()->SetCollisionObjectType(ECC_Pawn);
    GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Block);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    GetCapsuleComponent()->SetGenerateOverlapEvents(true);

    // Configuration du mouvement - optimise pour terrain accidente
    UCharacterMovementComponent* MovementComp = GetCharacterMovement();
    if (MovementComp)
    {
        MovementComp->MaxWalkSpeed = MoveSpeed;
        MovementComp->bOrientRotationToMovement = true;
        MovementComp->RotationRate = FRotator(0.0f, 720.0f, 0.0f);  // Rotation plus rapide
        MovementComp->GravityScale = 1.0f;
        
        // Ameliorer le mouvement sur terrain difficile
        MovementComp->MaxStepHeight = 80.0f;           // Peut monter plus haut
        MovementComp->SetWalkableFloorAngle(70.0f);    // Peut monter des pentes plus raides
        MovementComp->bCanWalkOffLedges = true;
        MovementComp->bCanWalkOffLedgesWhenCrouching = true;
        MovementComp->PerchRadiusThreshold = 0.0f;     // Pas de blocage sur les rebords
        MovementComp->PerchAdditionalHeight = 0.0f;
        MovementComp->BrakingDecelerationWalking = 100.0f;  // Moins de freinage
        MovementComp->GroundFriction = 4.0f;           // Moins de friction
        MovementComp->bUseFlatBaseForFloorChecks = true;
        
        // Permettre de pousser les objets
        MovementComp->bPushForceUsingZOffset = true;
        MovementComp->PushForceFactor = 500000.0f;
    }

    // Creer le mesh visible (custom FBX)
    VisibleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisibleMesh"));
    VisibleMesh->SetupAttachment(RootComponent);
    VisibleMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
    VisibleMesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));  // Taille ennemi
    VisibleMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));  // Orientation oeil vers avant
    
    // Charger le mesh custom EnemyEye
    static ConstructorHelpers::FObjectFinder<UStaticMesh> EnemyMesh(TEXT("/MonPremierMod/Meshes/Ennemy/EnemyEye.EnemyEye"));
    if (EnemyMesh.Succeeded())
    {
        VisibleMesh->SetStaticMesh(EnemyMesh.Object);
        UE_LOG(LogTemp, Warning, TEXT("TDEnemy: EnemyEye mesh charge!"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("TDEnemy: Impossible de charger EnemyEye!"));
    }
    
    // Forcer le chargement du materiau Material_001
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> EnemyMat(TEXT("/MonPremierMod/Meshes/Ennemy/Material_001.Material_001"));
    if (EnemyMat.Succeeded())
    {
        VisibleMesh->SetMaterial(0, EnemyMat.Object);
        UE_LOG(LogTemp, Warning, TEXT("TDEnemy: Material_001 CHARGE avec succes!"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("TDEnemy: ECHEC chargement Material_001 - utilise materiau par defaut du mesh"));
    }
    
    // Configuration du mesh visible
    VisibleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    VisibleMesh->SetCastShadow(true);
    VisibleMesh->SetVisibility(true);
    

    // Pas de controller AI par defaut
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    // Creer le composant audio pour sons one-shot avec attenuation 3D
    OneShotAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("OneShotAudio"));
    OneShotAudioComponent->SetupAttachment(RootComponent);
    OneShotAudioComponent->bAutoActivate = false;
    OneShotAudioComponent->bAllowSpatialization = true;
    OneShotAudioComponent->bOverrideAttenuation = true;
    OneShotAudioComponent->AttenuationOverrides.bAttenuate = true;
    OneShotAudioComponent->AttenuationOverrides.bSpatialize = true;
    OneShotAudioComponent->AttenuationOverrides.FalloffDistance = 3000.0f;
    OneShotAudioComponent->AttenuationOverrides.AttenuationShapeExtents = FVector(200.0f, 0.0f, 0.0f);

    // Creer le composant audio pour levitation (boucle) avec attenuation 3D
    LevitationAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("LevitationAudio"));
    LevitationAudioComponent->SetupAttachment(RootComponent);
    LevitationAudioComponent->bAutoActivate = false;
    LevitationAudioComponent->bAllowSpatialization = true;
    LevitationAudioComponent->bOverrideAttenuation = true;
    LevitationAudioComponent->AttenuationOverrides.bAttenuate = true;
    LevitationAudioComponent->AttenuationOverrides.bSpatialize = true;
    LevitationAudioComponent->AttenuationOverrides.FalloffDistance = 2000.0f;
    LevitationAudioComponent->AttenuationOverrides.AttenuationShapeExtents = FVector(150.0f, 0.0f, 0.0f);

    // Charger les sons
    static ConstructorHelpers::FObjectFinder<USoundWave> AttackSoundObj(TEXT("/MonPremierMod/Audios/Ennemy/AttackMob.AttackMob"));
    if (AttackSoundObj.Succeeded())
    {
        AttackSound = AttackSoundObj.Object;
        UE_LOG(LogTemp, Warning, TEXT("TDEnemy: AttackSound charge!"));
    }

    static ConstructorHelpers::FObjectFinder<USoundWave> TargetChangeSoundObj(TEXT("/MonPremierMod/Audios/Ennemy/Changementdecible.Changementdecible"));
    if (TargetChangeSoundObj.Succeeded())
    {
        TargetChangeSound = TargetChangeSoundObj.Object;
        UE_LOG(LogTemp, Warning, TEXT("TDEnemy: TargetChangeSound charge!"));
    }

    static ConstructorHelpers::FObjectFinder<USoundWave> JumpSoundObj(TEXT("/MonPremierMod/Audios/Ennemy/Jump.Jump"));
    if (JumpSoundObj.Succeeded())
    {
        JumpSound = JumpSoundObj.Object;
        UE_LOG(LogTemp, Warning, TEXT("TDEnemy: JumpSound charge!"));
    }

    static ConstructorHelpers::FObjectFinder<USoundWave> LevitationSoundObj(TEXT("/MonPremierMod/Audios/Ennemy/deplacementennemy.deplacementennemy"));
    if (LevitationSoundObj.Succeeded())
    {
        LevitationSound = LevitationSoundObj.Object;
        UE_LOG(LogTemp, Warning, TEXT("TDEnemy: LevitationSound charge!"));
    }

}

void ATDEnemy::BeginPlay()
{
    Super::BeginPlay();
    
    UE_LOG(LogTemp, Warning, TEXT("TDEnemy spawned: %s at %s"), *GetName(), *GetActorLocation().ToString());
    
    // Laisser le mesh utiliser son materiau original package dans le .pak
    // (ne pas override le materiau)
    
    // S'assurer que la vitesse est correcte
    OriginalMoveSpeed = MoveSpeed;
    if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
    {
        MovementComp->MaxWalkSpeed = MoveSpeed;
    }
    
    // Activer l'outline rouge
    EnableOutline();

    // Demarrer le son de levitation en boucle
    if (LevitationAudioComponent && LevitationSound)
    {
        // Forcer le son en boucle
        LevitationSound->bLooping = true;
        LevitationAudioComponent->SetSound(LevitationSound);
        LevitationAudioComponent->SetVolumeMultiplier(0.10f);
        LevitationAudioComponent->bAllowSpatialization = true;
        LevitationAudioComponent->bIsUISound = false;
        LevitationAudioComponent->Play();
        UE_LOG(LogTemp, Warning, TEXT("TDEnemy: Levitation sound started (looping)!"));
    }
}

void ATDEnemy::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bIsDead) return;
    
    // Timer anti-spam collision
    if (CollisionLogTimer > 0.0f) CollisionLogTimer -= DeltaTime;

    // === ATTERRISSAGE: attendre d'etre au sol avant de demander un chemin ===
    if (!bHasLanded)
    {
        UCharacterMovementComponent* MC = GetCharacterMovement();
        if (MC && MC->IsMovingOnGround())
        {
            bHasLanded = true;
            UE_LOG(LogTemp, Warning, TEXT("TDEnemy %s: ATTERRI a %s, demande chemin..."), *GetName(), *GetActorLocation().ToString());
            
            // Maintenant qu'on est au sol, demander un chemin vers la cible
            if (TargetBuilding && IsValid(TargetBuilding))
            {
                ATDCreatureSpawner* Spawner = nullptr;
                for (TActorIterator<ATDCreatureSpawner> SpIt(GetWorld()); SpIt; ++SpIt) { Spawner = *SpIt; break; }
                if (Spawner)
                {
                    TArray<FVector> Path = Spawner->GetGroundPathFor(GetActorLocation(), TargetBuilding->GetActorLocation());
                    if (Path.Num() > 0)
                    {
                        Waypoints = Path;
                        CurrentWaypointIndex = 0;
                        UE_LOG(LogTemp, Warning, TEXT("  -> %d waypoints recus"), Path.Num());
                    }
                    else
                    {
                        UE_LOG(LogTemp, Warning, TEXT("  -> AUCUN chemin au sol, FindNearestReachableTarget"));
                        FindNearestReachableTarget();
                    }
                }
            }
            else
            {
                FindNearestReachableTarget();
            }
        }
        else
        {
            return;  // Encore en l'air, ne rien faire
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
            if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
            {
                MovementComp->MaxWalkSpeed = MoveSpeed;
            }
        }
    }

    // Timer d'attaque
    if (AttackTimer > 0.0f)
    {
        AttackTimer -= DeltaTime;
    }
    
    // === BLACKLIST: reset toutes les 15s pour re-essayer les cibles ===
    BlacklistResetTimer += DeltaTime;
    if (BlacklistResetTimer >= 15.0f)
    {
        BlacklistedTargets.Empty();
        BlacklistResetTimer = 0.0f;
    }
    
    // Retarget cooldown (eviter scans TActorIterator trop frequents)
    if (RetargetCooldown > 0.0f) RetargetCooldown -= DeltaTime;
    
    // === DETECTION CIBLE DETRUITE: reagir rapidement mais pas tous en meme frame ===
    if (TargetBuilding && !IsValid(TargetBuilding))
    {
        TargetBuilding = nullptr;
        Waypoints.Empty();
        CurrentWaypointIndex = 0;
        // Etaler les BFS: delai aleatoire 0-0.5s pour eviter 10+ BFS dans la meme frame
        ScanTimer = 3.0f - FMath::FRandRange(0.0f, 0.5f);
    }
    
    // === SCAN PERIODIQUE: chercher cible si aucune ===
    ScanTimer += DeltaTime;
    if (ScanTimer >= 3.0f)
    {
        ScanTimer = 0.0f;
        if (!TargetBuilding)
        {
            FindNearestReachableTarget();
        }
        
        // === DEBUG: etat complet toutes les 3s ===
        UCharacterMovementComponent* DbgMC = GetCharacterMovement();
        FString StateStr;
        if (!TargetBuilding) StateStr = TEXT("PAS_DE_CIBLE");
        else if (FVector::Dist(GetActorLocation(), TargetBuilding->GetActorLocation()) <= AttackRange) StateStr = TEXT("EN_PORTEE");
        else StateStr = TEXT("EN_ROUTE");
        UE_LOG(LogTemp, Verbose, TEXT("[DBG] %s: etat=%s pos=%s wp=%d/%d cible=%s dist=%.0f speed=%.0f moveMode=%d blacklist=%d"),
            *GetName(), *StateStr, *GetActorLocation().ToString(),
            CurrentWaypointIndex, Waypoints.Num(),
            TargetBuilding ? *TargetBuilding->GetClass()->GetName() : TEXT("null"),
            TargetBuilding ? FVector::Dist(GetActorLocation(), TargetBuilding->GetActorLocation()) : -1.0f,
            DbgMC ? DbgMC->MaxWalkSpeed : -1.0f,
            DbgMC ? (int32)DbgMC->MovementMode.GetValue() : -1,
            BlacklistedTargets.Num());
    }
    
    // === LOGIQUE PRINCIPALE ===
    if (TargetBuilding && IsValid(TargetBuilding))
    {
        float DistanceToTarget = FVector::Dist(GetActorLocation(), TargetBuilding->GetActorLocation());
        
        if (DistanceToTarget <= AttackRange)
        {
            if (CanAttackTarget())
            {
                // On peut attaquer! Reset les compteurs de blocage
                StuckTimer = 0.0f;
                ConsecutiveStuckCount = 0;
                LastPosition = GetActorLocation();
                FailedAttackAttempts = 0;
                
                if (AttackTimer <= 0.0f)
                {
                    AttackTarget();
                    AttackTimer = AttackCooldown;
                }
            }
            else
            {
                // A portee 3D mais ne peut pas attaquer -> continuer a se rapprocher
                MoveTowardsTarget(DeltaTime);
                FailedAttackAttempts++;
                if (FailedAttackAttempts == 1)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[CANTATTACK] %s: en portee (dist=%.0f) mais CanAttack=false pour %s (hauteur=%.0f, horizDist=%.0f, range=%.0f)"),
                        *GetName(), DistanceToTarget, *TargetBuilding->GetName(),
                        TargetBuilding->GetActorLocation().Z - GetActorLocation().Z,
                        FVector::Dist2D(GetActorLocation(), TargetBuilding->GetActorLocation()),
                        AttackRange);
                }
                if (FailedAttackAttempts >= 10)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[CANTATTACK] %s: 10 tentatives echouees, blacklist %s (dist=%.0f, hauteur=%.0f)"),
                        *GetName(), *TargetBuilding->GetName(), DistanceToTarget,
                        TargetBuilding->GetActorLocation().Z - GetActorLocation().Z);
                    MarkTargetUnreachable(TargetBuilding);
                    FindNearestReachableTarget();
                    FailedAttackAttempts = 0;
                }
            }
        }
        else
        {
            // Se deplacer vers la cible
            MoveTowardsTarget(DeltaTime);
            
            // Detection de blocage
            CheckIfStuck(DeltaTime);
        }
    }
    else
    {
        // FindNearestReachableTarget est deja appele par ScanTimer (toutes les 3s)
        // Pas de double appel ici - evite TActorIterator spam
        if (!TargetBuilding)
        {
            NoCibleTimer += DeltaTime;
            
            // Marcher vers la base la plus proche (cache spawner)
            if (!CachedSpawner.IsValid())
            {
                for (TActorIterator<ATDCreatureSpawner> SpIt(GetWorld()); SpIt; ++SpIt) { CachedSpawner = *SpIt; break; }
            }
            ATDCreatureSpawner* LostSpawner = Cast<ATDCreatureSpawner>(CachedSpawner.Get());
            if (LostSpawner)
            {
                FVector BaseCenter = LostSpawner->GetNearestBaseCenter(GetActorLocation());
                if (!BaseCenter.IsZero())
                {
                    FVector Dir = (BaseCenter - GetActorLocation()).GetSafeNormal();
                    Dir.Z = 0.0f;
                    if (!Dir.IsNearlyZero())
                    {
                        AddMovementInput(Dir, 1.0f);
                        FRotator TargetRot = Dir.Rotation();
                        SetActorRotation(FMath::RInterpTo(GetActorRotation(), TargetRot, DeltaTime, 5.0f));
                    }
                }
            }
            
            // Autodestruction seulement apres 60s sans cible (vraiment perdu)
            if (NoCibleTimer >= 60.0f)
            {
                UE_LOG(LogTemp, Warning, TEXT("TDEnemy %s: 60s sans cible, autodestruction a %s"), *GetName(), *GetActorLocation().ToString());
                Die();
                return;
            }
        }
        else
        {
            NoCibleTimer = 0.0f;
        }
    }
}

void ATDEnemy::MoveTowardsTarget(float DeltaTime)
{
    if (!TargetBuilding || !IsValid(TargetBuilding))
        return;

    FVector MyLoc = GetActorLocation();
    FVector Direction;
    
    // Mode detour (contournement obstacle immediat)
    if (DetourTimer > 0.0f && !DetourDirection.IsNearlyZero())
    {
        FVector ToTarget = (TargetBuilding->GetActorLocation() - MyLoc).GetSafeNormal();
        ToTarget.Z = 0.0f;
        Direction = (DetourDirection * 0.7f + ToTarget * 0.3f).GetSafeNormal();
        DetourTimer -= DeltaTime;
        if (DetourTimer <= 0.0f)
        {
            UE_LOG(LogTemp, Verbose, TEXT("[MOVE] %s: fin detour, retour waypoints wp=%d/%d"),
                *GetName(), CurrentWaypointIndex, Waypoints.Num());
        }
    }
    // Suivre les waypoints du pathfinding terrain
    else if (CurrentWaypointIndex < Waypoints.Num())
    {
        FVector WP = Waypoints[CurrentWaypointIndex];
        float DistToWP = FVector::Dist2D(MyLoc, WP);
        
        if (DistToWP < 250.0f)
        {
            int32 OldIdx = CurrentWaypointIndex;
            CurrentWaypointIndex++;
            
            // Log chaque waypoint atteint
            float DistToTarget = FVector::Dist(MyLoc, TargetBuilding->GetActorLocation());
            if (CurrentWaypointIndex < Waypoints.Num())
            {
                FVector NextWP = Waypoints[CurrentWaypointIndex];
                float DistToNextWP = FVector::Dist2D(MyLoc, NextWP);
                float NextDistToTarget = FVector::Dist(NextWP, TargetBuilding->GetActorLocation());
                // Detecter aller-retour: le prochain WP est-il plus loin de la cible?
                bool bBacktrack = NextDistToTarget > DistToTarget + 200.0f;
                UE_LOG(LogTemp, Warning, TEXT("[WP] %s: wp %d->%d/%d pos=%s nextWP=%s distWP=%.0f distCible=%.0f %s"),
                    *GetName(), OldIdx, CurrentWaypointIndex, Waypoints.Num(),
                    *MyLoc.ToString(), *NextWP.ToString(), DistToNextWP, DistToTarget,
                    bBacktrack ? TEXT("BACKTRACK!") : TEXT("ok"));
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("[WP] %s: DERNIER wp %d atteint! distCible=%.0f -> mode DIRECT"),
                    *GetName(), OldIdx, DistToTarget);
            }
        }
        
        if (CurrentWaypointIndex < Waypoints.Num())
        {
            Direction = (Waypoints[CurrentWaypointIndex] - MyLoc).GetSafeNormal();
            Direction.Z = 0.0f;
        }
        else
        {
            // Tous les waypoints consommes -> aller direct vers la cible
            Direction = (TargetBuilding->GetActorLocation() - MyLoc).GetSafeNormal();
            Direction.Z = 0.0f;
        }
    }
    // Pas de waypoints -> aller direct vers la cible
    else
    {
        Direction = (TargetBuilding->GetActorLocation() - MyLoc).GetSafeNormal();
        Direction.Z = 0.0f;
    }
    
    AddMovementInput(Direction, 1.0f);
    
    if (!Direction.IsNearlyZero())
    {
        FRotator TargetRotation = Direction.Rotation();
        SetActorRotation(FMath::RInterpTo(GetActorRotation(), TargetRotation, DeltaTime, 5.0f));
    }
}

void ATDEnemy::CheckIfStuck(float DeltaTime)
{
    StuckTimer += DeltaTime;
    
    if (StuckTimer >= 0.5f)
    {
        float DistanceMoved = FVector::Dist(GetActorLocation(), LastPosition);
        LastPosition = GetActorLocation();
        
        if (DistanceMoved < 20.0f)
        {
            ConsecutiveStuckCount++;
            
            if (ConsecutiveStuckCount >= 2)
            {
                FVector MyPos = GetActorLocation();
                float DistToTarget = TargetBuilding ? FVector::Dist(MyPos, TargetBuilding->GetActorLocation()) : 99999.f;
                bool bAtLastWP = (CurrentWaypointIndex >= Waypoints.Num());
                
                // === OPTI 1: Si proche de la cible, attaquer directement ===
                // Meme si pas au dernier WP, si l'ennemi est stuck et proche -> forcer l'attaque
                if (DistToTarget < 1500.0f && TargetBuilding && IsValid(TargetBuilding))
                {
                    ConsecutiveStuckCount = 0;
                    DetourAttempts = 0;
                    StuckTimer = 0.0f;
                    if (DistToTarget <= AttackRange * 1.5f)
                    {
                        // A portee -> attaquer directement
                        AttackTarget();
                    }
                    else
                    {
                        // Trop loin pour attaquer -> marcher en ligne droite vers la cible (ignorer obstacles)
                        FVector DirectDir = (TargetBuilding->GetActorLocation() - MyPos).GetSafeNormal();
                        DirectDir.Z = 0.0f;
                        DetourDirection = DirectDir;
                        DetourTimer = 2.0f;
                    }
                    return;
                }
                
                // === OPTI 2: Detecter un mur bloquant via les collisions recentes ===
                AActor* BlockingWall = nullptr;
                
                if (bHitWallRecently)
                {
                    float BestWallDist = 800.0f;
                    FVector FwdDir = FVector::ZeroVector;
                    if (TargetBuilding)
                    {
                        FwdDir = (TargetBuilding->GetActorLocation() - MyPos).GetSafeNormal();
                        FwdDir.Z = 0.0f;
                    }
                    
                    for (TActorIterator<AActor> WIt(GetWorld()); WIt; ++WIt)
                    {
                        AActor* WA = *WIt;
                        if (!WA || !IsValid(WA)) continue;
                        FString WC = WA->GetClass()->GetName();
                        if (!WC.StartsWith(TEXT("Build_"))) continue;
                        // BLACKLIST: ignorer transport/infra (jamais destructibles pour passage)
                        if (WC.Contains(TEXT("ConveyorBelt")) || WC.Contains(TEXT("ConveyorLift")) ||
                            WC.Contains(TEXT("ConveyorAttachment")) || WC.Contains(TEXT("ConveyorPole")) ||
                            WC.Contains(TEXT("PowerLine")) || WC.Contains(TEXT("Wire")) ||
                            WC.Contains(TEXT("PowerPole")) || WC.Contains(TEXT("PowerTower")) ||
                            WC.Contains(TEXT("Pipeline")) || WC.Contains(TEXT("PipeHyper")) ||
                            WC.Contains(TEXT("PipeSupport")) || WC.Contains(TEXT("PipeJunction")) ||
                            WC.Contains(TEXT("RailroadTrack")) || WC.Contains(TEXT("TrainStation")) ||
                            WC.Contains(TEXT("TrainDocking")) || WC.Contains(TEXT("TrainPlatform")) ||
                            WC.Contains(TEXT("Sign")) || WC.Contains(TEXT("Light")) ||
                            WC.Contains(TEXT("Ladder")))
                            continue;
                        // BLACKLIST: ignorer machines de production (cibles primaires, pas obstacles)
                        if (WC.Contains(TEXT("Constructor")) || WC.Contains(TEXT("Assembler")) ||
                            WC.Contains(TEXT("Manufacturer")) || WC.Contains(TEXT("Smelter")) ||
                            WC.Contains(TEXT("Foundry")) || WC.Contains(TEXT("Refinery")) ||
                            WC.Contains(TEXT("Packager")) || WC.Contains(TEXT("Blender")) ||
                            WC.Contains(TEXT("Miner")) || WC.Contains(TEXT("WaterPump")) ||
                            WC.Contains(TEXT("OilExtractor")) || WC.Contains(TEXT("Fracking")) ||
                            WC.Contains(TEXT("Generator")) || WC.Contains(TEXT("Storage")) ||
                            WC.Contains(TEXT("ResourceSink")) || WC.Contains(TEXT("Mam")) ||
                            WC.Contains(TEXT("Workshop")) || WC.Contains(TEXT("HubTerminal")) ||
                            WC.Contains(TEXT("TradingPost")) || WC.Contains(TEXT("CentralStorage")))
                            continue;
                        
                        float WDist = FVector::Dist2D(MyPos, WA->GetActorLocation());
                        if (WDist > BestWallDist) continue;
                        
                        if (!FwdDir.IsNearlyZero())
                        {
                            FVector ToWall = (WA->GetActorLocation() - MyPos).GetSafeNormal();
                            ToWall.Z = 0.0f;
                            if (FVector::DotProduct(FwdDir, ToWall) < -0.3f) continue;
                        }
                        
                        BestWallDist = WDist;
                        BlockingWall = WA;
                    }
                    
                    // Si aucun acteur mur trouve (quarter-pipes 100% instances),
                    // se rapprocher ou attaquer selon la distance
                    if (!BlockingWall && DistToTarget < 2000.0f && TargetBuilding && IsValid(TargetBuilding))
                    {
                        ConsecutiveStuckCount = 0;
                        DetourAttempts = 0;
                        StuckTimer = 0.0f;
                        bHitWallRecently = false;
                        if (DistToTarget <= AttackRange * 1.5f)
                        {
                            AttackTarget();
                        }
                        else
                        {
                            // Trop loin -> marcher en ligne droite vers la cible
                            FVector DirectDir = (TargetBuilding->GetActorLocation() - MyPos).GetSafeNormal();
                            DirectDir.Z = 0.0f;
                            DetourDirection = DirectDir;
                            DetourTimer = 2.0f;
                        }
                        return;
                    }
                    
                    bHitWallRecently = false;
                }
                
                if (BlockingWall)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[STUCK] %s: MUR BLOQUANT detecte! %s (dist=%.0f) -> ATTAQUE pour percer"),
                        *GetName(), *BlockingWall->GetName(),
                        FVector::Dist(MyPos, BlockingWall->GetActorLocation()));
                    
                    SetTarget(BlockingWall);
                    Waypoints.Empty();
                    CurrentWaypointIndex = 0;
                    ConsecutiveStuckCount = 0;
                    DetourAttempts = 0;
                }
                // === OPTI 3: Max 2 detours (pas 4) - abandonner plus vite ===
                else if (DetourAttempts < 2)
                {
                    FVector Detour = FindWalkableDetour();
                    if (!Detour.IsNearlyZero())
                    {
                        DetourDirection = Detour;
                        DetourTimer = 1.5f;
                        DetourAttempts++;
                        ConsecutiveStuckCount = 0;
                        UE_LOG(LogTemp, Verbose, TEXT("[STUCK] %s: detour %d/2 dir=%s cible=%s dist=%.0f wp=%d/%d"),
                            *GetName(), DetourAttempts, *Detour.ToString(),
                            TargetBuilding ? *TargetBuilding->GetName() : TEXT("null"),
                            DistToTarget, CurrentWaypointIndex, Waypoints.Num());
                    }
                    else
                    {
                        // Aucune direction praticable -> blacklister immediatement
                        if (TargetBuilding && IsValid(TargetBuilding))
                        {
                            MarkTargetUnreachable(TargetBuilding);
                        }
                        if (RetargetCooldown <= 0.0f)
                        {
                            FindNearestReachableTarget();
                            RetargetCooldown = 3.0f;
                        }
                        ConsecutiveStuckCount = 0;
                        DetourAttempts = 0;
                    }
                }
                else
                {
                    // 2 detours echoues -> blacklister et retarget (avec cooldown)
                    if (TargetBuilding && IsValid(TargetBuilding))
                    {
                        UE_LOG(LogTemp, Verbose, TEXT("[STUCK] %s: 2 detours echoues! blacklist %s (dist=%.0f)"),
                            *GetName(), *TargetBuilding->GetName(), DistToTarget);
                        MarkTargetUnreachable(TargetBuilding);
                    }
                    if (RetargetCooldown <= 0.0f)
                    {
                        FindNearestReachableTarget();
                        RetargetCooldown = 3.0f;
                    }
                    ConsecutiveStuckCount = 0;
                    DetourAttempts = 0;
                }
            }
        }
        else
        {
            ConsecutiveStuckCount = 0;
        }
        
        StuckTimer = 0.0f;
    }
}

FVector ATDEnemy::FindWalkableDetour()
{
    if (!TargetBuilding) return FVector::ZeroVector;
    
    FVector MyLoc = GetActorLocation();
    FVector ToTarget = (TargetBuilding->GetActorLocation() - MyLoc).GetSafeNormal();
    ToTarget.Z = 0.0f;
    
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    
    FVector BestDir = FVector::ZeroVector;
    float BestScore = -999.0f;
    
    // Offset aleatoire pour varier les detours (evite de toujours prendre la meme direction)
    float RandomOffset = FMath::FRandRange(0.0f, 22.5f);
    
    // Scanner 16 directions (tous les 22.5 degres) pour plus de precision
    for (int32 i = 0; i < 16; i++)
    {
        float Angle = i * 22.5f + RandomOffset;
        float Rad = FMath::DegreesToRadians(Angle);
        FVector TestDir = FVector(FMath::Cos(Rad), FMath::Sin(Rad), 0.0f);
        
        float DotToTarget = FVector::DotProduct(TestDir, ToTarget);
        
        // Autoriser meme les directions laterales ET arriere apres 2+ tentatives
        float MinDot = (DetourAttempts >= 2) ? -0.95f : -0.7f;
        if (DotToTarget < MinDot) continue;
        
        // Test 1: Raycast horizontal a hauteur de poitrine - mur/falaise devant?
        FVector RayStart = MyLoc + FVector(0, 0, 60.0f);
        FVector RayEnd = RayStart + TestDir * 300.0f;
        
        FHitResult Hit;
        bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, RayStart, RayEnd, ECC_WorldStatic, Params);
        
        if (bBlocked && Hit.Distance < 80.0f)
        {
            // Mur tres proche -> bloque, skip
            continue;
        }
        
        // Test 2: Sol praticable a 2m dans cette direction
        FVector GroundCheckStart = MyLoc + TestDir * 200.0f + FVector(0, 0, 200.0f);
        FVector GroundCheckEnd = GroundCheckStart - FVector(0, 0, 500.0f);
        FHitResult GroundHit;
        bool bHasGround = GetWorld()->LineTraceSingleByChannel(GroundHit, GroundCheckStart, GroundCheckEnd, ECC_WorldStatic, Params);
        
        if (!bHasGround) continue;
        
        // Test 3: Le sol n'est pas trop haut (falaise infranchissable)
        float GroundZ = GroundHit.ImpactPoint.Z;
        if (GroundZ - MyLoc.Z > 80.0f) continue;  // MaxStepHeight = 80
        
        // Test 4: Le sol n'est pas trop bas (gouffre)
        if (MyLoc.Z - GroundZ > 300.0f) continue;
        
        // Score: favoriser les directions vers la cible, mais accepter les laterales
        // + bonus si la direction est libre loin (pas bloque a 150m)
        float FreeDistance = bBlocked ? Hit.Distance : 300.0f;
        float Score = DotToTarget * 2.0f + FreeDistance / 300.0f;
        
        if (Score > BestScore)
        {
            BestScore = Score;
            BestDir = TestDir;
        }
    }
    
    return BestDir;
}

void ATDEnemy::AttackTarget()
{
    if (!TargetBuilding || !IsValid(TargetBuilding))
        return;

    UE_LOG(LogTemp, Verbose, TEXT("TDEnemy %s attaque %s!"), *GetName(), *TargetBuilding->GetName());

    // Jouer le son d'attaque
    if (OneShotAudioComponent && AttackSound)
    {
        OneShotAudioComponent->SetSound(AttackSound);
        OneShotAudioComponent->SetVolumeMultiplier(0.25f);
        OneShotAudioComponent->Play();
    }
    
    // Trouver le spawner (cache pour perf)
    if (!CachedSpawner.IsValid())
    {
        for (TActorIterator<ATDCreatureSpawner> SpIt(GetWorld()); SpIt; ++SpIt) { CachedSpawner = *SpIt; break; }
    }
    ATDCreatureSpawner* Spawner = Cast<ATDCreatureSpawner>(CachedSpawner.Get());
    if (Spawner)
    {
        Spawner->DamageBuilding(TargetBuilding, AttackDamage);
    }
}

float ATDEnemy::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, 
    AController* EventInstigator, AActor* DamageCauser)
{
    if (bIsDead) return 0.0f;
    
    float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    
    Health -= ActualDamage;
    UE_LOG(LogTemp, Warning, TEXT("TDEnemy %s prend %.0f degats (HP: %.0f/%.0f)"), 
        *GetName(), ActualDamage, Health, MaxHealth);
    
    if (Health <= 0.0f && !bIsDead)
    {
        Die();
    }
    
    return ActualDamage;
}

void ATDEnemy::TakeDamageCustom(float DamageAmount)
{
    if (bIsDead) return;
    
    Health -= DamageAmount;
    UE_LOG(LogTemp, Verbose, TEXT("TDEnemy %s prend %.0f degats (HP: %.0f/%.0f)"), 
        *GetName(), DamageAmount, Health, MaxHealth);
    
    if (Health <= 0.0f)
    {
        Die();
    }
}

void ATDEnemy::Die()
{
    if (bIsDead) return;
    bIsDead = true;
    
    UE_LOG(LogTemp, Warning, TEXT("TDEnemy %s est mort!"), *GetName());
    
    // Arreter le son de levitation
    if (LevitationAudioComponent && LevitationAudioComponent->IsPlaying())
    {
        LevitationAudioComponent->Stop();
    }
    
    // Desactiver les collisions
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    
    // Detruire apres un court delai
    SetLifeSpan(2.0f);
}

void ATDEnemy::EnableOutline()
{
    // Activer le custom depth pour l'outline rouge sur le mesh visible
    if (VisibleMesh)
    {
        VisibleMesh->SetRenderCustomDepth(true);
        VisibleMesh->SetCustomDepthStencilValue(1);  // Valeur pour outline rouge
    }
}

void ATDEnemy::SetTarget(AActor* NewTarget)
{
    if (NewTarget && NewTarget != TargetBuilding)
    {
        TargetBuilding = NewTarget;
        ConsecutiveStuckCount = 0;
        FailedAttackAttempts = 0;
        DetourAttempts = 0;
        DetourTimer = 0.0f;
        UE_LOG(LogTemp, Warning, TEXT("TDEnemy %s cible: %s"), *GetName(), *NewTarget->GetName());
        
        // Jouer le son de changement de cible
        if (OneShotAudioComponent && TargetChangeSound)
        {
            OneShotAudioComponent->SetSound(TargetChangeSound);
            OneShotAudioComponent->SetVolumeMultiplier(0.25f);
            OneShotAudioComponent->Play();
        }
    }
    else if (!NewTarget)
    {
        TargetBuilding = nullptr;
    }
}

void ATDEnemy::MarkTargetUnreachable(AActor* Target)
{
    if (!Target) return;
    BlacklistedTargets.Add(Target);
}

bool ATDEnemy::IsTargetBlacklisted(AActor* Target)
{
    if (!Target) return false;
    for (auto& Entry : BlacklistedTargets)
    {
        if (Entry.IsValid() && Entry.Get() == Target)
            return true;
    }
    return false;
}

void ATDEnemy::FindNearestReachableTarget()
{
    // Cache spawner (eviter TActorIterator chaque appel)
    if (!CachedSpawner.IsValid())
    {
        for (TActorIterator<ATDCreatureSpawner> SpIt(GetWorld()); SpIt; ++SpIt) { CachedSpawner = *SpIt; break; }
    }
    ATDCreatureSpawner* Spawner = Cast<ATDCreatureSpawner>(CachedSpawner.Get());
    
    FVector MyLocation = GetActorLocation();
    
    // Collecter les candidats avec score
    struct FCandidate { AActor* Actor; float Score; };
    TArray<FCandidate> Candidates;
    
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        AActor* Actor = *It;
        if (!Actor || !IsValid(Actor) || Actor == this) continue;
        
        FString ClassName = Actor->GetClass()->GetName();
        bool bIsTDBuilding = ClassName.StartsWith(TEXT("TD")) && !ClassName.StartsWith(TEXT("TDEnemy")) && !ClassName.StartsWith(TEXT("TDCreature")) && !ClassName.StartsWith(TEXT("TDWorld")) && !ClassName.StartsWith(TEXT("TDDropship"));
        if (!ClassName.StartsWith(TEXT("Build_")) && !bIsTDBuilding) continue;
        
        // Ignorer detruits
        if (Spawner && Spawner->IsBuildingDestroyed(Actor)) continue;
        
        // Ignorer blacklistes
        if (IsTargetBlacklisted(Actor)) continue;
        
        // Ignorer transport/infrastructure
        if (ClassName.Contains(TEXT("ConveyorBelt")) || ClassName.Contains(TEXT("ConveyorLift")) ||
            ClassName.Contains(TEXT("ConveyorAttachment")) ||
            ClassName.Contains(TEXT("PowerLine")) || ClassName.Contains(TEXT("Wire")) ||
            ClassName.Contains(TEXT("PowerPole")) || ClassName.Contains(TEXT("PowerTower")) ||
            ClassName.Contains(TEXT("RailroadTrack")) || ClassName.Contains(TEXT("PillarBase")) ||
            ClassName.Contains(TEXT("Beam")) || ClassName.Contains(TEXT("Stair")) ||
            ClassName.Contains(TEXT("Pipeline")) || ClassName.Contains(TEXT("Pipe")))
            continue;
        
        // Ignorer murs et fondations (cibles en CheckIfStuck, pas ici)
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
        
        FVector ActorLoc = Actor->GetActorLocation();
        float HeightDiff = ActorLoc.Z - MyLocation.Z;
        float Distance = FVector::Dist(MyLocation, ActorLoc);
        
        // Max 150m (les ennemis peuvent etre loin de la base apres retarget)
        if (Distance > 15000.0f) continue;
        
        // Ennemi au sol: max +1500u de hauteur (tours avec fondations empilees)
        if (HeightDiff > 1500.0f) continue;
        
        // Score base sur la distance
        float Score = Distance;
        
        // Bonus priorite: tourelles (x0.5), machines (x0.7), structures (x1.0)
        if (bIsTDBuilding)
            Score *= 0.5f;
        else if (ClassName.Contains(TEXT("Constructor")) || ClassName.Contains(TEXT("Smelter")) ||
                 ClassName.Contains(TEXT("Assembler")) || ClassName.Contains(TEXT("Manufacturer")) ||
                 ClassName.Contains(TEXT("Miner")) || ClassName.Contains(TEXT("Foundry")) ||
                 ClassName.Contains(TEXT("Refinery")) || ClassName.Contains(TEXT("Generator")) ||
                 ClassName.Contains(TEXT("Packager")) || ClassName.Contains(TEXT("Blender")) ||
                 ClassName.Contains(TEXT("Storage")))
            Score *= 0.7f;
        
        // Penalite de hauteur
        if (HeightDiff > 0.0f)
            Score += HeightDiff * 2.0f;
        
        Candidates.Add({Actor, Score});
    }
    
    // Trier par score (meilleur en premier)
    Candidates.Sort([](const FCandidate& A, const FCandidate& B) { return A.Score < B.Score; });
    
    // Prendre le meilleur candidat par score (le plus proche avec priorite)
    AActor* BestTarget = nullptr;
    
    if (Candidates.Num() > 0)
    {
        BestTarget = Candidates[0].Actor;
    }
    
    if (BestTarget)
    {
        SetTarget(BestTarget);
        
        // BFS DYNAMIQUE depuis position actuelle vers la cible (chemins walkable)
        Waypoints.Empty();
        CurrentWaypointIndex = 0;
        if (Spawner)
        {
            TArray<FVector> Path = Spawner->GetGroundPathFor(MyLocation, BestTarget->GetActorLocation());
            if (Path.Num() > 0)
            {
                Waypoints = Path;
                CurrentWaypointIndex = 0;
                UE_LOG(LogTemp, Warning, TEXT("TDEnemy %s: retarget %s, %d waypoints dynamiques"), *GetName(), *BestTarget->GetName(), Path.Num());
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("TDEnemy %s: retarget %s, AUCUN waypoint (BFS vide), ira en direct"), *GetName(), *BestTarget->GetName());
            }
        }
    }
    else
    {
        // Si des cibles blacklistees mais aucun candidat restant -> vider la blacklist
        if (BlacklistedTargets.Num() > 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("[NOCIBLE] %s: blacklist epuisee (%d entries) -> reset blacklist"),
                *GetName(), BlacklistedTargets.Num());
            BlacklistedTargets.Empty();
            // Ne pas recurser - le prochain ScanTimer reessayera avec blacklist vide
        }
        else
        {
            // Vraiment aucune cible -> ralentir les scans (prochain dans ~10s)
            ScanTimer = -7.0f;
            UE_LOG(LogTemp, Verbose, TEXT("[NOCIBLE] %s pos=%s candidats=0 blacklist=0 -> scan ralenti"),
                *GetName(), *MyLocation.ToString());
        }
    }
}

bool ATDEnemy::CanAttackTarget()
{
    if (!TargetBuilding) return false;
    
    FVector MyLoc = GetActorLocation();
    FVector TargetLoc = TargetBuilding->GetActorLocation();
    
    // Portee etendue pour murs/fondations (leur centre est souvent loin)
    FString TargetClass = TargetBuilding->GetClass()->GetName();
    bool bIsWallTarget = TargetClass.Contains(TEXT("Wall")) || TargetClass.Contains(TEXT("Foundation")) ||
                         TargetClass.Contains(TEXT("Ramp")) || TargetClass.Contains(TEXT("Fence")) ||
                         TargetClass.Contains(TEXT("Roof")) || TargetClass.Contains(TEXT("Pillar"));
    float EffectiveRange = bIsWallTarget ? FMath::Max(AttackRange, 600.0f) : AttackRange;
    
    // Verifier la difference de hauteur - si trop haute, on ne peut pas attaquer
    float HeightDiff = TargetLoc.Z - MyLoc.Z;
    if (HeightDiff > EffectiveRange)
    {
        return false;  // Cible trop haute
    }
    
    // Si la distance horizontale est OK, on peut attaquer!
    float HorizDist = FVector::Dist2D(MyLoc, TargetLoc);
    if (HorizDist <= EffectiveRange)
    {
        return true;  // A portee horizontale = peut attaquer
    }
    
    // Sinon verifier distance 3D
    float RealDistance = FVector::Dist(MyLoc, TargetLoc);
    if (RealDistance <= EffectiveRange * 1.2f)
    {
        return true;
    }
    
    return false;
}

bool ATDEnemy::IsUnderStructure()
{
    // Raycast vers le haut pour detecter si on est sous une structure
    FVector Start = GetActorLocation();
    FVector End = Start + FVector(0, 0, 500);  // 5m vers le haut
    
    FHitResult HitResult;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    
    bool bHit = GetWorld()->LineTraceSingleByChannel(
        HitResult,
        Start,
        End,
        ECC_Visibility,
        Params
    );
    
    return bHit;  // Si on touche quelque chose au-dessus, on est sous une structure
}

void ATDEnemy::NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
    Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);
    if (bIsDead) return;
    
    FString OtherName = Other ? Other->GetClass()->GetName() : TEXT("null");
    FString OtherActorName = Other ? Other->GetName() : TEXT("null");
    
    // === Detecter collision avec mur instance (AbstractInstanceManager) ou batiment mur ===
    // Normal quasi-horizontale (Z faible) = mur vertical
    bool bIsVerticalWall = FMath::Abs(HitNormal.Z) < 0.3f;
    bool bIsInstancedWall = OtherName.Contains(TEXT("AbstractInstanceManager")) || OtherName.Contains(TEXT("InstancedFoliage"));
    bool bIsBuildWall = OtherName.Contains(TEXT("Wall")) || OtherName.Contains(TEXT("Foundation"));
    
    if (bIsVerticalWall && (bIsInstancedWall || bIsBuildWall))
    {
        bHitWallRecently = true;
        LastWallHitPoint = HitLocation;
        LastWallHitNormal = HitNormal;
    }
    
    // Log throttle
    if (CollisionLogTimer > 0.0f) return;
    CollisionLogTimer = 5.0f;  // 1 log toutes les 5s max (perf)
    
    // Determiner le type de collision
    FString CollisionType;
    if (bIsBuildWall)
        CollisionType = TEXT("MUR/FONDATION");
    else if (bIsInstancedWall && bIsVerticalWall)
        CollisionType = TEXT("MUR_INSTANCE");
    else if (OtherName.Contains(TEXT("Build_")))
        CollisionType = TEXT("BATIMENT");
    else if (OtherName.Contains(TEXT("Landscape")) || OtherName.Contains(TEXT("Terrain")))
        CollisionType = TEXT("TERRAIN");
    else if (OtherName.Contains(TEXT("TDEnemy")) || OtherName.Contains(TEXT("TDEnemyRam")) || OtherName.Contains(TEXT("TDEnemyFlying")))
        CollisionType = TEXT("AUTRE_ENNEMI");
    else
        CollisionType = TEXT("AUTRE");
    
    UE_LOG(LogTemp, Verbose, TEXT("[COLLISION] %s: type=%s objet=%s(%s) normal=%s pos=%s cible=%s detour=%d/%d wp=%d/%d"),
        *GetName(), *CollisionType, *OtherName, *OtherActorName,
        *HitNormal.ToString(), *GetActorLocation().ToString(),
        TargetBuilding ? *TargetBuilding->GetName() : TEXT("null"),
        DetourAttempts, 4,
        CurrentWaypointIndex, Waypoints.Num());
}

void ATDEnemy::ApplySlow(float Duration, float SlowFactor)
{
    if (bIsDead) return;

    SpeedMultiplier = FMath::Clamp(SlowFactor, 0.1f, 1.0f);
    SlowTimer = Duration;
    MoveSpeed = OriginalMoveSpeed * SpeedMultiplier;

    if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
    {
        MovementComp->MaxWalkSpeed = MoveSpeed;
    }

    UE_LOG(LogTemp, Verbose, TEXT("TDEnemy %s: SLOWED x%.1f for %.1fs (speed: %.0f)"), *GetName(), SpeedMultiplier, Duration, MoveSpeed);
}
