#include "TDDrone.h"
#include "TDDronePlatform.h"
#include "TDFireTurret.h"
#include "EngineUtils.h"

ATDDrone::ATDDrone()
{
    PrimaryActorTick.bCanEverTick = true;

    // Root scene (pour que le mesh puisse avoir un offset de rotation)
    DroneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DroneRoot"));
    RootComponent = DroneRoot;

    // Drone mesh attache au root avec rotation offset
    DroneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DroneMesh"));
    DroneMesh->SetupAttachment(DroneRoot);
    DroneMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> DroneMeshObj(TEXT("/MonPremierMod/Meshes/PlatformDrone/Drone/Drone.Drone"));
    if (DroneMeshObj.Succeeded())
    {
        DroneMesh->SetStaticMesh(DroneMeshObj.Object);
        UE_LOG(LogTemp, Warning, TEXT("TDDrone: Drone mesh LOADED!"));
    }
    DroneMesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));
    DroneMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
}

void ATDDrone::BeginPlay()
{
    Super::BeginPlay();

    BaseIdleLocation = GetActorLocation();
    UE_LOG(LogTemp, Warning, TEXT("TDDrone: BeginPlay at %s"), *GetActorLocation().ToString());
}

void ATDDrone::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!OwnerPlatform || !IsValid(OwnerPlatform))
    {
        // Platform detruite, se detruire aussi
        Destroy();
        return;
    }

    switch (DroneState)
    {
    case EDroneState::Idle:
    {
        // Smooth rotation reset au retour
        if (bResettingRotation)
        {
            FRotator CurRot = GetActorRotation();
            FRotator NewRot = FMath::RInterpTo(CurRot, ResetTargetRotation, DeltaTime, 3.0f);
            SetActorRotation(NewRot);
            if (CurRot.Equals(ResetTargetRotation, 1.0f))
            {
                bResettingRotation = false;
            }
        }

        // Animation hover leger
        IdleBobTimer += DeltaTime;
        FVector LandingLoc = OwnerPlatform->GetDroneLandingLocation();
        float BobOffset = FMath::Sin(IdleBobTimer * 2.0f) * 15.0f;
        SetActorLocation(FMath::VInterpTo(GetActorLocation(), LandingLoc + FVector(0, 0, BobOffset), DeltaTime, 5.0f));

        // Scanner pour des tourelles qui ont besoin de munitions
        ScanTimer += DeltaTime;
        if (ScanTimer >= ScanInterval)
        {
            ScanTimer = 0.0f;

            if (OwnerPlatform->HasAmmo())
            {
                ATDFireTurret* Turret = FindTurretNeedingAmmo();
                if (Turret)
                {
                    TargetTurret = Turret;
                    PickupAmmoFromPlatform();

                    if (CarriedAmmo > 0)
                    {
                        DroneState = EDroneState::FlyingToTurret;
                        UE_LOG(LogTemp, Verbose, TEXT("TDDrone: Flying to turret %s with %d ammo"), *TargetTurret->GetName(), CarriedAmmo);
                    }
                }
            }
        }
        break;
    }

    case EDroneState::FlyingToTurret:
    {
        if (!TargetTurret || !IsValid(TargetTurret))
        {
            // Turret detruite, retourner
            DroneState = EDroneState::Returning;
            break;
        }

        // Voler vers la tourelle (au-dessus d'elle)
        FVector TargetLoc = TargetTurret->GetActorLocation() + FVector(0, 0, HoverHeight);
        FlyTowards(TargetLoc, DeltaTime);

        float DistToTarget = FVector::Dist(GetActorLocation(), TargetLoc);
        if (DistToTarget <= DeliveryDistance)
        {
            // Arrive a la tourelle
            DroneState = EDroneState::Delivering;
            DeliverTimer = 0.0f;
            UE_LOG(LogTemp, Verbose, TEXT("TDDrone: Arrived at turret, delivering..."));
        }
        break;
    }

    case EDroneState::Delivering:
    {
        DeliverTimer += DeltaTime;

        // Rester en hover au-dessus de la tourelle et regarder vers elle
        if (TargetTurret && IsValid(TargetTurret))
        {
            FVector HoverLoc = TargetTurret->GetActorLocation() + FVector(0, 0, HoverHeight);
            SetActorLocation(FMath::VInterpTo(GetActorLocation(), HoverLoc, DeltaTime, 3.0f));

            // Rester droit (pitch=0) pendant la livraison, garder le yaw actuel
            FRotator CurRot = GetActorRotation();
            FRotator FlatRot = FRotator(0.0f, CurRot.Yaw, 0.0f);
            SetActorRotation(FMath::RInterpTo(CurRot, FlatRot, DeltaTime, 5.0f));
        }

        if (DeliverTimer >= DeliverDuration)
        {
            // Livrer les munitions
            if (TargetTurret && IsValid(TargetTurret) && CarriedAmmo > 0)
            {
                TargetTurret->Reload(CarriedAmmo);
                UE_LOG(LogTemp, Verbose, TEXT("TDDrone: Delivered %d ammo to turret %s"), CarriedAmmo, *TargetTurret->GetName());
                CarriedAmmo = 0;
            }

            // Retourner a la platform
            DroneState = EDroneState::Returning;
            TargetTurret = nullptr;
        }
        break;
    }

    case EDroneState::Returning:
    {
        FVector HomeLoc = OwnerPlatform->GetDroneLandingLocation();
        FlyTowards(HomeLoc, DeltaTime);

        float DistToHome = FVector::Dist(GetActorLocation(), HomeLoc);
        if (DistToHome <= DeliveryDistance)
        {
            // Snap position mais smooth la rotation vers celle de la platform
            SetActorLocation(HomeLoc);
            bResettingRotation = true;
            ResetTargetRotation = OwnerPlatform->GetActorRotation();
            DroneState = EDroneState::Idle;
            ScanTimer = 0.0f;
            IdleBobTimer = 0.0f;
            UE_LOG(LogTemp, Verbose, TEXT("TDDrone: Returned to platform"));
        }
        break;
    }

    default:
        break;
    }
}

ATDFireTurret* ATDDrone::FindTurretNeedingAmmo()
{
    if (!OwnerPlatform) return nullptr;

    FVector MyLoc = OwnerPlatform->GetActorLocation();
    ATDFireTurret* BestTurret = nullptr;
    float BestDistance = DetectionRange;

    for (TActorIterator<ATDFireTurret> It(GetWorld()); It; ++It)
    {
        ATDFireTurret* Turret = *It;
        if (!Turret || !IsValid(Turret)) continue;

        // Verifier si la tourelle a besoin de munitions
        if (Turret->GetCurrentAmmo() >= Turret->MaxAmmo) continue;

        float Dist = FVector::Dist(MyLoc, Turret->GetActorLocation());
        if (Dist < BestDistance)
        {
            BestDistance = Dist;
            BestTurret = Turret;
        }
    }

    return BestTurret;
}

void ATDDrone::FlyTowards(FVector Target, float DeltaTime)
{
    FVector CurrentLoc = GetActorLocation();
    FVector Direction = (Target - CurrentLoc).GetSafeNormal();
    FVector NewLoc = CurrentLoc + Direction * FlySpeed * DeltaTime;

    // Smooth interpolation
    NewLoc = FMath::VInterpTo(CurrentLoc, Target, DeltaTime, FlySpeed / FMath::Max(FVector::Dist(CurrentLoc, Target), 1.0f) * 5.0f);

    SetActorLocation(NewLoc);

    // Orienter le drone vers la direction de vol
    if (!Direction.IsNearlyZero())
    {
        FRotator TargetRot = Direction.Rotation();
        FRotator CurrentRot = GetActorRotation();
        FRotator NewRot = FMath::RInterpTo(CurrentRot, TargetRot, DeltaTime, 5.0f);
        SetActorRotation(NewRot);
    }
}

void ATDDrone::PickupAmmoFromPlatform()
{
    if (!OwnerPlatform || !TargetTurret) return;

    // Calculer combien de munitions la tourelle a besoin
    int32 AmmoNeeded = TargetTurret->MaxAmmo - TargetTurret->GetCurrentAmmo();
    if (AmmoNeeded <= 0) return;

    // Prendre le maximum possible de la platform
    CarriedAmmo = OwnerPlatform->TakeAmmo(AmmoNeeded);

    UE_LOG(LogTemp, Verbose, TEXT("TDDrone: Picked up %d ammo from platform (turret needs %d)"), CarriedAmmo, AmmoNeeded);
}
