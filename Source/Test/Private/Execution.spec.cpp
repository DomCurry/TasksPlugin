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
		}, UE::Tasks::FOptions().OnTaskGraph())
		.Then([this, Done]()
		{
			TestTrue(TEXT("Continuation is called"), ContinuationCalled);
			Done.Execute();
		});
	});

	LatentIt("An explicitly requested thread takes precedence over TaskGraphMainThread", [this](const auto& Done)
	{
		UE::Tasks::Async([this]()
		{
			ContinuationCalled = true;
			return FTaskGraphInterface::Get().GetCurrentThreadIfKnown();
		}, UE::Tasks::FOptions().Set(EAsyncExecution::TaskGraphMainThread).Set(ENamedThreads::ActualRenderingThread))
		.Then([this, Done](const UE::Tasks::TResult<ENamedThreads::Type>& Result)
		{
			TestTrue(TEXT("Continuation is called"), ContinuationCalled);
			TestTrue(TEXT("Result is completed"), Result.HasValue());
			TestEqual(TEXT("Result honours the explicitly requested thread"), Result.GetValue(), ENamedThreads::ActualRenderingThread);
			Done.Execute();
		});
	});

	LatentIt("An explicit thread with an incompatible execution policy raises an ensure", [this](const auto& Done)
	{
		AddExpectedError(TEXT("an explicit thread is only honoured by EAsyncExecution::TaskGraph"), EAutomationExpectedErrorFlags::Contains, 1);

		UE::Tasks::Async([this]()
		{
			ContinuationCalled = true;
		}, UE::Tasks::FOptions().Set(EAsyncExecution::ThreadPool).Set(ENamedThreads::GameThread))
		.Then([this, Done]()
		{
			TestTrue(TEXT("Continuation still runs"), ContinuationCalled);
			Done.Execute();
		});
	});

	// Covers the `default:` arm in TContinuationTask::DoTask generically via a bogus enum value.
	// The concrete reachable case - EAsyncExecution::LargeThreadPool outside WITH_EDITOR - can't be
	// exercised here because this automation suite always runs with WITH_EDITOR enabled.
	LatentIt("An unsupported execution policy fails the future instead of hanging", [this](const auto& Done)
	{
		UE::Tasks::Async([this]()
		{
			ContinuationCalled = true;
		}, UE::Tasks::FOptions().Set(static_cast<EAsyncExecution>(255)))
		.Then([this, Done](const UE::Tasks::TResult<void>& Result)
		{
			TestFalse(TEXT("Continuation did not run"), ContinuationCalled);
			TestTrue(TEXT("Result is completed with an error"), Result.HasError());
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
			}, UE::Tasks::FOptions().OnDedicatedThread())
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
			}, UE::Tasks::FOptions().OnThreadPool())
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
