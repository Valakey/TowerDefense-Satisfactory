#pragma once

#include "CoreMinimal.h"
#include "Module/GameWorldModule.h"
#include "MonPremierModGameWorldModule.generated.h"

UCLASS(Blueprintable)
class MONPREMIERMOD_API UMonPremierModGameWorldModule : public UGameWorldModule
{
    GENERATED_BODY()

public:
    UMonPremierModGameWorldModule();

    virtual void DispatchLifecycleEvent(ELifecyclePhase Phase) override;
};
