#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sound/SoundWave.h"
#include "TDCreatureSpawner.generated.h"

class STDWaveHUD;
class ATDDropship;
class ATDEnemyFlying;

// === GRILLE 3D VOXEL PAR BASE ===
// Chaque base a sa propre sphere, ses voxels, ses chemins
// Voxel: 0=air, 1=terrain solide, 2=batiment
struct FBaseGrid
{
    // Identification
    int32 ZoneIndex = -1;
    FVector Center = FVector::ZeroVector;
    float Radius = 0.0f;  // Rayon de la sphere (jusqu'aux spawners)
    
    // Grille 3D
    FVector Origin = FVector::ZeroVector;  // Coin min (X,Y,Z)
    int32 W = 0, H = 0, D = 0;            // Dimensions en cellules
    float CellSize = 100.0f;              // Taille horizontale (1m)
    float VoxelH = 200.0f;               // Taille verticale (2m)
    float MinZ = 0.0f;                    // Z du bas
    TArray<uint8> Voxels;                 // 0=air, 1=terrain, 2=batiment
    
    // Sol derive des voxels (par colonne X,Y)
    TArray<float> GroundZ;     // Z du sol pour chaque colonne (-99999 si pas de sol)
    TArray<bool> Walkable;     // Cellule praticable au sol?
    
    // Chemins pre-calcules par spawn zone
    TMap<int32, TArray<FVector>> GroundPaths;   // Ennemis terrestres
    TMap<int32, TArray<FVector>> FlyPaths;      // Ennemis volants
    
    // Ratio volants pour cette base
    float FlyingRatio = 0.3f;
    
    // Zones de spawn et batiments de cette base
    TArray<FVector> SpawnZones;
    TArray<int32> SpawnZoneIndices;  // Index globaux des spawn zones
    TArray<TWeakObjectPtr<AActor>> Buildings;
    
    // Helpers inline
    int32 VoxelIdx(int32 X, int32 Y, int32 Z) const { return Z * W * H + Y * W + X; }
    int32 ColIdx(int32 X, int32 Y) const { return Y * W + X; }
    bool IsValid3D(int32 X, int32 Y, int32 Z) const { return X >= 0 && X < W && Y >= 0 && Y < H && Z >= 0 && Z < D; }
    bool IsValid2D(int32 X, int32 Y) const { return X >= 0 && X < W && Y >= 0 && Y < H; }
    
    FIntPoint WorldToCell(const FVector& Pos) const
    {
        return FIntPoint(
            FMath::FloorToInt((Pos.X - Origin.X) / CellSize),
            FMath::FloorToInt((Pos.Y - Origin.Y) / CellSize)
        );
    }
    
    int32 WorldToZ(float Z) const
    {
        return FMath::FloorToInt((Z - MinZ) / VoxelH);
    }
    
    FVector CellToWorld(int32 X, int32 Y, int32 Z) const
    {
        return FVector(
            Origin.X + X * CellSize + CellSize * 0.5f,
            Origin.Y + Y * CellSize + CellSize * 0.5f,
            MinZ + Z * VoxelH + VoxelH * 0.5f
        );
    }
    
    bool IsInSphere(const FVector& Pos) const
    {
        return FVector::Dist(Pos, Center) <= Radius;
    }
};

USTRUCT()
struct FAttackPath
{
    GENERATED_BODY()
    
    UPROPERTY()
    AActor* Target = nullptr;
    
    FVector TargetLocation = FVector::ZeroVector;
    int32 Priority = 0;  // 1=turret, 2=machine, 3=structure
    bool bIsEnclosed = false;
    
    UPROPERTY()
    AActor* WallToBreak = nullptr;
    
    FVector BestApproachDir = FVector::ZeroVector;
    int32 ZoneIndex = -1;
    FVector ZoneCenter = FVector::ZeroVector;
    float ZoneRadius = 0.0f;
};

UCLASS(Blueprintable)
class MONPREMIERMOD_API ATDCreatureSpawner : public AActor
{
    GENERATED_BODY()

public:
    ATDCreatureSpawner();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // Rayon de spawn autour des batiments
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tower Defense")
    float SpawnRadius = 2000.0f;

    // Distance min des batiments
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tower Defense")
    float MinDistanceFromBuilding = 500.0f;

    // Max creatures actives (20 par zone x nombre de zones)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tower Defense")
    int32 MaxActiveCreatures = 500;

    // Temps entre vagues (secondes) - 2min30
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tower Defense")
    float TimeBetweenWaves = 150.0f;

    // Nombre max de vagues par nuit
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tower Defense")
    int32 MaxWaves = 3;

    // Multiplicateur de mobs par batiment selon la vague (vague1=4, vague2=8, vague3=12)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tower Defense")
    int32 BaseMobsPerBuilding = 2;

    // Vague actuelle (1-3)
    UPROPERTY(BlueprintReadOnly, Category = "Tower Defense")
    int32 CurrentWave = 0;

    // Toutes les vagues de la nuit sont terminees
    UPROPERTY(BlueprintReadOnly, Category = "Tower Defense")
    bool bNightWavesComplete = false;

    // Classe de creature a spawner (configurable en Blueprint)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tower Defense")
    TSubclassOf<AActor> CreatureClass;

protected:
    float WaveTimer = 0.0f;

    // === CYCLE JOUR/NUIT ===
    bool bWasNight = false;              // Etat precedent pour detecter transition
    bool bWaveTriggeredThisNight = false; // Eviter de relancer les vagues 2x meme nuit
    bool bDayCleanupDone = false;        // Nettoyage mobs fait ce jour
    float DayCheckTimer = 0.0f;          // Timer pour verifier jour/nuit
    
    // IMPORTANT: UPROPERTY pour garder une reference forte (sinon GC detruit les creatures)
    UPROPERTY()
    TArray<AActor*> ActiveCreatures;

    // Spawn progressif
    TArray<FVector> PendingSpawnLocations;
    FTimerHandle SpawnTimerHandle;
    int32 CurrentWaveCreatureCount = 0;
    float SpawnInterval = 0.15f;  // 150ms entre chaque spawn

    // Widget HUD permanent (Slate)
    TSharedPtr<STDWaveHUD> WaveHUD;
    
    // Timer pour mise a jour du compteur (1 seconde)
    float HUDUpdateTimer = 0.0f;
    float HUDUpdateInterval = 1.0f;

    // Son de message de vague
    UPROPERTY()
    USoundWave* WaveMessageSound;

    void SpawnWave();
    void CreateWaveHUD();
    void UpdateHUDMobCounter();
    int32 CountBuildings();
    FVector GetSpawnLocation();
    FVector GetSpawnLocationNearPoint(const FVector& Center);
    void CleanupDeadCreatures();
    AActor* FindNearestBuilding(const FVector& FromLocation);
    
    // Nouveau systeme de spawn intelligent
    void CollectAllBuildings(TArray<AActor*>& OutBuildings);
    void CreateSpawnZones(const TArray<AActor*>& Buildings, TArray<FVector>& OutZones);
    
    // Pathfinding par raycasts: verifie si un ennemi au sol peut marcher de A a B
    bool ValidateGroundPath(const FVector& Start, const FVector& End);
    
    // FlyingRatio est maintenant per-base dans FBaseGrid
    // Utiliser GetFlyingRatioFor(Location) pour obtenir le ratio
    
    // === BASE ANALYZER ===
    void AnalyzeBase();
    
    UPROPERTY()
    TArray<FAttackPath> AttackPaths;
    
    UPROPERTY()
    TArray<FVector> AnalyzedZoneCenters;
    
    UPROPERTY()
    TArray<float> AnalyzedZoneRadii;
    
public:
    // Ennemis appellent cette fonction pour obtenir leur plan d'attaque
    FAttackPath GetBestAttackPath(const FVector& FromLocation, bool bCanFly = false);
    
    // Public pour que les Dropships puissent l'appeler
    void SpawnCreatureAt(const FVector& Location);
    void SpawnFlyingCreatureAt(const FVector& Location, AActor* Target);
    void SpawnRamCreatureAt(const FVector& Location, AActor* Target);
    
protected:
    void ShowWaveMessage(int32 WaveNum, int32 CreatureCount);
    bool IsLocationBlocked(const FVector& Location);
    void SpawnNextCreature();
    void DisplayScreenMessage(const FString& Message, FColor Color, float Duration);
    
    // === SYSTEME DE PV DES BATIMENTS ===
public:
    // PV des batiments (AActor* -> PV actuels)
    UPROPERTY()
    TMap<AActor*, float> BuildingHealth;
    
    // PV max des batiments
    UPROPERTY()
    TMap<AActor*, float> BuildingMaxHealth;
    
    // Initialiser les PV d'un batiment selon son type
    float GetBuildingMaxHealth(AActor* Building);
    
    // Infliger des degats a un batiment
    UFUNCTION(BlueprintCallable)
    void DamageBuilding(AActor* Building, float Damage);
    
    // Obtenir les PV actuels d'un batiment
    UFUNCTION(BlueprintCallable)
    float GetBuildingCurrentHealth(AActor* Building);
    
    // === EFFET VISUEL BATIMENTS ATTAQUES ===
protected:
    // Batiments en cours d'attaque avec timer de pulse
    TMap<AActor*, float> AttackedBuildingTimers;
    
    // Materiaux originaux des batiments (pour restaurer)
    TMap<AActor*, TArray<UMaterialInterface*>> OriginalMaterials;
    
    // Duree de l'effet pulse (secondes)
    float AttackPulseDuration = 0.5f;
    
    // Batiments "detruits" (demanteles via IFGDismantleInterface)
    UPROPERTY()
    TArray<AActor*> DestroyedBuildings;
    
public:
    // Verifier si un batiment est marque comme detruit
    UFUNCTION(BlueprintCallable)
    bool IsBuildingDestroyed(AActor* Building) const;
    
    // Marquer un batiment comme attaque (declenche effet visuel)
    void MarkBuildingAttacked(AActor* Building);
    
    // === TERRAIN MAPPING: une grille 3D voxel PAR BASE ===
    TArray<FBaseGrid> BaseGrids;
    bool bTerrainMapReady = false;
    bool bGridDirty = true;          // true = rebuild necessaire au prochain spawn
    int32 LastBuildingCount = 0;     // Pour detecter si le joueur a construit
    
    // Construire les grilles 3D pour chaque base
    void BuildAllBaseGrids(const TArray<FVector>& SpawnZones, const TArray<AActor*>& Buildings);
    void BuildSingleBaseGrid(FBaseGrid& Grid);
    
    // BFS sur une grille specifique
    TArray<FVector> FindGroundPath(const FBaseGrid& Grid, const FVector& Start, const FVector& End);
    TArray<FVector> FindFlyPath(const FBaseGrid& Grid, const FVector& Start, const FVector& End);
    
    // Trouver la bonne base pour une position
    int32 FindNearestBaseGrid(const FVector& Position) const;
    
public:
    // === API PUBLIQUE ===
    TArray<FVector> GetGroundPathFor(const FVector& SpawnLoc, const FVector& TargetLoc);
    TArray<FVector> GetFlyPathFor(const FVector& SpawnLoc, const FVector& TargetLoc);
    float GetFlyingRatioFor(const FVector& SpawnLoc) const;
    void RefreshTerrainAroundBuilding(const FVector& BuildingLocation);
    
    // Obtenir le centre de la base la plus proche (pour ennemis perdus)
    FVector GetNearestBaseCenter(const FVector& Position) const;

protected:
    // Mettre a jour les effets visuels des batiments attaques
    void UpdateAttackedBuildingsVisuals(float DeltaTime);
};
