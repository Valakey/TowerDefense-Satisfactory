#include "TDCreatureSpawner.h"
#include "TDWaveHUD.h"
#include "TDEnemy.h"
#include "TDEnemyFlying.h"
#include "TDEnemyRam.h"
#include "TDTurret.h"
#include "TDDropship.h"
#include "TDShieldGenerator.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "UObject/ConstructorHelpers.h"
#include "AIController.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "Components/PrimitiveComponent.h"
#include "FGDismantleInterface.h"
#include "Buildables/FGBuildable.h"
#include "AbstractInstanceInterface.h"
#include "TimerManager.h"
#include "Widgets/SWeakWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "FGTimeSubsystem.h"

ATDCreatureSpawner::ATDCreatureSpawner()
{
    PrimaryActorTick.bCanEverTick = true;
    UE_LOG(LogTemp, Warning, TEXT("TDCreatureSpawner: Constructeur appele"));

    // Charger le son de message de vague
    static ConstructorHelpers::FObjectFinder<USoundWave> WaveMessageSoundObj(TEXT("/MonPremierMod/Audios/VagueHUD/MessageVague.MessageVague"));
    if (WaveMessageSoundObj.Succeeded())
    {
        WaveMessageSound = WaveMessageSoundObj.Object;
        UE_LOG(LogTemp, Warning, TEXT("TDCreatureSpawner: WaveMessageSound charge!"));
    }
}

void ATDCreatureSpawner::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogTemp, Warning, TEXT("TDCreatureSpawner: BeginPlay - Systeme Tower Defense actif!"));
    UE_LOG(LogTemp, Warning, TEXT("TDCreatureSpawner: Config - SpawnRadius=%.0f, TimeBetweenWaves=%.0f"), SpawnRadius, TimeBetweenWaves);
    
    // Utiliser notre classe TDEnemy custom (pas les creatures Satisfactory qui se detruisent)
    CreatureClass = ATDEnemy::StaticClass();
    UE_LOG(LogTemp, Warning, TEXT("TDCreatureSpawner: Utilisation de TDEnemy custom"));
    
    // Creer le HUD apres un court delai pour s'assurer que le PlayerController est pret
    FTimerHandle HUDTimerHandle;
    GetWorld()->GetTimerManager().SetTimer(HUDTimerHandle, this, &ATDCreatureSpawner::CreateWaveHUD, 2.0f, false);
}

void ATDCreatureSpawner::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // === CYCLE JOUR/NUIT: detection automatique ===
    AFGTimeOfDaySubsystem* TimeSub = AFGTimeOfDaySubsystem::Get(GetWorld());
    bool bIsNight = TimeSub ? TimeSub->IsNight() : false;

    // Transition jour->nuit: lancer la vague 1 automatiquement
    if (bIsNight && !bWasNight)
    {
        if (!bWaveTriggeredThisNight && CurrentWave == 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("TDCreatureSpawner: NUIT DETECTEE -> Lancement vague 1!"));
            bWaveTriggeredThisNight = true;
            bDayCleanupDone = false;
            SpawnWave();
            WaveTimer = 0.0f;
        }
    }

    // Transition nuit->jour: reset les flags pour la prochaine nuit
    if (!bIsNight && bWasNight)
    {
        UE_LOG(LogTemp, Warning, TEXT("TDCreatureSpawner: JOUR DETECTE -> reset flags nuit"));
        bWaveTriggeredThisNight = false;
    }

    bWasNight = bIsNight;

    // Nettoyage des mobs restants en plein jour (milieu de journee)
    // Si c'est le jour ET il reste des mobs ET on a pas encore cleanup
    if (!bIsNight && !bDayCleanupDone && ActiveCreatures.Num() > 0 && bNightWavesComplete)
    {
        // Attendre un peu apres le lever du jour pour laisser les combats finir
        DayCheckTimer += DeltaTime;
        if (DayCheckTimer >= 120.0f)  // 2min apres le jour = cleanup
        {
            UE_LOG(LogTemp, Warning, TEXT("TDCreatureSpawner: CLEANUP JOUR -> Destruction de %d mobs restants!"), ActiveCreatures.Num());
            for (AActor* Creature : ActiveCreatures)
            {
                if (Creature && IsValid(Creature))
                {
                    Creature->Destroy();
                }
            }
            ActiveCreatures.Empty();
            bDayCleanupDone = true;
            DayCheckTimer = 0.0f;
        }
    }
    else
    {
        DayCheckTimer = 0.0f;
    }

    // Timer automatique pour vagues 2 et 3 (apres que vague 1 soit lancee)
    if (CurrentWave > 0 && !bNightWavesComplete)
    {
        WaveTimer += DeltaTime;
        if (WaveTimer >= TimeBetweenWaves)
        {
            WaveTimer = 0.0f;
            SpawnWave();
        }
    }
    
    // Reset des vagues quand toutes terminees et mobs tues
    if (bNightWavesComplete && ActiveCreatures.Num() == 0)
    {
        bNightWavesComplete = false;
        CurrentWave = 0;
        WaveTimer = 0.0f;
        UE_LOG(LogTemp, Warning, TEXT("TDCreatureSpawner: Vagues resetees, prochaine nuit = nouvelles vagues"));
    }

    // Nettoyage periodique des creatures mortes (toutes les 5 secondes au lieu de chaque frame)
    static float CleanupTimer = 0.0f;
    CleanupTimer += DeltaTime;
    if (CleanupTimer >= 5.0f)
    {
        CleanupTimer = 0.0f;
        CleanupDeadCreatures();
    }
    
    // Mettre a jour le compteur de mobs sur le HUD (toutes les 1 seconde)
    HUDUpdateTimer += DeltaTime;
    if (HUDUpdateTimer >= HUDUpdateInterval)
    {
        HUDUpdateTimer = 0.0f;
        UpdateHUDMobCounter();
    }
    
    // Mettre a jour les effets visuels des batiments attaques
    UpdateAttackedBuildingsVisuals(DeltaTime);
}

void ATDCreatureSpawner::SpawnWave()
{
    // Verifier si on a atteint le max de vagues
    if (CurrentWave >= MaxWaves)
    {
        bNightWavesComplete = true;
        UE_LOG(LogTemp, Warning, TEXT("TDCreatureSpawner: Toutes les vagues de la nuit terminees!"));
        return;
    }
    
    CurrentWave++;
    
    // === COLLECTER TOUS LES BATIMENTS DU JOUEUR ===
    TArray<AActor*> AllBuildings;
    CollectAllBuildings(AllBuildings);
    
    if (AllBuildings.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("TDCreatureSpawner: Aucun batiment trouve!"));
        return;
    }
    
    // === CREER DES ZONES DE SPAWN (groupes de batiments) ===
    TArray<FVector> SpawnZones;
    CreateSpawnZones(AllBuildings, SpawnZones);
    
    // Nombre de mobs proportionnel aux batiments et a la vague
    // Vague 1: 5 mobs/bat, Vague 2: 10 mobs/bat, Vague 3: 15 mobs/bat
    int32 MobMultiplier = CurrentWave * BaseMobsPerBuilding;  // 5, 10, 15
    int32 CreaturesPerZone = AllBuildings.Num() * MobMultiplier / FMath::Max(1, SpawnZones.Num());
    CreaturesPerZone = FMath::Max(CreaturesPerZone, MobMultiplier);  // Au minimum le multiplicateur
    
    int32 TotalCreatures = CreaturesPerZone * SpawnZones.Num();
    TotalCreatures = FMath::Min(TotalCreatures, MaxActiveCreatures - ActiveCreatures.Num());
    
    UE_LOG(LogTemp, Warning, TEXT("TDCreatureSpawner: === VAGUE %d/%d ==="), CurrentWave, MaxWaves);
    UE_LOG(LogTemp, Warning, TEXT("TDCreatureSpawner: %d batiments, %d zones, %d mobs/zone, %d total"), 
        AllBuildings.Num(), SpawnZones.Num(), CreaturesPerZone, TotalCreatures);

    if (!CreatureClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("TDCreatureSpawner: Aucune classe de creature!"));
        return;
    }

    // === CREER UN DROPSHIP PAR ZONE ===
    int32 TotalCreaturesPlanned = 0;
    
    for (const FVector& ZoneCenter : SpawnZones)
    {
        // Collecter les positions de spawn pour cette zone
        TArray<FVector> ZoneSpawnLocations;
        for (int32 i = 0; i < CreaturesPerZone; i++)
        {
            FVector SpawnLoc = GetSpawnLocationNearPoint(ZoneCenter);
            if (!SpawnLoc.IsZero())
            {
                ZoneSpawnLocations.Add(SpawnLoc);
            }
        }
        
        if (ZoneSpawnLocations.Num() > 0)
        {
            // Creer un dropship pour cette zone
            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            
            ATDDropship* Dropship = GetWorld()->SpawnActor<ATDDropship>(
                ATDDropship::StaticClass(),
                ZoneCenter + FVector(0, 0, 10000.0f),  // Spawn dans le ciel
                FRotator::ZeroRotator,
                SpawnParams
            );
            
            if (Dropship)
            {
                Dropship->Initialize(ZoneCenter, ZoneSpawnLocations, this);
                TotalCreaturesPlanned += ZoneSpawnLocations.Num();
                UE_LOG(LogTemp, Warning, TEXT("TDCreatureSpawner: Dropship cree pour zone, %d ennemis"), ZoneSpawnLocations.Num());
            }
        }
    }
    
    // === SPAWN BELIERS RETARDE (attendre que les dropships arrivent ~7s) ===
    // Stocker les positions de spawn des beliers pour les spawner apres le delai
    struct FPendingRam { FVector Location; FVector ZoneCenter; };
    TArray<FPendingRam> PendingRams;
    int32 TotalRams = 0;
    for (const FVector& ZoneCenter : SpawnZones)
    {
        int32 RamCount = FMath::RandRange(1, 3);
        for (int32 i = 0; i < RamCount; i++)
        {
            FVector RamSpawnLoc = GetSpawnLocationNearPoint(ZoneCenter);
            if (!RamSpawnLoc.IsZero())
            {
                PendingRams.Add({RamSpawnLoc, ZoneCenter});
                TotalRams++;
            }
        }
    }
    UE_LOG(LogTemp, Warning, TEXT("TDCreatureSpawner: %d beliers en attente (spawn dans 7s)"), TotalRams);
    
    // Timer: spawn les beliers 7 secondes apres (quand les dropships sont arrives)
    if (PendingRams.Num() > 0)
    {
        TWeakObjectPtr<ATDCreatureSpawner> WeakThis = this;
        TArray<FPendingRam> CapturedRams = PendingRams;
        FTimerHandle RamTimerHandle;
        GetWorld()->GetTimerManager().SetTimer(RamTimerHandle, [WeakThis, CapturedRams]()
        {
            if (!WeakThis.IsValid()) return;
            ATDCreatureSpawner* Spawner = WeakThis.Get();
            for (const FPendingRam& Ram : CapturedRams)
            {
                AActor* RamTarget = Spawner->FindNearestBuilding(Ram.Location);
                Spawner->SpawnRamCreatureAt(Ram.Location, RamTarget);
            }
            UE_LOG(LogTemp, Warning, TEXT("TDCreatureSpawner: %d beliers spawnes (apres dropship)"), CapturedRams.Num());
        }, 7.0f, false);
    }

    // === ANALYSER LA BASE (plan d'attaque global) ===
    AnalyzeBase();
    
    // === CONSTRUIRE LES GRILLES 3D PAR BASE (seulement si necessaire) ===
    bool bNeedRebuild = bGridDirty || (AllBuildings.Num() != LastBuildingCount) || !bTerrainMapReady;
    if (bNeedRebuild)
    {
        UE_LOG(LogTemp, Warning, TEXT("TDCreatureSpawner: Grid DIRTY -> rebuild (buildings: %d -> %d)"), LastBuildingCount, AllBuildings.Num());
        BuildAllBaseGrids(SpawnZones, AllBuildings);
        LastBuildingCount = AllBuildings.Num();
        bGridDirty = false;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("TDCreatureSpawner: Grid PROPRE -> skip rebuild (rien change)"));
    }

    CurrentWaveCreatureCount = TotalCreaturesPlanned + TotalRams;
    
    // === AFFICHER MESSAGE SUR ECRAN ===
    ShowWaveMessage(CurrentWave, CurrentWaveCreatureCount);
    
    UE_LOG(LogTemp, Warning, TEXT("TDCreatureSpawner: %d creatures via %d dropships + %d beliers"), CurrentWaveCreatureCount, SpawnZones.Num(), TotalRams);
}

void ATDCreatureSpawner::SpawnNextCreature()
{
    if (PendingSpawnLocations.Num() == 0)
    {
        // Plus de creatures a spawner, arreter le timer
        GetWorld()->GetTimerManager().ClearTimer(SpawnTimerHandle);
        UE_LOG(LogTemp, Warning, TEXT("TDCreatureSpawner: Spawn termine! %d creatures actives"), ActiveCreatures.Num());
        return;
    }
    
    // Prendre la prochaine position et spawner
    FVector SpawnLoc = PendingSpawnLocations[0];
    PendingSpawnLocations.RemoveAt(0);
    
    SpawnCreatureAt(SpawnLoc);
    
    // Le compteur est mis a jour automatiquement dans Tick via UpdateHUDMobCounter()
}

int32 ATDCreatureSpawner::CountBuildings()
{
    TArray<AActor*> Buildings;
    CollectAllBuildings(Buildings);
    return Buildings.Num();
}

void ATDCreatureSpawner::CollectAllBuildings(TArray<AActor*>& OutBuildings)
{
    OutBuildings.Empty();
    
    // PAS DE LIMITE DE DISTANCE - on detecte TOUTES les machines de production
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        AActor* Actor = *It;
        if (!Actor) continue;
        
        FString ClassName = Actor->GetClass()->GetName();
        
        // Uniquement les machines de production (pas convoyeurs, pylones, stockages)
        if (ClassName.Contains(TEXT("Constructor")) ||    // Constructeurs
            ClassName.Contains(TEXT("Assembler")) ||      // Assembleurs
            ClassName.Contains(TEXT("Manufacturer")) ||   // Fabricants
            ClassName.Contains(TEXT("Smelter")) ||        // Fonderies
            ClassName.Contains(TEXT("Foundry")) ||        // Fonderies
            ClassName.Contains(TEXT("Refinery")) ||       // Raffineries
            ClassName.Contains(TEXT("Generator")) ||      // Generateurs
            ClassName.Contains(TEXT("Packager")) ||       // Conditionneuses
            ClassName.Contains(TEXT("Blender")))          // Melangeurs
        {
            OutBuildings.Add(Actor);
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("CollectAllBuildings: %d machines de production detectees"), OutBuildings.Num());
}

void ATDCreatureSpawner::CreateSpawnZones(const TArray<AActor*>& Buildings, TArray<FVector>& OutZones)
{
    OutZones.Empty();
    
    if (Buildings.Num() == 0) return;
    
    // === CLUSTERING: Regrouper les batiments en "villes" ===
    const float ClusterRadius = 10000.0f;  // 100m - batiments a moins de 100m = meme ville
    const float SpawnMargin = 5000.0f;     // 50m - spawn a 50m du batiment le plus externe
    
    // Copier les batiments pour le clustering
    TArray<AActor*> Unassigned = Buildings;
    TArray<TArray<AActor*>> Clusters;
    
    while (Unassigned.Num() > 0)
    {
        // Commencer un nouveau cluster avec le premier batiment
        TArray<AActor*> CurrentCluster;
        CurrentCluster.Add(Unassigned[0]);
        Unassigned.RemoveAt(0);
        
        // Ajouter tous les batiments proches (expansion iterative)
        bool bFoundNew = true;
        while (bFoundNew)
        {
            bFoundNew = false;
            for (int32 i = Unassigned.Num() - 1; i >= 0; i--)
            {
                AActor* Building = Unassigned[i];
                
                // Verifier si ce batiment est proche d'un batiment du cluster
                for (AActor* ClusterBuilding : CurrentCluster)
                {
                    float Dist = FVector::Dist(Building->GetActorLocation(), ClusterBuilding->GetActorLocation());
                    if (Dist < ClusterRadius)
                    {
                        CurrentCluster.Add(Building);
                        Unassigned.RemoveAt(i);
                        bFoundNew = true;
                        break;
                    }
                }
            }
        }
        
        Clusters.Add(CurrentCluster);
    }
    
    UE_LOG(LogTemp, Warning, TEXT("=== CLUSTERING: %d villes detectees ==="), Clusters.Num());
    
    // === Pour chaque cluster/ville, creer des zones de spawn a la peripherie ===
    for (int32 ClusterIdx = 0; ClusterIdx < Clusters.Num(); ClusterIdx++)
    {
        TArray<AActor*>& Cluster = Clusters[ClusterIdx];
        
        if (Cluster.Num() == 0) continue;
        
        // Calculer le centre du cluster
        FVector ClusterCenter = FVector::ZeroVector;
        for (AActor* Building : Cluster)
        {
            ClusterCenter += Building->GetActorLocation();
        }
        ClusterCenter /= Cluster.Num();
        
        // Trouver le batiment le plus eloigne du centre (rayon du cluster)
        float ClusterRadiusActual = 0.0f;
        for (AActor* Building : Cluster)
        {
            float Dist = FVector::Dist2D(ClusterCenter, Building->GetActorLocation());
            if (Dist > ClusterRadiusActual)
            {
                ClusterRadiusActual = Dist;
            }
        }
        
        // Rayon de spawn = juste a l'exterieur du cluster
        float SpawnRadius = ClusterRadiusActual + SpawnMargin;
        SpawnRadius = FMath::Max(SpawnRadius, 3000.0f);  // Min 30m
        
        // Nombre de zones selon la taille du cluster (2 a 6 par ville)
        int32 NumZonesForCluster = FMath::Clamp(Cluster.Num() / 3, 2, 6);
        
        UE_LOG(LogTemp, Warning, TEXT("Ville %d: %d batiments, rayon=%.0fm, %d zones de spawn a %.0fm"), 
            ClusterIdx + 1, Cluster.Num(), ClusterRadiusActual / 100.0f, NumZonesForCluster, SpawnRadius / 100.0f);
        
        // Creer les zones en cercle autour de ce cluster
        for (int32 i = 0; i < NumZonesForCluster; i++)
        {
            float Angle = (360.0f / NumZonesForCluster) * i + (ClusterIdx * 45.0f);  // Decaler les angles entre clusters
            
            FVector SpawnZone;
            SpawnZone.X = ClusterCenter.X + FMath::Cos(FMath::DegreesToRadians(Angle)) * SpawnRadius;
            SpawnZone.Y = ClusterCenter.Y + FMath::Sin(FMath::DegreesToRadians(Angle)) * SpawnRadius;
            SpawnZone.Z = ClusterCenter.Z + 100.0f;
            
            OutZones.Add(SpawnZone);
            UE_LOG(LogTemp, Warning, TEXT("  Zone %d: %s (angle %.0f)"), OutZones.Num(), *SpawnZone.ToString(), Angle);
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("=== TOTAL: %d zones de spawn pour %d villes ==="), OutZones.Num(), Clusters.Num());
}

FVector ATDCreatureSpawner::GetSpawnLocation()
{
    // Fallback: spawn autour du spawner
    return GetSpawnLocationNearPoint(GetActorLocation());
}

FVector ATDCreatureSpawner::GetSpawnLocationNearPoint(const FVector& Center)
{
    // Les zones de spawn sont deja a l'exterieur de la base avec la bonne hauteur
    // On spawn directement pres de la zone avec un petit offset aleatoire
    const float SpreadRadius = 500.0f;  // 5m de dispersion autour de la zone
    
    float Angle = FMath::RandRange(0.0f, 360.0f);
    float Distance = FMath::RandRange(0.0f, SpreadRadius);
    
    FVector Offset;
    Offset.X = FMath::Cos(FMath::DegreesToRadians(Angle)) * Distance;
    Offset.Y = FMath::Sin(FMath::DegreesToRadians(Angle)) * Distance;
    Offset.Z = 0.0f;

    FVector SpawnLoc = Center + Offset;
    
    // Garder spawn en l'air (sortie du vaisseau) - les waypoints seront donnes apres atterrissage
    UE_LOG(LogTemp, Warning, TEXT("Spawn position: %s"), *SpawnLoc.ToString());
    return SpawnLoc;
}

bool ATDCreatureSpawner::IsLocationBlocked(const FVector& Location)
{
    // Verifier seulement les collisions immediates (pas de distance globale)
    // On veut juste eviter de spawner DANS un batiment
    const float CheckRadius = 200.0f;  // 2m de rayon
    
    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    
    bool bHasOverlap = GetWorld()->OverlapMultiByChannel(
        Overlaps,
        Location,
        FQuat::Identity,
        ECC_WorldStatic,
        FCollisionShape::MakeSphere(CheckRadius),
        QueryParams
    );
    
    if (bHasOverlap)
    {
        for (const FOverlapResult& Overlap : Overlaps)
        {
            if (Overlap.GetActor())
            {
                FString ClassName = Overlap.GetActor()->GetClass()->GetName();
                // Bloquer seulement si collision directe avec un batiment
                if (ClassName.Contains(TEXT("Build_")) ||
                    ClassName.Contains(TEXT("Foundation")) ||
                    ClassName.Contains(TEXT("Wall")))
                {
                    return true;
                }
            }
        }
    }
    
    return false;
}

bool ATDCreatureSpawner::ValidateGroundPath(const FVector& Start, const FVector& End)
{
    // Simule une marche au sol de Start vers End par pas de 200 unites
    // Verifie a chaque pas: y a-t-il du sol? Le sol n'est-il pas trop haut (falaise)?
    FVector Direction = (End - Start).GetSafeNormal2D();
    float TotalDist = FVector::Dist2D(Start, End);
    float StepSize = 200.0f;
    int32 MaxSteps = FMath::CeilToInt(TotalDist / StepSize);
    MaxSteps = FMath::Min(MaxSteps, 100);  // Securite
    
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    
    FVector CurrentPos = Start;
    float CurrentZ = Start.Z;
    
    for (int32 i = 0; i < MaxSteps; i++)
    {
        FVector NextPos2D = CurrentPos + Direction * StepSize;
        
        // Raycast vers le bas pour trouver le sol
        FVector GroundStart = FVector(NextPos2D.X, NextPos2D.Y, CurrentZ + 200.0f);
        FVector GroundEnd = FVector(NextPos2D.X, NextPos2D.Y, CurrentZ - 500.0f);
        FHitResult GroundHit;
        bool bHasGround = GetWorld()->LineTraceSingleByChannel(GroundHit, GroundStart, GroundEnd, ECC_WorldStatic, Params);
        
        if (!bHasGround)
        {
            // Pas de sol -> gouffre, chemin impossible
            return false;
        }
        
        float NewGroundZ = GroundHit.ImpactPoint.Z;
        float HeightChange = NewGroundZ - CurrentZ;
        
        // Si le sol monte de plus de 80 unites (MaxStepHeight) -> falaise
        if (HeightChange > 80.0f)
        {
            return false;
        }
        
        // Raycast horizontal pour verifier qu'il n'y a pas de mur infranchissable
        FVector WallStart = FVector(CurrentPos.X, CurrentPos.Y, CurrentZ + 60.0f);
        FVector WallEnd = FVector(NextPos2D.X, NextPos2D.Y, CurrentZ + 60.0f);
        FHitResult WallHit;
        bool bWallBlocked = GetWorld()->LineTraceSingleByChannel(WallHit, WallStart, WallEnd, ECC_WorldStatic, Params);
        
        if (bWallBlocked && WallHit.Distance < StepSize * 0.5f)
        {
            // Mur bloquant a mi-chemin -> verifier si c'est un batiment (franchissable via destruction) ou du terrain (infranchissable)
            AActor* WallActor = WallHit.GetActor();
            if (WallActor)
            {
                FString WallClass = WallActor->GetClass()->GetName();
                if (!WallClass.StartsWith(TEXT("Build_")))
                {
                    // C'est du terrain/falaise -> chemin bloque
                    return false;
                }
                // Si c'est un Build_*, l'ennemi pourra le casser -> OK
            }
            else
            {
                // Collision avec geometry statique (terrain) -> bloque
                return false;
            }
        }
        
        CurrentPos = FVector(NextPos2D.X, NextPos2D.Y, NewGroundZ);
        CurrentZ = NewGroundZ;
    }
    
    return true;
}

void ATDCreatureSpawner::SpawnCreatureAt(const FVector& Location)
{
    if (!CreatureClass)
    {
        UE_LOG(LogTemp, Error, TEXT("SpawnCreatureAt: CreatureClass est NULL!"));
        return;
    }
    
    // Trouver la cible d'abord pour determiner si on spawn un volant
    AActor* Target = FindNearestBuilding(Location);
    
    // Determiner si on spawn un volant (ratio per-base, SANS override)
    float EffectiveRatio = GetFlyingRatioFor(Location);
    
    // Verifier si un chemin sol pre-calcule existe pour cette base
    bool bHasGroundPath = false;
    if (Target)
    {
        TArray<FVector> GP = GetGroundPathFor(Location, Target->GetActorLocation());
        bHasGroundPath = (GP.Num() > 0);
    }
    
    // Si aucun chemin sol pre-calcule -> forcer volant
    bool bSpawnFlying = !bHasGroundPath || (FMath::FRand() < EffectiveRatio);
    
    if (bSpawnFlying && Target)
    {
        SpawnFlyingCreatureAt(Location, Target);
        return;
    }
    
    // === SPAWN ENNEMI AU SOL (code existant) ===
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    AActor* NewCreature = GetWorld()->SpawnActor<AActor>(CreatureClass, Location, FRotator::ZeroRotator, SpawnParams);
    
    if (!NewCreature)
    {
        UE_LOG(LogTemp, Error, TEXT("SpawnCreatureAt: SpawnActor a retourne NULL!"));
        return;
    }
    
    NewCreature->SetLifeSpan(0.0f);
    
    // Desactiver les degats de chute
    ACharacter* CreatureChar = Cast<ACharacter>(NewCreature);
    if (CreatureChar)
    {
        UCharacterMovementComponent* MovementComp = CreatureChar->GetCharacterMovement();
        if (MovementComp)
        {
            MovementComp->GravityScale = 1.0f;
            MovementComp->SetMovementMode(MOVE_Falling);
        }
        
        CreatureChar->SetCanBeDamaged(false);
        FTimerHandle InvulnTimer;
        TWeakObjectPtr<ACharacter> WeakCreature = CreatureChar;
        GetWorld()->GetTimerManager().SetTimer(InvulnTimer, [WeakCreature]()
        {
            if (WeakCreature.IsValid())
            {
                WeakCreature.Get()->SetCanBeDamaged(true);
            }
        }, 5.0f, false);
    }
    
    if (!IsValid(NewCreature)) return;
    
    ActiveCreatures.Add(NewCreature);
    
    // Custom Depth - seulement sur le mesh visible, pas TOUS les composants
    // (trop de custom depth renders = GPU crash D3D12 avec 250+ mobs)
    // Les ennemis activent deja leur propre outline via EnableOutline()
    
    // Configurer la cible et les waypoints
    if (Target)
    {
        ATDEnemy* Enemy = Cast<ATDEnemy>(NewCreature);
        if (Enemy)
        {
            Enemy->SetTarget(Target);
            // Waypoints donnes APRES atterrissage (dans TDEnemy::Tick)
            // L'ennemi spawn en l'air (vaisseau), le chemin sera calcule une fois au sol
        }
    }
}

void ATDCreatureSpawner::SpawnFlyingCreatureAt(const FVector& Location, AActor* Target)
{
    // Spawn en hauteur au-dessus de la position au sol
    FVector FlyingSpawnLoc = Location + FVector(0, 0, 800.0f);
    
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    ATDEnemyFlying* FlyingEnemy = GetWorld()->SpawnActor<ATDEnemyFlying>(
        ATDEnemyFlying::StaticClass(), FlyingSpawnLoc, FRotator::ZeroRotator, SpawnParams);
    
    if (!FlyingEnemy)
    {
        UE_LOG(LogTemp, Error, TEXT("SpawnFlyingCreatureAt: Echec spawn volant!"));
        return;
    }
    
    FlyingEnemy->SetLifeSpan(0.0f);
    
    ActiveCreatures.Add(FlyingEnemy);
    
    if (Target)
    {
        FlyingEnemy->SetTarget(Target);
        
        // Pathfinding 3D via grille voxel per-base
        TArray<FVector> Path3D = GetFlyPathFor(FlyingSpawnLoc, Target->GetActorLocation());
        if (Path3D.Num() > 0)
        {
            FlyingEnemy->Waypoints = Path3D;
            FlyingEnemy->CurrentWaypointIndex = 0;
        }
        
        UE_LOG(LogTemp, Warning, TEXT("TDEnemyFlying spawn a %s, cible: %s, %d waypoints 3D"), 
            *FlyingSpawnLoc.ToString(), *Target->GetActorLocation().ToString(), Path3D.Num());
    }
}

void ATDCreatureSpawner::SpawnRamCreatureAt(const FVector& Location, AActor* Target)
{
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ATDEnemyRam* RamEnemy = GetWorld()->SpawnActor<ATDEnemyRam>(
        ATDEnemyRam::StaticClass(), Location, FRotator::ZeroRotator, SpawnParams);

    if (!RamEnemy)
    {
        UE_LOG(LogTemp, Error, TEXT("SpawnRamCreatureAt: Echec spawn belier!"));
        return;
    }

    RamEnemy->SetLifeSpan(0.0f);
    ActiveCreatures.Add(RamEnemy);

    if (Target)
    {
        RamEnemy->SetTarget(Target);
        // Waypoints donnes APRES atterrissage (dans TDEnemyRam::Tick)
        UE_LOG(LogTemp, Warning, TEXT("TDEnemyRam spawn a %s, cible: %s"),
            *Location.ToString(), *Target->GetActorLocation().ToString());
    }
}

void ATDCreatureSpawner::ShowWaveMessage(int32 WaveNum, int32 CreatureCount)
{
    // Jouer le son de message de vague (2D - non spatialise)
    if (WaveMessageSound)
    {
        UGameplayStatics::PlaySound2D(GetWorld(), WaveMessageSound, 0.27f);
        UE_LOG(LogTemp, Warning, TEXT("Son de vague joue!"));
    }

    // Utiliser le widget Slate pour l'annonce de vague
    if (WaveHUD.IsValid())
    {
        WaveHUD->ShowWaveAnnouncement(WaveNum, CreatureCount);
        UE_LOG(LogTemp, Warning, TEXT("HUD Slate: Vague %d annoncee avec %d creatures"), WaveNum, CreatureCount);
    }
    else
    {
        // Fallback: notification Satisfactory
        FString Msg = FString::Printf(TEXT("=== VAGUE %d === %d CREATURES!"), WaveNum, CreatureCount);
        DisplayScreenMessage(Msg, FColor::Red, 5.0f);
        UE_LOG(LogTemp, Warning, TEXT("Fallback message vague: %s"), *Msg);
    }
}

void ATDCreatureSpawner::DisplayScreenMessage(const FString& Message, FColor Color, float Duration)
{
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!PC) return;
    
    // Appeler ShowTextNotification via reflection (evite les includes problematiques)
    AHUD* HUD = PC->GetHUD();
    if (HUD)
    {
        // Chercher la fonction ShowTextNotification sur le GameUI
        UFunction* ShowTextFunc = nullptr;
        
        // Obtenir GameUI via GetGameUI()
        UFunction* GetGameUIFunc = HUD->FindFunction(FName("GetGameUI"));
        if (GetGameUIFunc)
        {
            UObject* GameUI = nullptr;
            HUD->ProcessEvent(GetGameUIFunc, &GameUI);
            
            if (GameUI)
            {
                ShowTextFunc = GameUI->FindFunction(FName("ShowTextNotification"));
                if (ShowTextFunc)
                {
                    struct { FText Text; } Params;
                    Params.Text = FText::FromString(Message);
                    GameUI->ProcessEvent(ShowTextFunc, &Params);
                    UE_LOG(LogTemp, Warning, TEXT("ShowTextNotification: %s"), *Message);
                    return;
                }
            }
        }
    }
    
    // Fallback: ClientMessage (apparait dans le chat du jeu)
    PC->ClientMessage(Message, NAME_None);
    UE_LOG(LogTemp, Warning, TEXT("ClientMessage: %s"), *Message);
}

void ATDCreatureSpawner::CleanupDeadCreatures()
{
    int32 CountBefore = ActiveCreatures.Num();
    
    ActiveCreatures.RemoveAll([](AActor* Creature)
    {
        if (!IsValid(Creature))
        {
            UE_LOG(LogTemp, Warning, TEXT("Creature morte/invalide retiree"));
            return true;
        }
        return false;
    });
    
    int32 CountAfter = ActiveCreatures.Num();
    if (CountBefore != CountAfter)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cleanup: %d creatures retirees, %d restantes"), 
            CountBefore - CountAfter, CountAfter);
    }
}

void ATDCreatureSpawner::AnalyzeBase()
{
    AttackPaths.Empty();
    AnalyzedZoneCenters.Empty();
    AnalyzedZoneRadii.Empty();
    
    UE_LOG(LogTemp, Warning, TEXT("=== BASE ANALYZER: Debut analyse ==="));
    
    // === ETAPE 1: Clustering des batiments en zones (meme algo que CreateSpawnZones) ===
    const float ClusterRadius = 10000.0f;  // 100m
    
    TArray<AActor*> AllActors;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        AActor* Actor = *It;
        if (!Actor || !IsValid(Actor)) continue;
        FString CN = Actor->GetClass()->GetName();
        bool bIsTDBuilding = CN.StartsWith(TEXT("TD")) && !CN.StartsWith(TEXT("TDEnemy")) && !CN.StartsWith(TEXT("TDCreature")) && !CN.StartsWith(TEXT("TDWorld")) && !CN.StartsWith(TEXT("TDDropship"));
        if (CN.StartsWith(TEXT("Build_")) || bIsTDBuilding)
        {
            AllActors.Add(Actor);
        }
    }
    
    // Clustering
    TArray<AActor*> Unassigned = AllActors;
    TArray<TArray<AActor*>> Clusters;
    
    while (Unassigned.Num() > 0)
    {
        TArray<AActor*> Cluster;
        Cluster.Add(Unassigned[0]);
        Unassigned.RemoveAt(0);
        
        bool bFoundNew = true;
        while (bFoundNew)
        {
            bFoundNew = false;
            for (int32 i = Unassigned.Num() - 1; i >= 0; i--)
            {
                for (AActor* CB : Cluster)
                {
                    if (FVector::Dist(Unassigned[i]->GetActorLocation(), CB->GetActorLocation()) < ClusterRadius)
                    {
                        Cluster.Add(Unassigned[i]);
                        Unassigned.RemoveAt(i);
                        bFoundNew = true;
                        break;
                    }
                }
            }
        }
        Clusters.Add(Cluster);
    }
    
    UE_LOG(LogTemp, Warning, TEXT("ANALYZER: %d bases detectees"), Clusters.Num());
    
    // === ETAPE 2: Pour chaque zone, calculer centre + rayon + analyser chaque cible ===
    for (int32 ZoneIdx = 0; ZoneIdx < Clusters.Num(); ZoneIdx++)
    {
        TArray<AActor*>& Cluster = Clusters[ZoneIdx];
        if (Cluster.Num() == 0) continue;
        
        // Centre et rayon de la zone
        FVector ZCenter = FVector::ZeroVector;
        for (AActor* A : Cluster) ZCenter += A->GetActorLocation();
        ZCenter /= Cluster.Num();
        
        float ZRadius = 0.0f;
        for (AActor* A : Cluster)
        {
            float D = FVector::Dist2D(ZCenter, A->GetActorLocation());
            if (D > ZRadius) ZRadius = D;
        }
        ZRadius += 500.0f;  // Marge
        
        AnalyzedZoneCenters.Add(ZCenter);
        AnalyzedZoneRadii.Add(ZRadius);
        
        UE_LOG(LogTemp, Warning, TEXT("ANALYZER: Zone %d - Centre (%.0f,%.0f,%.0f) Rayon %.0fm (%d batiments)"),
            ZoneIdx, ZCenter.X, ZCenter.Y, ZCenter.Z, ZRadius / 100.0f, Cluster.Num());
        
        // Analyser chaque batiment de cette zone
        for (AActor* Actor : Cluster)
        {
            FString ClassName = Actor->GetClass()->GetName();
            int32 Prio = 0;
            
            bool bIsTDBld = ClassName.StartsWith(TEXT("TD")) && !ClassName.StartsWith(TEXT("TDEnemy")) && !ClassName.StartsWith(TEXT("TDCreature")) && !ClassName.StartsWith(TEXT("TDWorld")) && !ClassName.StartsWith(TEXT("TDDropship"));
            if (bIsTDBld)
                Prio = 1;  // Tourelles/defenses mod
            else if (ClassName.Contains(TEXT("Constructor")) || ClassName.Contains(TEXT("Smelter")) ||
                     ClassName.Contains(TEXT("Assembler")) || ClassName.Contains(TEXT("Manufacturer")) ||
                     ClassName.Contains(TEXT("Miner")) || ClassName.Contains(TEXT("Foundry")) ||
                     ClassName.Contains(TEXT("Refinery")) || ClassName.Contains(TEXT("Generator")) ||
                     ClassName.Contains(TEXT("Packager")) || ClassName.Contains(TEXT("Blender")))
                Prio = 2;  // Machines de production
            else
                Prio = 3;  // Structures (murs, fondations, etc.)
            
            FAttackPath Path;
            Path.Target = Actor;
            Path.Priority = Prio;
            Path.TargetLocation = Actor->GetActorLocation();
            Path.ZoneIndex = ZoneIdx;
            Path.ZoneCenter = ZCenter;
            Path.ZoneRadius = ZRadius;
            Path.bIsEnclosed = false;
            Path.WallToBreak = nullptr;
            
            // Pour turrets et machines: scan radial pour detecter encerclement
            if (Prio <= 2)
            {
                FVector TargetLoc = Actor->GetActorLocation() + FVector(0, 0, 50.0f);
                int32 BlockedDirs = 0;
                int32 OpenDirs = 0;
                int32 TotalDirs = 16;
                AActor* ClosestWall = nullptr;
                float ClosestWallDist = MAX_FLT;
                FVector BestOpenDir = FVector::ZeroVector;
                
                FCollisionQueryParams Params;
                Params.AddIgnoredActor(Actor);
                
                for (int32 i = 0; i < TotalDirs; i++)
                {
                    float Angle = i * (360.0f / TotalDirs);
                    FVector Dir = FVector(FMath::Cos(FMath::DegreesToRadians(Angle)), FMath::Sin(FMath::DegreesToRadians(Angle)), 0.0f);
                    FVector End = TargetLoc + Dir * 2000.0f;
                    
                    FHitResult HitResult;
                    bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, TargetLoc, End, ECC_WorldStatic, Params);
                    
                    if (bHit && HitResult.GetActor())
                    {
                        FString HitClass = HitResult.GetActor()->GetClass()->GetName();
                        if (HitClass.Contains(TEXT("Wall")))
                        {
                            BlockedDirs++;
                            float Dist = HitResult.Distance;
                            if (Dist < ClosestWallDist)
                            {
                                ClosestWallDist = Dist;
                                ClosestWall = HitResult.GetActor();
                            }
                        }
                        else
                        {
                            OpenDirs++;
                            if (BestOpenDir.IsZero()) BestOpenDir = Dir;
                        }
                    }
                    else
                    {
                        OpenDirs++;
                        if (BestOpenDir.IsZero()) BestOpenDir = Dir;
                    }
                }
                
                float BlockRatio = (TotalDirs > 0) ? (float)BlockedDirs / (float)TotalDirs : 0.0f;
                if (BlockRatio > 0.6f && ClosestWall)
                {
                    Path.bIsEnclosed = true;
                    Path.WallToBreak = ClosestWall;
                }
                else
                {
                    Path.BestApproachDir = BestOpenDir;
                }
            }
            
            AttackPaths.Add(Path);
        }
    }
    
    // Trier par priorite
    AttackPaths.Sort([](const FAttackPath& A, const FAttackPath& B) { return A.Priority < B.Priority; });
    
    UE_LOG(LogTemp, Warning, TEXT("=== BASE ANALYZER: %d cibles dans %d zones ==="), AttackPaths.Num(), Clusters.Num());
}

FAttackPath ATDCreatureSpawner::GetBestAttackPath(const FVector& FromLocation, bool bCanFly)
{
    // Trouver la zone la plus proche de l'ennemi
    int32 NearestZone = -1;
    float NearestZoneDist = MAX_FLT;
    
    for (int32 i = 0; i < AnalyzedZoneCenters.Num(); i++)
    {
        float Dist = FVector::Dist2D(FromLocation, AnalyzedZoneCenters[i]);
        if (Dist < NearestZoneDist)
        {
            NearestZoneDist = Dist;
            NearestZone = i;
        }
    }
    
    // Chercher la meilleure cible dans CETTE zone uniquement
    FAttackPath BestPath;
    float BestScore = -MAX_FLT;
    
    for (const FAttackPath& Path : AttackPaths)
    {
        if (!Path.Target || !IsValid(Path.Target)) continue;
        
        // Ignorer les batiments deja detruits (structures marquees)
        if (DestroyedBuildings.Contains(Path.Target)) continue;
        
        // Filtrer par zone
        if (Path.ZoneIndex != NearestZone) continue;
        
        float Distance = FVector::Dist(FromLocation, Path.TargetLocation);
        float HeightDiff = Path.TargetLocation.Z - FromLocation.Z;
        
        if (!bCanFly && HeightDiff > 400.0f) continue;
        
        // Score = priorite (inversee) - distance + bonus ouvert
        float Score = (4 - Path.Priority) * 10000.0f - Distance;
        if (!Path.bIsEnclosed) Score += 5000.0f;
        
        if (Score > BestScore)
        {
            BestScore = Score;
            BestPath = Path;
        }
    }
    
    return BestPath;
}

AActor* ATDCreatureSpawner::FindNearestBuilding(const FVector& FromLocation)
{
    // Priorite: turrets > machines > structures
    // NOUVEAU: valider accessibilite terrain (chemin au sol existe)
    AActor* BestTurret = nullptr;    float BestTurretDist = MAX_FLT;
    AActor* BestMachine = nullptr;   float BestMachineDist = MAX_FLT;
    AActor* BestStructure = nullptr; float BestStructureDist = MAX_FLT;
    float MaxHeightDiff = 400.0f;
    
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        AActor* Actor = *It;
        if (!Actor) continue;
        
        FVector ActorLoc = Actor->GetActorLocation();
        float HeightDiff = ActorLoc.Z - FromLocation.Z;
        if (HeightDiff > MaxHeightDiff) continue;
        
        float Distance = FVector::Dist(FromLocation, ActorLoc);
        FString ClassName = Actor->GetClass()->GetName();
        
        // Filtrer: seuls Build_* et TD* sont des batiments valides
        bool bIsTDBuilding = ClassName.StartsWith(TEXT("TD")) && !ClassName.StartsWith(TEXT("TDEnemy")) && !ClassName.StartsWith(TEXT("TDCreature")) && !ClassName.StartsWith(TEXT("TDWorld")) && !ClassName.StartsWith(TEXT("TDDropship"));
        if (!ClassName.StartsWith(TEXT("Build_")) && !bIsTDBuilding) continue;
        
        // Ignorer poutres et escaliers (non-destructibles)
        if (ClassName.Contains(TEXT("Beam")) || ClassName.Contains(TEXT("Stair")))
            continue;
        
        // Ignorer murs et fondations (ennemis se bloquent dessus)
        if (ClassName.Contains(TEXT("Wall")) || ClassName.Contains(TEXT("Foundation")) ||
            ClassName.Contains(TEXT("Ramp")) || ClassName.Contains(TEXT("Fence")) ||
            ClassName.Contains(TEXT("Roof")) || ClassName.Contains(TEXT("Frame")) ||
            ClassName.Contains(TEXT("Pillar")) || ClassName.Contains(TEXT("Quarter")))
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
        
        // Prio 1: Tourelles/defenses (TD*)
        if (bIsTDBuilding)
        {
            if (Distance < BestTurretDist) { BestTurretDist = Distance; BestTurret = Actor; }
        }
        // Prio 2: Machines de production
        else if (ClassName.Contains(TEXT("Constructor")) ||
                 ClassName.Contains(TEXT("Smelter")) ||
                 ClassName.Contains(TEXT("Assembler")) ||
                 ClassName.Contains(TEXT("Manufacturer")) ||
                 ClassName.Contains(TEXT("Miner")) ||
                 ClassName.Contains(TEXT("Foundry")) ||
                 ClassName.Contains(TEXT("Refinery")) ||
                 ClassName.Contains(TEXT("Generator")) ||
                 ClassName.Contains(TEXT("Packager")) ||
                 ClassName.Contains(TEXT("Blender")))
        {
            if (Distance < BestMachineDist) { BestMachineDist = Distance; BestMachine = Actor; }
        }
        // Prio 3: Autres structures Build_*
        else
        {
            if (Distance < BestStructureDist) { BestStructureDist = Distance; BestStructure = Actor; }
        }
    }
    
    if (BestTurret) return BestTurret;
    if (BestMachine) return BestMachine;
    return BestStructure;
}

void ATDCreatureSpawner::CreateWaveHUD()
{
    if (WaveHUD.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("CreateWaveHUD: HUD deja cree"));
        return;
    }
    
    // Creer le widget Slate
    WaveHUD = SNew(STDWaveHUD);
    
    if (WaveHUD.IsValid() && GEngine && GEngine->GameViewport)
    {
        // Ajouter au viewport via Slate
        GEngine->GameViewport->AddViewportWidgetContent(
            SNew(SWeakWidget).PossiblyNullContent(WaveHUD.ToSharedRef()),
            100  // Z-order eleve
        );
        UE_LOG(LogTemp, Warning, TEXT("HUD Tower Defense Slate cree et ajoute au viewport!"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Echec creation du HUD Slate (GameViewport: %s)"), 
            GEngine && GEngine->GameViewport ? TEXT("OK") : TEXT("NULL"));
    }
}

void ATDCreatureSpawner::UpdateHUDMobCounter()
{
    if (WaveHUD.IsValid())
    {
        WaveHUD->UpdateMobCounter(ActiveCreatures.Num());
    }
}

// === SYSTEME DE PV DES BATIMENTS ===

float ATDCreatureSpawner::GetBuildingMaxHealth(AActor* Building)
{
    if (!Building) return 100.0f;
    
    FString ClassName = Building->GetClass()->GetName();
    
    // PV selon le type de batiment (plus complexe = plus de PV)
    
    // Tier 1 - Batiments simples (100-200 PV)
    if (ClassName.Contains(TEXT("SmelterMk1")) || 
        ClassName.Contains(TEXT("ConstructorMk1")))
    {
        return 150.0f;
    }
    
    // Tier 2 - Batiments moyens (200-400 PV)
    if (ClassName.Contains(TEXT("AssemblerMk1")) ||
        ClassName.Contains(TEXT("Foundry")))
    {
        return 300.0f;
    }
    
    // Tier 3 - Batiments complexes (400-600 PV)
    if (ClassName.Contains(TEXT("ManufacturerMk1")) ||
        ClassName.Contains(TEXT("Refinery")) ||
        ClassName.Contains(TEXT("Blender")))
    {
        return 500.0f;
    }
    
    // Tier 4 - Batiments tres complexes (600-1000 PV)
    if (ClassName.Contains(TEXT("NuclearPower")) ||
        ClassName.Contains(TEXT("Particle")) ||
        ClassName.Contains(TEXT("QuantumEncoder")))
    {
        return 1000.0f;
    }
    
    // Generateurs (300-800 PV)
    if (ClassName.Contains(TEXT("Generator")))
    {
        if (ClassName.Contains(TEXT("Nuclear")))
            return 800.0f;
        if (ClassName.Contains(TEXT("Fuel")) || ClassName.Contains(TEXT("Coal")))
            return 400.0f;
        return 200.0f;  // Biomass
    }
    
    // Miners (200-500 PV)
    if (ClassName.Contains(TEXT("Miner")))
    {
        if (ClassName.Contains(TEXT("Mk3")))
            return 500.0f;
        if (ClassName.Contains(TEXT("Mk2")))
            return 350.0f;
        return 200.0f;  // Mk1
    }
    
    // Stockage (100-300 PV)
    if (ClassName.Contains(TEXT("Storage")) || ClassName.Contains(TEXT("Container")))
    {
        return 200.0f;
    }
    
    // HUB - tres important! (2000 PV)
    if (ClassName.Contains(TEXT("HUB")) || ClassName.Contains(TEXT("TradingPost")))
    {
        return 2000.0f;
    }
    
    // Space Elevator (5000 PV)
    if (ClassName.Contains(TEXT("SpaceElevator")))
    {
        return 5000.0f;
    }
    
    // Default pour autres batiments
    if (ClassName.Contains(TEXT("Build_")))
    {
        return 150.0f;
    }
    
    return 100.0f;
}

void ATDCreatureSpawner::DamageBuilding(AActor* Building, float Damage)
{
    if (!Building || !IsValid(Building) || Building->IsPendingKillPending()) return;
    
    // === SECURITE ULTIME: Cast AFGBuildable ===
    // Tous les vrais batiments du joueur heritent de AFGBuildable.
    // Terrain, rochers, falaises, paysage -> AUCUN n'est AFGBuildable -> rejete ici
    AFGBuildable* AsBuildable = Cast<AFGBuildable>(Building);
    FString ActorClassName = Building->GetClass()->GetName();
    bool bIsTDBuilding = ActorClassName.StartsWith(TEXT("TD")) && !ActorClassName.StartsWith(TEXT("TDEnemy")) && !ActorClassName.StartsWith(TEXT("TDCreature")) && !ActorClassName.StartsWith(TEXT("TDWorld")) && !ActorClassName.StartsWith(TEXT("TDDropship"));
    
    if (!AsBuildable && !bIsTDBuilding)
    {
        UE_LOG(LogTemp, Error, TEXT("DamageBuilding: BLOQUE acteur non-AFGBuildable: %s [%s]"), *Building->GetName(), *ActorClassName);
        return;
    }
    
    // Ignorer les batiments deja detruits (caches sous la map)
    if (DestroyedBuildings.Contains(Building)) return;
    
    // Verifier si le batiment est protege par un Shield Generator
    for (TActorIterator<ATDShieldGenerator> It(GetWorld()); It; ++It)
    {
        ATDShieldGenerator* Shield = *It;
        if (Shield && IsValid(Shield) && Shield->IsActorInRange(Building))
        {
            if (Shield->TryProtectBuilding(Building))
            {
                return; // Degat absorbe par le shield
            }
        }
    }
    
    // Initialiser les PV si pas encore fait
    if (!BuildingHealth.Contains(Building))
    {
        float MaxHP = GetBuildingMaxHealth(Building);
        BuildingHealth.Add(Building, MaxHP);
        BuildingMaxHealth.Add(Building, MaxHP);
        UE_LOG(LogTemp, Warning, TEXT("Batiment %s initialise avec %.0f PV"), *Building->GetName(), MaxHP);
    }
    
    // Declencher l'effet visuel pulse rouge
    MarkBuildingAttacked(Building);
    
    // Reduire les PV
    float CurrentHP = BuildingHealth[Building];
    CurrentHP -= Damage;
    BuildingHealth[Building] = CurrentHP;
    
    float MaxHP = BuildingMaxHealth[Building];
    float Percent = (CurrentHP / MaxHP) * 100.0f;
    
    UE_LOG(LogTemp, Warning, TEXT("Batiment %s: %.0f/%.0f PV (%.0f%%)"), 
        *Building->GetName(), FMath::Max(0.0f, CurrentHP), MaxHP, Percent);
    
    // Batiment detruit!
    if (CurrentHP <= 0.0f)
    {
        UE_LOG(LogTemp, Error, TEXT("=== BATIMENT DETRUIT: %s ==="), *Building->GetName());
        
        DisplayScreenMessage(FString::Printf(TEXT("BATIMENT DETRUIT: %s"), *Building->GetName()), FColor::Red, 5.0f);
        
        BuildingHealth.Remove(Building);
        BuildingMaxHealth.Remove(Building);
        DestroyedBuildings.Add(Building);
        
        // LOG DETAILLE avant destruction
        FString ClassName = Building->GetClass()->GetName();
        FString FullPath = Building->GetClass()->GetPathName();
        FVector Loc = Building->GetActorLocation();
        AFGBuildable* Buildable = Cast<AFGBuildable>(Building);
        bool bHasDismantleIF = Building->GetClass()->ImplementsInterface(UFGDismantleInterface::StaticClass());
        
        UE_LOG(LogTemp, Error, TEXT("=== PRE-DESTRUCTION ==="));
        UE_LOG(LogTemp, Error, TEXT("  Nom: %s"), *Building->GetName());
        UE_LOG(LogTemp, Error, TEXT("  Classe: %s"), *ClassName);
        UE_LOG(LogTemp, Error, TEXT("  Path: %s"), *FullPath);
        UE_LOG(LogTemp, Error, TEXT("  Position: X=%.0f Y=%.0f Z=%.0f"), Loc.X, Loc.Y, Loc.Z);
        UE_LOG(LogTemp, Error, TEXT("  Est AFGBuildable: %s"), Buildable ? TEXT("OUI") : TEXT("NON"));
        UE_LOG(LogTemp, Error, TEXT("  A IFGDismantleInterface: %s"), bHasDismantleIF ? TEXT("OUI") : TEXT("NON"));
        UE_LOG(LogTemp, Error, TEXT("  IsPendingKill: %s"), Building->IsPendingKillPending() ? TEXT("OUI") : TEXT("NON"));
        
        // === Mettre a jour la grille terrain (le batiment va disparaitre) ===
        RefreshTerrainAroundBuilding(Loc);
        
        // Retirer des visuels d'attaque AVANT destruction
        AttackedBuildingTimers.Remove(Building);
        
        // Restaurer le scale a 1.0 pour nettoyer les end-of-frame updates pendants
        Building->SetActorScale3D(FVector(1.0f));
        
        // Desactiver collision immediatement pour que les ennemis ne le ciblent plus
        Building->SetActorEnableCollision(false);
        
        if (bHasDismantleIF)
        {
            UE_LOG(LogTemp, Error, TEXT(">>> APPEL Execute_Dismantle (differe 1s) sur %s [%s]"), *Building->GetName(), *ClassName);
            // Differer de 1 seconde pour laisser TOUS les end-of-frame updates se terminer
            // Evite le crash ComponentsThatNeedEndOfFrameUpdate dans OnDismantleEffectFinished
            TWeakObjectPtr<AActor> WeakBuilding = Building;
            FTimerHandle TempHandle;
            GetWorld()->GetTimerManager().SetTimer(TempHandle, [WeakBuilding]()
            {
                if (WeakBuilding.IsValid() && !WeakBuilding->IsPendingKillPending())
                {
                    IFGDismantleInterface::Execute_Dismantle(WeakBuilding.Get());
                }
            }, 1.0f, false);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT(">>> APPEL Destroy() sur %s [%s]"), *Building->GetName(), *ClassName);
            Building->Destroy();
            UE_LOG(LogTemp, Error, TEXT("<<< Destroy() OK sur %s"), *Building->GetName());
        }
    }
}

float ATDCreatureSpawner::GetBuildingCurrentHealth(AActor* Building)
{
    if (!Building) return 0.0f;
    
    // Initialiser si pas encore fait
    if (!BuildingHealth.Contains(Building))
    {
        float MaxHP = GetBuildingMaxHealth(Building);
        BuildingHealth.Add(Building, MaxHP);
        BuildingMaxHealth.Add(Building, MaxHP);
    }
    
    return BuildingHealth[Building];
}

bool ATDCreatureSpawner::IsBuildingDestroyed(AActor* Building) const
{
    if (!Building) return true;
    return DestroyedBuildings.Contains(Building);
}

void ATDCreatureSpawner::MarkBuildingAttacked(AActor* Building)
{
    if (!Building || !IsValid(Building) || Building->IsPendingKillPending()) return;
    if (DestroyedBuildings.Contains(Building)) return;
    
    // Securite: seulement les vrais batiments (pas terrain/rochers)
    AFGBuildable* AsBuildable = Cast<AFGBuildable>(Building);
    FString CN = Building->GetClass()->GetName();
    bool bIsTD = CN.StartsWith(TEXT("TD")) && !CN.StartsWith(TEXT("TDEnemy")) && !CN.StartsWith(TEXT("TDCreature")) && !CN.StartsWith(TEXT("TDWorld")) && !CN.StartsWith(TEXT("TDDropship"));
    if (!AsBuildable && !bIsTD) return;
    
    // Reset ou ajouter le timer pour ce batiment
    AttackedBuildingTimers.Add(Building, AttackPulseDuration);
}

void ATDCreatureSpawner::UpdateAttackedBuildingsVisuals(float DeltaTime)
{
    // Liste des batiments a retirer (timer expire)
    TArray<AActor*> ToRemove;
    
    for (auto& Pair : AttackedBuildingTimers)
    {
        AActor* Building = Pair.Key;
        float& Timer = Pair.Value;
        
        if (!Building || !IsValid(Building) || Building->IsPendingKillPending() || DestroyedBuildings.Contains(Building))
        {
            ToRemove.Add(Building);
            continue;
        }
        
        Timer -= DeltaTime;
        
        // Calculer l'intensite du pulse (1.0 au debut, 0.0 a la fin)
        float PulseIntensity = FMath::Clamp(Timer / AttackPulseDuration, 0.0f, 1.0f);
        
        // Appliquer un effet de scale pulse sur le batiment
        // Scale entre 1.0 et 1.05 pour un effet subtil mais visible
        float ScaleFactor = 1.0f + (PulseIntensity * 0.05f * FMath::Sin(Timer * 20.0f));
        Building->SetActorScale3D(FVector(ScaleFactor));
        
        // Timer expire - restaurer le scale normal
        if (Timer <= 0.0f)
        {
            Building->SetActorScale3D(FVector(1.0f));
            ToRemove.Add(Building);
        }
    }
    
    // Nettoyer les batiments termines
    for (AActor* Building : ToRemove)
    {
        AttackedBuildingTimers.Remove(Building);
    }
}

// === TERRAIN MAPPING: Grille 3D voxel PAR BASE ===

int32 ATDCreatureSpawner::FindNearestBaseGrid(const FVector& Position) const
{
    float BestDist = MAX_FLT;
    int32 BestIdx = -1;
    for (int32 i = 0; i < BaseGrids.Num(); i++)
    {
        float Dist = FVector::Dist(Position, BaseGrids[i].Center);
        if (Dist < BestDist) { BestDist = Dist; BestIdx = i; }
    }
    return BestIdx;
}

float ATDCreatureSpawner::GetFlyingRatioFor(const FVector& SpawnLoc) const
{
    int32 Idx = FindNearestBaseGrid(SpawnLoc);
    if (Idx >= 0) return BaseGrids[Idx].FlyingRatio;
    return 0.3f;
}

TArray<FVector> ATDCreatureSpawner::GetGroundPathFor(const FVector& SpawnLoc, const FVector& TargetLoc)
{
    if (!bTerrainMapReady) return TArray<FVector>();
    int32 Idx = FindNearestBaseGrid(TargetLoc);
    if (Idx < 0) return TArray<FVector>();
    
    // BFS DYNAMIQUE depuis la position actuelle vers la cible
    return FindGroundPath(BaseGrids[Idx], SpawnLoc, TargetLoc);
}

TArray<FVector> ATDCreatureSpawner::GetFlyPathFor(const FVector& SpawnLoc, const FVector& TargetLoc)
{
    if (!bTerrainMapReady) return TArray<FVector>();
    int32 Idx = FindNearestBaseGrid(TargetLoc);
    if (Idx < 0) return TArray<FVector>();
    
    // BFS 3D DYNAMIQUE depuis la position actuelle vers la cible
    return FindFlyPath(BaseGrids[Idx], SpawnLoc, TargetLoc);
}

FVector ATDCreatureSpawner::GetNearestBaseCenter(const FVector& Position) const
{
    float BestDist = MAX_FLT;
    FVector BestCenter = FVector::ZeroVector;
    for (const FBaseGrid& G : BaseGrids)
    {
        float D = FVector::Dist(Position, G.Center);
        if (D < BestDist) { BestDist = D; BestCenter = G.Center; }
    }
    return BestCenter;
}

// === BuildAllBaseGrids: une grille 3D par base ===
void ATDCreatureSpawner::BuildAllBaseGrids(const TArray<FVector>& SpawnZones, const TArray<AActor*>& Buildings)
{
    BaseGrids.Empty();
    bTerrainMapReady = false;
    
    if (SpawnZones.Num() == 0 || Buildings.Num() == 0) return;
    
    // --- Clustering des batiments en bases (meme algo que AnalyzeBase) ---
    const float ClusterRadius = 10000.0f;
    TArray<AActor*> Unassigned = Buildings;
    TArray<TArray<AActor*>> Clusters;
    
    while (Unassigned.Num() > 0)
    {
        TArray<AActor*> Cluster;
        Cluster.Add(Unassigned[0]);
        Unassigned.RemoveAt(0);
        
        bool bGrew = true;
        while (bGrew)
        {
            bGrew = false;
            for (int32 i = Unassigned.Num() - 1; i >= 0; i--)
            {
                for (AActor* C : Cluster)
                {
                    if (FVector::Dist(Unassigned[i]->GetActorLocation(), C->GetActorLocation()) < ClusterRadius)
                    {
                        Cluster.Add(Unassigned[i]);
                        Unassigned.RemoveAt(i);
                        bGrew = true;
                        break;
                    }
                }
            }
        }
        if (Cluster.Num() > 0) Clusters.Add(Cluster);
    }
    
    UE_LOG(LogTemp, Warning, TEXT("BuildAllBaseGrids: %d bases detectees, %d spawn zones"), Clusters.Num(), SpawnZones.Num());
    
    // --- Pour chaque base: creer un FBaseGrid ---
    for (int32 c = 0; c < Clusters.Num(); c++)
    {
        FBaseGrid Grid;
        Grid.ZoneIndex = c;
        
        // Centre de la base
        FVector Sum = FVector::ZeroVector;
        for (AActor* B : Clusters[c])
        {
            if (B) { Sum += B->GetActorLocation(); Grid.Buildings.Add(B); }
        }
        Grid.Center = Sum / Clusters[c].Num();
        
        // Associer les spawn zones les plus proches a cette base
        for (int32 s = 0; s < SpawnZones.Num(); s++)
        {
            // Trouver quelle base est la plus proche de cette spawn zone
            float BestDist = MAX_FLT;
            int32 BestCluster = -1;
            for (int32 cc = 0; cc < Clusters.Num(); cc++)
            {
                FVector CC = FVector::ZeroVector;
                for (AActor* B : Clusters[cc]) { if (B) CC += B->GetActorLocation(); }
                CC /= Clusters[cc].Num();
                float D = FVector::Dist(SpawnZones[s], CC);
                if (D < BestDist) { BestDist = D; BestCluster = cc; }
            }
            if (BestCluster == c)
            {
                Grid.SpawnZones.Add(SpawnZones[s]);
                Grid.SpawnZoneIndices.Add(s);
            }
        }
        
        // Rayon = distance au spawner le plus loin
        Grid.Radius = 0.0f;
        for (const FVector& SZ : Grid.SpawnZones)
        {
            Grid.Radius = FMath::Max(Grid.Radius, FVector::Dist(Grid.Center, SZ));
        }
        Grid.Radius += 500.0f;
        
        if (Grid.SpawnZones.Num() == 0)
        {
            Grid.Radius = 5000.0f;  // Default si pas de spawn zone
        }
        
        UE_LOG(LogTemp, Warning, TEXT("Base %d: centre=(%.0f,%.0f,%.0f) rayon=%.0f, %d batiments, %d spawns"),
            c, Grid.Center.X, Grid.Center.Y, Grid.Center.Z, Grid.Radius,
            Grid.Buildings.Num(), Grid.SpawnZones.Num());
        
        // Construire la grille 3D de cette base
        BuildSingleBaseGrid(Grid);
        
        BaseGrids.Add(MoveTemp(Grid));
    }
    
    bTerrainMapReady = true;
    UE_LOG(LogTemp, Warning, TEXT("=== %d GRILLES 3D CONSTRUITES ==="), BaseGrids.Num());
}

// === BuildSingleBaseGrid: construire la grille 3D d'UNE base ===
void ATDCreatureSpawner::BuildSingleBaseGrid(FBaseGrid& G)
{
    // Bornes Z des batiments de cette base
    float MinBZ = MAX_FLT, MaxBZ = -MAX_FLT;
    for (auto& BW : G.Buildings)
    {
        AActor* B = BW.Get();
        if (!B) continue;
        MinBZ = FMath::Min(MinBZ, B->GetActorLocation().Z);
        MaxBZ = FMath::Max(MaxBZ, B->GetActorLocation().Z);
    }
    if (MinBZ > MaxBZ) { MinBZ = G.Center.Z - 1000.0f; MaxBZ = G.Center.Z + 1000.0f; }
    
    // Grille: carre englobant la sphere, clipping spherique
    G.Origin = FVector(G.Center.X - G.Radius, G.Center.Y - G.Radius, 0.0f);
    G.W = FMath::CeilToInt(G.Radius * 2.0f / G.CellSize);
    G.H = G.W;
    G.W = FMath::Min(G.W, 300); // Max 300x300 par base
    G.H = FMath::Min(G.H, 300);
    
    // Couches Z : detecter le sol reel de la map sous la base
    float TerrainZ = MinBZ - 3000.0f;  // Fallback
    {
        FCollisionQueryParams TerrainParams;
        TerrainParams.AddIgnoredActor(this);
        FHitResult TerrainHit;
        FVector RayStart = G.Center + FVector(0, 0, 1000.0f);
        FVector RayEnd = G.Center + FVector(0, 0, -20000.0f);  // 200m vers le bas
        if (GetWorld()->LineTraceSingleByChannel(TerrainHit, RayStart, RayEnd, ECC_WorldStatic, TerrainParams))
        {
            TerrainZ = TerrainHit.ImpactPoint.Z - 1000.0f;  // 10m sous le sol detecte
        }
    }
    G.MinZ = TerrainZ;
    float MaxZ = MaxBZ + 2000.0f;
    G.D = FMath::CeilToInt((MaxZ - G.MinZ) / G.VoxelH);
    G.D = FMath::Min(G.D, 100);  // Augmente limite a 100 couches (etait 80)
    
    int64 TotalVox = (int64)G.W * G.H * G.D;
    if (TotalVox > 3000000) { G.D = FMath::Min(G.D, (int32)(3000000 / (G.W * G.H))); TotalVox = (int64)G.W * G.H * G.D; }
    
    G.Voxels.SetNumZeroed(TotalVox);  // 0 = air
    G.GroundZ.SetNum(G.W * G.H);
    G.Walkable.SetNum(G.W * G.H);
    for (float& gz : G.GroundZ) gz = -99999.0f;
    for (bool& w : G.Walkable) w = false;
    
    UE_LOG(LogTemp, Warning, TEXT("  Base %d: grille %dx%dx%d = %lld voxels"), G.ZoneIndex, G.W, G.H, G.D, TotalVox);
    
    // === Phase 1: Raycasts verticaux - remplir voxels ===
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    float SphereRadSq = G.Radius * G.Radius;
    int32 Scanned = 0, Skipped = 0;
    
    for (int32 Y = 0; Y < G.H; Y++)
    {
        for (int32 X = 0; X < G.W; X++)
        {
            FVector WP(G.Origin.X + X * G.CellSize + G.CellSize * 0.5f,
                       G.Origin.Y + Y * G.CellSize + G.CellSize * 0.5f, 0.0f);
            
            // Clipping spherique 2D
            float Dx = WP.X - G.Center.X, Dy = WP.Y - G.Center.Y;
            if (Dx*Dx + Dy*Dy > SphereRadSq) { Skipped++; continue; }
            Scanned++;
            
            FVector RayStart(WP.X, WP.Y, MaxBZ + 2000.0f);
            FVector RayEnd(WP.X, WP.Y, MinBZ - 5000.0f);
            TArray<FHitResult> Hits;
            GetWorld()->LineTraceMultiByChannel(Hits, RayStart, RayEnd, ECC_WorldStatic, Params);
            
            int32 ColIdx = G.ColIdx(X, Y);
            
            for (const FHitResult& Hit : Hits)
            {
                int32 VZ = G.WorldToZ(Hit.ImpactPoint.Z);
                if (VZ < 0 || VZ >= G.D) continue;
                int32 VI = G.VoxelIdx(X, Y, VZ);
                
                AActor* HA = Hit.GetActor();
                bool bIsBuilding = false;
                if (HA)
                {
                    FString CN = HA->GetClass()->GetName();
                    bool bIsTD = CN.StartsWith(TEXT("TD")) && !CN.StartsWith(TEXT("TDEnemy")) && !CN.StartsWith(TEXT("TDCreature")) && !CN.StartsWith(TEXT("TDWorld")) && !CN.StartsWith(TEXT("TDDropship"));
                    bIsBuilding = CN.StartsWith(TEXT("Build_")) || bIsTD;
                }
                
                if (bIsBuilding)
                {
                    G.Voxels[VI] = 2;  // batiment
                }
                else
                {
                    G.Voxels[VI] = 1;  // terrain
                    // Le hit terrain le plus haut = le sol pour cette colonne
                    if (Hit.ImpactPoint.Z > G.GroundZ[ColIdx])
                    {
                        G.GroundZ[ColIdx] = Hit.ImpactPoint.Z;
                    }
                }
            }
            
            // Fallback single trace si rien trouve
            if (Hits.Num() == 0)
            {
                FHitResult SH;
                if (GetWorld()->LineTraceSingleByChannel(SH, RayStart, RayEnd, ECC_WorldStatic, Params))
                {
                    G.GroundZ[ColIdx] = SH.ImpactPoint.Z;
                    int32 VZ = G.WorldToZ(SH.ImpactPoint.Z);
                    if (VZ >= 0 && VZ < G.D) G.Voxels[G.VoxelIdx(X, Y, VZ)] = 1;
                }
            }
        }
    }
    
    // === Phase 1.5: Marquer les voxels via BOUNDING BOX de chaque batiment ===
    // Beaucoup plus fiable que les raycasts: detecte murs, fondations, architecture, formes courbes
    // Zero raycast - juste du calcul de chevauchement bbox/voxel
    {
        int32 BBoxVoxels = 0, BBoxBuildings = 0;
        
        for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        {
            AActor* Actor = *It;
            if (!Actor || !IsValid(Actor)) continue;
            
            FString CN = Actor->GetClass()->GetName();
            bool bIsTD = CN.StartsWith(TEXT("TD")) && !CN.StartsWith(TEXT("TDEnemy")) && !CN.StartsWith(TEXT("TDCreature")) && !CN.StartsWith(TEXT("TDWorld")) && !CN.StartsWith(TEXT("TDDropship"));
            if (!CN.StartsWith(TEXT("Build_")) && !bIsTD) continue;
            
            // Verifier que ce batiment est dans la sphere de cette base
            FVector BLoc = Actor->GetActorLocation();
            if (FVector::Dist2D(BLoc, G.Center) > G.Radius + 500.0f) continue;
            
            // Obtenir la bounding box complete (tous les composants)
            FBox BBox = Actor->GetComponentsBoundingBox(true);
            if (!BBox.IsValid) continue;
            
            // Etendre legerement la bbox pour couvrir les bords (epaisseur murs)
            BBox = BBox.ExpandBy(FVector(25.0f, 25.0f, 10.0f));
            
            // Convertir bbox en coordonnees voxel
            int32 VMinX = FMath::Clamp(FMath::FloorToInt((BBox.Min.X - G.Origin.X) / G.CellSize), 0, G.W - 1);
            int32 VMaxX = FMath::Clamp(FMath::FloorToInt((BBox.Max.X - G.Origin.X) / G.CellSize), 0, G.W - 1);
            int32 VMinY = FMath::Clamp(FMath::FloorToInt((BBox.Min.Y - G.Origin.Y) / G.CellSize), 0, G.H - 1);
            int32 VMaxY = FMath::Clamp(FMath::FloorToInt((BBox.Max.Y - G.Origin.Y) / G.CellSize), 0, G.H - 1);
            int32 VMinZ = FMath::Clamp(G.WorldToZ(BBox.Min.Z), 0, G.D - 1);
            int32 VMaxZ = FMath::Clamp(G.WorldToZ(BBox.Max.Z), 0, G.D - 1);
            
            BBoxBuildings++;
            
            // Marquer tous les voxels dans la bbox comme batiment
            for (int32 VZ = VMinZ; VZ <= VMaxZ; VZ++)
            {
                for (int32 VY = VMinY; VY <= VMaxY; VY++)
                {
                    for (int32 VX = VMinX; VX <= VMaxX; VX++)
                    {
                        int32 VI = G.VoxelIdx(VX, VY, VZ);
                        if (G.Voxels[VI] == 0)  // Seulement si c'etait air
                        {
                            G.Voxels[VI] = 2;
                            BBoxVoxels++;
                        }
                    }
                }
            }
        }
        
        UE_LOG(LogTemp, Warning, TEXT("  Base %d: Phase 1.5 BBox - %d batiments scannes, %d voxels marques"), 
            G.ZoneIndex, BBoxBuildings, BBoxVoxels);
    }
    
    // === Phase 2: Determiner walkability au sol ===
    static const int32 NDX[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    static const int32 NDY[] = {-1, -1, -1, 0, 0, 1, 1, 1};
    int32 WalkCount = 0;
    
    for (int32 Y = 0; Y < G.H; Y++)
    {
        for (int32 X = 0; X < G.W; X++)
        {
            int32 CI = G.ColIdx(X, Y);
            if (G.GroundZ[CI] < -90000.0f) continue;
            
            int32 Neighbors = 0;
            for (int32 d = 0; d < 8; d++)
            {
                int32 NX = X + NDX[d], NY = Y + NDY[d];
                if (!G.IsValid2D(NX, NY)) continue;
                float NZ = G.GroundZ[G.ColIdx(NX, NY)];
                if (NZ > -90000.0f && FMath::Abs(NZ - G.GroundZ[CI]) <= 200.0f) Neighbors++;
            }
            G.Walkable[CI] = (Neighbors >= 2);
            if (G.Walkable[CI]) WalkCount++;
        }
    }
    
    // === Phase 3: Analyse hauteur -> FlyingRatio ===
    int32 GroundBuildings = 0, ElevatedBuildings = 0;
    for (auto& BW : G.Buildings)
    {
        AActor* B = BW.Get();
        if (!B) continue;
        FIntPoint BC = G.WorldToCell(B->GetActorLocation());
        if (!G.IsValid2D(BC.X, BC.Y)) continue;
        float GZ = G.GroundZ[G.ColIdx(BC.X, BC.Y)];
        float HAG = (GZ > -90000.0f) ? (B->GetActorLocation().Z - GZ) : (B->GetActorLocation().Z - MinBZ);
        if (HAG < 1200.0f) GroundBuildings++; else ElevatedBuildings++;
    }
    
    float TotalB = (float)(GroundBuildings + ElevatedBuildings);
    if (TotalB > 0)
    {
        float EPct = (float)ElevatedBuildings / TotalB * 100.0f;
        if (EPct >= 95.0f) G.FlyingRatio = 0.80f;      // Quasi tout aerien
        else if (EPct >= 70.0f) G.FlyingRatio = 0.50f;  // Majoritairement aerien
        else if (EPct >= 40.0f) G.FlyingRatio = 0.35f;  // Mixte
        else G.FlyingRatio = 0.20f;                       // Principalement au sol
    }
    
    // Si aucune cellule walkable -> plus de volants
    if (WalkCount == 0) G.FlyingRatio = FMath::Max(G.FlyingRatio, 0.80f);
    
    // === Phase 4: Pre-calculer chemins sol + vol par spawn zone ===
    for (int32 s = 0; s < G.SpawnZones.Num(); s++)
    {
        int32 GlobalIdx = (s < G.SpawnZoneIndices.Num()) ? G.SpawnZoneIndices[s] : s;
        
        TArray<FVector> GP = FindGroundPath(G, G.SpawnZones[s], G.Center);
        if (GP.Num() > 0) G.GroundPaths.Add(GlobalIdx, GP);
        
        TArray<FVector> FP = FindFlyPath(G, G.SpawnZones[s], G.Center);
        if (FP.Num() > 0) G.FlyPaths.Add(GlobalIdx, FP);
    }
    
    UE_LOG(LogTemp, Warning, TEXT("  Base %d: %d scanned, %d skipped, %d walkable, %d ground/%d elevated, FlyRatio=%.0f%%, %d groundPaths, %d flyPaths"),
        G.ZoneIndex, Scanned, Skipped, WalkCount, GroundBuildings, ElevatedBuildings, G.FlyingRatio * 100.0f,
        G.GroundPaths.Num(), G.FlyPaths.Num());
}

// === BFS au sol (derive des voxels 3D) ===
TArray<FVector> ATDCreatureSpawner::FindGroundPath(const FBaseGrid& G, const FVector& Start, const FVector& End)
{
    TArray<FVector> Result;
    FIntPoint SC = G.WorldToCell(Start), EC = G.WorldToCell(End);
    SC.X = FMath::Clamp(SC.X, 0, G.W - 1); SC.Y = FMath::Clamp(SC.Y, 0, G.H - 1);
    EC.X = FMath::Clamp(EC.X, 0, G.W - 1); EC.Y = FMath::Clamp(EC.Y, 0, G.H - 1);
    
    int32 SI = G.ColIdx(SC.X, SC.Y), EI = G.ColIdx(EC.X, EC.Y);
    int32 Total = G.W * G.H;
    
    // Snap start/end vers cellule walkable la plus proche (rayon 15 cells)
    auto SnapToWalkable = [&](FIntPoint& Cell, int32& Idx) {
        if (G.Walkable[Idx] && G.GroundZ[Idx] > -90000.0f) return;
        float BestD = MAX_FLT; int32 BestI = -1; FIntPoint BestC = Cell;
        int32 R = 15;
        for (int32 dy = -R; dy <= R; dy++) {
            for (int32 dx = -R; dx <= R; dx++) {
                int32 NX = Cell.X + dx, NY = Cell.Y + dy;
                if (!G.IsValid2D(NX, NY)) continue;
                int32 NI = G.ColIdx(NX, NY);
                if (!G.Walkable[NI] || G.GroundZ[NI] < -90000.0f) continue;
                float D = FMath::Sqrt((float)(dx*dx + dy*dy));
                if (D < BestD) { BestD = D; BestI = NI; BestC = FIntPoint(NX, NY); }
            }
        }
        if (BestI >= 0) { Cell = BestC; Idx = BestI; }
    };
    SnapToWalkable(SC, SI);
    SnapToWalkable(EC, EI);
    
    TArray<int32> From; From.SetNum(Total);
    for (int32& v : From) v = -1;
    From[SI] = SI;
    
    TArray<int32> Q; Q.Reserve(Total / 4); Q.Add(SI);
    int32 Head = 0;
    static const int32 DX[] = {-1,0,1,-1,1,-1,0,1};
    static const int32 DY[] = {-1,-1,-1,0,0,1,1,1};
    bool bFound = false;
    
    while (Head < Q.Num() && Head < 200000)
    {
        int32 Cur = Q[Head++];
        if (Cur == EI) { bFound = true; break; }
        int32 CX = Cur % G.W, CY = Cur / G.W;
        float CZ = G.GroundZ[Cur];
        
        for (int32 d = 0; d < 8; d++)
        {
            int32 NX = CX + DX[d], NY = CY + DY[d];
            if (!G.IsValid2D(NX, NY)) continue;
            int32 NI = G.ColIdx(NX, NY);
            if (From[NI] != -1) continue;
            if (!G.Walkable[NI]) continue;
            float NZ = G.GroundZ[NI];
            // Max 2m montee ET descente (200u) - pente raide mais praticable
            if (FMath::Abs(NZ - CZ) > 200.0f) continue;
            From[NI] = Cur;
            Q.Add(NI);
        }
    }
    
    if (!bFound)
    {
        float BD = MAX_FLT; int32 BI = -1;
        for (int32 i = 0; i < Q.Num(); i++)
        {
            int32 IX = Q[i] % G.W, IY = Q[i] / G.W;
            float D = FMath::Abs(IX - EC.X) + FMath::Abs(IY - EC.Y);
            if (D < BD) { BD = D; BI = Q[i]; }
        }
        if (BI >= 0) { EI = BI; bFound = true; }
    }
    if (!bFound) return Result;
    
    TArray<FVector> Full;
    int32 Cur = EI; int32 S = 0;
    while (Cur != SI && S < 10000)
    {
        int32 PX = Cur % G.W, PY = Cur / G.W;
        FVector WP(G.Origin.X + PX * G.CellSize + G.CellSize * 0.5f,
                   G.Origin.Y + PY * G.CellSize + G.CellSize * 0.5f,
                   G.GroundZ[Cur] + 50.0f);
        Full.Insert(WP, 0);
        Cur = From[Cur]; S++;
    }
    
    for (int32 i = 0; i < Full.Num(); i += 2) Result.Add(Full[i]);
    if (Full.Num() > 0 && (Full.Num()-1) % 2 != 0) Result.Add(Full.Last());
    
    // === SNAP Z: raycaster chaque waypoint vers le sol reel ===
    UWorld* W = GetWorld();
    if (W)
    {
        FCollisionQueryParams SnapParams;
        SnapParams.bTraceComplex = false;
        for (int32 i = 0; i < Result.Num(); i++)
        {
            FVector RayStart = FVector(Result[i].X, Result[i].Y, Result[i].Z + 500.0f);
            FVector RayEnd = FVector(Result[i].X, Result[i].Y, Result[i].Z - 1000.0f);
            FHitResult SnapHit;
            if (W->LineTraceSingleByChannel(SnapHit, RayStart, RayEnd, ECC_WorldStatic, SnapParams))
            {
                Result[i].Z = SnapHit.ImpactPoint.Z + 50.0f;  // 50u au-dessus du sol
            }
        }
    }
    
    return Result;
}

// === BFS 3D vol (a travers les voxels air) ===
TArray<FVector> ATDCreatureSpawner::FindFlyPath(const FBaseGrid& G, const FVector& Start, const FVector& End)
{
    TArray<FVector> Result;
    if (G.Voxels.Num() == 0) return Result;
    
    int32 SX = FMath::Clamp(G.WorldToCell(Start).X, 0, G.W-1);
    int32 SY = FMath::Clamp(G.WorldToCell(Start).Y, 0, G.H-1);
    int32 SZ = FMath::Clamp(G.WorldToZ(Start.Z), 0, G.D-1);
    int32 EX = FMath::Clamp(G.WorldToCell(End).X, 0, G.W-1);
    int32 EY = FMath::Clamp(G.WorldToCell(End).Y, 0, G.H-1);
    int32 EZ = FMath::Clamp(G.WorldToZ(End.Z), 0, G.D-1);
    
    int32 SI = G.VoxelIdx(SX, SY, SZ), EI = G.VoxelIdx(EX, EY, EZ);
    int64 Total = (int64)G.W * G.H * G.D;
    
    // Si le voxel de depart ou d'arrivee est solide, trouver le plus proche air
    if (G.Voxels[SI] != 0) { for (int32 dz = -2; dz <= 2; dz++) { int32 NZ = SZ+dz; if (NZ >= 0 && NZ < G.D && G.Voxels[G.VoxelIdx(SX,SY,NZ)] == 0) { SZ = NZ; SI = G.VoxelIdx(SX,SY,SZ); break; } } }
    if (G.Voxels[EI] != 0) { for (int32 dz = -2; dz <= 2; dz++) { int32 NZ = EZ+dz; if (NZ >= 0 && NZ < G.D && G.Voxels[G.VoxelIdx(EX,EY,NZ)] == 0) { EZ = NZ; EI = G.VoxelIdx(EX,EY,EZ); break; } } }
    
    TArray<int32> From; From.SetNum(Total);
    for (int32& v : From) v = -1;
    From[SI] = SI;
    
    TArray<int32> Q; Q.Reserve(10000); Q.Add(SI);
    int32 Head = 0;
    bool bFound = false;
    
    while (Head < Q.Num() && Head < 300000)
    {
        int32 Cur = Q[Head++];
        if (Cur == EI) { bFound = true; break; }
        
        int32 CZ = Cur / (G.W * G.H);
        int32 CY = (Cur - CZ * G.W * G.H) / G.W;
        int32 CX = Cur % G.W;
        
        // 6 directions (faces) pour la perf, pas 26
        static const int32 D6[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
        for (int32 d = 0; d < 6; d++)
        {
            int32 NX = CX+D6[d][0], NY = CY+D6[d][1], NZ = CZ+D6[d][2];
            if (!G.IsValid3D(NX, NY, NZ)) continue;
            int32 NI = G.VoxelIdx(NX, NY, NZ);
            if (From[NI] != -1) continue;
            if (G.Voxels[NI] != 0) continue;  // Seulement air
            From[NI] = Cur;
            Q.Add(NI);
        }
    }
    
    if (!bFound)
    {
        float BD = MAX_FLT; int32 BI = -1;
        for (int32 i = 0; i < Q.Num(); i++)
        {
            int32 I = Q[i];
            int32 IZ = I / (G.W * G.H), IY = (I - IZ*G.W*G.H)/G.W, IX = I % G.W;
            float D = FMath::Abs(IX-EX) + FMath::Abs(IY-EY) + FMath::Abs(IZ-EZ);
            if (D < BD) { BD = D; BI = I; }
        }
        if (BI >= 0) { EI = BI; bFound = true; }
    }
    if (!bFound) return Result;
    
    TArray<FVector> Full;
    int32 Cur = EI; int32 S = 0;
    while (Cur != SI && S < 10000)
    {
        int32 PZ = Cur / (G.W * G.H);
        int32 PY = (Cur - PZ*G.W*G.H) / G.W;
        int32 PX = Cur % G.W;
        Full.Insert(G.CellToWorld(PX, PY, PZ), 0);
        Cur = From[Cur]; S++;
    }
    
    for (int32 i = 0; i < Full.Num(); i += 8) Result.Add(Full[i]);
    if (Full.Num() > 0 && (Full.Num()-1) % 8 != 0) Result.Add(Full.Last());
    return Result;
}

// === FLOW FIELD: BFS inverse depuis la cible ===
// 1 seul BFS par batiment cible, partage par tous les ennemis qui le ciblent
void ATDCreatureSpawner::BuildFlowFieldForTarget(const FBaseGrid& Grid, int32 GridIdx, AActor* Target)
{
    FGroundFlowField FF;
    FF.BaseGridIdx = GridIdx;
    int32 Total = Grid.W * Grid.H;
    FF.Directions.SetNum(Total);
    for (uint8& d : FF.Directions) d = 255;  // inaccessible par defaut
    
    FIntPoint TC = Grid.WorldToCell(Target->GetActorLocation());
    TC.X = FMath::Clamp(TC.X, 0, Grid.W - 1);
    TC.Y = FMath::Clamp(TC.Y, 0, Grid.H - 1);
    int32 TI = Grid.ColIdx(TC.X, TC.Y);
    
    // Snap cible vers cellule walkable la plus proche (rayon 15 cells)
    if (!Grid.Walkable[TI] || Grid.GroundZ[TI] < -90000.0f)
    {
        float BestD = MAX_FLT; int32 BestI = -1; FIntPoint BestC = TC;
        int32 R = 15;
        for (int32 dy = -R; dy <= R; dy++)
        {
            for (int32 dx = -R; dx <= R; dx++)
            {
                int32 NX = TC.X + dx, NY = TC.Y + dy;
                if (!Grid.IsValid2D(NX, NY)) continue;
                int32 NI = Grid.ColIdx(NX, NY);
                if (!Grid.Walkable[NI] || Grid.GroundZ[NI] < -90000.0f) continue;
                float D = FMath::Sqrt((float)(dx*dx + dy*dy));
                if (D < BestD) { BestD = D; BestI = NI; BestC = FIntPoint(NX, NY); }
            }
        }
        if (BestI >= 0) { TC = BestC; TI = BestI; }
    }
    
    FF.Directions[TI] = 8;  // 8 = SUR la cible
    
    // BFS depuis la cible vers l'exterieur
    TArray<int32> Q;
    Q.Reserve(Total / 4);
    Q.Add(TI);
    int32 Head = 0;
    static const int32 DX[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    static const int32 DY[] = {-1, -1, -1, 0, 0, 1, 1, 1};
    
    while (Head < Q.Num() && Head < 200000)
    {
        int32 Cur = Q[Head++];
        int32 CX = Cur % Grid.W, CY = Cur / Grid.W;
        float CZ = Grid.GroundZ[Cur];
        
        for (int32 d = 0; d < 8; d++)
        {
            int32 NX = CX + DX[d], NY = CY + DY[d];
            if (!Grid.IsValid2D(NX, NY)) continue;
            int32 NI = Grid.ColIdx(NX, NY);
            if (FF.Directions[NI] != 255) continue;  // deja visite
            if (!Grid.Walkable[NI]) continue;
            float NZ = Grid.GroundZ[NI];
            if (FMath::Abs(NZ - CZ) > 200.0f) continue;  // pente trop raide
            
            // Direction INVERSE: de N vers Cur (vers la cible)
            // d va de Cur vers N, donc l'oppose (7-d) va de N vers Cur
            FF.Directions[NI] = (uint8)(7 - d);
            Q.Add(NI);
        }
    }
    
    FF.bValid = true;
    GroundFlowFields.Add(Target, FF);
    
    UE_LOG(LogTemp, Log, TEXT("FlowField: construit pour %s, %d cellules accessibles sur %d"),
        *Target->GetName(), Q.Num(), Total);
}

// Lookup O(1): obtenir la direction vers la cible depuis n'importe quelle position
FVector ATDCreatureSpawner::QueryGroundFlow(const FVector& Position, AActor* Target)
{
    if (!Target || !IsValid(Target) || !bTerrainMapReady)
        return FVector::ZeroVector;
    
    // Lazy build: construire le flow field si pas encore fait
    FGroundFlowField* FF = GroundFlowFields.Find(Target);
    if (!FF || !FF->bValid)
    {
        int32 Idx = FindNearestBaseGrid(Target->GetActorLocation());
        if (Idx < 0) return FVector::ZeroVector;
        BuildFlowFieldForTarget(BaseGrids[Idx], Idx, Target);
        FF = GroundFlowFields.Find(Target);
        if (!FF) return FVector::ZeroVector;
    }
    
    int32 GIdx = FF->BaseGridIdx;
    if (GIdx < 0 || GIdx >= BaseGrids.Num()) return FVector::ZeroVector;
    const FBaseGrid& G = BaseGrids[GIdx];
    
    FIntPoint Cell = G.WorldToCell(Position);
    if (!G.IsValid2D(Cell.X, Cell.Y)) return FVector::ZeroVector;
    
    int32 CI = G.ColIdx(Cell.X, Cell.Y);
    uint8 Dir = FF->Directions[CI];
    
    if (Dir == 8) return FVector::ZeroVector;     // deja sur la cible
    if (Dir == 255) return FVector::ZeroVector;   // inaccessible -> fallback direct dans l'ennemi
    
    static const float FDX[] = {-1.f, 0.f, 1.f, -1.f, 1.f, -1.f, 0.f, 1.f};
    static const float FDY[] = {-1.f, -1.f, -1.f, 0.f, 0.f, 1.f, 1.f, 1.f};
    
    return FVector(FDX[Dir], FDY[Dir], 0.0f).GetSafeNormal();
}

// Invalider tous les flow fields caches (quand terrain ou batiments changent)
void ATDCreatureSpawner::InvalidateAllFlowFields()
{
    int32 Count = GroundFlowFields.Num();
    GroundFlowFields.Empty();
    if (Count > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("FlowField: %d flow fields invalides"), Count);
    }
}

// === Refresh quand un batiment est detruit ===
void ATDCreatureSpawner::RefreshTerrainAroundBuilding(const FVector& BuildingLocation)
{
    // Marquer la grille comme dirty pour rebuild a la prochaine vague
    bGridDirty = true;
    
    // Invalider les flow fields (seront reconstruits lazily)
    InvalidateAllFlowFields();
    
    if (!bTerrainMapReady) return;
    int32 Idx = FindNearestBaseGrid(BuildingLocation);
    if (Idx < 0) return;
    
    // === Effacement LOCAL instantane (rayon 2000u = 20m) ===
    // Juste liberer les voxels autour du batiment detruit -> les ennemis peuvent passer
    // Pas de rebuild complet, pas de freeze
    FBaseGrid& G = BaseGrids[Idx];
    float ClearRadius = 2000.0f;
    int32 CRCells = FMath::CeilToInt(ClearRadius / G.CellSize);
    
    FIntPoint BCell = G.WorldToCell(BuildingLocation);
    int32 BZ = G.WorldToZ(BuildingLocation.Z);
    
    int32 Cleared = 0;
    for (int32 DY = -CRCells; DY <= CRCells; DY++)
    {
        for (int32 DX = -CRCells; DX <= CRCells; DX++)
        {
            int32 CX = BCell.X + DX, CY = BCell.Y + DY;
            if (!G.IsValid2D(CX, CY)) continue;
            
            // Verifier distance 2D
            float WX = G.Origin.X + CX * G.CellSize + G.CellSize * 0.5f;
            float WY = G.Origin.Y + CY * G.CellSize + G.CellSize * 0.5f;
            if (FMath::Square(WX - BuildingLocation.X) + FMath::Square(WY - BuildingLocation.Y) > ClearRadius * ClearRadius) continue;
            
            // Liberer les voxels batiment dans cette colonne (±10 couches Z)
            for (int32 DZ = -10; DZ <= 10; DZ++)
            {
                int32 CZ = BZ + DZ;
                if (CZ < 0 || CZ >= G.D) continue;
                int32 VI = G.VoxelIdx(CX, CY, CZ);
                if (G.Voxels[VI] == 2)  // Seulement les voxels batiment
                {
                    G.Voxels[VI] = 0;  // Liberer en air
                    Cleared++;
                }
            }
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("RefreshTerrain: %d voxels liberes autour de (%.0f, %.0f, %.0f) base %d"), 
        Cleared, BuildingLocation.X, BuildingLocation.Y, BuildingLocation.Z, Idx);
}

// === ANCIEN CODE DESACTIVE ===
#if 0
    int32 SX_OLD = 0; int32 SY_OLD = 0; int32 SZ_OLD = 0;
    int32 EX_OLD = 0; int32 EY_OLD = 0; int32 EZ_OLD = 0;
    
    // Clamp aux bornes
    SX = FMath::Clamp(SX, 0, GridWidth - 1);
    SY = FMath::Clamp(SY, 0, GridHeight - 1);
    SZ = FMath::Clamp(SZ, 0, GridDepth - 1);
    EX = FMath::Clamp(EX, 0, GridWidth - 1);
    EY = FMath::Clamp(EY, 0, GridHeight - 1);
    EZ = FMath::Clamp(EZ, 0, GridDepth - 1);
    
    int32 StartIdx = VoxelIndex(SX, SY, SZ);
    int32 EndIdx = VoxelIndex(EX, EY, EZ);
    int64 TotalVoxels = (int64)GridWidth * GridHeight * GridDepth;
    
    // BFS 3D: 26 directions (6 faces + 12 aretes + 8 coins)
    TArray<int32> CameFrom;
    CameFrom.SetNum(TotalVoxels);
    for (int32& v : CameFrom) v = -1;
    CameFrom[StartIdx] = StartIdx;
    
    TArray<int32> Queue;
    Queue.Reserve(10000);
    Queue.Add(StartIdx);
    int32 Head = 0;
    bool bFound = false;
    
    while (Head < Queue.Num() && Head < 500000)  // Limite iterations
    {
        int32 Current = Queue[Head++];
        if (Current == EndIdx) { bFound = true; break; }
        
        int32 CZ = Current / (GridWidth * GridHeight);
        int32 CY = (Current - CZ * GridWidth * GridHeight) / GridWidth;
        int32 CX = Current % GridWidth;
        
        // 26 voisins en 3D
        for (int32 dz = -1; dz <= 1; dz++)
        {
            for (int32 dy = -1; dy <= 1; dy++)
            {
                for (int32 dx = -1; dx <= 1; dx++)
                {
                    if (dx == 0 && dy == 0 && dz == 0) continue;
                    
                    int32 NX = CX + dx;
                    int32 NY = CY + dy;
                    int32 NZ = CZ + dz;
                    
                    if (!IsValidVoxel(NX, NY, NZ)) continue;
                    
                    int32 NIdx = VoxelIndex(NX, NY, NZ);
                    if (CameFrom[NIdx] != -1) continue;
                    
                    // 0=air -> peut voler, 1=terrain -> bloque, 2=batiment -> bloque
                    if (VoxelGrid[NIdx] != 0) continue;
                    
                    CameFrom[NIdx] = Current;
                    Queue.Add(NIdx);
                }
            }
        }
    }
    
    if (!bFound)
    {
        // Chercher le voxel visite le plus proche de la destination
        float BestDist = MAX_FLT;
        int32 BestIdx = -1;
        for (int32 i = 0; i < Queue.Num(); i++)
        {
            int32 Idx = Queue[i];
            int32 IZ = Idx / (GridWidth * GridHeight);
            int32 IY = (Idx - IZ * GridWidth * GridHeight) / GridWidth;
            int32 IX = Idx % GridWidth;
            float Dist = FMath::Abs(IX - EX) + FMath::Abs(IY - EY) + FMath::Abs(IZ - EZ);
            if (Dist < BestDist) { BestDist = Dist; BestIdx = Idx; }
        }
        if (BestIdx >= 0) { EndIdx = BestIdx; bFound = true; }
    }
    
    if (!bFound) return Result;
    
    // Reconstruire le chemin
    TArray<FVector> FullPath;
    int32 Current = EndIdx;
    int32 Safety = 0;
    while (Current != StartIdx && Safety < 10000)
    {
        int32 PZ = Current / (GridWidth * GridHeight);
        int32 PY = (Current - PZ * GridWidth * GridHeight) / GridWidth;
        int32 PX = Current % GridWidth;
        FVector WP(
            GridOrigin.X + PX * GridCellSize + GridCellSize * 0.5f,
            GridOrigin.Y + PY * GridCellSize + GridCellSize * 0.5f,
            GridMinZ + PZ * VoxelHeight + VoxelHeight * 0.5f
        );
        FullPath.Insert(WP, 0);
        Current = CameFrom[Current];
        Safety++;
    }
    
    // Simplifier: 1 waypoint tous les 8
    for (int32 i = 0; i < FullPath.Num(); i += 8)
    {
        Result.Add(FullPath[i]);
    }
    if (FullPath.Num() > 0 && (FullPath.Num() - 1) % 8 != 0)
    {
        Result.Add(FullPath.Last());
    }
    
    return Result;
}

void ATDCreatureSpawner::BuildTerrainMap(const TArray<FVector>& SpawnZones, const TArray<AActor*>& Buildings)
{
    if (SpawnZones.Num() == 0 || Buildings.Num() == 0) return;
    
    // === SPHERE = centre de la base, rayon = jusqu'aux spawners ===
    // Les spawners (X rouges) sont au bord, la base (cercle noir) est au centre
    FVector BaseCenter = FVector::ZeroVector;
    float MinBuildZ = MAX_FLT, MaxBuildZ = -MAX_FLT;
    for (AActor* B : Buildings)
    {
        if (!B) continue;
        FVector Loc = B->GetActorLocation();
        BaseCenter += Loc;
        MinBuildZ = FMath::Min(MinBuildZ, Loc.Z);
        MaxBuildZ = FMath::Max(MaxBuildZ, Loc.Z);
    }
    BaseCenter /= Buildings.Num();
    float AvgZ = BaseCenter.Z;
    
    // Rayon = distance du centre au spawner le plus loin
    SphereRadius = 0.0f;
    for (const FVector& Zone : SpawnZones)
    {
        float Dist = FVector::Dist(BaseCenter, Zone);
        SphereRadius = FMath::Max(SphereRadius, Dist);
    }
    SphereRadius += 500.0f;  // Petite marge
    SphereCenter = BaseCenter;
    
    UE_LOG(LogTemp, Warning, TEXT("BuildTerrainMap: Sphere centre=(%.0f,%.0f,%.0f) rayon=%.0f"), 
        SphereCenter.X, SphereCenter.Y, SphereCenter.Z, SphereRadius);
    
    // Grille 2D = carre englobant la sphere, le clipping spherique fait le reste
    float MinX = SphereCenter.X - SphereRadius;
    float MaxX = SphereCenter.X + SphereRadius;
    float MinY = SphereCenter.Y - SphereRadius;
    float MaxY = SphereCenter.Y + SphereRadius;
    
    GridOrigin = FVector(MinX, MinY, AvgZ);
    GridWidth = FMath::CeilToInt((MaxX - MinX) / GridCellSize);
    GridHeight = FMath::CeilToInt((MaxY - MinY) / GridCellSize);
    
    // Limiter la taille (100u cells = plus de cellules, mais sphere clippe)
    GridWidth = FMath::Min(GridWidth, 500);
    GridHeight = FMath::Min(GridHeight, 500);
    
    int32 TotalCells = GridWidth * GridHeight;
    TerrainGroundZ.SetNumZeroed(TotalCells);
    TerrainWalkable.SetNumZeroed(TotalCells);
    TerrainCellBuilding.SetNum(TotalCells);
    AllDetectedObjects.Empty();
    LastSpawnZones = SpawnZones;
    bTerrainMapReady = false;
    
    // === Grille 3D: couches verticales clippees a la sphere ===
    GridMinZ = AvgZ - SphereRadius;
    float GridMaxZ = AvgZ + SphereRadius;
    GridDepth = FMath::CeilToInt((GridMaxZ - GridMinZ) / VoxelHeight);
    GridDepth = FMath::Min(GridDepth, 100);  // Max 100 couches
    
    int64 TotalVoxels = (int64)GridWidth * GridHeight * GridDepth;
    if (TotalVoxels > 5000000) // Securite: max 5M voxels
    {
        // Reduire la resolution si trop de voxels
        VoxelHeight = 300.0f;
        GridDepth = FMath::CeilToInt((GridMaxZ - GridMinZ) / VoxelHeight);
        GridDepth = FMath::Min(GridDepth, 60);
        TotalVoxels = (int64)GridWidth * GridHeight * GridDepth;
    }
    
    VoxelGrid.SetNumZeroed(TotalVoxels);  // 0 = air par defaut
    
    UE_LOG(LogTemp, Warning, TEXT("BuildTerrainMap: grille 3D = %dx%dx%d = %lld voxels (VoxelH=%.0f)"), 
        GridWidth, GridHeight, GridDepth, TotalVoxels, VoxelHeight);
    
    UE_LOG(LogTemp, Warning, TEXT("BuildTerrainMap: grille %dx%d = %d cellules, sphere R=%.0f, origine=(%.0f,%.0f)"), 
        GridWidth, GridHeight, TotalCells, SphereRadius, GridOrigin.X, GridOrigin.Y);
    
    // === Phase 1: MULTI-RAYCAST 3D - scanner TOUT: terrain + objets + batiments ===
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    Params.bReturnPhysicalMaterial = false;
    
    int32 TotalObjectsDetected = 0;
    int32 CellsScanned = 0;
    int32 CellsSkipped = 0;
    float SphereRadiusSq = SphereRadius * SphereRadius;
    
    for (int32 Y = 0; Y < GridHeight; Y++)
    {
        for (int32 X = 0; X < GridWidth; X++)
        {
            int32 Idx = Y * GridWidth + X;
            FVector WorldPos = GridToWorld(X, Y);
            
            // === CLIPPING SPHERIQUE: ignorer les cellules hors sphere ===
            float DistSq = FVector::DistSquared2D(FVector(WorldPos.X, WorldPos.Y, 0), FVector(SphereCenter.X, SphereCenter.Y, 0));
            if (DistSq > SphereRadiusSq)
            {
                TerrainGroundZ[Idx] = -99999.0f;
                CellsSkipped++;
                continue;
            }
            CellsScanned++;
            
            // LineTraceMulti: UN seul ray qui traverse TOUT de haut en bas
            // Portee verticale basee sur la vraie hauteur des batiments + grande marge
            float VerticalRange = FMath::Max(5000.0f, (MaxBuildZ - MinBuildZ) + 3000.0f);
            FVector RayStart = FVector(WorldPos.X, WorldPos.Y, MaxBuildZ + 2000.0f);
            FVector RayEnd = FVector(WorldPos.X, WorldPos.Y, MinBuildZ - VerticalRange);
            TArray<FHitResult> Hits;
            
            GetWorld()->LineTraceMultiByChannel(Hits, RayStart, RayEnd, ECC_WorldStatic, Params);
            
            // Le hit le plus bas = le sol (terrain)
            float LowestZ = -99999.0f;
            for (const FHitResult& Hit : Hits)
            {
                // Enregistrer le sol (le point le plus bas qui n'est pas un batiment)
                AActor* HitActor = Hit.GetActor();
                if (HitActor)
                {
                    FString HitClass = HitActor->GetClass()->GetName();
                    bool bIsBuilding = HitClass.StartsWith(TEXT("Build_")) || 
                        (HitClass.StartsWith(TEXT("TD")) && !HitClass.StartsWith(TEXT("TDEnemy")) && !HitClass.StartsWith(TEXT("TDCreature")) && !HitClass.StartsWith(TEXT("TDWorld")) && !HitClass.StartsWith(TEXT("TDDropship")));
                    
                    if (bIsBuilding)
                    {
                        // Creer un FGridObject detaille
                        FGridObject Obj;
                        Obj.Actor = HitActor;
                        Obj.Location = HitActor->GetActorLocation();
                        Obj.CellX = X;
                        Obj.CellY = Y;
                        
                        // Taille via bounding box
                        FVector Origin, BoxExtent;
                        HitActor->GetActorBounds(false, Origin, BoxExtent);
                        Obj.Size = BoxExtent * 2.0f;
                        
                        // === CLASSIFICATION AUTOMATIQUE ===
                        if (HitClass.Contains(TEXT("TDTurret")) || HitClass.Contains(TEXT("TDFireTurret")) ||
                            HitClass.Contains(TEXT("TDShockwave")) || HitClass.Contains(TEXT("TDShield")) ||
                            HitClass.Contains(TEXT("TDDrone")))
                        {
                            Obj.Priority = 1; Obj.TypeName = TEXT("Defense");
                            Obj.bIsTarget = true; Obj.bBlocksPath = false;
                        }
                        else if (HitClass.Contains(TEXT("Constructor")) || HitClass.Contains(TEXT("Assembler")) ||
                                 HitClass.Contains(TEXT("Manufacturer")) || HitClass.Contains(TEXT("Smelter")) ||
                                 HitClass.Contains(TEXT("Foundry")) || HitClass.Contains(TEXT("Refinery")) ||
                                 HitClass.Contains(TEXT("Generator")) || HitClass.Contains(TEXT("Miner")) ||
                                 HitClass.Contains(TEXT("Packager")) || HitClass.Contains(TEXT("Blender")))
                        {
                            Obj.Priority = 2; Obj.TypeName = TEXT("Machine");
                            Obj.bIsTarget = true; Obj.bBlocksPath = true;
                        }
                        else if (HitClass.Contains(TEXT("Wall")) || HitClass.Contains(TEXT("Foundation")) ||
                                 HitClass.Contains(TEXT("Ramp")) || HitClass.Contains(TEXT("Pillar")) ||
                                 HitClass.Contains(TEXT("Fence")))
                        {
                            Obj.Priority = 3; Obj.TypeName = TEXT("Structure");
                            Obj.bIsTarget = false; Obj.bBlocksPath = true;
                        }
                        else if (HitClass.Contains(TEXT("Conveyor")) || HitClass.Contains(TEXT("Pipeline")) ||
                                 HitClass.Contains(TEXT("Pipe")) || HitClass.Contains(TEXT("Splitter")) ||
                                 HitClass.Contains(TEXT("Merger")) || HitClass.Contains(TEXT("Junction")))
                        {
                            Obj.Priority = 4; Obj.TypeName = TEXT("Transport");
                            Obj.bIsTarget = false; Obj.bBlocksPath = false;
                            Obj.bDestructible = true;
                        }
                        else
                        {
                            Obj.Priority = 5; Obj.TypeName = TEXT("Batiment");
                            Obj.bIsTarget = true; Obj.bBlocksPath = false;
                        }
                        
                        AllDetectedObjects.Add(Obj);
                        
                        // Marquer le voxel 3D comme batiment
                        int32 BVZ = FMath::FloorToInt((Hit.ImpactPoint.Z - GridMinZ) / VoxelHeight);
                        if (BVZ >= 0 && BVZ < GridDepth)
                        {
                            int32 BVI = VoxelIndex(X, Y, BVZ);
                            if (BVI >= 0 && BVI < VoxelGrid.Num()) VoxelGrid[BVI] = 2; // batiment
                        }
                        
                        if (!TerrainCellBuilding[Idx].IsValid())
                        {
                            TerrainCellBuilding[Idx] = HitActor;
                        }
                        TotalObjectsDetected++;
                        continue;
                    }
                }
                
                // C'est du terrain/sol - marquer voxel 3D comme terrain solide
                int32 VZ = FMath::FloorToInt((Hit.ImpactPoint.Z - GridMinZ) / VoxelHeight);
                if (VZ >= 0 && VZ < GridDepth)
                {
                    int32 VI = VoxelIndex(X, Y, VZ);
                    if (VI >= 0 && VI < VoxelGrid.Num()) VoxelGrid[VI] = 1; // terrain
                }
                
                if (Hit.ImpactPoint.Z > LowestZ || LowestZ < -90000.0f)
                {
                    LowestZ = Hit.ImpactPoint.Z;
                }
            }
            
            // Si aucun hit multi, essayer un single trace pour le sol
            if (Hits.Num() == 0)
            {
                FHitResult SingleHit;
                if (GetWorld()->LineTraceSingleByChannel(SingleHit, RayStart, RayEnd, ECC_WorldStatic, Params))
                {
                    LowestZ = SingleHit.ImpactPoint.Z;
                }
            }
            
            TerrainGroundZ[Idx] = LowestZ;
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("BuildTerrainMap: Phase 1 - %d cellules scannees, %d ignorees (hors sphere), %d objets detectes"), 
        CellsScanned, CellsSkipped, TotalObjectsDetected);
    
    // === Phase 2: Raycasts HORIZONTAUX entre cellules adjacentes ===
    // Pour chaque cellule, lancer des rays vers les 8 voisins a hauteur de poitrine
    // Si un ray est bloque par du terrain (pas un batiment) -> pas de connexion
    static const int32 DX[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    static const int32 DY[] = {-1, -1, -1, 0, 0, 1, 1, 1};
    
    // Reset les connexions bloquees (membre de classe, maintenant TMap avec info de blocage)
    BlockedConnections.Empty();
    int32 HorizontalRayCount = 0;
    int32 BlockedCount = 0;
    
    for (int32 Y = 0; Y < GridHeight; Y++)
    {
        for (int32 X = 0; X < GridWidth; X++)
        {
            int32 Idx = Y * GridWidth + X;
            float GZ = TerrainGroundZ[Idx];
            if (GZ < -90000.0f) continue;
            
            FVector CellPos = GridToWorld(X, Y);
            CellPos.Z = GZ + 60.0f;  // Hauteur de poitrine au-dessus du sol
            
            for (int32 d = 0; d < 8; d++)
            {
                int32 NX = X + DX[d];
                int32 NY = Y + DY[d];
                if (!IsValidCell(NX, NY)) continue;
                
                int32 NIdx = NY * GridWidth + NX;
                float NZ = TerrainGroundZ[NIdx];
                if (NZ < -90000.0f) continue;
                
                FVector NeighborPos = GridToWorld(NX, NY);
                NeighborPos.Z = NZ + 60.0f;
                
                // Raycast horizontal entre les deux cellules
                FHitResult HorizHit;
                bool bBlocked = GetWorld()->LineTraceSingleByChannel(HorizHit, CellPos, NeighborPos, ECC_WorldStatic, Params);
                HorizontalRayCount++;
                
                if (bBlocked)
                {
                    AActor* BlockActor = HorizHit.GetActor();
                    bool bIsTerrainBlock = true;  // Par defaut: c'est du terrain
                    
                    if (BlockActor)
                    {
                        FString BlockClass = BlockActor->GetClass()->GetName();
                        bool bIsBuildingBlock = BlockClass.StartsWith(TEXT("Build_")) || 
                            (BlockClass.StartsWith(TEXT("TD")) && !BlockClass.StartsWith(TEXT("TDEnemy")) && !BlockClass.StartsWith(TEXT("TDCreature")));
                        
                        if (bIsBuildingBlock)
                        {
                            bIsTerrainBlock = false;  // C'est un batiment, l'ennemi peut le casser
                        }
                    }
                    
                    // Creer un FGridBlockage avec les infos detaillees
                    int64 Key = (int64)FMath::Min(Idx, NIdx) * 100000 + FMath::Max(Idx, NIdx);
                    
                    if (bIsTerrainBlock)
                    {
                        FGridBlockage Blockage;
                        Blockage.bIsTerrain = true;
                        Blockage.BreakCost = 9999.0f;  // Infranchissable
                        Blockage.Priority = 99;
                        BlockedConnections.Add(Key, Blockage);
                        BlockedCount++;
                    }
                    else
                    {
                        // C'est un batiment qui bloque -> franchissable en le detruisant
                        FGridBlockage Blockage;
                        Blockage.bIsTerrain = false;
                        Blockage.BlockingActor = BlockActor;
                        
                        // Cout selon le type de batiment
                        FString BlockClass = BlockActor->GetClass()->GetName();
                        if (BlockClass.Contains(TEXT("Conveyor")) || BlockClass.Contains(TEXT("Pipe")))
                        {
                            Blockage.BreakCost = 2.0f;  // Facile a casser, faible prio
                            Blockage.Priority = 4;
                        }
                        else if (BlockClass.Contains(TEXT("Wall")) || BlockClass.Contains(TEXT("Foundation")) || BlockClass.Contains(TEXT("Fence")))
                        {
                            Blockage.BreakCost = 10.0f;  // Mur = cout moyen
                            Blockage.Priority = 3;
                        }
                        else
                        {
                            Blockage.BreakCost = 5.0f;   // Machine/autre
                            Blockage.Priority = 2;
                        }
                        
                        BlockedConnections.Add(Key, Blockage);
                        BlockedCount++;
                    }
                }
            }
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("BuildTerrainMap: Phase 2 - %d raycasts horizontaux, %d connexions bloquees par terrain"), HorizontalRayCount, BlockedCount);
    
    // === Phase 2b: Determiner la praticabilite avec les connexions horizontales ===
    for (int32 Y = 0; Y < GridHeight; Y++)
    {
        for (int32 X = 0; X < GridWidth; X++)
        {
            int32 Idx = Y * GridWidth + X;
            float GZ = TerrainGroundZ[Idx];
            
            if (GZ < -90000.0f)
            {
                TerrainWalkable[Idx] = false;
                continue;
            }
            
            // Compter les voisins accessibles (hauteur OK + pas de mur terrain entre)
            int32 AccessibleNeighbors = 0;
            for (int32 d = 0; d < 8; d++)
            {
                int32 NX = X + DX[d];
                int32 NY = Y + DY[d];
                if (!IsValidCell(NX, NY)) continue;
                
                int32 NIdx = NY * GridWidth + NX;
                float NZ = TerrainGroundZ[NIdx];
                if (NZ < -90000.0f) continue;
                
                // Check 1: hauteur franchissable
                if (FMath::Abs(NZ - GZ) > 80.0f) continue;
                
                // Check 2: pas de terrain infranchissable entre les deux cellules
                int64 Key = (int64)FMath::Min(Idx, NIdx) * 100000 + FMath::Max(Idx, NIdx);
                FGridBlockage* Blk = BlockedConnections.Find(Key);
                if (Blk && Blk->bIsTerrain) continue;  // Terrain = impassable
                
                AccessibleNeighbors++;
            }
            
            TerrainWalkable[Idx] = (AccessibleNeighbors >= 2);
        }
    }
    
    // === Phase 2b: Enregistrer TOUS les batiments dans la grille + analyse hauteur ===
    int32 GroundLevelBuildings = 0;
    int32 ElevatedBuildings = 0;
    int32 RegisteredFromArray = 0;
    
    for (AActor* B : Buildings)
    {
        if (!B || !IsValid(B)) continue;
        FVector BLoc = B->GetActorLocation();
        FIntPoint BCell = WorldToGrid(BLoc);
        if (!IsValidCell(BCell.X, BCell.Y)) continue;
        
        int32 BIdx = BCell.Y * GridWidth + BCell.X;
        
        // Enregistrer dans la cellule (si pas deja detecte par raycast)
        if (!TerrainCellBuilding[BIdx].IsValid())
        {
            TerrainCellBuilding[BIdx] = B;
            RegisteredFromArray++;
        }
        
        // Classifier par hauteur relative au sol
        // Fondation = 400u, mur = 400u, donc 1000u = 2.5 etages = encore "au sol"
        float GroundZ = TerrainGroundZ[BIdx];
        if (GroundZ > -90000.0f)
        {
            float HeightAboveGround = BLoc.Z - GroundZ;
            if (HeightAboveGround < 1200.0f)  // ~3 etages = encore au sol
            {
                GroundLevelBuildings++;
            }
            else
            {
                ElevatedBuildings++;
            }
        }
        else
        {
            // Pas de sol detecte - comparer avec MinBuildZ
            float HeightAboveLowest = BLoc.Z - MinBuildZ;
            if (HeightAboveLowest < 1200.0f)
            {
                GroundLevelBuildings++;  // Proche du niveau le plus bas
            }
            else
            {
                ElevatedBuildings++;
            }
        }
    }
    
    // Compter les cellules praticables
    int32 WalkableCount = 0;
    for (bool b : TerrainWalkable) { if (b) WalkableCount++; }
    
    // === Phase 4: Adapter le ratio volants/sol avec analyse 3D complete ===
    float TotalB = (float)(GroundLevelBuildings + ElevatedBuildings);
    
    // Verifier aussi si des chemins au sol existent (via CachedPaths plus tard)
    bool bAnyGroundPathExists = false;
    // On le saura apres Phase 3, donc on pre-calcule ici et on ajustera apres
    
    if (TotalB > 0)
    {
        float ElevatedPct = (float)ElevatedBuildings / TotalB * 100.0f;
        
        if (ElevatedPct >= 90.0f)
        {
            // Base quasi 100% aerienne -> presque que des volants
            FlyingRatio = 0.95f;
            UE_LOG(LogTemp, Warning, TEXT("BuildTerrainMap: BASE AERIENNE detectee (%.0f%% en hauteur) -> 95%% volants"), ElevatedPct);
        }
        else if (ElevatedPct >= 60.0f)
        {
            // Base majoritairement aerienne -> beaucoup de volants
            FlyingRatio = 0.70f;
            UE_LOG(LogTemp, Warning, TEXT("BuildTerrainMap: BASE MIXTE HAUTE (%.0f%% en hauteur) -> 70%% volants"), ElevatedPct);
        }
        else if (ElevatedPct >= 30.0f)
        {
            // Base mixte -> moitie-moitie
            FlyingRatio = 0.50f;
            UE_LOG(LogTemp, Warning, TEXT("BuildTerrainMap: BASE MIXTE (%.0f%% en hauteur) -> 50%% volants"), ElevatedPct);
        }
        else if (ElevatedPct >= 10.0f)
        {
            // Base principalement au sol avec quelques etages
            FlyingRatio = 0.35f;
            UE_LOG(LogTemp, Warning, TEXT("BuildTerrainMap: BASE SOL + ETAGES (%.0f%% en hauteur) -> 35%% volants"), ElevatedPct);
        }
        else
        {
            // Base 100% au sol
            FlyingRatio = 0.25f;
            UE_LOG(LogTemp, Warning, TEXT("BuildTerrainMap: BASE AU SOL (%.0f%% en hauteur) -> 25%% volants"), ElevatedPct);
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("BuildTerrainMap: %d/%d praticables (%.0f%%), %d objets raycasts + %d tableau"), 
        WalkableCount, TotalCells, TotalCells > 0 ? (100.0f * WalkableCount / TotalCells) : 0.0f,
        TotalObjectsDetected, RegisteredFromArray);
    UE_LOG(LogTemp, Warning, TEXT("BuildTerrainMap: %d batiments au sol, %d en hauteur -> FlyingRatio=%.0f%%"), 
        GroundLevelBuildings, ElevatedBuildings, FlyingRatio * 100.0f);
    
    // === Phase 3: Pre-calculer les chemins depuis chaque zone de spawn ===
    CachedPaths.Empty();
    
    // BaseCenter deja calcule plus haut, on le reutilise directement
    
    for (int32 i = 0; i < SpawnZones.Num(); i++)
    {
        TArray<FVector> Path = FindPathOnGrid(SpawnZones[i], BaseCenter);
        if (Path.Num() > 0)
        {
            CachedPaths.Add(i, Path);
            UE_LOG(LogTemp, Warning, TEXT("BuildTerrainMap: chemin zone %d -> base = %d waypoints"), i, Path.Num());
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("BuildTerrainMap: AUCUN chemin zone %d -> base (volants forces)"), i);
        }
    }
    
    // === Phase 5: Si AUCUN chemin au sol -> forcer 100% volants ===
    if (CachedPaths.Num() == 0 && SpawnZones.Num() > 0)
    {
        FlyingRatio = 1.0f;
        UE_LOG(LogTemp, Warning, TEXT("BuildTerrainMap: AUCUN chemin au sol praticable -> FlyingRatio FORCE a 100%%"));
    }
    
    UE_LOG(LogTemp, Warning, TEXT("=== TERRAIN MAP COMPLETE: FlyingRatio final = %.0f%% ==="), FlyingRatio * 100.0f);
    bTerrainMapReady = true;
}

// === API PUBLIQUE: chemins pour les ennemis ===

TArray<FVector> ATDCreatureSpawner::GetPathTo(const FVector& From, const FVector& To)
{
    if (!bTerrainMapReady) return TArray<FVector>();
    return FindPathOnGrid(From, To);
}

TArray<FVector> ATDCreatureSpawner::GetPathToNearestBuilding(const FVector& From, AActor*& OutTarget)
{
    OutTarget = nullptr;
    if (!bTerrainMapReady) return TArray<FVector>();
    
    // Chercher le batiment le plus proche sur la grille qui est accessible
    FIntPoint FromCell = WorldToGrid(From);
    float BestDist = MAX_FLT;
    AActor* BestBuilding = nullptr;
    
    int32 TotalCells = GridWidth * GridHeight;
    for (int32 Idx = 0; Idx < TotalCells; Idx++)
    {
        if (!TerrainCellBuilding[Idx].IsValid()) continue;
        AActor* B = TerrainCellBuilding[Idx].Get();
        if (IsBuildingDestroyed(B)) continue;
        
        float Dist = FVector::Dist(From, B->GetActorLocation());
        if (Dist < BestDist)
        {
            BestDist = Dist;
            BestBuilding = B;
        }
    }
    
    if (!BestBuilding) return TArray<FVector>();
    
    OutTarget = BestBuilding;
    return FindPathOnGrid(From, BestBuilding->GetActorLocation());
}

void ATDCreatureSpawner::RefreshTerrainAroundBuilding(const FVector& BuildingLocation)
{
    if (!bTerrainMapReady) return;
    
    FIntPoint Center = WorldToGrid(BuildingLocation);
    int32 Radius = 3;  // 3 cellules = 6m autour du batiment detruit
    
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    
    float AvgZ = GridOrigin.Z;
    
    // Re-scanner les cellules autour du batiment detruit
    for (int32 DY = -Radius; DY <= Radius; DY++)
    {
        for (int32 DX = -Radius; DX <= Radius; DX++)
        {
            int32 X = Center.X + DX;
            int32 Y = Center.Y + DY;
            if (!IsValidCell(X, Y)) continue;
            
            int32 Idx = Y * GridWidth + X;
            
            // Nettoyer la reference au batiment detruit
            if (TerrainCellBuilding[Idx].IsValid() && IsBuildingDestroyed(TerrainCellBuilding[Idx].Get()))
            {
                TerrainCellBuilding[Idx] = nullptr;
            }
            
            // Re-raycast pour obtenir la hauteur du sol (le batiment n'est plus la)
            FVector WorldPos = GridToWorld(X, Y);
            FVector RayStart = FVector(WorldPos.X, WorldPos.Y, AvgZ + 2000.0f);
            FVector RayEnd = FVector(WorldPos.X, WorldPos.Y, AvgZ - 2000.0f);
            FHitResult Hit;
            
            if (GetWorld()->LineTraceSingleByChannel(Hit, RayStart, RayEnd, ECC_WorldStatic, Params))
            {
                TerrainGroundZ[Idx] = Hit.ImpactPoint.Z;
            }
            else
            {
                TerrainGroundZ[Idx] = -99999.0f;
            }
        }
    }
    
    // Recalculer la praticabilite des cellules modifiees
    static const int32 NDX[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    static const int32 NDY[] = {-1, -1, -1, 0, 0, 1, 1, 1};
    
    for (int32 DY = -Radius - 1; DY <= Radius + 1; DY++)
    {
        for (int32 DX = -Radius - 1; DX <= Radius + 1; DX++)
        {
            int32 X = Center.X + DX;
            int32 Y = Center.Y + DY;
            if (!IsValidCell(X, Y)) continue;
            
            int32 Idx = Y * GridWidth + X;
            float GZ = TerrainGroundZ[Idx];
            if (GZ < -90000.0f) { TerrainWalkable[Idx] = false; continue; }
            
            int32 WalkableNeighbors = 0;
            for (int32 d = 0; d < 8; d++)
            {
                int32 NX = X + NDX[d];
                int32 NY = Y + NDY[d];
                if (!IsValidCell(NX, NY)) continue;
                float NZ = TerrainGroundZ[NY * GridWidth + NX];
                if (NZ > -90000.0f && FMath::Abs(NZ - GZ) <= 80.0f) WalkableNeighbors++;
            }
            TerrainWalkable[Idx] = (WalkableNeighbors >= 2);
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("RefreshTerrainAroundBuilding: grille mise a jour autour de (%.0f, %.0f)"), BuildingLocation.X, BuildingLocation.Y);
}

TArray<FVector> ATDCreatureSpawner::FindPathOnGrid(const FVector& Start, const FVector& End)
{
    TArray<FVector> Result;
    
    FIntPoint StartCell = WorldToGrid(Start);
    FIntPoint EndCell = WorldToGrid(End);
    
    if (!IsValidCell(StartCell.X, StartCell.Y) || !IsValidCell(EndCell.X, EndCell.Y))
        return Result;
    
    int32 StartIdx = StartCell.Y * GridWidth + StartCell.X;
    int32 EndIdx = EndCell.Y * GridWidth + EndCell.X;
    int32 TotalCells = GridWidth * GridHeight;
    
    // BFS avec tableau CameFrom
    TArray<int32> CameFrom;
    CameFrom.SetNum(TotalCells);
    for (int32& v : CameFrom) v = -1;
    
    CameFrom[StartIdx] = StartIdx;  // Marquer le depart
    
    // Queue simple par tableau + index
    TArray<int32> Queue;
    Queue.Reserve(TotalCells / 4);
    Queue.Add(StartIdx);
    int32 Head = 0;
    
    static const int32 DX[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    static const int32 DY[] = {-1, -1, -1, 0, 0, 1, 1, 1};
    
    bool bFound = false;
    
    while (Head < Queue.Num())
    {
        int32 Current = Queue[Head++];
        
        if (Current == EndIdx) { bFound = true; break; }
        
        int32 CX = Current % GridWidth;
        int32 CY = Current / GridWidth;
        float CZ = TerrainGroundZ[Current];
        
        for (int32 d = 0; d < 8; d++)
        {
            int32 NX = CX + DX[d];
            int32 NY = CY + DY[d];
            if (!IsValidCell(NX, NY)) continue;
            
            int32 NIdx = NY * GridWidth + NX;
            if (CameFrom[NIdx] != -1) continue;
            if (!TerrainWalkable[NIdx]) continue;
            
            float NZ = TerrainGroundZ[NIdx];
            // Peut monter max 80u (rampes) et descendre max 300u
            if (NZ - CZ > 80.0f) continue;
            if (CZ - NZ > 300.0f) continue;
            
            // Verifier la connexion horizontale
            int64 ConnKey = (int64)FMath::Min(Current, NIdx) * 100000 + FMath::Max(Current, NIdx);
            FGridBlockage* Blockage = BlockedConnections.Find(ConnKey);
            if (Blockage && Blockage->bIsTerrain) continue;  // Terrain = infranchissable
            // Batiment bloquant = traversable (l'ennemi le cassera)
            
            CameFrom[NIdx] = Current;
            Queue.Add(NIdx);
        }
    }
    
    if (!bFound)
    {
        // Pas de chemin trouve - essayer la cellule praticable la plus proche de End
        float BestDist = MAX_FLT;
        int32 BestIdx = -1;
        for (int32 i = 0; i < Queue.Num(); i++)
        {
            int32 Idx = Queue[i];
            int32 IX = Idx % GridWidth;
            int32 IY = Idx / GridWidth;
            float Dist = FMath::Abs(IX - EndCell.X) + FMath::Abs(IY - EndCell.Y);
            if (Dist < BestDist)
            {
                BestDist = Dist;
                BestIdx = Idx;
            }
        }
        if (BestIdx >= 0)
        {
            EndIdx = BestIdx;
            bFound = true;
        }
    }
    
    if (!bFound) return Result;
    
    // Reconstruire le chemin
    TArray<FVector> FullPath;
    int32 Current = EndIdx;
    int32 Safety = 0;
    while (Current != StartIdx && Safety < 10000)
    {
        int32 CX = Current % GridWidth;
        int32 CY = Current / GridWidth;
        FVector WP = GridToWorld(CX, CY);
        WP.Z = TerrainGroundZ[Current] + 50.0f;  // 50u au-dessus du sol
        FullPath.Insert(WP, 0);
        Current = CameFrom[Current];
        Safety++;
    }
    
    // Simplifier: garder 1 waypoint tous les 5 pour eviter un chemin trop detaille
    for (int32 i = 0; i < FullPath.Num(); i += 5)
    {
        Result.Add(FullPath[i]);
    }
    // Toujours inclure le dernier point
    if (FullPath.Num() > 0 && (FullPath.Num() - 1) % 5 != 0)
    {
        Result.Add(FullPath.Last());
    }
    
    return Result;
}
#endif // ANCIEN CODE DESACTIVE
