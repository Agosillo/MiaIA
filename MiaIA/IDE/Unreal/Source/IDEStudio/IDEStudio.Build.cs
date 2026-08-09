// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class IDEStudio : ModuleRules
{
    public IDEStudio(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "IDE",
            "InputCore",
            "Slate",
            "SlateCore",
            "UMG"
        });
    }
}
