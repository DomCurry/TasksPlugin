// Copyright Dominic Curry. All Rights Reserved.
#include <CoreMinimal.h>
#include <AsyncFutures.h>

BEGIN_DEFINE_SPEC(FAsyncFuturesSpec_TaskOptions, "AsyncFutures.TaskOptions", EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ServerContext)

END_DEFINE_SPEC(FAsyncFuturesSpec_TaskOptions)


void FAsyncFuturesSpec_TaskOptions::Define()
{
	LatentIt("Can run in the current thread", [this](const auto& Done)
	{
		ENamedThreads::Type CurrentThread = FTaskGraphInterface::Get().GetCurrentThreadIfKnown();

		UE::Tasks::Async([]()
		{
			return FTaskGraphInterface::Get().GetCurrentThreadIfKnown();
		}, UE::Tasks::FOptions().OnTaskGraph(CurrentThread))
		.Then([this, Done, CurrentThread](const UE::Tasks::TResult<ENamedThreads::Type>& Result)
		{
			TestTrue("Async function is completed", Result.HasValue());
			TestEqual("Execution thread", Result.GetValue(), CurrentThread);
			Done.Execute();
		});
	});

	LatentIt("Can run in a different thread", [this](const auto& Done)
	{
		UE::Tasks::Async([]()
		{
			return UE::Tasks::TResult(FTaskGraphInterface::Get().GetCurrentThreadIfKnown());
		}, UE::Tasks::FOptions().OnTaskGraph(ENamedThreads::ActualRenderingThread))
		.Then([this, Done](UE::Tasks::TResult<ENamedThreads::Type> Result)
		{
			ENamedThreads::Type CurrentThread = FTaskGraphInterface::Get().GetCurrentThreadIfKnown();
			TestTrue("Async function is completed", Result.HasValue());
			TestEqual("Prevous thread", Result.GetValue(), ENamedThreads::ActualRenderingThread);
			TestEqual("Execution thread", CurrentThread, ENamedThreads::GameThread);
			Done.Execute();
		},UE::Tasks::FOptions().OnGameThread());
	});

	LatentIt("Can run in game thread", [this](const auto& Done)
	{
		UE::Tasks::Async([this, Done]()
		{
			TestEqual("Execution thread", FTaskGraphInterface::Get().GetCurrentThreadIfKnown(), ENamedThreads::GameThread);
			Done.Execute();
		},UE::Tasks::FOptions().OnGameThread());
	});

	LatentIt("Can use previous thread in Then", [this](const auto& Done)
	{
		UE::Tasks::Async([]()
		{
			return UE::Tasks::TResult(FTaskGraphInterface::Get().GetCurrentThreadIfKnown());
		}, UE::Tasks::FOptions().OnGameThread())
		.Then([this, Done](UE::Tasks::TResult<ENamedThreads::Type> Result)
		{
			TestTrue("Async function is completed", Result.HasValue());
			TestEqual("Execution thread", Result.GetValue(), ENamedThreads::GameThread);
			Done.Execute();
		});
	});

	if (FPlatformProcess::SupportsMultithreading())
	{
		LatentIt("OnDedicatedThread runs on a dedicated thread", [this](const auto& Done)
		{
			UE::Tasks::Async([]()
			{
				return FTaskGraphInterface::Get().GetCurrentThreadIfKnown();
			}, UE::Tasks::FOptions().OnDedicatedThread())
			.Then([this, Done](const UE::Tasks::TResult<ENamedThreads::Type>& Result)
			{
				TestTrue("Async function is completed", Result.HasValue());
				TestNotEqual("Result is not the game thread", Result.GetValue(), ENamedThreads::GameThread);
				Done.Execute();
			});
		});

		LatentIt("OnThreadPool runs on the thread pool", [this](const auto& Done)
		{
			UE::Tasks::Async([]()
			{
				return FTaskGraphInterface::Get().GetCurrentThreadIfKnown();
			}, UE::Tasks::FOptions().OnThreadPool())
			.Then([this, Done](const UE::Tasks::TResult<ENamedThreads::Type>& Result)
			{
				TestTrue("Async function is completed", Result.HasValue());
				TestNotEqual("Result is not the game thread", Result.GetValue(), ENamedThreads::GameThread);
				Done.Execute();
			});
		});
	}

	LatentIt("Can schedule Then on a worker thread", [this](const auto& Done)
	{
		UE::Tasks::Async([]()
		{
			return UE::Tasks::TResult<void>();
		})
		.Then([](UE::Tasks::TResult<void> Result)
		{
			return UE::Tasks::TResult(FTaskGraphInterface::Get().GetCurrentThreadIfKnown());
		}, UE::Tasks::FOptions().OnTaskGraph(ENamedThreads::AnyBackgroundThreadNormalTask))
		.Then([this, Done](UE::Tasks::TResult<ENamedThreads::Type> Result)
		{
			TestTrue("Async function is completed", Result.HasValue());
			TestEqual("Executed on the thread pool", (Result.GetValue() & ENamedThreads::ThreadIndexMask), ENamedThreads::AnyThread);
			Done.Execute();
		},UE::Tasks::FOptions().OnGameThread());
	});
}
