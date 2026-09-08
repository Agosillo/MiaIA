// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

using UnrealBuildTool;
using System;

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

        // The online assistant is a normal Studio feature. Keep an explicit
        // opt-out for restricted/offline builds, but do not make Visual Studio
        // inherit a special environment variable just to compile the UI.
        bool WithWitAI = !string.Equals(
            Environment.GetEnvironmentVariable("MIAIA_WITH_WIT_AI"),
            "0",
            StringComparison.Ordinal);

        PublicDefinitions.Add(
            "MIAIA_WITH_WIT_AI=" + (WithWitAI ? "1" : "0"));

        if (WithWitAI)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "HTTP",
                "Json"
            });
        }

    }
}
