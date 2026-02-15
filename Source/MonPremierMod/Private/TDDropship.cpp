#include "TDDropship.h"
#include "TDCreatureSpawner.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ATDDropship::ATDDropship()
{
    PrimaryActorTick.bCanEverTick = true;

    // Creer le mesh du vaisseau
    ShipMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShipMesh"));
    RootComponent = ShipMesh;
    ShipMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Charger le mesh du vaisseau
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ShipMeshObj(TEXT("/MonPremierMod/Meshes/SpaceShip/ShipFBX.ShipFBX"));
    if (ShipMeshObj.Succeeded())
    {
        ShipMesh->SetStaticMesh(ShipMeshObj.Object);
        ShipMesh->SetWorldScale3D(FVector(10.0f, 10.0f, 10.0f));  // Scale x10
        UE_LOG(LogTemp, Warning, TEXT("TDDropship: Mesh charge!"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("TDDropship: Mesh FAILED to load!"));
    }

    // Creer composant audio principal avec attenuation 3D
    MainAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("MainAudio"));
    MainAudioComponent->SetupAttachment(RootComponent);
    MainAudioComponent->bAutoActivate = false;
    MainAudioComponent->bAllowSpatialization = true;
    MainAudioComponent->bOverrideAttenuation = true;
    MainAudioComponent->AttenuationOverrides.bAttenuate = true;
    MainAudioComponent->AttenuationOverrides.bSpatialize = true;
    MainAudioComponent->AttenuationOverrides.FalloffDistance = 8000.0f;
    MainAudioComponent->AttenuationOverrides.AttenuationShapeExtents = FVector(500.0f, 0.0f, 0.0f);

    // Creer composant audio pour spawn avec attenuation 3D
    SpawnAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("SpawnAudio"));
    SpawnAudioComponent->SetupAttachment(RootComponent);
    SpawnAudioComponent->bAutoActivate = false;
    SpawnAudioComponent->bAllowSpatialization = true;
    SpawnAudioComponent->bOverrideAttenuation = true;
    SpawnAudioComponent->AttenuationOverrides.bAttenuate = true;
    SpawnAudioComponent->AttenuationOverrides.bSpatialize = true;
    SpawnAudioComponent->AttenuationOverrides.FalloffDistance = 5000.0f;
    SpawnAudioComponent->AttenuationOverrides.AttenuationShapeExtents = FVector(300.0f, 0.0f, 0.0f);

    // Charger les sons
    static ConstructorHelpers::FObjectFinder<USoundWave> ArrivalSoundObj(TEXT("/MonPremierMod/Audios/SpaceShip/Arriver.Arriver"));
    if (ArrivalSoundObj.Succeeded())
    {
        ArrivalSound = ArrivalSoundObj.Object;
        UE_LOG(LogTemp, Warning, TEXT("TDDropship: ArrivalSound charge!"));
    }

    static ConstructorHelpers::FObjectFinder<USoundWave> HoverSoundObj(TEXT("/MonPremierMod/Audios/SpaceShip/DropMobStand.DropMobStand"));
    if (HoverSoundObj.Succeeded())
    {
        HoverSound = HoverSoundObj.Object;
        UE_LOG(LogTemp, Warning, TEXT("TDDropship: HoverSound charge!"));
    }

    static ConstructorHelpers::FObjectFinder<USoundWave> SpawnMobSoundObj(TEXT("/MonPremierMod/Audios/SpaceShip/SpawnMob.SpawnMob"));
    if (SpawnMobSoundObj.Succeeded())
    {
        SpawnMobSound = SpawnMobSoundObj.Object;
        UE_LOG(LogTemp, Warning, TEXT("TDDropship: SpawnMobSound charge!"));
    }

    static ConstructorHelpers::FObjectFinder<USoundWave> DepartureSoundObj(TEXT("/MonPremierMod/Audios/SpaceShip/depars.depars"));
    if (DepartureSoundObj.Succeeded())
    {
        DepartureSound = DepartureSoundObj.Object;
        UE_LOG(LogTemp, Warning, TEXT("TDDropship: DepartureSound charge!"));
    }

    // Charger le systeme Niagara de flammes
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> FlameSystemObj(TEXT("/MonPremierMod/Particles/NS_StylizedFire.NS_StylizedFire"));
    if (FlameSystemObj.Succeeded())
    {
        FlameNiagaraSystem = FlameSystemObj.Object;
        UE_LOG(LogTemp, Warning, TEXT("TDDropship: FlameNiagaraSystem charge!"));
    }
    
    // Creer le composant Niagara pour les flammes
    ThrusterFlameEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ThrusterFlameEffect"));
    ThrusterFlameEffect->SetupAttachment(RootComponent);
    ThrusterFlameEffect->SetRelativeLocation(FVector(0.0f, 0.0f, -100.0f));  // Sous le vaisseau (+50 unites)
    ThrusterFlameEffect->SetRelativeScale3D(FVector(200.0f, 200.0f, 200.0f));  // Scale x200
    ThrusterFlameEffect->bAutoActivate = false;  // Active manuellement dans BeginPlay
    UE_LOG(LogTemp, Warning, TEXT("TDDropship: ThrusterFlameEffect cree!"));
}

void ATDDropship::BeginPlay()
{
    Super::BeginPlay();
    
    // Activer l'effet de flammes Niagara
    if (ThrusterFlameEffect && FlameNiagaraSystem)
    {
        ThrusterFlameEffect->SetAsset(FlameNiagaraSystem);
        ThrusterFlameEffect->SetWorldScale3D(FVector(200.0f, 200.0f, 200.0f));
        ThrusterFlameEffect->Activate();
        UE_LOG(LogTemp, Warning, TEXT("TDDropship: Flammes Niagara activees avec scale 200!"));
    }
}

void ATDDropship::Initialize(const FVector& TargetLocation, const TArray<FVector>& EnemySpawnLocations, ATDCreatureSpawner* Spawner)
{
    TargetGroundLocation = TargetLocation;
    PendingSpawns = EnemySpawnLocations;
    OwnerSpawner = Spawner;

    // Position de hover (au-dessus du sol)
    HoverLocation = TargetGroundLocation + FVector(0, 0, HoverHeight);

    // Position de depart (dans le ciel)
    SkyLocation = TargetGroundLocation + FVector(0, 0, SkyHeight);

    // Commencer dans le ciel
    SetActorLocation(SkyLocation);
    CurrentState = EDropshipState::Descending;

    // Jouer le son d'arrivee
    if (MainAudioComponent && ArrivalSound)
    {
        MainAudioComponent->SetSound(ArrivalSound);
        MainAudioComponent->SetVolumeMultiplier(0.6f);
        MainAudioComponent->Play();
    }

    UE_LOG(LogTemp, Warning, TEXT("TDDropship: Initialise - %d ennemis a deposer"), PendingSpawns.Num());
}

void ATDDropship::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    switch (CurrentState)
    {
        case EDropshipState::Descending:
            UpdateDescending(DeltaTime);
            break;
        case EDropshipState::Hovering:
            UpdateHovering(DeltaTime);
            break;
        case EDropshipState::Ascending:
            UpdateAscending(DeltaTime);
            break;
        case EDropshipState::Done:
            Destroy();
            break;
    }
}

void ATDDropship::UpdateDescending(float DeltaTime)
{
    FVector CurrentLoc = GetActorLocation();
    FVector Direction = (HoverLocation - CurrentLoc).GetSafeNormal();
    float DistanceToTarget = FVector::Dist(CurrentLoc, HoverLocation);

    if (DistanceToTarget > 50.0f)
    {
        // Continuer la descente
        FVector NewLoc = CurrentLoc + Direction * MoveSpeed * DeltaTime;
        SetActorLocation(NewLoc);
    }
    else
    {
        // Arrive en position de hover
        SetActorLocation(HoverLocation);
        CurrentState = EDropshipState::Hovering;
        SpawnTimer = 0.0f;

        // Jouer le son de hover/position
        if (MainAudioComponent && HoverSound)
        {
            MainAudioComponent->SetSound(HoverSound);
            MainAudioComponent->SetVolumeMultiplier(0.5f);
            MainAudioComponent->Play();
        }

        UE_LOG(LogTemp, Warning, TEXT("TDDropship: En position de hover, debut du spawn"));
    }
}

void ATDDropship::UpdateHovering(float DeltaTime)
{
    // Spawn progressif des ennemis
    SpawnTimer += DeltaTime;

    if (SpawnTimer >= SpawnInterval)
    {
        SpawnTimer = 0.0f;
        SpawnNextEnemy();
    }

    // Quand tous les ennemis sont spawnes, remonter
    if (PendingSpawns.Num() == 0)
    {
        CurrentState = EDropshipState::Ascending;

        // Jouer le son de depart
        if (MainAudioComponent && DepartureSound)
        {
            MainAudioComponent->SetSound(DepartureSound);
            MainAudioComponent->SetVolumeMultiplier(0.6f);
            MainAudioComponent->Play();
        }

        UE_LOG(LogTemp, Warning, TEXT("TDDropship: Tous les ennemis deposes, remontee"));
    }
}

void ATDDropship::UpdateAscending(float DeltaTime)
{
    FVector CurrentLoc = GetActorLocation();
    
    // Destination plus haute pour disparaitre (4x la hauteur de depart)
    FVector DisappearLocation = TargetGroundLocation + FVector(0, 0, SkyHeight * 4.0f);
    FVector Direction = FVector(0, 0, 1);  // Monter tout droit
    
    float CurrentHeight = CurrentLoc.Z - TargetGroundLocation.Z;

    if (CurrentHeight < SkyHeight * 4.0f)
    {
        // Continuer la montee (4x plus rapide)
        FVector NewLoc = CurrentLoc + Direction * MoveSpeed * 4.0f * DeltaTime;
        SetActorLocation(NewLoc);
    }
    else
    {
        // Disparu dans le ciel (a 20000 unites)
        CurrentState = EDropshipState::Done;
        UE_LOG(LogTemp, Warning, TEXT("TDDropship: Disparu dans le ciel"));
    }
}

void ATDDropship::SpawnNextEnemy()
{
    if (PendingSpawns.Num() == 0 || !OwnerSpawner) return;

    // Prendre la prochaine position
    FVector SpawnLoc = PendingSpawns[0];
    PendingSpawns.RemoveAt(0);

    // Jouer le son de spawn
    if (SpawnAudioComponent && SpawnMobSound)
    {
        SpawnAudioComponent->SetSound(SpawnMobSound);
        SpawnAudioComponent->SetVolumeMultiplier(0.4f);
        SpawnAudioComponent->Play();
    }

    // Spawner l'ennemi via le spawner
    OwnerSpawner->SpawnCreatureAt(SpawnLoc);
}
