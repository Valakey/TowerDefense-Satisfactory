#pragma once

#include "CoreMinimal.h"
#include "Equipment/FGEquipment.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TDShieldGeneratorPlacer.generated.h"

class ATDShieldGenerator;
class UStaticMeshComponent;

UCLASS()
class MONPREMIERMOD_API ATDShieldGeneratorPlacer : public AFGEquipment
{
    GENERATED_BODY()

public:
    ATDShieldGeneratorPlacer();

    virtual void Tick(float DeltaTime) override;
    virtual void Equip(class AFGCharacterPlayer* character) override;
    virtual void UnEquip() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield Placer")
    float PlaceDistance = 2000.0f;

protected:
    virtual void HandleDefaultEquipmentActionEvent(EDefaultEquipmentAction action, EDefaultEquipmentActionEvent actionEvent) override;

private:
    void PlaceShieldGenerator();
    bool TraceForPlacement(FVector& OutLocation, FRotator& OutRotation) const;

    UPROPERTY()
    USceneComponent* GhostRoot;

    UPROPERTY()
    UStaticMeshComponent* GhostBase;

    UPROPERTY()
    UStaticMeshComponent* GhostDome;

    UPROPERTY()
    UMaterialInterface* RangeBaseMaterial;

    UPROPERTY()
    UMaterialInstanceDynamic* RangeMaterial;

    FVector GhostLocation;
    FRotator GhostRotation;
    bool bValidPlacement;
};
