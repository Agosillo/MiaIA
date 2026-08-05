// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class IDE : ModuleRules
{
	public IDE(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });


        string MiaIARoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", ".."));

        PublicIncludePaths.Add(
              Path.Combine(MiaIARoot, "SDK", "Include")
          );

        PublicIncludePaths.Add(
            Path.Combine(MiaIARoot, "Core", "Public")
        );

        PublicAdditionalLibraries.Add(
            Path.Combine(MiaIARoot, "x64", "Release", "SDK.lib")
        );


        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}
