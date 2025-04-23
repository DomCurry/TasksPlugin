// Copyright Dominic Curry. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

public class AsyncFutures : ModuleRules
{
	public AsyncFutures(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePathModuleNames.AddRange(new string[] {
		});

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
		});
	}
}