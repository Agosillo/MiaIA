// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

using UnrealBuildTool;
using System.IO;

public class IDE : ModuleRules
{
	public IDE(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });


        string MiaIARoot = Path.GetFullPath(Path.Combine(
            ModuleDirectory,
            "..",
            "..",
            "..",
            ".."
        ));

        PublicIncludePaths.Add(
              Path.Combine(MiaIARoot, "SDK", "Include")
          );

        PublicIncludePaths.Add(
            Path.Combine(MiaIARoot, "Core", "Public")
        );

        PublicIncludePaths.Add(
            Path.Combine(MiaIARoot, "CLI", "Include")
        );

        PublicIncludePaths.Add(
            Path.Combine(MiaIARoot, "IDE", "StudioCore", "Include")
        );

        PublicAdditionalLibraries.Add(
            Path.Combine(MiaIARoot, "x64", "Release", "StudioCore.lib")
        );

        PublicAdditionalLibraries.Add(
            Path.Combine(MiaIARoot, "x64", "Release", "CLI.lib")
        );

        PublicAdditionalLibraries.Add(
            Path.Combine(MiaIARoot, "x64", "Release", "SDK.lib")
        );

        PublicAdditionalLibraries.Add(
            Path.Combine(MiaIARoot, "x64", "Release", "Engine.lib")
        );

        string VcpkgLibraryDirectory = Path.Combine(
            System.Environment.GetFolderPath(
                System.Environment.SpecialFolder.LocalApplicationData),
            "MiaIA",
            "vcpkg_installed",
            "x64-windows-static-md-v143",
            "lib"
        );

        if (!Directory.Exists(VcpkgLibraryDirectory))
        {
            throw new BuildException(
                $"MiaIA vcpkg libraries were not found at '{VcpkgLibraryDirectory}'. " +
                "Build the MiaIA native solution in Release | x64 first."
            );
        }

        string[] VcpkgLibraries = Directory.GetFiles(
            VcpkgLibraryDirectory,
            "*.lib"
        );
        System.Array.Sort(
            VcpkgLibraries,
            System.StringComparer.OrdinalIgnoreCase
        );
        PublicAdditionalLibraries.AddRange(VcpkgLibraries);


        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}
