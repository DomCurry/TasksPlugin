// Copyright Dominic Curry. All Rights Reserved.
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
