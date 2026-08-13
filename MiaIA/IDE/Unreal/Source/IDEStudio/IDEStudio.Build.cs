// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

using UnrealBuildTool;

public class IDEStudio : ModuleRules
{
    public IDEStudio(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "AppFramework",
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
