// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SeaHorse : ModuleRules
{
	public SeaHorse(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicIncludePaths.Add(ModuleDirectory);
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "CommonUI", "CommonInput", "GameplayTags", "DeveloperSettings", "SlateCore", "PropertyPath", "OnlineSubsystem" });

		PrivateDependencyModuleNames.AddRange(new string[] { "OnlineSubsystemUtils", "Slate", "PreLoadScreen" });
		DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UMGEditor");
		}
		
		// OnlineSubsystemSteam is enabled in SeaHorse.uproject and loaded dynamically above.
	}
}
