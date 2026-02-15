#include "TDTurretPlacer.h"
#include "TDTurret.h"
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

ATDTurretPlacer::ATDTurretPlacer()
{
    PrimaryActorTick.bCanEverTick = true;

    USceneComponent* RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
    SetRootComponent(RootComp);

    // Ghost root (detache du root pour etre libre dans le monde)
    GhostRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GhostRoot"));
    GhostRoot->SetupAttachment(RootComp);

    // Ghost base
    GhostBase = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GhostBase"));
    GhostBase->SetupAttachment(GhostRoot);
    GhostBase->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GhostBase->SetVisibility(false);
    GhostBase->SetRelativeLocation(FVector(0, 0, -20));
    GhostBase->SetRelativeScale3D(FVector(0.8f, 0.8f, 0.6f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> BaseMeshFinder(TEXT("/MonPremierMod/Meshes/Turrets/LaserTurret/Base/baseTurret.baseTurret"));
    if (BaseMeshFinder.Succeeded())
    {
        GhostBase->SetStaticMesh(BaseMeshFinder.Object);
    }

    // Ghost head
    GhostHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GhostHead"));
    GhostHead->SetupAttachment(GhostRoot);
    GhostHead->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GhostHead->SetVisibility(false);
    GhostHead->SetRelativeLocation(FVector(-25, 0, 55));
    GhostHead->SetRelativeScale3D(FVector(0.7f, 0.7f, 0.7f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> HeadMeshFinder(TEXT("/MonPremierMod/Meshes/Turrets/LaserTurret/Head/head_turret.head_turret"));
    if (HeadMeshFinder.Succeeded())
    {
        GhostHead->SetStaticMesh(HeadMeshFinder.Object);
    }

    // Cercle de portee (sphere)
    RangeCircle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RangeCircle"));
    RangeCircle->SetupAttachment(GhostRoot);
    RangeCircle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RangeCircle->SetVisibility(false);
    // Range = 4000u, sphere = 100u diametre, scale = 4000/50 = 80
    RangeCircle->SetRelativeScale3D(FVector(80.0f, 80.0f, 80.0f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshObj(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereMeshObj.Succeeded())
    {
        RangeCircle->SetStaticMesh(SphereMeshObj.Object);
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> RangeMatFinder(TEXT("/MonPremierMod/Materials/M_ShieldDome.M_ShieldDome"));
    if (RangeMatFinder.Succeeded())
    {
        RangeBaseMaterial = RangeMatFinder.Object;
    }

    mEquipmentSlot = EEquipmentSlot::ES_ARMS;
    mDefaultEquipmentActions = static_cast<uint8>(EDefaultEquipmentAction::PrimaryFire);
    mNeedsDefaultEquipmentMappingContext = true;

    bValidPlacement = false;
}

void ATDTurretPlacer::Equip(AFGCharacterPlayer* character)
{
    Super::Equip(character);

    if (GhostRoot)
    {
        GhostRoot->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
    }
    if (GhostBase)
    {
        GhostBase->SetVisibility(true);
        GhostBase->SetRenderCustomDepth(true);
        GhostBase->SetCustomDepthStencilValue(5);
    }
    if (GhostHead)
    {
        GhostHead->SetVisibility(true);
        GhostHead->SetRenderCustomDepth(true);
        GhostHead->SetCustomDepthStencilValue(5);
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

void ATDTurretPlacer::UnEquip()
{
    if (GhostBase) GhostBase->SetVisibility(false);
    if (GhostHead) GhostHead->SetVisibility(false);
    if (RangeCircle) RangeCircle->SetVisibility(false);
    Super::UnEquip();
}

void ATDTurretPlacer::Tick(float DeltaTime)
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
        GhostRoot->SetWorldLocation(GhostLocation + FVector(0, 0, -40.0f));
        GhostRoot->SetWorldRotation(GhostRotation);
        if (GhostBase) GhostBase->SetVisibility(true);
        if (GhostHead) GhostHead->SetVisibility(true);
        if (RangeCircle) RangeCircle->SetVisibility(true);
    }
    else
    {
        if (GhostBase) GhostBase->SetVisibility(false);
        if (GhostHead) GhostHead->SetVisibility(false);
        if (RangeCircle) RangeCircle->SetVisibility(false);
    }
}

bool ATDTurretPlacer::TraceForPlacement(FVector& OutLocation, FRotator& OutRotation) const
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
        // Offset Z pour que la tourelle ne s'enfonce pas dans le sol
        // Le BaseMesh de TDTurret a un offset de -70, donc on compense
        OutLocation = HitResult.ImpactPoint + FVector(0, 0, 90.0f);
        OutRotation = FRotator(0, CamRotation.Yaw, 0);
        return true;
    }

    return false;
}

void ATDTurretPlacer::HandleDefaultEquipmentActionEvent(EDefaultEquipmentAction action, EDefaultEquipmentActionEvent actionEvent)
{
    Super::HandleDefaultEquipmentActionEvent(action, actionEvent);

    if (action == EDefaultEquipmentAction::PrimaryFire && actionEvent == EDefaultEquipmentActionEvent::Pressed)
    {
        if (HasAuthority())
        {
            PlaceTurret();
        }
    }
}

void ATDTurretPlacer::PlaceTurret()
{
    if (!bValidPlacement) return;

    AFGCharacterPlayer* Player = GetInstigatorCharacter();
    if (!Player) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    ATDTurret* Turret = GetWorld()->SpawnActor<ATDTurret>(ATDTurret::StaticClass(), GhostLocation + FVector(0, 0, 5.0f), GhostRotation, SpawnParams);

    if (Turret)
    {
        UE_LOG(LogTemp, Warning, TEXT("MonPremierMod: Tourelle placee a %s"), *GhostLocation.ToString());

        // Consommer l'item du slot d'equipement
        UFGInventoryComponentEquipment* EquipInv = Player->GetEquipmentSlot(EEquipmentSlot::ES_ARMS);
        if (EquipInv)
        {
            UClass* DescClass = LoadClass<UFGItemDescriptor>(nullptr, TEXT("/MonPremierMod/Items/Desc_TDTurret.Desc_TDTurret_C"));
            if (DescClass)
            {
                EquipInv->Remove(TSubclassOf<UFGItemDescriptor>(DescClass), 1);
                UE_LOG(LogTemp, Warning, TEXT("MonPremierMod: Item consomme du slot equipement"));
            }
        }
    }
}
