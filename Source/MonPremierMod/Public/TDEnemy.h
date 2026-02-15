#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "TDEnemy.generated.h"

UCLASS()
class MONPREMIERMOD_API ATDEnemy : public ACharacter
{
    GENERATED_BODY()

public:
    ATDEnemy();
    
    // Mesh visible (sphere rouge)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* VisibleMesh;

    // Composant audio pour sons one-shot (attaque, saut, changement cible)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
    UAudioComponent* OneShotAudioComponent;

    // Composant audio pour son de levitation (en boucle)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
    UAudioComponent* LevitationAudioComponent;

    // Sons
    UPROPERTY()
    USoundWave* AttackSound;

    UPROPERTY()
    USoundWave* TargetChangeSound;

    UPROPERTY()
    USoundWave* JumpSound;

    UPROPERTY()
    USoundWave* LevitationSound;


    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    
    // Detection collision mur
    virtual void NotifyHit(class UPrimitiveComponent* MyComp, AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;
    
    // Override TakeDamage pour recevoir les degats du joueur
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, 
        AController* EventInstigator, AActor* DamageCauser) override;

    // Cible a attaquer
    UPROPERTY()
    AActor* TargetBuilding;

    // Vitesse de deplacement
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
    float MoveSpeed = 700.0f;

    // Degats infliges aux batiments
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
    float AttackDamage = 20.0f;

    // Distance d'attaque
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
    float AttackRange = 400.0f;

    // Cooldown entre attaques
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
    float AttackCooldown = 1.0f;

    // Points de vie
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
    float Health = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
    float MaxHealth = 200.0f;

    // Definir la cible
    UFUNCTION(BlueprintCallable, Category = "Enemy")
    void SetTarget(AActor* NewTarget);
    
    // Waypoints du pathfinding terrain (chemin pre-calcule)
    TArray<FVector> Waypoints;
    int32 CurrentWaypointIndex = 0;

    // Activer l'outline rouge
    void EnableOutline();

    // Prendre des degats (pour les tourelles)
    UFUNCTION(BlueprintCallable, Category = "Enemy")
    void TakeDamageCustom(float DamageAmount);

    // Slow effect
    UFUNCTION(BlueprintCallable, Category = "Enemy")
    void ApplySlow(float Duration, float SlowFactor);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
    float SpeedMultiplier = 1.0f;

    float SlowTimer = 0.0f;
    float OriginalMoveSpeed = 0.0f;

    // Etat de mort (public pour les tourelles)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
    bool bIsDead = false;

private:
    float AttackTimer = 0.0f;
    
    // Detection de blocage
    FVector LastPosition = FVector::ZeroVector;
    float StuckTimer = 0.0f;
    int32 ConsecutiveStuckCount = 0;
    
    // Contournement d'obstacle (detour intelligent, PAS random)
    FVector DetourDirection = FVector::ZeroVector;
    float DetourTimer = 0.0f;
    int32 DetourAttempts = 0;  // Nombre de detours tentes pour cette cible
    
    // Verification ligne de vue pour attaque
    int32 FailedAttackAttempts = 0;
    
    // Scan periodique du batiment le plus proche
    float ScanTimer = 0.0f;
    
    // Blacklist temporaire: cibles qu'on n'arrive pas a atteindre
    TArray<TWeakObjectPtr<AActor>> BlacklistedTargets;
    float BlacklistResetTimer = 0.0f;
    
    // Atterrissage: attendre d'etre au sol avant de donner les waypoints
    bool bHasLanded = false;
    
    // Timer autodestruction si aucune cible trouvee (spawn orphelin)
    float NoCibleTimer = 0.0f;
    
    void MoveTowardsTarget(float DeltaTime);
    void AttackTarget();
    void Die();
    void CheckIfStuck(float DeltaTime);
    FVector FindWalkableDetour();  // Scan 8 directions pour trouver chemin autour d'un obstacle
    void FindNearestReachableTarget();
    void MarkTargetUnreachable(AActor* Target);
    bool IsTargetBlacklisted(AActor* Target);
    bool CanAttackTarget();
    bool IsUnderStructure();
    
    // Timer anti-spam collision logs (1 log par seconde max)
    float CollisionLogTimer = 0.0f;
    
    // Detection mur via collision (AbstractInstanceManager = murs instances)
    bool bHitWallRecently = false;
    FVector LastWallHitPoint = FVector::ZeroVector;
    FVector LastWallHitNormal = FVector::ZeroVector;
    
    // Cache spawner (eviter TActorIterator chaque frame)
    TWeakObjectPtr<AActor> CachedSpawner;
    
    // Throttle recherche de cible (max 1x / 3s)
    float RetargetCooldown = 0.0f;
};
