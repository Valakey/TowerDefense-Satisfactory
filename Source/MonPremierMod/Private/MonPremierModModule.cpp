#include "MonPremierModModule.h"
#include "Modules/ModuleManager.h"
#include "Engine/World.h"
#include "TDCreatureSpawner.h"
#include "Patching/NativeHookManager.h"
#include "FGSchematicManager.h"
#include "FGSchematic.h"
#include "Registry/ModContentRegistry.h"

void FMonPremierModModule::StartupModule()
{
    UE_LOG(LogTemp, Warning, TEXT("MonPremierMod - Tower Defense charge !"));
    
    // Charger le schematic
    UClass* SchematicClass = LoadClass<UFGSchematic>(nullptr, TEXT("/MonPremierMod/Items/Schematic_TDTurret.Schematic_TDTurret_C"));
    if (SchematicClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("MonPremierMod: Schematic_TDTurret charge avec succes!"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("MonPremierMod: ERREUR - Impossible de charger Schematic_TDTurret!"));
    }
}

void FMonPremierModModule::ShutdownModule()
{
    UE_LOG(LogTemp, Warning, TEXT("MonPremierMod - Tower Defense decharge"));
}

IMPLEMENT_GAME_MODULE(FMonPremierModModule, MonPremierMod);
