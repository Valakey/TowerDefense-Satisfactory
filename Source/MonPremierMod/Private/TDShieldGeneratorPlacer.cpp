#include "TDShieldGeneratorPlacer.h"
#include "TDShieldGenerator.h"
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

ATDShieldGeneratorPlacer::ATDShieldGeneratorPlacer()
{
    PrimaryActorTick.bCanEverTick = true;

    USceneComponent* RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
    SetRootComponent(RootComp);

    GhostRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GhostRoot"));
    GhostRoot->SetupAttachment(RootComp);

    // Ghost base (Tesla coil mesh)
    GhostBase = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GhostBase"));
    GhostBase->SetupAttachment(GhostRoot);
    GhostBase->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GhostBase->SetVisibility(false);
    GhostBase->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));
    GhostBase->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> BaseMeshFinder(TEXT("/MonPremierMod/Meshes/Turrets/TeslaTurret/TeslaTurret.TeslaTurret"));
    if (BaseMeshFinder.Succeeded())
    {
        GhostBase->SetStaticMesh(BaseMeshFinder.Object);
    }

    // Ghost dome (sphere pour la portee du bouclier)
    GhostDome = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GhostDome"));
    GhostDome->SetupAttachment(GhostRoot);
    GhostDome->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GhostDome->SetVisibility(false);
    // Shield radius 2000u, sphere base = 100u diameter, scale = 2000/50 = 40
    GhostDome->SetRelativeScale3D(FVector(40.0f, 40.0f, 40.0f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshObj(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereMeshObj.Succeeded())
    {
        GhostDome->SetStaticMesh(SphereMeshObj.Object);
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> RangeMatFinder(TEXT("/MonPremierMod/Materials/M_ShieldDome.M_ShieldDome"));
    if (RangeMatFinder.Succeeded())
    {
        RangeBaseMaterial = RangeMatFinder.Object;
    }

    mEquipmentSlot = EEquipmentSlot::ES_ARMS;
    mDefaultEquipmentActions = static_cast<int32>(EDefaultEquipmentAction::PrimaryFire);
    mNeedsDefaultEquipmentMappingContext = true;
}

void ATDShieldGeneratorPlacer::Tick(float DeltaTime)
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
        if (GhostBase) GhostBase->SetVisibility(true);
        if (GhostDome) GhostDome->SetVisibility(true);
    }
    else
    {
        if (GhostBase) GhostBase->SetVisibility(false);
        if (GhostDome) GhostDome->SetVisibility(false);
    }
}

void ATDShieldGeneratorPlacer::Equip(AFGCharacterPlayer* character)
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
    if (GhostDome)
    {
        if (!RangeMaterial && RangeBaseMaterial)
        {
            RangeMaterial = UMaterialInstanceDynamic::Create(RangeBaseMaterial, this);
        }
        if (RangeMaterial)
        {
            GhostDome->SetMaterial(0, RangeMaterial);
        }
        GhostDome->SetVisibility(true);
    }
}

void ATDShieldGeneratorPlacer::UnEquip()
{
    if (GhostBase) GhostBase->SetVisibility(false);
    if (GhostDome) GhostDome->SetVisibility(false);
    Super::UnEquip();
}

void ATDShieldGeneratorPlacer::HandleDefaultEquipmentActionEvent(EDefaultEquipmentAction action, EDefaultEquipmentActionEvent actionEvent)
{
    Super::HandleDefaultEquipmentActionEvent(action, actionEvent);

    if (action == EDefaultEquipmentAction::PrimaryFire && actionEvent == EDefaultEquipmentActionEvent::Pressed)
    {
        if (bValidPlacement)
        {
            PlaceShieldGenerator();
        }
    }
}

void ATDShieldGeneratorPlacer::PlaceShieldGenerator()
{
    if (!GetWorld()) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ATDShieldGenerator* Shield = GetWorld()->SpawnActor<ATDShieldGenerator>(
        ATDShieldGenerator::StaticClass(), GhostLocation, GhostRotation, SpawnParams);

    if (Shield)
    {
        UE_LOG(LogTemp, Warning, TEXT("TDShieldGenerator place a %s"), *GhostLocation.ToString());

        // Consommer l'item du slot d'equipement
        AFGCharacterPlayer* Player = Cast<AFGCharacterPlayer>(GetInstigator());
        if (Player)
        {
            UFGInventoryComponentEquipment* EquipInv = Player->GetEquipmentSlot(EEquipmentSlot::ES_ARMS);
            if (EquipInv)
            {
                UClass* DescClass = LoadClass<UFGItemDescriptor>(nullptr, TEXT("/MonPremierMod/Items/Desc_TDShieldGenerator.Desc_TDShieldGenerator_C"));
                if (DescClass)
                {
                    EquipInv->Remove(TSubclassOf<UFGItemDescriptor>(DescClass), 1);
                    UE_LOG(LogTemp, Warning, TEXT("TDShieldGenerator: Item consomme du slot equipement"));
                }
            }
        }
    }
}

bool ATDShieldGeneratorPlacer::TraceForPlacement(FVector& OutLocation, FRotator& OutRotation) const
{
    APlayerController* PC = Cast<APlayerController>(GetInstigatorController());
    if (!PC) return false;

    FVector CamLoc;
    FRotator CamRot;
    PC->GetPlayerViewPoint(CamLoc, CamRot);

    FVector TraceEnd = CamLoc + CamRot.Vector() * PlaceDistance;

    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    if (GetInstigator()) Params.AddIgnoredActor(GetInstigator());

    if (GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, TraceEnd, ECC_WorldStatic, Params))
    {
        OutLocation = Hit.ImpactPoint;
        OutRotation = FRotator(0, CamRot.Yaw, 0);
        return true;
    }

    OutLocation = TraceEnd;
    OutRotation = FRotator(0, CamRot.Yaw, 0);
    return false;
}
