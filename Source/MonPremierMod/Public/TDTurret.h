#pragma once

#include "CoreMinimal.h"
#include "Buildables/FGBuildable.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundCue.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "FGPowerConnectionComponent.h"
#include "FGPowerInfoComponent.h"
#include "TDTurret.generated.h"

class ATDEnemy;
class ATDEnemyFlying;

UCLASS()
class MONPREMIERMOD_API ATDTurret : public AFGBuildable
{
    GENERATED_BODY()

public:
    ATDTurret(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void Factory_Tick(float dt) override;

    // Composants visuels
    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* BaseMesh;

    UPROPERTY(VisibleAnywhere)
    USceneComponent* HeadPivot;  // Point de pivot fixe pour rotation

    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* TurretHead;

    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* LaserBeam;

    // Composants Audio
    UPROPERTY(VisibleAnywhere, Category = "Audio")
    UAudioComponent* LaserAudioComponent;

    UPROPERTY(VisibleAnywhere, Category = "Audio")
    UAudioComponent* OneShotAudioComponent;  // Pour sons one-shot (LockOn, Charge)

    UPROPERTY(EditAnywhere, Category = "Audio")
    USoundBase* LaserFireSound;

    UPROPERTY(EditAnywhere, Category = "Audio")
    USoundBase* LockOnSound;

    UPROPERTY(EditAnywhere, Category = "Audio")
    USoundBase* LaserChargeSound;  // Son de precharge pendant le delai

    UPROPERTY(EditAnywhere, Category = "Audio")
    USoundBase* IdleSound;

    // Effets visuels
    UPROPERTY(VisibleAnywhere, Category = "VFX")
    UParticleSystemComponent* MuzzleFlashComponent;

    UPROPERTY(EditAnywhere, Category = "VFX")
    UParticleSystem* ImpactParticle;

    // Stats de la tourelle
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
    float Damage = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
    float DamagePerSecond = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
    float Range = 4000.0f;  // 40 metres

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
    float RotationSpeed = 180.0f;  // Degres par seconde

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
    int32 Level = 1;

    // Electricite
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Power")
    UFGPowerConnectionComponent* PowerConnection;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Power")
    UFGPowerInfoComponent* PowerInfo;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Power")
    float PowerConsumption = 5.0f;  // MW (meme qu'une foreuse Mk1)

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Power")
    bool bHasPower = false;

    // Etat
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turret")
    bool bIsActive = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turret")
    AActor* CurrentTarget = nullptr;

    // Sante de la tourelle
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
    float Health = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
    float MaxHealth = 500.0f;

    // Fonctions
    UFUNCTION(BlueprintCallable, Category = "Turret")
    void TakeDamageCustom(float DamageAmount);

    UFUNCTION(BlueprintCallable, Category = "Turret")
    void Upgrade();

    UFUNCTION(BlueprintCallable, Category = "Turret")
    bool HasPower() const { return bHasPower; }

private:
    // Ciblage
    AActor* FindBestTarget();
    bool IsEnemyDead(AActor* Enemy);
    void DealDamageToEnemy(AActor* Enemy, float DmgAmount);
    void RotateTowardsTarget(float DeltaTime);
    void FireLaser(float DeltaTime);
    void UpdateLaserVisual();
    void StopLaser();

    // Timer pour les degats
    float DamageTimer = 0.0f;
    float DamageInterval = 0.1f;  // Degats toutes les 0.1s

    // Delai avant tir apres verrouillage
    float LockOnTimer = 0.0f;
    float LockOnDelay = 1.0f;  // 1 seconde avant de tirer
    bool bIsLockedOn = false;

    // Animation idle
    float IdleTimer = 0.0f;
    float IdleLookDirection = 1.0f;  // 1 = droite, -1 = gauche
    float IdleYawOffset = 0.0f;
    void UpdateIdleAnimation(float DeltaTime);

    // Verifier l'electricite
    void CheckPowerStatus();

    // Gestion visuelle du laser
    FVector LaserEndPoint;
    bool bLaserActive = false;
    
    // Timer debug (par instance, pas static)
    float DebugLogTimer = 0.0f;
    
    // PERF: throttle target scan
    float TargetScanTimer = 0.0f;
    float TargetScanInterval = 0.3f;
};
