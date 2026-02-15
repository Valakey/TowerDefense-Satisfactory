#pragma once

#include "CoreMinimal.h"
#include "Equipment/FGEquipment.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TDTurretPlacer.generated.h"

class ATDTurret;
class UStaticMeshComponent;

UCLASS()
class MONPREMIERMOD_API ATDTurretPlacer : public AFGEquipment
{
    GENERATED_BODY()

public:
    ATDTurretPlacer();

    virtual void Tick(float DeltaTime) override;
    virtual void Equip(class AFGCharacterPlayer* character) override;
    virtual void UnEquip() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret Placer")
    float PlaceDistance = 2000.0f;

protected:
    virtual void HandleDefaultEquipmentActionEvent(EDefaultEquipmentAction action, EDefaultEquipmentActionEvent actionEvent) override;

private:
    void PlaceTurret();
    bool TraceForPlacement(FVector& OutLocation, FRotator& OutRotation) const;

    UPROPERTY()
    USceneComponent* GhostRoot;

    UPROPERTY()
    UStaticMeshComponent* GhostBase;

    UPROPERTY()
    UStaticMeshComponent* GhostHead;

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
