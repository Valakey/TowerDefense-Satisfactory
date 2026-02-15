#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "TDDropship.generated.h"

class ATDCreatureSpawner;

UENUM(BlueprintType)
enum class EDropshipState : uint8
{
    Descending,     // Descend du ciel
    Hovering,       // En position, spawn des ennemis
    Ascending,      // Remonte vers le ciel
    Done            // Termine
};

UCLASS(Blueprintable)
class MONPREMIERMOD_API ATDDropship : public AActor
{
    GENERATED_BODY()

public:
    ATDDropship();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // Initialiser le dropship avec sa destination et les ennemis a spawner
    void Initialize(const FVector& TargetLocation, const TArray<FVector>& EnemySpawnLocations, ATDCreatureSpawner* Spawner);

    // Etat actuel du dropship
    UPROPERTY(BlueprintReadOnly, Category = "Dropship")
    EDropshipState CurrentState = EDropshipState::Descending;

    // Hauteur de depart (dans le ciel)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dropship")
    float SkyHeight = 10000.0f;

    // Vitesse de descente/montee
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dropship")
    float MoveSpeed = 1500.0f;

    // Hauteur de hover au-dessus du sol
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dropship")
    float HoverHeight = 300.0f;

    // Intervalle entre spawn d'ennemis (secondes)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dropship")
    float SpawnInterval = 0.3f;

protected:
    UPROPERTY(VisibleAnywhere, Category = "Components")
    UStaticMeshComponent* ShipMesh;

    // Composants audio
    UPROPERTY(VisibleAnywhere, Category = "Audio")
    UAudioComponent* MainAudioComponent;

    UPROPERTY(VisibleAnywhere, Category = "Audio")
    UAudioComponent* SpawnAudioComponent;

    // Sons
    UPROPERTY()
    USoundWave* ArrivalSound;

    UPROPERTY()
    USoundWave* HoverSound;

    UPROPERTY()
    USoundWave* SpawnMobSound;

    UPROPERTY()
    USoundWave* DepartureSound;

    // Effet de flammes Niagara
    UPROPERTY(VisibleAnywhere, Category = "Effects")
    UNiagaraComponent* ThrusterFlameEffect;
    
    UPROPERTY()
    UNiagaraSystem* FlameNiagaraSystem;

    // Position cible (au sol)
    FVector TargetGroundLocation;
    
    // Position de hover
    FVector HoverLocation;
    
    // Position de depart (dans le ciel)
    FVector SkyLocation;

    // Ennemis a spawner
    TArray<FVector> PendingSpawns;
    
    // Reference au spawner
    UPROPERTY()
    ATDCreatureSpawner* OwnerSpawner;

    // Timer pour spawn progressif
    float SpawnTimer = 0.0f;

    void UpdateDescending(float DeltaTime);
    void UpdateHovering(float DeltaTime);
    void UpdateAscending(float DeltaTime);
    void SpawnNextEnemy();
};
