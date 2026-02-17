#pragma once

#include "CoreMinimal.h"
#include "Equipment/FGEquipment.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TDLaserFencePlacer.generated.h"

class ATDLaserFence;
class UStaticMeshComponent;

UCLASS()
class MONPREMIERMOD_API ATDLaserFencePlacer : public AFGEquipment
{
    GENERATED_BODY()

public:
    ATDLaserFencePlacer();

    virtual void Tick(float DeltaTime) override;
    virtual void Equip(class AFGCharacterPlayer* character) override;
    virtual void UnEquip() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fence Placer")
    float PlaceDistance = 2000.0f;

protected:
    virtual void HandleDefaultEquipmentActionEvent(EDefaultEquipmentAction action, EDefaultEquipmentActionEvent actionEvent) override;

private:
    void PlaceFence();
    bool TraceForPlacement(FVector& OutLocation, FRotator& OutRotation) const;

    UPROPERTY()
    USceneComponent* GhostRoot;

    UPROPERTY()
    UStaticMeshComponent* GhostPylon;

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
