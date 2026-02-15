#include "TDShockwaveTurretPlacer.h"
#include "TDShockwaveTurret.h"
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

ATDShockwaveTurretPlacer::ATDShockwaveTurretPlacer()
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
    GhostBase->SetRelativeLocation(FVector(0, 0, 380));
    GhostBase->SetRelativeScale3D(FVector(3.0f, 3.0f, 6.0f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> BaseMeshFinder(TEXT("/MonPremierMod/Meshes/Turrets/ShockWave/Base/Base_ShockWave.Base_ShockWave"));
    if (BaseMeshFinder.Succeeded())
    {
        GhostBase->SetStaticMesh(BaseMeshFinder.Object);
    }

    // Ghost head (marteau)
    GhostHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GhostHead"));
    GhostHead->SetupAttachment(GhostRoot);
    GhostHead->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GhostHead->SetVisibility(false);
    GhostHead->SetRelativeLocation(FVector(0, 0.5, -1200));
    GhostHead->SetRelativeScale3D(FVector(0.6f, 0.6f, 0.6f));
    GhostHead->SetRelativeRotation(FRotator(180.0f, 90.0f, 90.0f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> HeadMeshFinder(TEXT("/MonPremierMod/Meshes/Turrets/ShockWave/Head/Head_ShockWave.Head_ShockWave"));
    if (HeadMeshFinder.Succeeded())
    {
        GhostHead->SetStaticMesh(HeadMeshFinder.Object);
    }

    // Cercle de portee (sphere)
    RangeCircle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RangeCircle"));
    RangeCircle->SetupAttachment(GhostRoot);
    RangeCircle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RangeCircle->SetVisibility(false);
    // ShockwaveRadius = 2000u, sphere = 100u diametre, scale = 2000/50 = 40
    RangeCircle->SetRelativeScale3D(FVector(40.0f, 40.0f, 40.0f));

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

void ATDShockwaveTurretPlacer::Equip(AFGCharacterPlayer* character)
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

void ATDShockwaveTurretPlacer::UnEquip()
{
    if (GhostBase) GhostBase->SetVisibility(false);
    if (GhostHead) GhostHead->SetVisibility(false);
    if (RangeCircle) RangeCircle->SetVisibility(false);
    Super::UnEquip();
}

void ATDShockwaveTurretPlacer::Tick(float DeltaTime)
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

bool ATDShockwaveTurretPlacer::TraceForPlacement(FVector& OutLocation, FRotator& OutRotation) const
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
        OutLocation = HitResult.ImpactPoint + FVector(0, 0, 90.0f);
        OutRotation = FRotator(0, CamRotation.Yaw, 0);
        return true;
    }

    return false;
}

void ATDShockwaveTurretPlacer::HandleDefaultEquipmentActionEvent(EDefaultEquipmentAction action, EDefaultEquipmentActionEvent actionEvent)
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

void ATDShockwaveTurretPlacer::PlaceTurret()
{
    if (!bValidPlacement) return;

    AFGCharacterPlayer* Player = GetInstigatorCharacter();
    if (!Player) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    ATDShockwaveTurret* Turret = GetWorld()->SpawnActor<ATDShockwaveTurret>(ATDShockwaveTurret::StaticClass(), GhostLocation + FVector(0, 0, 5.0f), GhostRotation, SpawnParams);

    if (Turret)
    {
        UE_LOG(LogTemp, Warning, TEXT("MonPremierMod: ShockwaveTurret placee a %s"), *GhostLocation.ToString());

        // Consommer l'item du slot d'equipement
        UFGInventoryComponentEquipment* EquipInv = Player->GetEquipmentSlot(EEquipmentSlot::ES_ARMS);
        if (EquipInv)
        {
            UClass* DescClass = LoadClass<UFGItemDescriptor>(nullptr, TEXT("/MonPremierMod/Items/Desc_TDShockwaveTurret.Desc_TDShockwaveTurret_C"));
            if (DescClass)
            {
                EquipInv->Remove(TSubclassOf<UFGItemDescriptor>(DescClass), 1);
                UE_LOG(LogTemp, Warning, TEXT("MonPremierMod: ShockwaveTurret item consomme du slot equipement"));
            }
        }
    }
}
