#include "TDShockwaveTurret.h"
#include "TDEnemy.h"
#include "TDEnemyFlying.h"
#include "TDEnemyRam.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ATDShockwaveTurret::ATDShockwaveTurret(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = true;
    mFactoryTickFunction.bCanEverTick = true;

    // Base mesh
    BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
    BaseMesh->SetupAttachment(RootComponent);
    BaseMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 380.0f));
    BaseMesh->SetRelativeScale3D(FVector(3.0f, 3.0f, 6.0f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> BaseMeshObj(TEXT("/MonPremierMod/Meshes/Turrets/ShockWave/Base/Base_ShockWave.Base_ShockWave"));
    if (BaseMeshObj.Succeeded())
    {
        BaseMesh->SetStaticMesh(BaseMeshObj.Object);
        UE_LOG(LogTemp, Warning, TEXT("TDShockwaveTurret: Base mesh LOADED!"));
    }

    // Hammer pivot (point d'attache pour le marteau, monte/descend)
    HammerPivot = CreateDefaultSubobject<USceneComponent>(TEXT("HammerPivot"));
    HammerPivot->SetupAttachment(BaseMesh);
    HammerPivot->SetRelativeLocation(FVector(0.0f, 0.0f, HammerTopZ));
    HammerPivot->SetAbsolute(false, false, true);

    // Hammer head mesh
    HammerHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HammerHead"));
    HammerHead->SetupAttachment(HammerPivot);
    HammerHead->SetRelativeLocation(FVector(-5.0f, 0.0f, -1200.0f));
    HammerHead->SetRelativeScale3D(FVector(1.5f, 1.5f, 1.5f));
    HammerHead->SetRelativeRotation(FRotator(180.0f, 90.0f, 90.0f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> HeadMeshObj(TEXT("/MonPremierMod/Meshes/Turrets/ShockWave/Head/Head_ShockWave.Head_ShockWave"));
    if (HeadMeshObj.Succeeded())
    {
        HammerHead->SetStaticMesh(HeadMeshObj.Object);
        UE_LOG(LogTemp, Warning, TEXT("TDShockwaveTurret: Hammer mesh LOADED!"));
    }

    // Audio - Impact
    ImpactAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("ImpactAudioComponent"));
    ImpactAudioComponent->SetupAttachment(BaseMesh);
    ImpactAudioComponent->bAutoActivate = false;
    ImpactAudioComponent->bAllowSpatialization = true;
    ImpactAudioComponent->SetVolumeMultiplier(1.0f);
    ImpactAudioComponent->bOverrideAttenuation = true;
    ImpactAudioComponent->AttenuationOverrides.bAttenuate = true;
    ImpactAudioComponent->AttenuationOverrides.FalloffDistance = 3000.0f;

    // Audio - Rising
    RisingAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("RisingAudioComponent"));
    RisingAudioComponent->SetupAttachment(HammerPivot);
    RisingAudioComponent->bAutoActivate = false;
    RisingAudioComponent->bAllowSpatialization = true;
    RisingAudioComponent->SetVolumeMultiplier(0.5f);
    RisingAudioComponent->bOverrideAttenuation = true;
    RisingAudioComponent->AttenuationOverrides.bAttenuate = true;
    RisingAudioComponent->AttenuationOverrides.FalloffDistance = 2000.0f;

    // Audio - Drop (swoosh)
    DropAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("DropAudioComponent"));
    DropAudioComponent->SetupAttachment(HammerPivot);
    DropAudioComponent->bAutoActivate = false;
    DropAudioComponent->bAllowSpatialization = true;
    DropAudioComponent->SetVolumeMultiplier(1.0f);
    DropAudioComponent->bOverrideAttenuation = true;
    DropAudioComponent->AttenuationOverrides.bAttenuate = true;
    DropAudioComponent->AttenuationOverrides.FalloffDistance = 3000.0f;

    // Charger les sons
    static ConstructorHelpers::FObjectFinder<USoundBase> ImpactSoundObj(TEXT("/MonPremierMod/Audios/Turret/Retro_Impact_LoFi_09.Retro_Impact_LoFi_09"));
    if (ImpactSoundObj.Succeeded()) { ImpactSound = ImpactSoundObj.Object; }

    static ConstructorHelpers::FObjectFinder<USoundBase> RisingSoundObj(TEXT("/MonPremierMod/Audios/Turret/Retro_Charge_13.Retro_Charge_13"));
    if (RisingSoundObj.Succeeded()) { RisingSound = RisingSoundObj.Object; }

    static ConstructorHelpers::FObjectFinder<USoundBase> DropSoundObj(TEXT("/MonPremierMod/Audios/Turret/Retro_Swooosh_02.Retro_Swooosh_02"));
    if (DropSoundObj.Succeeded()) { DropSound = DropSoundObj.Object; }

    // Composants electricite
    PowerConnection = CreateDefaultSubobject<UFGPowerConnectionComponent>(TEXT("PowerConnection"));
    PowerConnection->SetupAttachment(BaseMesh);
    PowerConnection->SetRelativeLocation(FVector(0, 0, 100.0f));

    PowerInfo = CreateDefaultSubobject<UFGPowerInfoComponent>(TEXT("PowerInfo"));

    CurrentHammerZ = HammerTopZ;
}

void ATDShockwaveTurret::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning, TEXT("TDShockwaveTurret spawned at %s"), *GetActorLocation().ToString());

    // Configurer le systeme electrique
    if (PowerConnection && PowerInfo)
    {
        PowerConnection->SetPowerInfo(PowerInfo);
        PowerInfo->SetTargetConsumption(PowerConsumption);
        UE_LOG(LogTemp, Warning, TEXT("TDShockwaveTurret: Power system initialized (%.1f MW)"), PowerConsumption);
    }

    // Forcer le tick actif
    PrimaryActorTick.SetTickFunctionEnable(true);
    PrimaryActorTick.bCanEverTick = true;
}

void ATDShockwaveTurret::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Verifier l'electricite
    CheckPowerStatus();

    if (!bHasPower)
    {
        // Sans electricite, remettre le marteau en haut et ne rien faire
        if (HammerState != EHammerState::Idle)
        {
            HammerState = EHammerState::Idle;
            HammerTimer = 0.0f;
            IdleTimer = 0.0f;
            CurrentHammerZ = HammerTopZ;
            UpdateHammerPosition();

            if (DropAudioComponent && DropAudioComponent->IsPlaying()) DropAudioComponent->Stop();
            if (RisingAudioComponent && RisingAudioComponent->IsPlaying()) RisingAudioComponent->Stop();
        }
        return;
    }

    switch (HammerState)
    {
    case EHammerState::Idle:
    {
        // PERF: Throttle enemy scan toutes les 0.5s
        EnemyScanTimer += DeltaTime;
        if (EnemyScanTimer >= 0.5f)
        {
            EnemyScanTimer = 0.0f;
            bCachedEnemiesInRange = HasEnemiesInRange();
        }
        // Attendre qu'il y ait des ennemis a proximite
        if (bCachedEnemiesInRange)
        {
            IdleTimer += DeltaTime;
            if (IdleTimer >= IdleCooldown)
            {
                // Commencer a tomber!
                HammerState = EHammerState::Dropping;
                HammerTimer = 0.0f;
                IdleTimer = 0.0f;

                // Jouer le son de swoosh (chute)
                if (DropSound && DropAudioComponent)
                {
                    DropAudioComponent->SetSound(DropSound);
                    DropAudioComponent->Play();
                }

                UE_LOG(LogTemp, Verbose, TEXT("TDShockwaveTurret: DROPPING hammer!"));
            }
        }
        else
        {
            IdleTimer = 0.0f;
        }
        break;
    }

    case EHammerState::Dropping:
    {
        HammerTimer += DeltaTime;
        float Alpha = FMath::Clamp(HammerTimer / DropTime, 0.0f, 1.0f);
        // Acceleration (ease in) pour effet de chute
        float EasedAlpha = Alpha * Alpha;
        CurrentHammerZ = FMath::Lerp(HammerTopZ, HammerBottomZ, EasedAlpha);
        UpdateHammerPosition();

        if (Alpha >= 1.0f)
        {
            // IMPACT!
            HammerState = EHammerState::Impact;
            CurrentHammerZ = HammerBottomZ;
            UpdateHammerPosition();
            ApplyShockwave();

            // Jouer le son d'impact
            if (ImpactSound && ImpactAudioComponent)
            {
                ImpactAudioComponent->SetSound(ImpactSound);
                ImpactAudioComponent->Play();
            }

            UE_LOG(LogTemp, Verbose, TEXT("TDShockwaveTurret: IMPACT! Shockwave applied!"));

            // Passer directement a la remontee
            HammerState = EHammerState::Rising;
            HammerTimer = 0.0f;
        }
        break;
    }

    case EHammerState::Rising:
    {
        HammerTimer += DeltaTime;
        float Alpha = FMath::Clamp(HammerTimer / RiseTime, 0.0f, 1.0f);
        // Deceleration (ease out) pour effet de remontee mecanique
        float EasedAlpha = 1.0f - FMath::Pow(1.0f - Alpha, 2.0f);
        CurrentHammerZ = FMath::Lerp(HammerBottomZ, HammerTopZ, EasedAlpha);
        UpdateHammerPosition();

        // Son de remontee
        if (Alpha < 0.05f && RisingSound && RisingAudioComponent && !RisingAudioComponent->IsPlaying())
        {
            RisingAudioComponent->SetSound(RisingSound);
            RisingAudioComponent->Play();
        }

        if (Alpha >= 1.0f)
        {
            // Retour en haut
            CurrentHammerZ = HammerTopZ;
            UpdateHammerPosition();
            HammerState = EHammerState::Idle;
            HammerTimer = 0.0f;
            IdleTimer = 0.0f;

            if (RisingAudioComponent && RisingAudioComponent->IsPlaying())
            {
                RisingAudioComponent->Stop();
            }

            UE_LOG(LogTemp, Verbose, TEXT("TDShockwaveTurret: Hammer back to TOP, ready!"));
        }
        break;
    }

    default:
        break;
    }
}

void ATDShockwaveTurret::Factory_Tick(float dt)
{
    Super::Factory_Tick(dt);
}

bool ATDShockwaveTurret::HasEnemiesInRange()
{
    FVector MyLoc = GetActorLocation();

    // Verifier ennemis au sol
    for (TActorIterator<ATDEnemy> It(GetWorld()); It; ++It)
    {
        ATDEnemy* Enemy = *It;
        if (Enemy && !Enemy->bIsDead)
        {
            float Dist = FVector::Dist(MyLoc, Enemy->GetActorLocation());
            if (Dist <= ShockwaveRadius)
            {
                return true;
            }
        }
    }

    // Verifier ennemis volants
    for (TActorIterator<ATDEnemyFlying> It(GetWorld()); It; ++It)
    {
        ATDEnemyFlying* Enemy = *It;
        if (Enemy && !Enemy->bIsDead)
        {
            float Dist = FVector::Dist(MyLoc, Enemy->GetActorLocation());
            if (Dist <= ShockwaveRadius)
            {
                return true;
            }
        }
    }

    // Verifier beliers
    for (TActorIterator<ATDEnemyRam> It(GetWorld()); It; ++It)
    {
        ATDEnemyRam* Enemy = *It;
        if (Enemy && !Enemy->bIsDead)
        {
            float Dist = FVector::Dist(MyLoc, Enemy->GetActorLocation());
            if (Dist <= ShockwaveRadius)
            {
                return true;
            }
        }
    }

    return false;
}

void ATDShockwaveTurret::ApplyShockwave()
{
    FVector MyLoc = GetActorLocation();
    int32 SlowedCount = 0;

    // Slow ennemis au sol
    for (TActorIterator<ATDEnemy> It(GetWorld()); It; ++It)
    {
        ATDEnemy* Enemy = *It;
        if (Enemy && !Enemy->bIsDead)
        {
            float Dist = FVector::Dist(MyLoc, Enemy->GetActorLocation());
            if (Dist <= ShockwaveRadius)
            {
                Enemy->ApplySlow(SlowDuration, SlowFactor);
                SlowedCount++;
            }
        }
    }

    // Slow ennemis volants
    for (TActorIterator<ATDEnemyFlying> It(GetWorld()); It; ++It)
    {
        ATDEnemyFlying* Enemy = *It;
        if (Enemy && !Enemy->bIsDead)
        {
            float Dist = FVector::Dist(MyLoc, Enemy->GetActorLocation());
            if (Dist <= ShockwaveRadius)
            {
                Enemy->ApplySlow(SlowDuration, SlowFactor);
                SlowedCount++;
            }
        }
    }

    // Slow beliers
    for (TActorIterator<ATDEnemyRam> It(GetWorld()); It; ++It)
    {
        ATDEnemyRam* Enemy = *It;
        if (Enemy && !Enemy->bIsDead)
        {
            float Dist = FVector::Dist(MyLoc, Enemy->GetActorLocation());
            if (Dist <= ShockwaveRadius)
            {
                Enemy->ApplySlow(SlowDuration, SlowFactor);
                SlowedCount++;
            }
        }
    }

    UE_LOG(LogTemp, Verbose, TEXT("TDShockwaveTurret: Shockwave hit %d enemies! (radius=%.0f, slow=%.0fx for %.1fs)"), SlowedCount, ShockwaveRadius, SlowFactor, SlowDuration);
}

void ATDShockwaveTurret::UpdateHammerPosition()
{
    if (HammerPivot)
    {
        HammerPivot->SetRelativeLocation(FVector(0.0f, 0.0f, CurrentHammerZ));
    }
}

void ATDShockwaveTurret::CheckPowerStatus()
{
    bool bPreviousPower = bHasPower;
    bHasPower = false;

    if (PowerInfo && PowerInfo->HasPower())
    {
        bHasPower = true;
    }
    else if (PowerConnection && PowerConnection->HasPower())
    {
        bHasPower = true;
    }

    if (bHasPower != bPreviousPower)
    {
        UE_LOG(LogTemp, Warning, TEXT("TDShockwaveTurret %s: Power state changed -> %s"), *GetName(), bHasPower ? TEXT("ON") : TEXT("OFF"));
    }
}

void ATDShockwaveTurret::TakeDamageCustom(float DamageAmount)
{
    Health -= DamageAmount;

    if (Health <= 0.0f)
    {
        Health = 0.0f;
        UE_LOG(LogTemp, Warning, TEXT("TDShockwaveTurret DESTROYED!"));
        Destroy();
    }
}
