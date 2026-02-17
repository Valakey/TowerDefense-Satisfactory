#include "TDLaserFencePlacer.h"
#include "TDLaserFence.h"
#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"
#include "FGInventoryComponentEquipment.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

ATDLaserFencePlacer::ATDLaserFencePlacer()
{
    PrimaryActorTick.bCanEverTick = true;

    USceneComponent* RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
    SetRootComponent(RootComp);

    // Ghost root (detache du root pour positionner librement)
    GhostRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GhostRoot"));
    GhostRoot->SetupAttachment(RootComp);

    // Ghost pylone
    GhostPylon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GhostPylon"));
    GhostPylon->SetupAttachment(GhostRoot);
    GhostPylon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GhostPylon->SetVisibility(false);
    GhostPylon->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> PylonMeshFinder(
        TEXT("/MonPremierMod/Meshes/Fence/Fence.Fence"));
    if (PylonMeshFinder.Succeeded())
    {
        GhostPylon->SetStaticMesh(PylonMeshFinder.Object);
    }

    // Cercle de portee (sphere holographique montrant la range de connexion 800u)
    RangeCircle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RangeCircle"));
    RangeCircle->SetupAttachment(GhostRoot);
    RangeCircle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RangeCircle->SetVisibility(false);
    // Range = 800u, sphere UE = 100u diametre, scale = 800/50 = 16
    RangeCircle->SetRelativeScale3D(FVector(16.0f, 16.0f, 16.0f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshObj(
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereMeshObj.Succeeded())
    {
        RangeCircle->SetStaticMesh(SphereMeshObj.Object);
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> RangeMatFinder(
        TEXT("/MonPremierMod/Materials/M_ShieldDome.M_ShieldDome"));
    if (RangeMatFinder.Succeeded())
    {
        RangeBaseMaterial = RangeMatFinder.Object;
    }

    mEquipmentSlot = EEquipmentSlot::ES_ARMS;
    mDefaultEquipmentActions = static_cast<uint8>(EDefaultEquipmentAction::PrimaryFire);
    mNeedsDefaultEquipmentMappingContext = true;

    bValidPlacement = false;
}

void ATDLaserFencePlacer::Equip(AFGCharacterPlayer* character)
{
    Super::Equip(character);

    if (GhostRoot)
    {
        GhostRoot->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
    }
    if (GhostPylon)
    {
        GhostPylon->SetVisibility(true);
        GhostPylon->SetRenderCustomDepth(true);
        GhostPylon->SetCustomDepthStencilValue(5);
    }
    if (RangeCircle)
    {
        if (!RangeMaterial && RangeBaseMaterial)
        {
            RangeMaterial = UMaterialInstanceDynamic::Create(RangeBaseMaterial, this);
        }
        if (RangeMaterial)
        {
            RangeCircle->SetMaterial(0, RangeMaterial);
        }
        RangeCircle->SetVisibility(true);
    }
}

void ATDLaserFencePlacer::UnEquip()
{
    if (GhostPylon) GhostPylon->SetVisibility(false);
    if (RangeCircle) RangeCircle->SetVisibility(false);
    Super::UnEquip();
}

void ATDLaserFencePlacer::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!IsEquipped() || !GhostRoot) return;

    FVector Location;
    FRotator Rotation;
    bValidPlacement = TraceForPlacement(Location, Rotation);

    if (bValidPlacement)
    {
        GhostLocation = Location;
        GhostRotation = Rotation;
        GhostRoot->SetWorldLocation(GhostLocation);
        GhostRoot->SetWorldRotation(GhostRotation);
        if (GhostPylon) GhostPylon->SetVisibility(true);
        if (RangeCircle) RangeCircle->SetVisibility(true);
    }
    else
    {
        if (GhostPylon) GhostPylon->SetVisibility(false);
        if (RangeCircle) RangeCircle->SetVisibility(false);
    }
}

bool ATDLaserFencePlacer::TraceForPlacement(FVector& OutLocation, FRotator& OutRotation) const
{
    AFGCharacterPlayer* Player = GetInstigatorCharacter();
    if (!Player) return false;

    APlayerController* PC = Cast<APlayerController>(Player->GetController());
    if (!PC) return false;

    FVector CamLocation;
    FRotator CamRotation;
    PC->GetPlayerViewPoint(CamLocation, CamRotation);

    FVector TraceStart = CamLocation;
    FVector TraceEnd = CamLocation + CamRotation.Vector() * PlaceDistance;

    FHitResult HitResult;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Player);
    Params.AddIgnoredActor(this);

    if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, Params))
    {
        OutLocation = HitResult.ImpactPoint + FVector(0.0f, 0.0f, 95.0f);
        OutRotation = FRotator(0.0f, CamRotation.Yaw, 0.0f);
        return true;
    }

    return false;
}

void ATDLaserFencePlacer::HandleDefaultEquipmentActionEvent(EDefaultEquipmentAction action, EDefaultEquipmentActionEvent actionEvent)
{
    Super::HandleDefaultEquipmentActionEvent(action, actionEvent);

    if (action == EDefaultEquipmentAction::PrimaryFire && actionEvent == EDefaultEquipmentActionEvent::Pressed)
    {
        if (HasAuthority())
        {
            PlaceFence();
        }
    }
}

void ATDLaserFencePlacer::PlaceFence()
{
    if (!bValidPlacement) return;

    AFGCharacterPlayer* Player = GetInstigatorCharacter();
    if (!Player) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    ATDLaserFence* Fence = GetWorld()->SpawnActor<ATDLaserFence>(
        ATDLaserFence::StaticClass(), GhostLocation, GhostRotation, SpawnParams);

    if (Fence)
    {
        UE_LOG(LogTemp, Warning, TEXT("TDLaserFence: Pylone place a %s"), *GhostLocation.ToString());

        // Consommer l'item du slot d'equipement
        UFGInventoryComponentEquipment* EquipInv = Player->GetEquipmentSlot(EEquipmentSlot::ES_ARMS);
        if (EquipInv)
        {
            UClass* DescClass = LoadClass<UFGItemDescriptor>(nullptr,
                TEXT("/MonPremierMod/Items/Desc_TDLaserFence.Desc_TDLaserFence_C"));
            if (DescClass)
            {
                EquipInv->Remove(TSubclassOf<UFGItemDescriptor>(DescClass), 1);
                UE_LOG(LogTemp, Warning, TEXT("TDLaserFence: Item consomme du slot equipement"));
            }
        }
    }
}
