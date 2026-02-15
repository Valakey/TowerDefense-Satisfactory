#include "TDWorldSubsystem.h"
#include "TDCreatureSpawner.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

void UTDWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Warning, TEXT("TDWorldSubsystem: Initialize"));
    if (GetWorld())
    {
        GetWorld()->OnWorldBeginPlay.AddUObject(this, &UTDWorldSubsystem::OnWorldBeginPlay);
    }
}

void UTDWorldSubsystem::Deinitialize()
{
    Super::Deinitialize();
}

bool UTDWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    if (UWorld* World = Cast<UWorld>(Outer))
    {
        return World->IsGameWorld();
    }
    return false;
}

void UTDWorldSubsystem::OnWorldBeginPlay()
{
    UE_LOG(LogTemp, Warning, TEXT("TDWorldSubsystem: Monde pret! Creation spawner..."));
    if (!SpawnerInstance && GetWorld())
    {
        APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
        FVector SpawnLoc = PlayerPawn ? PlayerPawn->GetActorLocation() : FVector::ZeroVector;
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        SpawnerInstance = GetWorld()->SpawnActor<ATDCreatureSpawner>(ATDCreatureSpawner::StaticClass(), SpawnLoc, FRotator::ZeroRotator, Params);
        if (SpawnerInstance)
        {
            UE_LOG(LogTemp, Warning, TEXT("TDWorldSubsystem: Spawner cree! Vagues dans 60s"));
        }
    }
}