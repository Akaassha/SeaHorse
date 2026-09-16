// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SeaHorse : ModuleRules
{
	public SeaHorse(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicIncludePaths.Add(ModuleDirectory);
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "CommonUI", "CommonInput", "GameplayTags", "DeveloperSettings", "SlateCore", "PropertyPath", "OnlineSubsystem" });

		PrivateDependencyModuleNames.AddRange(new string[] { "OnlineSubsystemUtils", "Slate", "PreLoadScreen", "Niagara" });
		DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");
		bool bSteamAvatars = Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Linux || Target.Platform == UnrealTargetPlatform.Mac;
		PublicDefinitions.Add("SH_WITH_STEAM_AVATARS=" + (bSteamAvatars ? "1" : "0"));
		if (bSteamAvatars) { AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks"); }

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UMGEditor");
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
		
		// OnlineSubsystemSteam is enabled in SeaHorse.uproject and loaded dynamically above.
	}
}
