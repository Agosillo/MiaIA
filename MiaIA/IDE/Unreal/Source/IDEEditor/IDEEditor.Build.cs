// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

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
