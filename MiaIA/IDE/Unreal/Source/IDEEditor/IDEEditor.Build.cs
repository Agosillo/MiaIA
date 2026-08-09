// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class IDEEditor : ModuleRules
{
    public IDEEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "IDE",
            "IDEStudio"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AssetRegistry",
            "BlueprintEditorLibrary",
            "BlueprintGraph",
            "InputCore",
            "Kismet",
            "Slate",
            "SlateCore",
            "UnrealEd"
        });
    }
}
