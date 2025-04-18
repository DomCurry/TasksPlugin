// Copyright(c) Dominic Curry. All rights reserved.
using UnrealBuildTool;

public class AsyncFutureTests : ModuleRules
{
	public AsyncFutureTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
        });

        PrivateDependencyModuleNames.AddRange(new string[] {
			"AsyncFutures"
		});
	}
}
