#include "TDFireTurretPlacer.h"
#include "TDFireTurret.h"
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

ATDFireTurretPlacer::ATDFireTurretPlacer()
{
    PrimaryActorTick.bCanEverTick = true;

    USceneComponent* RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
    SetRootComponent(RootComp);

    // Ghost root (detache du root pour etre libre dans le monde)
    GhostRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GhostRoot"));
    GhostRoot->SetupAttachment(RootComp);

    // Ghost base (meme mesh que LaserTurret)
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

    // Ghost head (nouveau mesh FireTurret)
    GhostHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GhostHead"));
    GhostHead->SetupAttachment(GhostRoot);
    GhostHead->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GhostHead->SetVisibility(false);
    GhostHead->SetRelativeLocation(FVector(-25, 0, 55));
    GhostHead->SetRelativeScale3D(FVector(0.4f, 0.4f, 0.4f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> HeadMeshFinder(TEXT("/MonPremierMod/Meshes/Turrets/FireTurret/Head/Meshy_AI_Sci_fi_turret_head_wi_0210071244_texture.Meshy_AI_Sci_fi_turret_head_wi_0210071244_texture"));
    if (HeadMeshFinder.Succeeded())
    {
        GhostHead->SetStaticMesh(HeadMeshFinder.Object);
    }

    // Cercle de portee (cylindre plat)
    RangeCircle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RangeCircle"));
    RangeCircle->SetupAttachment(GhostRoot);
    RangeCircle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RangeCircle->SetVisibility(false);
    RangeCircle->SetRelativeLocation(FVector(0, 0, 0.0f));
    // Sphere = 100u diametre, Range = 4500u -> scale = 4500/50 = 90
    RangeCircle->SetRelativeScale3D(FVector(90.0f, 90.0f, 90.0f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Sphere"));
    if (CylinderMesh.Succeeded())
    {
        RangeCircle->SetStaticMesh(CylinderMesh.Object);
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

void ATDFireTurretPlacer::Equip(AFGCharacterPlayer* character)
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
            if (RangeMaterial)
            {
                RangeMaterial->SetVectorParameterValue(TEXT("ShieldColor"), FLinearColor(0.8f, 0.4f, 0.1f, 1.0f));
                RangeMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.5f);
            }
        }
        if (RangeMaterial)
        {
            RangeCircle->SetMaterial(0, RangeMaterial);
        }
        RangeCircle->SetVisibility(true);
    }
}

void ATDFireTurretPlacer::UnEquip()
{
    if (GhostBase) GhostBase->SetVisibility(false);
    if (GhostHead) GhostHead->SetVisibility(false);
    if (RangeCircle) RangeCircle->SetVisibility(false);
    Super::UnEquip();
}

void ATDFireTurretPlacer::Tick(float DeltaTime)
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

bool ATDFireTurretPlacer::TraceForPlacement(FVector& OutLocation, FRotator& OutRotation) const
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

void ATDFireTurretPlacer::HandleDefaultEquipmentActionEvent(EDefaultEquipmentAction action, EDefaultEquipmentActionEvent actionEvent)
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

void ATDFireTurretPlacer::PlaceTurret()
{
    if (!bValidPlacement) return;

    AFGCharacterPlayer* Player = GetInstigatorCharacter();
    if (!Player) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    ATDFireTurret* Turret = GetWorld()->SpawnActor<ATDFireTurret>(ATDFireTurret::StaticClass(), GhostLocation + FVector(0, 0, 5.0f), GhostRotation, SpawnParams);

    if (Turret)
    {
        UE_LOG(LogTemp, Warning, TEXT("MonPremierMod: FireTurret placee a %s"), *GhostLocation.ToString());

        // Consommer l'item du slot d'equipement
        UFGInventoryComponentEquipment* EquipInv = Player->GetEquipmentSlot(EEquipmentSlot::ES_ARMS);
        if (EquipInv)
        {
            UClass* DescClass = LoadClass<UFGItemDescriptor>(nullptr, TEXT("/MonPremierMod/Items/Desc_TDFireTurret.Desc_TDFireTurret_C"));
            if (DescClass)
            {
                EquipInv->Remove(TSubclassOf<UFGItemDescriptor>(DescClass), 1);
                UE_LOG(LogTemp, Warning, TEXT("MonPremierMod: FireTurret item consomme du slot equipement"));
            }
        }
    }
}
