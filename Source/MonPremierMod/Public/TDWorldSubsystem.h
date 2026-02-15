#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TDWorldSubsystem.generated.h"

class ATDCreatureSpawner;

UCLASS()
class MONPREMIERMOD_API UTDWorldSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

protected:
    UPROPERTY()
    ATDCreatureSpawner* SpawnerInstance;

    void OnWorldBeginPlay();
};
