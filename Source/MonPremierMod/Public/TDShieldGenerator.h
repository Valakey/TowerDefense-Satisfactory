#pragma once

#include "CoreMinimal.h"
#include "Buildables/FGBuildable.h"
#include "FGPowerConnectionComponent.h"
#include "FGPowerInfoComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/AudioComponent.h"
#include "TDShieldGenerator.generated.h"

UENUM()
enum class ETDShieldState : uint8
{
    Idle,       // Pas de courant
    Charging,   // Anneaux apparaissent 1/sec du bas vers le haut
    Ready,      // 10 anneaux charges, pret a tirer
    Firing      // Arc + dome visibles brievement
};

UCLASS()
class MONPREMIERMOD_API ATDShieldGenerator : public AFGBuildable
{
    GENERATED_BODY()

public:
    ATDShieldGenerator(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void Factory_Tick(float dt) override;

    // Composants visuels
    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* BaseMesh;

    UPROPERTY()
    TArray<UStaticMeshComponent*> Rings;

    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* ArcMesh;

    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* ShieldDomeMesh;

    UPROPERTY(VisibleAnywhere)
    USphereComponent* ShieldSphere;

    // Electricite
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Power")
    UFGPowerConnectionComponent* PowerConnection;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Power")
    UFGPowerInfoComponent* PowerInfo;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Power")
    float PowerConsumption = 10.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Power")
    bool bHasPower = false;

    // Config
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield")
    float ShieldRadius = 2000.0f;

    // Etat
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shield")
    ETDShieldState ShieldState = ETDShieldState::Idle;

    // Appele depuis DamageBuilding - retourne true si degat bloque
    bool TryProtectBuilding(AActor* Building);
    bool IsActorInRange(AActor* Actor) const;
    bool HasPower() const { return bHasPower; }

private:
    static const int32 NUM_RINGS = 10;
    static const float RING_BASE_Z;
    static const float RING_SPACING;
    static const float CHARGE_TIME_PER_RING;
    static const float FIRING_VISUAL_DURATION;

    int32 CurrentChargedRings = 0;
    float ChargeTimer = 0.0f;
    float FiringTimer = 0.0f;
    TWeakObjectPtr<AActor> ProtectedBuilding;

    // Coordination multi-towers: 1 shield par batiment
    static TSet<uint32> ShieldedBuildingIDs;

    // Sons
    UPROPERTY()
    USoundBase* ChargeSound;

    UPROPERTY()
    USoundBase* BeamSound;

    UPROPERTY(VisibleAnywhere)
    UAudioComponent* ChargeAudioComponent;

    UPROPERTY(VisibleAnywhere)
    UAudioComponent* BeamAudioComponent;

    void CheckPowerStatus();
    void HideAllRings();
    void ShowRing(int32 Index);
    void FireShield(AActor* Building);
    void StopFiring();
    void UpdateArcVisual();
    void ShowDome(AActor* Target);
    void HideDome();
};
