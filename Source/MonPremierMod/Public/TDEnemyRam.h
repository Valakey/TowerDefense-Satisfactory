#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Sound/SoundWave.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "TDEnemyRam.generated.h"

UENUM(BlueprintType)
enum class ERamState : uint8
{
    Roaming,
    Approaching,
    Preparing,
    Charging,
    Stunned,
    Falling
};

UCLASS()
class MONPREMIERMOD_API ATDEnemyRam : public APawn
{
    GENERATED_BODY()

public:
    ATDEnemyRam();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // Collision
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USphereComponent* CollisionSphere;

    // Mesh visible (corps du belier)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* VisibleMesh;

    // Effet Niagara propulseur (actif pendant la charge)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Effects")
    UNiagaraComponent* ThrusterEffect;

    UPROPERTY()
    UNiagaraSystem* FlameNiagaraSystem;

    // Effet stun (3 etoiles tournantes)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Effects")
    UStaticMeshComponent* StunStar1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Effects")
    UStaticMeshComponent* StunStar2;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Effects")
    UStaticMeshComponent* StunStar3;

    // Audio
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
    UAudioComponent* OneShotAudioComponent;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
    UAudioComponent* HoverAudioComponent;

    UPROPERTY()
    USoundWave* ChargeSound;
    UPROPERTY()
    USoundWave* ImpactSound;
    UPROPERTY()
    USoundWave* HoverSound;
    UPROPERTY()
    USoundWave* StunSound;
    UPROPERTY()
    USoundWave* TargetLockSound;

    // Cible a charger
    UPROPERTY()
    AActor* TargetBuilding;

    // Vitesse de deplacement (lent)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ram")
    float MoveSpeed = 300.0f;

    // Vitesse de charge (tres rapide)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ram")
    float ChargeSpeed = 2500.0f;

    // Degats de charge (enormes)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ram")
    float ChargeDamage = 1000.0f;

    // Reduction de degats si slow (30%)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ram")
    float SlowDamageReduction = 0.3f;

    // Range de detection des batiments (50m)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ram")
    float DetectionRange = 5000.0f;

    // Distance minimum pour charger (20m)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ram")
    float MinChargeDistance = 2000.0f;

    // Temps de preparation avant charge
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ram")
    float PrepareTime = 3.0f;

    // Duree du stun apres impact
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ram")
    float StunDuration = 10.0f;

    // PV
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ram")
    float Health = 1000.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ram")
    float MaxHealth = 1000.0f;

    UPROPERTY()
    float SpeedMultiplier = 1.0f;

    UPROPERTY()
    bool bIsDead = false;

    UPROPERTY()
    AActor* PrimaryTarget;

    // Definir la cible
    UFUNCTION(BlueprintCallable, Category = "Ram")
    void SetTarget(AActor* NewTarget);
    
    // Waypoints du pathfinding terrain
    TArray<FVector> Waypoints;
    int32 CurrentWaypointIndex = 0;
    
    // Atterrissage: attendre d'etre au sol avant de donner les waypoints
    bool bHasLanded = false;

    // Prendre des degats (pour les tourelles)
    UFUNCTION(BlueprintCallable, Category = "Ram")
    void TakeDamageCustom(float DamageAmount);

    // Slow effect
    UFUNCTION(BlueprintCallable, Category = "Ram")
    void ApplySlow(float Duration, float SlowFactor);

    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
        AController* EventInstigator, AActor* DamageCauser) override;

protected:
    ERamState CurrentState = ERamState::Roaming;

    float BobTimer = 0.0f;
    float BobFrequency = 1.0f;
    float BobAmplitude = 5.0f;
    float BaseHeight = 0.0f;
    float PrepareTimer = 0.0f;
    float StunTimer = 0.0f;
    float StunStarAngle = 0.0f;
    FVector ChargeDirection;
    FVector ChargeStartLocation;
    float ChargeDistanceTraveled = 0.0f;
    float MaxChargeDistance = 3000.0f;
    float AttackRange = 300.0f;
    float NoAttackTimer = 0.0f;
    float TeleportTimeout = 50.0f;
    bool bIsBreakingWall = false;
    float OriginalMoveSpeed = 300.0f;
    float SlowTimer = 0.0f;

    void UpdateRoaming(float DeltaTime);
    void UpdateApproaching(float DeltaTime);
    void UpdatePreparing(float DeltaTime);
    void UpdateCharging(float DeltaTime);
    void UpdateStunned(float DeltaTime);
    void UpdateFalling(float DeltaTime);
    void UpdateStunStars(float DeltaTime);
    void ShowStunStars(bool bShow);
    void TeleportToBase();
    void Die();

    AActor* FindBuildingInRange();
};
