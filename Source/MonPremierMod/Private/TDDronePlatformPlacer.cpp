#include "TDDronePlatformPlacer.h"
#include "TDDronePlatform.h"
#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"
#include "Equipment/FGEquipment.h"
#include "Resources/FGItemDescriptor.h"
#include "FGInventoryComponentEquipment.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

ATDDronePlatformPlacer::ATDDronePlatformPlacer()
{
    PrimaryActorTick.bCanEverTick = true;

    bValidPlacement = false;

    mEquipmentSlot = EEquipmentSlot::ES_ARMS;
    mDefaultEquipmentActions = static_cast<uint8>(EDefaultEquipmentAction::PrimaryFire);
    mNeedsDefaultEquipmentMappingContext = true;

    USceneComponent* RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
    SetRootComponent(RootComp);

    // Ghost root (detache du root pour etre libre dans le monde)
    GhostRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GhostRoot"));
    GhostRoot->SetupAttachment(RootComp);

    // Ghost platform
    GhostPlatform = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GhostPlatform"));
    GhostPlatform->SetupAttachment(GhostRoot);
    GhostPlatform->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GhostPlatform->SetVisibility(false);
    GhostPlatform->SetRelativeLocation(FVector(0, 0, 70.0f));
    GhostPlatform->SetRelativeScale3D(FVector(1.5f, 1.5f, 1.5f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> PlatformMeshObj(TEXT("/MonPremierMod/Meshes/PlatformDrone/Platform/Platform.Platform"));
    if (PlatformMeshObj.Succeeded())
    {
        GhostPlatform->SetStaticMesh(PlatformMeshObj.Object);
    }

    // Ghost drone
    GhostDrone = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GhostDrone"));
    GhostDrone->SetupAttachment(GhostRoot);
    GhostDrone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GhostDrone->SetVisibility(false);
    GhostDrone->SetRelativeLocation(FVector(0, 0, 90.0f));
    GhostDrone->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> DroneMeshObj(TEXT("/MonPremierMod/Meshes/PlatformDrone/Drone/Drone.Drone"));
    if (DroneMeshObj.Succeeded())
    {
        GhostDrone->SetStaticMesh(DroneMeshObj.Object);
    }

    // Sphere de portee (DetectionRange drone = 2000u, sphere 100u diametre -> scale = 2000/50 = 40)
    RangeCircle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RangeCircle"));
    RangeCircle->SetupAttachment(GhostRoot);
    RangeCircle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RangeCircle->SetVisibility(false);
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
}

void ATDDronePlatformPlacer::Equip(AFGCharacterPlayer* character)
{
    Super::Equip(character);

    if (GhostRoot)
    {
        GhostRoot->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
    }
    if (GhostPlatform)
    {
        GhostPlatform->SetVisibility(true);
        GhostPlatform->SetRenderCustomDepth(true);
        GhostPlatform->SetCustomDepthStencilValue(5);
    }
    if (GhostDrone)
    {
        GhostDrone->SetVisibility(true);
        GhostDrone->SetRenderCustomDepth(true);
        GhostDrone->SetCustomDepthStencilValue(5);
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

void ATDDronePlatformPlacer::UnEquip()
{
    if (GhostPlatform) GhostPlatform->SetVisibility(false);
    if (GhostDrone) GhostDrone->SetVisibility(false);
    if (RangeCircle) RangeCircle->SetVisibility(false);
    Super::UnEquip();
}

void ATDDronePlatformPlacer::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!IsEquipped() || !GhostRoot) return;

    FVector TraceLocation;
    FRotator TraceRotation;
    bValidPlacement = TraceForPlacement(TraceLocation, TraceRotation);

    if (bValidPlacement)
    {
        GhostLocation = TraceLocation;
        GhostRotation = TraceRotation;
        GhostRoot->SetWorldLocation(GhostLocation);
        GhostRoot->SetWorldRotation(GhostRotation);
        if (GhostPlatform) GhostPlatform->SetVisibility(true);
        if (GhostDrone) GhostDrone->SetVisibility(true);
        if (RangeCircle) RangeCircle->SetVisibility(true);
    }
    else
    {
        if (GhostPlatform) GhostPlatform->SetVisibility(false);
        if (GhostDrone) GhostDrone->SetVisibility(false);
        if (RangeCircle) RangeCircle->SetVisibility(false);
    }
}

bool ATDDronePlatformPlacer::TraceForPlacement(FVector& OutLocation, FRotator& OutRotation) const
{
    AFGCharacterPlayer* Player = GetInstigatorCharacter();
    if (!Player) return false;

    APlayerController* PC = Cast<APlayerController>(Player->GetController());
    if (!PC) return false;

    FVector CamLoc;
    FRotator CamRot;
    PC->GetPlayerViewPoint(CamLoc, CamRot);

    FVector TraceStart = CamLoc;
    FVector TraceEnd = CamLoc + CamRot.Vector() * PlaceDistance;

    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    Params.AddIgnoredActor(Player);

    bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params);

    if (bHit)
    {
        OutLocation = Hit.ImpactPoint;
        OutRotation = FRotator(0.0f, Player->GetActorRotation().Yaw, 0.0f);
        return true;
    }

    return false;
}

void ATDDronePlatformPlacer::HandleDefaultEquipmentActionEvent(EDefaultEquipmentAction action, EDefaultEquipmentActionEvent actionEvent)
{
    Super::HandleDefaultEquipmentActionEvent(action, actionEvent);

    if (action == EDefaultEquipmentAction::PrimaryFire && actionEvent == EDefaultEquipmentActionEvent::Pressed)
    {
        if (HasAuthority())
        {
            PlacePlatform();
        }
    }
}

void ATDDronePlatformPlacer::PlacePlatform()
{
    if (!bValidPlacement) return;

    AFGCharacterPlayer* Player = GetInstigatorCharacter();
    if (!Player) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    ATDDronePlatform* Platform = GetWorld()->SpawnActor<ATDDronePlatform>(ATDDronePlatform::StaticClass(), GhostLocation + FVector(0, 0, 5.0f), GhostRotation, SpawnParams);

    if (Platform)
    {
        UE_LOG(LogTemp, Warning, TEXT("MonPremierMod: DronePlatform placee a %s"), *GhostLocation.ToString());

        // Consommer l'item du slot d'equipement
        UFGInventoryComponentEquipment* EquipInv = Player->GetEquipmentSlot(EEquipmentSlot::ES_ARMS);
        if (EquipInv)
        {
            UClass* DescClass = LoadClass<UFGItemDescriptor>(nullptr, TEXT("/MonPremierMod/Items/Desc_TDDronePlatform.Desc_TDDronePlatform_C"));
            if (DescClass)
            {
                EquipInv->Remove(TSubclassOf<UFGItemDescriptor>(DescClass), 1);
                UE_LOG(LogTemp, Warning, TEXT("MonPremierMod: DronePlatform item consomme du slot equipement"));
            }
        }
    }
}
