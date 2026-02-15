#include "TDDronePlatform.h"
#include "TDDrone.h"
#include "FGFactoryConnectionComponent.h"
#include "FGInventoryComponent.h"

ATDDronePlatform::ATDDronePlatform(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = true;
    mFactoryTickFunction.bCanEverTick = true;

    // Platform mesh
    PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
    PlatformMesh->SetupAttachment(RootComponent);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> PlatformMeshObj(TEXT("/MonPremierMod/Meshes/PlatformDrone/Platform/Platform.Platform"));
    if (PlatformMeshObj.Succeeded())
    {
        PlatformMesh->SetStaticMesh(PlatformMeshObj.Object);
        UE_LOG(LogTemp, Warning, TEXT("TDDronePlatform: Platform mesh LOADED!"));
    }
    PlatformMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));
    PlatformMesh->SetRelativeScale3D(FVector(1.5f, 1.5f, 1.5f));

    // Conveyor input
    ConveyorInput = CreateDefaultSubobject<UFGFactoryConnectionComponent>(TEXT("ConveyorInput"));
    ConveyorInput->SetupAttachment(PlatformMesh);
    ConveyorInput->SetRelativeLocation(FVector(0.0f, 0.0f, -40.0f));
    ConveyorInput->SetDirection(EFactoryConnectionDirection::FCD_INPUT);
    ConveyorInput->SetConnectorClearance(100.0f);

    // Inventaire (1 slot)
    PlatformInventory = CreateDefaultSubobject<UFGInventoryComponent>(TEXT("PlatformInventory"));
    PlatformInventory->SetDefaultSize(1);

    OwnedDrone = nullptr;
}

void ATDDronePlatform::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning, TEXT("TDDronePlatform spawned at %s"), *GetActorLocation().ToString());

    // Lier le conveyor input a l'inventaire
    if (ConveyorInput && PlatformInventory)
    {
        ConveyorInput->SetInventory(PlatformInventory);
        ConveyorInput->SetInventoryAccessIndex(0);
        UE_LOG(LogTemp, Warning, TEXT("TDDronePlatform: Conveyor linked to inventory"));
    }

    // Forcer le tick actif
    PrimaryActorTick.SetTickFunctionEnable(true);
    PrimaryActorTick.bCanEverTick = true;

    // Spawner le drone
    SpawnDrone();
}

void ATDDronePlatform::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Detruire le drone quand la platform est detruite
    if (OwnedDrone && IsValid(OwnedDrone))
    {
        OwnedDrone->Destroy();
        OwnedDrone = nullptr;
    }

    Super::EndPlay(EndPlayReason);
}

void ATDDronePlatform::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void ATDDronePlatform::Factory_Tick(float dt)
{
    Super::Factory_Tick(dt);

    // Tirer les items du conveyor dans l'inventaire
    GrabFromConveyor();
}

void ATDDronePlatform::SpawnDrone()
{
    if (OwnedDrone) return;

    FVector SpawnLoc = GetDroneLandingLocation();
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    OwnedDrone = GetWorld()->SpawnActor<ATDDrone>(ATDDrone::StaticClass(), SpawnLoc, GetActorRotation(), SpawnParams);
    if (OwnedDrone)
    {
        OwnedDrone->OwnerPlatform = this;
        UE_LOG(LogTemp, Warning, TEXT("TDDronePlatform: Drone spawned!"));
    }
}

FVector ATDDronePlatform::GetDroneLandingLocation() const
{
    return GetActorLocation() + FVector(0.0f, 0.0f, 90.0f);
}

void ATDDronePlatform::GrabFromConveyor()
{
    if (!ConveyorInput || !ConveyorInput->IsConnected() || !PlatformInventory) return;

    // Verifier si l'inventaire a de la place
    if (PlatformInventory->FindEmptyIndex() == INDEX_NONE)
    {
        // Pas d'emplacement vide, verifier si on peut stacker
        FInventoryStack ExistingStack;
        if (PlatformInventory->GetStackFromIndex(0, ExistingStack))
        {
            if (ExistingStack.HasItems())
            {
                int32 MaxStackSize = PlatformInventory->GetSlotSize(0, ExistingStack.Item.GetItemClass());
                if (ExistingStack.NumItems >= MaxStackSize)
                {
                    return; // Inventaire plein
                }
            }
        }
    }

    // Grabber depuis le conveyor connecte
    UFGFactoryConnectionComponent* ConnectedOutput = ConveyorInput->GetConnection();
    if (ConnectedOutput)
    {
        FInventoryItem Item;
        float OffsetBeyond = 0.0f;
        if (ConnectedOutput->Factory_GrabOutput(Item, OffsetBeyond))
        {
            PlatformInventory->AddItem(Item);
        }
    }
}

int32 ATDDronePlatform::TakeAmmo(int32 AmountRequested)
{
    if (!PlatformInventory || AmountRequested <= 0) return 0;

    FInventoryStack Stack;
    if (!PlatformInventory->GetStackFromIndex(0, Stack) || !Stack.HasItems())
    {
        return 0;
    }

    int32 AmountToTake = FMath::Min(AmountRequested, Stack.NumItems);
    PlatformInventory->RemoveFromIndex(0, AmountToTake);

    UE_LOG(LogTemp, Warning, TEXT("TDDronePlatform: Took %d items from inventory"), AmountToTake);
    return AmountToTake;
}

bool ATDDronePlatform::HasAmmo() const
{
    if (!PlatformInventory) return false;
    return !PlatformInventory->IsEmpty();
}

int32 ATDDronePlatform::GetAmmoCount() const
{
    if (!PlatformInventory) return 0;

    FInventoryStack Stack;
    if (PlatformInventory->GetStackFromIndex(0, Stack) && Stack.HasItems())
    {
        return Stack.NumItems;
    }
    return 0;
}
