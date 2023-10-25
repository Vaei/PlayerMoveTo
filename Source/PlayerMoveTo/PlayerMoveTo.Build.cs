// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PlayerMoveTo : ModuleRules
{
	public PlayerMoveTo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"GameplayTasks", 
				"GameplayAbilities",
			}
			);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"NavigationSystem",
				"AIModule",
			}
			);
	}
}
