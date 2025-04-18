// Copyright(c) Dominic Curry. All rights reserved.
using UnrealBuildTool;
using System.IO;

public class AsyncFutures : ModuleRules
{
	public AsyncFutures(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePathModuleNames.AddRange(new string[] {
			"Core"
		});

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
		});
	}
}