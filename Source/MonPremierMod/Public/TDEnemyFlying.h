#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TDEnemyFlying.generated.h"

UCLASS()
class MONPREMIERMOD_API ATDEnemyFlying : public APawn
{
    GENERATED_BODY()

public:
    ATDEnemyFlying();

    // Collision
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USphereComponent* CollisionSphere;

    // Mesh visible (corps + oeil)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* VisibleMesh;

    // Laser beam (comme la tourelle mais noir)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* LaserBeam;

    // Effet Niagara reacteur
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Effects")
    UNiagaraComponent* ThrusterEffect;

    UPROPERTY()
    UNiagaraSystem* FlameNiagaraSystem;

    // Audio
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
    UAudioComponent* OneShotAudioComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
    UAudioComponent* FlyingAudioComponent;

    // Sons
    UPROPERTY()
    USoundWave* AttackSound;

    UPROPERTY()
    USoundWave* FlyingSound;

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
        AController* EventInstigator, AActor* DamageCauser) override;

    // Cible a attaquer
    UPROPERTY()
    AActor* TargetBuilding;
    
    // Waypoints du pathfinding 3D (chemin pre-calcule)
    TArray<FVector> Waypoints;
    int32 CurrentWaypointIndex = 0;

    // Vitesse de vol
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
    float FlySpeed = 500.0f;

    // Degats infliges aux batiments
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
    float AttackDamage = 16.0f;

    // Distance d'attaque (10m = 1000 units)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
    float AttackRange = 1000.0f;

    // Cooldown entre attaques
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
    float AttackCooldown = 1.2f;

    // Points de vie (moins que l'ennemi au sol)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
    float Health = 120.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
    float MaxHealth = 120.0f;

    // Hauteur de vol au-dessus de la cible
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
    float FlyHeightOffset = 300.0f;

    // Definir la cible
    UFUNCTION(BlueprintCallable, Category = "Enemy")
    void SetTarget(AActor* NewTarget);

    // Prendre des degats (pour les tourelles)
    UFUNCTION(BlueprintCallable, Category = "Enemy")
    void TakeDamageCustom(float DamageAmount);

    // Slow effect
    UFUNCTION(BlueprintCallable, Category = "Enemy")
    void ApplySlow(float Duration, float SlowFactor);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
    float SpeedMultiplier = 1.0f;

    float SlowTimer = 0.0f;
    float OriginalFlySpeed = 0.0f;

    // Etat de mort
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
    bool bIsDead = false;

private:
    float AttackTimer = 0.0f;

    // Mouvement sinusoidal pour effet de vol
    float BobTimer = 0.0f;
    float BobAmplitude = 30.0f;
    float BobFrequency = 2.0f;

    // Detection de blocage (simplifiee - plus de wall avoidance)
    float StuckTimer = 0.0f;

    // Hologramme: quand le volant traverse un objet, il devient transparent/holographique
    UPROPERTY()
    UMaterialInterface* HologramBaseMaterial = nullptr;
    UPROPERTY()
    UMaterialInstanceDynamic* HologramMaterial = nullptr;
    bool bIsHologramActive = false;
    float HologramTimer = 0.0f;
    float HologramDuration = 0.5f; // Duree hologramme apres sortie d'un objet
    float HologramScanTimer = 0.0f; // Throttle du scan fallback
    int32 OriginalMaterialCount = 0;
    TArray<UMaterialInterface*> OriginalMaterials; // Sauvegarder TOUS les materiaux originaux


    void FlyTowardsTarget(float DeltaTime);
    void RotateEyeTowardsTarget(float DeltaTime);
    void AttackTarget(float DeltaTime);
    void UpdateLaserVisual();
    void StopLaser();
    void Die();

    bool bLaserActive = false;
    float DamageTimer = 0.0f;
    float DamageInterval = 0.15f;  // Degats toutes les 0.15s

    // Materiaux pour blink noir pendant le tir
    UPROPERTY()
    UMaterialInterface* OriginalMaterial = nullptr;
    UPROPERTY()
    UMaterialInstanceDynamic* BlackMaterial = nullptr;
    
    // === PERF: cache spawner + throttle targeting ===
    TWeakObjectPtr<AActor> CachedSpawner;
    float TargetScanTimer = 0.0f;
    float TargetScanInterval = 0.5f;  // Scan toutes les 0.5s au lieu de chaque frame
    bool bBlinkState = false;
    float BlinkTimer = 0.0f;
};
