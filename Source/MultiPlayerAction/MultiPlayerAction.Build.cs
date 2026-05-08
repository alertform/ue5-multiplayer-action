// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MultiPlayerAction : ModuleRules
{
	public MultiPlayerAction(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			// GAS
			"GameplayAbilities", "GameplayTags", "GameplayTasks",
			// AI
			"AIModule",
			// Networking
			"NetCore", "OnlineSubsystem",
			// UI
			"UMG", "Slate", "SlateCore"
		});
	}
}
