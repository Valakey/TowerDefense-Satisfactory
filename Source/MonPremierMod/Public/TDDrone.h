#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "TDDrone.generated.h"

class ATDDronePlatform;
class ATDFireTurret;

UCLASS()
class MONPREMIERMOD_API ATDDrone : public AActor
{
    GENERATED_BODY()

public:
    ATDDrone();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // Root scene
    UPROPERTY(VisibleAnywhere)
    USceneComponent* DroneRoot;

    // Mesh du drone
    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* DroneMesh;

    // Reference vers la plateforme proprietaire
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Drone")
    ATDDronePlatform* OwnerPlatform;

    // Stats
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drone")
    float FlySpeed = 250.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drone")
    float DetectionRange = 2000.0f;  // 20m

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drone")
    float DeliveryDistance = 150.0f;  // Distance pour considerer "arrive"

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drone")
    float HoverHeight = 200.0f;  // Hauteur au-dessus de la platform

private:
    // Etats du drone
    enum class EDroneState : uint8
    {
        Idle,             // Sur la platform, attend
        FlyingToTurret,   // En route vers la tourelle
        Delivering,       // Livre les munitions
        Returning         // Retour vers la platform
    };

    EDroneState DroneState = EDroneState::Idle;

    // Cible actuelle
    UPROPERTY()
    ATDFireTurret* TargetTurret;

    // Munitions transportees
    int32 CarriedAmmo = 0;

    // Timers
    float ScanTimer = 0.0f;
    float ScanInterval = 2.0f;  // Scanner toutes les 2 secondes
    float DeliverTimer = 0.0f;
    float DeliverDuration = 1.0f;  // 1 seconde pour livrer

    // Fonctions
    ATDFireTurret* FindTurretNeedingAmmo();
    void FlyTowards(FVector Target, float DeltaTime);
    void PickupAmmoFromPlatform();

    // Animation idle (hover leger)
    float IdleBobTimer = 0.0f;
    FVector BaseIdleLocation;

    // Smooth rotation reset au retour
    bool bResettingRotation = false;
    FRotator ResetTargetRotation;
};
