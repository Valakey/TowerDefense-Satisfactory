using UnrealBuildTool;
using System.IO;
using System;

public class MonPremierMod : ModuleRules
{
    public MonPremierMod(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "NavigationSystem",
            "AIModule",
            "FactoryGame",
            "SML",
            "UMG",
            "Slate",
            "SlateCore",
            "Niagara"
        });

        if (Target.Type == TargetRules.TargetType.Editor)
        {
            PublicDependencyModuleNames.AddRange(new string[] {
                "OnlineBlueprintSupport",
                "AnimGraph"
            });
        }
    }
}
