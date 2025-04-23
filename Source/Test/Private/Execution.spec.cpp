// Copyright Dominic Curry. All Rights Reserved.
#include <CoreMinimal.h>
#include <AsyncFutures.h>
#include "Async/TaskGraphInterfaces.h"

BEGIN_DEFINE_SPEC(FAsyncFuturesSpec_Execution, "AsyncFutures.Execution", EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ServerContext)

bool ContinuationCalled = false;

END_DEFINE_SPEC(FAsyncFuturesSpec_Execution)

void FAsyncFuturesSpec_Execution::Define()
{
	BeforeEach([this]()
	{
		ContinuationCalled = false;
	});

	LatentIt("Can schedule on the task graph", [this](const auto& Done)
	{
		UE::Tasks::Async([this]()
		{
			ContinuationCalled = true;
		}, UE::Tasks::FOptions().Set(EAsyncExecution::TaskGraph))
		.Then([this, Done]()
		{
			TestTrue(TEXT("Continuation is called"), ContinuationCalled);
			Done.Execute();
		});
	});

	LatentIt("Scheduling on the main thread overrides the specified thread", [this](const auto& Done)
	{
		UE::Tasks::Async([this]()
		{
			ContinuationCalled = true;
			return FTaskGraphInterface::Get().GetCurrentThreadIfKnown();
		}, UE::Tasks::FOptions().Set(EAsyncExecution::TaskGraphMainThread).Set(ENamedThreads::RHIThread))
		.Then([this, Done](const UE::Tasks::TResult<ENamedThreads::Type>& Result)
		{
			TestTrue(TEXT("Continuation is called"), ContinuationCalled);
			TestTrue(TEXT("Result is completed"), Result.HasValue());
			TestEqual(TEXT("Result is the game thread"), Result.GetValue(), ENamedThreads::GameThread);
			Done.Execute();
		});
	});

	if (FPlatformProcess::SupportsMultithreading())
	{
		LatentIt("Can schedule a long-running task", [this](const auto& Done)
		{
			UE::Tasks::Async([this]()
			{
				ContinuationCalled = true;
				return FTaskGraphInterface::Get().GetCurrentThreadIfKnown();
			}, UE::Tasks::FOptions().Set(EAsyncExecution::Thread))
			.Then([this, Done](const UE::Tasks::TResult<ENamedThreads::Type>& Result)
			{
				TestTrue(TEXT("Continuation is called"), ContinuationCalled);
				TestTrue(TEXT("Result is completed"), Result.HasValue());
				TestNotEqual(TEXT("Result is not the game thread"), Result.GetValue(), ENamedThreads::GameThread);
				TestNotEqual(TEXT("Result is not the RHI thread"), Result.GetValue(), ENamedThreads::RHIThread);
				TestNotEqual(TEXT("Result is not the Render thread"), Result.GetValue(), ENamedThreads::ActualRenderingThread);
				Done.Execute();
			});
		});
	}

	if (FPlatformProcess::SupportsMultithreading() || FForkProcessHelper::IsForkedMultithreadInstance())
	{
		LatentIt("Can schedule a long-running, forkable task", [this](const auto& Done)
		{
			UE::Tasks::Async([this]()
			{
				ContinuationCalled = true;
				return FTaskGraphInterface::Get().GetCurrentThreadIfKnown();
			}, UE::Tasks::FOptions().Set(EAsyncExecution::ThreadIfForkSafe))
			.Then([this, Done](const UE::Tasks::TResult<ENamedThreads::Type>& Result)
			{
				TestTrue(TEXT("Continuation is called"), ContinuationCalled);
				TestTrue(TEXT("Result is completed"), Result.HasValue());
				TestNotEqual(TEXT("Result is not the game thread"), Result.GetValue(), ENamedThreads::GameThread);
				TestNotEqual(TEXT("Result is not the RHI thread"), Result.GetValue(), ENamedThreads::RHIThread);
				TestNotEqual(TEXT("Result is not the Render thread"), Result.GetValue(), ENamedThreads::ActualRenderingThread);
				Done.Execute();
			});
		});
	}

	if (FPlatformProcess::SupportsMultithreading())
	{
		LatentIt("Can schedule a task on the thread pool", [this](const auto& Done)
		{
			UE::Tasks::Async([this]()
			{
				ContinuationCalled = true;
				return FTaskGraphInterface::Get().GetCurrentThreadIfKnown();
			}, UE::Tasks::FOptions().Set(EAsyncExecution::ThreadPool))
			.Then([this, Done](const UE::Tasks::TResult<ENamedThreads::Type>& Result)
			{
				TestTrue(TEXT("Continuation is called"), ContinuationCalled);
				TestTrue(TEXT("Result is completed"), Result.HasValue());
				TestNotEqual(TEXT("Result is not the game thread"), Result.GetValue(), ENamedThreads::GameThread);
				TestNotEqual(TEXT("Result is not the RHI thread"), Result.GetValue(), ENamedThreads::RHIThread);
				TestNotEqual(TEXT("Result is not the Render thread"), Result.GetValue(), ENamedThreads::ActualRenderingThread);
				Done.Execute();
			});
		});
	}

#if WITH_EDITOR
	if (FPlatformProcess::SupportsMultithreading())
	{
		LatentIt("Can schedule a task on the editor thread pool", [this](const auto& Done)
		{
			UE::Tasks::Async([this]()
			{
				ContinuationCalled = true;
				return FTaskGraphInterface::Get().GetCurrentThreadIfKnown();
			}, UE::Tasks::FOptions().Set(EAsyncExecution::LargeThreadPool))
			.Then([this, Done](const UE::Tasks::TResult<ENamedThreads::Type>& Result)
			{
				TestTrue(TEXT("Continuation is called"), ContinuationCalled);
				TestTrue(TEXT("Result is completed"), Result.HasValue());
				TestNotEqual(TEXT("Result is not the game thread"), Result.GetValue(), ENamedThreads::GameThread);
				TestNotEqual(TEXT("Result is not the RHI thread"), Result.GetValue(), ENamedThreads::RHIThread);
				TestNotEqual(TEXT("Result is not the Render thread"), Result.GetValue(), ENamedThreads::ActualRenderingThread);
				Done.Execute();
			});
		});
	}
#endif
}
