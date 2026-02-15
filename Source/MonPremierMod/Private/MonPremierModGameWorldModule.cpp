#include "MonPremierModGameWorldModule.h"
#include "FGSchematic.h"
#include "FGSchematicManager.h"
#include "FGRecipe.h"
#include "FGRecipeManager.h"
#include "Resources/FGEquipmentDescriptor.h"
#include "TDTurretPlacer.h"
#include "TDFireTurretPlacer.h"
#include "TDShockwaveTurretPlacer.h"
#include "TDDronePlatformPlacer.h"
#include "TDShieldGeneratorPlacer.h"
#include "Registry/ModContentRegistry.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Unlocks/FGUnlockRecipe.h"

UMonPremierModGameWorldModule::UMonPremierModGameWorldModule()
{
    bRootModule = true;
    
    UE_LOG(LogTemp, Warning, TEXT("========== MonPremierMod GameWorldModule Constructor v3.14 =========="));
    
    // Charger le schematic
    static ConstructorHelpers::FClassFinder<UFGSchematic> SchematicFinder(TEXT("/MonPremierMod/Items/Schematic_TDTurret"));
    if (SchematicFinder.Succeeded())
    {
        mSchematics.Add(SchematicFinder.Class);
        UE_LOG(LogTemp, Warning, TEXT("MonPremierMod: Schematic_TDTurret charge! BUILD-320"));
    }

    static ConstructorHelpers::FClassFinder<UFGSchematic> FireSchematicFinder(TEXT("/MonPremierMod/Items/Schematic_TDFireTurret"));
    if (FireSchematicFinder.Succeeded())
    {
        mSchematics.Add(FireSchematicFinder.Class);
        UE_LOG(LogTemp, Warning, TEXT("MonPremierMod: Schematic_TDFireTurret charge!"));
    }

    static ConstructorHelpers::FClassFinder<UFGSchematic> ShockwaveSchematicFinder(TEXT("/MonPremierMod/Items/Schematic_TDShockwaveTurret"));
    if (ShockwaveSchematicFinder.Succeeded())
    {
        mSchematics.Add(ShockwaveSchematicFinder.Class);
        UE_LOG(LogTemp, Warning, TEXT("MonPremierMod: Schematic_TDShockwaveTurret charge!"));
    }

    static ConstructorHelpers::FClassFinder<UFGSchematic> DroneSchematicFinder(TEXT("/MonPremierMod/Items/Schematic_TDDronePlatform"));
    if (DroneSchematicFinder.Succeeded())
    {
        mSchematics.Add(DroneSchematicFinder.Class);
        UE_LOG(LogTemp, Warning, TEXT("MonPremierMod: Schematic_TDDronePlatform charge!"));
    }

    static ConstructorHelpers::FClassFinder<UFGSchematic> ShieldSchematicFinder(TEXT("/MonPremierMod/Items/Schematic_TDShieldGenerator"));
    if (ShieldSchematicFinder.Succeeded())
    {
        mSchematics.Add(ShieldSchematicFinder.Class);
        UE_LOG(LogTemp, Warning, TEXT("MonPremierMod: Schematic_TDShieldGenerator charge!"));
    }
}

void UMonPremierModGameWorldModule::DispatchLifecycleEvent(ELifecyclePhase Phase)
{
    Super::DispatchLifecycleEvent(Phase);

    if (Phase == ELifecyclePhase::CONSTRUCTION)
    {
        UE_LOG(LogTemp, Warning, TEXT("MonPremierMod: GameWorldModule CONSTRUCTION - %d schematics"), mSchematics.Num());
        
        // Forcer mEquipmentClass sur Desc_TDTurret car l'editeur ne peut pas voir la classe C++
        UClass* DescClass = LoadClass<UFGItemDescriptor>(nullptr, TEXT("/MonPremierMod/Items/Desc_TDTurret.Desc_TDTurret_C"));
        if (DescClass)
        {
            UFGEquipmentDescriptor* DescCDO = Cast<UFGEquipmentDescriptor>(DescClass->GetDefaultObject());
            if (DescCDO)
            {
                DescCDO->mEquipmentClass = ATDTurretPlacer::StaticClass();
                UE_LOG(LogTemp, Warning, TEXT("MonPremierMod: mEquipmentClass force sur Desc_TDTurret -> TDTurretPlacer"));
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("MonPremierMod: Desc_TDTurret n'est pas un FGEquipmentDescriptor!"));
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("MonPremierMod: Desc_TDTurret introuvable!"));
        }

        // Forcer mEquipmentClass sur Desc_TDFireTurret
        UClass* FireDescClass = LoadClass<UFGItemDescriptor>(nullptr, TEXT("/MonPremierMod/Items/Desc_TDFireTurret.Desc_TDFireTurret_C"));
        if (FireDescClass)
        {
            UFGEquipmentDescriptor* FireDescCDO = Cast<UFGEquipmentDescriptor>(FireDescClass->GetDefaultObject());
            if (FireDescCDO)
            {
                FireDescCDO->mEquipmentClass = ATDFireTurretPlacer::StaticClass();
                UE_LOG(LogTemp, Warning, TEXT("MonPremierMod: mEquipmentClass force sur Desc_TDFireTurret -> TDFireTurretPlacer"));
            }
        }

        // Forcer mEquipmentClass sur Desc_TDShockwaveTurret
        UClass* ShockDescClass = LoadClass<UFGItemDescriptor>(nullptr, TEXT("/MonPremierMod/Items/Desc_TDShockwaveTurret.Desc_TDShockwaveTurret_C"));
        if (ShockDescClass)
        {
            UFGEquipmentDescriptor* ShockDescCDO = Cast<UFGEquipmentDescriptor>(ShockDescClass->GetDefaultObject());
            if (ShockDescCDO)
            {
                ShockDescCDO->mEquipmentClass = ATDShockwaveTurretPlacer::StaticClass();
                UE_LOG(LogTemp, Warning, TEXT("MonPremierMod: mEquipmentClass force sur Desc_TDShockwaveTurret -> TDShockwaveTurretPlacer"));
            }
        }

        // Forcer mEquipmentClass sur Desc_TDDronePlatform
        UClass* DroneDescClass = LoadClass<UFGItemDescriptor>(nullptr, TEXT("/MonPremierMod/Items/Desc_TDDronePlatform.Desc_TDDronePlatform_C"));
        if (DroneDescClass)
        {
            UFGEquipmentDescriptor* DroneDescCDO = Cast<UFGEquipmentDescriptor>(DroneDescClass->GetDefaultObject());
            if (DroneDescCDO)
            {
                DroneDescCDO->mEquipmentClass = ATDDronePlatformPlacer::StaticClass();
                UE_LOG(LogTemp, Warning, TEXT("MonPremierMod: mEquipmentClass force sur Desc_TDDronePlatform -> TDDronePlatformPlacer"));
            }
        }

        // Forcer mEquipmentClass sur Desc_TDShieldGenerator
        UClass* ShieldDescClass = LoadClass<UFGItemDescriptor>(nullptr, TEXT("/MonPremierMod/Items/Desc_TDShieldGenerator.Desc_TDShieldGenerator_C"));
        if (ShieldDescClass)
        {
            UFGEquipmentDescriptor* ShieldDescCDO = Cast<UFGEquipmentDescriptor>(ShieldDescClass->GetDefaultObject());
            if (ShieldDescCDO)
            {
                ShieldDescCDO->mEquipmentClass = ATDShieldGeneratorPlacer::StaticClass();
                UE_LOG(LogTemp, Warning, TEXT("MonPremierMod: mEquipmentClass force sur Desc_TDShieldGenerator -> TDShieldGeneratorPlacer"));
            }
        }

        // === Forcer mConveyorMesh sur tous les descripteurs pour remplacer le cube blanc ===
        auto SetConveyorMesh = [](const TCHAR* DescPath, const TCHAR* MeshPath)
        {
            UClass* DC = LoadClass<UFGItemDescriptor>(nullptr, DescPath);
            UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, MeshPath);
            if (DC && Mesh)
            {
                UFGItemDescriptor* CDO = DC->GetDefaultObject<UFGItemDescriptor>();
                FProperty* Prop = DC->FindPropertyByName(TEXT("mConveyorMesh"));
                if (CDO && Prop)
                {
                    UStaticMesh** MeshPtr = Prop->ContainerPtrToValuePtr<UStaticMesh*>(CDO);
                    *MeshPtr = Mesh;
                    UE_LOG(LogTemp, Warning, TEXT("MonPremierMod: mConveyorMesh force sur %s"), DescPath);
                }
            }
        };

        const TCHAR* CrateMesh = TEXT("/MonPremierMod/Meshes/Items/Crate_Drop.Crate_Drop");
        SetConveyorMesh(TEXT("/MonPremierMod/Items/Desc_TDTurret.Desc_TDTurret_C"), CrateMesh);
        SetConveyorMesh(TEXT("/MonPremierMod/Items/Desc_TDFireTurret.Desc_TDFireTurret_C"), CrateMesh);
        SetConveyorMesh(TEXT("/MonPremierMod/Items/Desc_TDShockwaveTurret.Desc_TDShockwaveTurret_C"), CrateMesh);
        SetConveyorMesh(TEXT("/MonPremierMod/Items/Desc_TDDronePlatform.Desc_TDDronePlatform_C"), CrateMesh);
        SetConveyorMesh(TEXT("/MonPremierMod/Items/Desc_TDDrone.Desc_TDDrone_C"), CrateMesh);
        SetConveyorMesh(TEXT("/MonPremierMod/Items/Desc_TDShieldGenerator.Desc_TDShieldGenerator_C"), CrateMesh);
    }
    
    // SML base class auto-registers mSchematics during CONSTRUCTION via Super::DispatchLifecycleEvent
    
    if (Phase == ELifecyclePhase::POST_INITIALIZATION)
    {
        // Diagnostic: inspecter les unlocks du schematic FireTurret
        for (TSubclassOf<UFGSchematic> SchematicClass : mSchematics)
        {
            if (!SchematicClass) continue;
            
            UFGSchematic* SchematicCDO = SchematicClass->GetDefaultObject<UFGSchematic>();
            if (!SchematicCDO) continue;
            
            UE_LOG(LogTemp, Warning, TEXT("MonPremierMod: === Schematic %s ==="), *SchematicClass->GetName());
            
            TArray<UFGUnlock*> Unlocks = UFGSchematic::GetUnlocks(SchematicClass);
            UE_LOG(LogTemp, Warning, TEXT("MonPremierMod:   Unlocks count: %d"), Unlocks.Num());
            
            for (UFGUnlock* Unlock : Unlocks)
            {
                if (!Unlock) continue;
                UE_LOG(LogTemp, Warning, TEXT("MonPremierMod:   Unlock type: %s"), *Unlock->GetClass()->GetName());
                
                UFGUnlockRecipe* RecipeUnlock = Cast<UFGUnlockRecipe>(Unlock);
                if (RecipeUnlock)
                {
                    TArray<TSubclassOf<UFGRecipe>> Recipes = RecipeUnlock->GetRecipesToUnlock();
                    UE_LOG(LogTemp, Warning, TEXT("MonPremierMod:   Recipes in unlock: %d"), Recipes.Num());
                    for (auto RecipeClass : Recipes)
                    {
                        if (RecipeClass)
                        {
                            UE_LOG(LogTemp, Warning, TEXT("MonPremierMod:     Recipe: %s"), *RecipeClass->GetName());
                            
                            // Inspecter mProducedIn
                            TArray<TSubclassOf<UObject>> ProducedIn = UFGRecipe::GetProducedIn(RecipeClass);
                            UE_LOG(LogTemp, Warning, TEXT("MonPremierMod:     ProducedIn count: %d"), ProducedIn.Num());
                            for (int32 idx = 0; idx < ProducedIn.Num(); idx++)
                            {
                                UE_LOG(LogTemp, Warning, TEXT("MonPremierMod:       ProducedIn[%d]: %s"), idx, ProducedIn[idx] ? *ProducedIn[idx]->GetName() : TEXT("NULL"));
                            }
                            
                            // Inspecter products
                            TArray<FItemAmount> Products = UFGRecipe::GetProducts(RecipeClass);
                            UE_LOG(LogTemp, Warning, TEXT("MonPremierMod:     Products count: %d"), Products.Num());
                            for (auto& P : Products)
                            {
                                UE_LOG(LogTemp, Warning, TEXT("MonPremierMod:       Product: %s x%d"), P.ItemClass ? *P.ItemClass->GetName() : TEXT("NULL"), P.Amount);
                            }
                        }
                    }
                }
            }
        }
        
        // Forcer le deblocage du schematic pour le joueur
        AFGSchematicManager* SchematicManager = AFGSchematicManager::Get(GetWorld());
        if (SchematicManager)
        {
            for (TSubclassOf<UFGSchematic> SchematicClass : mSchematics)
            {
                if (SchematicClass)
                {
                    SchematicManager->GiveAccessToSchematic(SchematicClass, nullptr);
                    UE_LOG(LogTemp, Warning, TEXT("MonPremierMod: Schematic debloque pour le joueur! %s"), *SchematicClass->GetName());
                }
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("MonPremierMod: SchematicManager introuvable!"));
        }
    }
}
