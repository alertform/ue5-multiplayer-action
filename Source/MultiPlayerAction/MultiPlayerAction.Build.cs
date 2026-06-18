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
			// UKismetAnimationLibrary::CalculateDirection (strafe Direction in MAAnimInstance)
			"AnimGraphRuntime",
			// AI
			"AIModule", "NavigationSystem",
			// Networking + Session
			"NetCore", "OnlineSubsystem", "OnlineSubsystemUtils",
			// UI
			"UMG", "Slate", "SlateCore",
			"ModelViewViewModel", "FieldNotification",
			// Camera shakes (UDefaultCameraShakeBase + Perlin pattern live in this plugin, enabled by default)
			"EngineCameras",
			// Root-motion warping for the dash slash
			"MotionWarping"
		});
	}
}
