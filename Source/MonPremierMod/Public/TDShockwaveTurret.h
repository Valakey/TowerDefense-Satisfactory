#pragma once

#include "CoreMinimal.h"
#include "Buildables/FGBuildable.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundCue.h"
#include "FGPowerConnectionComponent.h"
#include "FGPowerInfoComponent.h"
#include "TDShockwaveTurret.generated.h"

class ATDEnemy;
class ATDEnemyFlying;

UCLASS()
class MONPREMIERMOD_API ATDShockwaveTurret : public AFGBuildable
{
    GENERATED_BODY()

public:
    ATDShockwaveTurret(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void Factory_Tick(float dt) override;

    // Composants visuels
    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* BaseMesh;

    UPROPERTY(VisibleAnywhere)
    USceneComponent* HammerPivot;

    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* HammerHead;

    // Audio
    UPROPERTY(VisibleAnywhere, Category = "Audio")
    UAudioComponent* ImpactAudioComponent;

    UPROPERTY(EditAnywhere, Category = "Audio")
    USoundBase* ImpactSound;

    UPROPERTY(VisibleAnywhere, Category = "Audio")
    UAudioComponent* RisingAudioComponent;

    UPROPERTY(EditAnywhere, Category = "Audio")
    USoundBase* RisingSound;

    UPROPERTY(VisibleAnywhere, Category = "Audio")
    UAudioComponent* DropAudioComponent;

    UPROPERTY(EditAnywhere, Category = "Audio")
    USoundBase* DropSound;

    // Electricite
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Power")
    UFGPowerConnectionComponent* PowerConnection;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Power")
    UFGPowerInfoComponent* PowerInfo;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Power")
    float PowerConsumption = 5.0f;  // MW (meme que laser turret)

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Power")
    bool bHasPower = false;

    // Stats
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shockwave")
    float ShockwaveRadius = 2000.0f;  // 20m

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shockwave")
    float SlowDuration = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shockwave")
    float SlowFactor = 0.3f;  // 70% slow

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shockwave")
    float RiseTime = 5.0f;  // Temps pour remonter

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shockwave")
    float DropTime = 0.3f;  // Temps pour tomber (rapide)

    // Sante
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
    float Health = 600.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
    float MaxHealth = 600.0f;

    UFUNCTION(BlueprintCallable, Category = "Turret")
    void TakeDamageCustom(float DamageAmount);

private:
    // Etats du marteau
    enum class EHammerState : uint8
    {
        Idle,       // En haut, attend
        Dropping,   // En train de tomber
        Impact,     // Vient de toucher le sol
        Rising      // En train de remonter
    };

    EHammerState HammerState = EHammerState::Idle;

    // Animation
    float HammerTimer = 0.0f;
    float HammerTopZ = 250.0f;    // Position haute du marteau (relative)
    float HammerBottomZ = 160.0f;   // Position basse (impact)
    float CurrentHammerZ = 250.0f;

    // Cooldown avant prochain slam
    float IdleCooldown = 1.0f;  // Attente en haut avant de retomber
    float IdleTimer = 0.0f;

    // Detection ennemis
    bool HasEnemiesInRange();
    void ApplyShockwave();
    void UpdateHammerPosition();
    
    // PERF: throttle enemy detection
    float EnemyScanTimer = 0.0f;
    bool bCachedEnemiesInRange = false;

    // Electricite
    void CheckPowerStatus();

    // Idle animation (rotation lente de la base quand pas d'ennemis)
    float IdleRotTimer = 0.0f;
};
