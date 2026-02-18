#pragma once

#include "CoreMinimal.h"
#include "Buildables/FGBuildable.h"
#include "FGPowerConnectionComponent.h"
#include "FGPowerInfoComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "TimerManager.h"
#include "TDLaserFence.generated.h"

// Connexion barriere entre deux pylones
USTRUCT()
struct FBarrierConnection
{
    GENERATED_BODY()

    UPROPERTY()
    TWeakObjectPtr<ATDLaserFence> OtherPylon;

    // Plusieurs barres laser visuelles (au lieu d'un seul mur)
    UPROPERTY()
    TArray<UStaticMeshComponent*> LaserBeams;

    UPROPERTY()
    UBoxComponent* BarrierCollision = nullptr;
};

UCLASS()
class MONPREMIERMOD_API ATDLaserFence : public AFGBuildable
{
    GENERATED_BODY()

public:
    ATDLaserFence(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Factory_Tick(float dt) override;

    // Mesh du pylone
    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* PylonMesh;

    // Colonne collision invisible (monte a BarrierHeight pour boucher le trou au-dessus du pylone)
    UPROPERTY(VisibleAnywhere)
    UBoxComponent* PylonCollisionColumn;

    // Electricite
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Power")
    UFGPowerConnectionComponent* PowerConnection;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Power")
    UFGPowerInfoComponent* PowerInfo;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Power")
    float PowerConsumption = 2.0f;  // MW

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Power")
    bool bHasPower = false;

    // Sante
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fence")
    float Health = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fence")
    float MaxHealth = 300.0f;

    // Config
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fence")
    float ConnectionRange = 800.0f;  // 8m entre pylones

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fence")
    float BarrierHeight = 10000.0f;  // 100m collision - bloque les flying

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fence")
    float BarrierVisualHeight = 350.0f;  // ~3.5m visuel - hauteur du pylone

    // Fonctions publiques
    UFUNCTION(BlueprintCallable, Category = "Fence")
    void TakeDamageCustom(float DamageAmount);

    bool HasPowerInNetwork() const;

    // Registre statique de tous les pylones actifs (thread-safe iteration)
    static TArray<ATDLaserFence*> AllPylons;

    // Breches: positions des pylones detruits (pour que les ennemis contournent)
    static TArray<FVector> BreachPoints;

    // Trouver la breche la plus proche (retourne ZeroVector si aucune)
    static FVector FindNearestBreach(const FVector& FromLocation, float MaxDistance = 5000.0f);

    // Connexions barrieres (public pour que les autres pylones puissent checker)
    UPROPERTY()
    TArray<FBarrierConnection> Barriers;

private:
    void ScanForNearbyPylons();
    void CreateBarrierTo(ATDLaserFence* OtherPylon);
    void RemoveBarrierTo(ATDLaserFence* OtherPylon);
    void RemoveAllBarriers();
    void UpdateBarrierState();
    void CheckPowerStatus();
    void Die();
    bool IsAlreadyConnectedTo(ATDLaserFence* Other) const;

    bool bPlayerIgnoreSet = false;

    // Timer game-thread pour scan + creation barrieres
    FTimerHandle GameThreadTimerHandle;
    void GameThreadUpdate();

    // Material holographique (meme que shield dome)
    UPROPERTY()
    UMaterialInterface* BarrierMaterial;

    // Mesh cube pour les barrieres visuelles
    UPROPERTY()
    UStaticMesh* CubeMesh;

    // Overlap callback pour repousser les ennemis
    UFUNCTION()
    void OnBarrierBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
