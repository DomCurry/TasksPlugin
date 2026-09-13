// Copyright Dominic Curry. All Rights Reserved.
using UnrealBuildTool;

public class AsyncFutureTests : ModuleRules
{
	public AsyncFutureTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Keep unity disabled: this module is the standing guard against ODR
		// violations in AsyncFutures' public headers (multiple includers in
		// separate translation units is exactly what a unity build hides).
		bUseUnity = false;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
        });

        PrivateDependencyModuleNames.AddRange(new string[] {
			"AsyncFutures"
		});
	}
}
