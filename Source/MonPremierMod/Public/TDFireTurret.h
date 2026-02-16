#pragma once

#include "CoreMinimal.h"
#include "Buildables/FGBuildable.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundCue.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "FGCharacterPlayer.h"
#include "FGUseableInterface.h"
#include "TDFireTurret.generated.h"

class ATDEnemy;
class ATDEnemyFlying;

UCLASS()
class MONPREMIERMOD_API ATDFireTurret : public AFGBuildable
{
    GENERATED_BODY()

public:
    ATDFireTurret(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void Factory_Tick(float dt) override;

    // IFGUseableInterface
    virtual bool IsUseable_Implementation() const override;
    virtual FText GetLookAtDecription_Implementation(class AFGCharacterPlayer* byCharacter, const FUseState& state) const override;
    virtual void OnUse_Implementation(class AFGCharacterPlayer* byCharacter, const FUseState& state) override;

    // Composants visuels
    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* BaseMesh;

    UPROPERTY(VisibleAnywhere)
    USceneComponent* HeadPivot;

    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* TurretHead;

    // Composants Audio
    UPROPERTY(VisibleAnywhere, Category = "Audio")
    UAudioComponent* FireAudioComponent;

    UPROPERTY(VisibleAnywhere, Category = "Audio")
    UAudioComponent* OneShotAudioComponent;

    UPROPERTY(VisibleAnywhere, Category = "Audio")
    UAudioComponent* ShootAudioComponent;

    UPROPERTY(EditAnywhere, Category = "Audio")
    USoundBase* BulletFireSound;

    UPROPERTY(EditAnywhere, Category = "Audio")
    USoundBase* LockOnSound;

    UPROPERTY(EditAnywhere, Category = "Audio")
    USoundBase* IdleSound;

    // Effets visuels
    UPROPERTY(VisibleAnywhere, Category = "VFX")
    UParticleSystemComponent* MuzzleFlashComponent;

    UPROPERTY(EditAnywhere, Category = "VFX")
    UParticleSystem* ImpactParticle;

    UPROPERTY(VisibleAnywhere, Category = "VFX")
    UStaticMeshComponent* BulletTracers[10];

    UPROPERTY(VisibleAnywhere, Category = "VFX")
    UNiagaraComponent* FlameEffect;

    UPROPERTY()
    UNiagaraSystem* FlameNiagaraAsset;

    UPROPERTY()
    UMaterialInstanceDynamic* TracerMaterial;

    // Stats de la tourelle
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
    float DamagePerBullet = 4.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
    float Range = 4500.0f;  // 45 metres (5m de plus que laser)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
    float FireRate = 5.0f;  // Balles par seconde

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
    float RotationSpeed = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
    int32 Level = 1;

    // Munitions
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
    int32 CurrentAmmo = 200;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
    int32 MaxAmmo = 200;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ammo")
    bool bHasAmmo = true;

    UPROPERTY()
    TSubclassOf<class UFGItemDescriptor> AmmoItemClass;

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
    void Reload(int32 AmmoAmount);

    UFUNCTION(BlueprintCallable, Category = "Turret")
    int32 GetCurrentAmmo() const { return CurrentAmmo; }

private:
    // Ciblage
    AActor* FindBestTarget();
    bool IsEnemyDead(AActor* Enemy);
    void DealDamageToEnemy(AActor* Enemy, float DmgAmount);
    void RotateTowardsTarget(float DeltaTime);
    void FireBullet();
    void StopFiring();

    // Timer pour le tir
    float FireTimer = 0.0f;
    float FireInterval = 0.2f;  // 1/FireRate

    // Delai avant tir apres verrouillage
    float LockOnTimer = 0.0f;
    float LockOnDelay = 0.5f;  // Plus rapide que le laser
    bool bIsLockedOn = false;

    // Animation idle
    float IdleTimer = 0.0f;
    float IdleLookDirection = 1.0f;
    float IdleYawOffset = 0.0f;
    void UpdateIdleAnimation(float DeltaTime);

    bool bIsFiring = false;
    
    // PERF: throttle target scan
    float TargetScanTimer = 0.0f;
    float TargetScanInterval = 0.3f;

    // Bullet tracers
    FTimerHandle TracerTimerHandle;
    void HideBulletTracers();

    // Flame effect
    FTimerHandle FlameTimerHandle;
    void StopFlameEffect();
};
