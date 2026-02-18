#pragma once

#include "CoreMinimal.h"
#include "Buildables/FGBuildable.h"
#include "Components/StaticMeshComponent.h"
#include "FGFactoryConnectionComponent.h"
#include "FGInventoryComponent.h"
#include "TDDronePlatform.generated.h"

class ATDDrone;

UCLASS()
class MONPREMIERMOD_API ATDDronePlatform : public AFGBuildable
{
    GENERATED_BODY()

public:
    ATDDronePlatform(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void Factory_Tick(float dt) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // Composants visuels
    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* PlatformMesh;

    // Conveyor input
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory")
    UFGFactoryConnectionComponent* ConveyorInput;

    // Inventaire (1 slot)
    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    UFGInventoryComponent* PlatformInventory;

    // Drone associe
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Drone")
    ATDDrone* OwnedDrone;

    // Position ou le drone se pose sur la platform
    UFUNCTION(BlueprintPure, Category = "Drone")
    FVector GetDroneLandingLocation() const;

    // Prendre des munitions de l'inventaire
    int32 TakeAmmo(int32 AmountRequested);

    // Verifier si l'inventaire a des items
    bool HasAmmo() const;
    int32 GetAmmoCount() const;

    // Sante
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform")
    float Health = 400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform")
    float MaxHealth = 400.0f;

    UFUNCTION(BlueprintCallable, Category = "Platform")
    void TakeDamageCustom(float DamageAmount);

private:
    void SpawnDrone();
    void GrabFromConveyor();
};
