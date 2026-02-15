#pragma once

#include "CoreMinimal.h"
#include "Equipment/FGEquipment.h"
#include "TDDronePlatformPlacer.generated.h"

class ATDDronePlatform;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UCLASS()
class MONPREMIERMOD_API ATDDronePlatformPlacer : public AFGEquipment
{
    GENERATED_BODY()

public:
    ATDDronePlatformPlacer();

    virtual void Tick(float DeltaTime) override;
    virtual void Equip(class AFGCharacterPlayer* character) override;
    virtual void UnEquip() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform Placer")
    float PlaceDistance = 2000.0f;

protected:
    virtual void HandleDefaultEquipmentActionEvent(EDefaultEquipmentAction action, EDefaultEquipmentActionEvent actionEvent) override;

private:
    void PlacePlatform();
    bool TraceForPlacement(FVector& OutLocation, FRotator& OutRotation) const;

    UPROPERTY()
    USceneComponent* GhostRoot;

    UPROPERTY()
    UStaticMeshComponent* GhostPlatform;

    UPROPERTY()
    UStaticMeshComponent* GhostDrone;

    UPROPERTY()
    UStaticMeshComponent* RangeCircle;

    UPROPERTY()
    UMaterialInterface* RangeBaseMaterial;

    UPROPERTY()
    UMaterialInstanceDynamic* RangeMaterial;

    FVector GhostLocation;
    FRotator GhostRotation;
    bool bValidPlacement;
};
