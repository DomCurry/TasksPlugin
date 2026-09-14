// Copyright Dominic Curry. All Rights Reserved.
#include <CoreMinimal.h>
#include <AsyncFutures.h>
#include "Async/TaskGraphInterfaces.h"

namespace
{
	// Chains the next link from *inside* the previous continuation. Every upstream here is already
	// ready, so each link would run inline within the one before it - which is exactly the recursion
	// the inline depth cap exists to bound.
	UE::Tasks::TAsyncFuture<void> RunNestedChain(int32 Remaining, TSharedRef<std::atomic<int32>, ESPMode::ThreadSafe> Count)
	{
		if (Remaining <= 0)
		{
			return UE::Tasks::MakeReadyFuture();
		}

		return UE::Tasks::MakeReadyFuture().Then([Remaining, Count]()
		{
			Count->fetch_add(1);
			return RunNestedChain(Remaining - 1, Count);
		});
	}
}

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

	//LatentIt("An explicit thread with an incompatible execution policy raises an ensure", [this](const auto& Done)
	//{
	//	// The ensure's log output races an async crash-reporter submission, so which line (if any)
	//	// lands at Error severity isn't predictable - suppress it rather than pattern-match it, and
	//	// assert the actual contract below instead: execution proceeds despite the bad policy combo.
	//	AddExpectedError(TEXT(".*"), EAutomationExpectedErrorFlags::Contains, -1);
	//
	//	UE::Tasks::Async([this]()
	//	{
	//		ContinuationCalled = true;
	//	}, UE::Tasks::FOptions().Set(EAsyncExecution::ThreadPool).Set(ENamedThreads::GameThread))
	//	.Then([this, Done]()
	//	{
	//		TestTrue(TEXT("Continuation still runs"), ContinuationCalled);
	//		Done.Execute();
	//	});
	//});

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

	It("Then on an already-resolved future runs before Then returns", [this]()
	{
		const TSharedRef<std::atomic<bool>, ESPMode::ThreadSafe> Ran =
			MakeShared<std::atomic<bool>, ESPMode::ThreadSafe>(false);

		UE::Tasks::MakeReadyFuture(5).Then([Ran](int32 Value) { Ran->store(true); });

		TestTrue(TEXT("Continuation ran synchronously"), Ran->load());
	});

	It("Async stays asynchronous even though its upstream is already resolved", [this]()
	{
		// Async() is MakeReadyFuture().Then(...), so without an explicit opt-out it would qualify for
		// inline execution and stop being async at all.
		const TSharedRef<std::atomic<bool>, ESPMode::ThreadSafe> Ran =
			MakeShared<std::atomic<bool>, ESPMode::ThreadSafe>(false);
		FEvent* CompletionEvent = FPlatformProcess::GetSynchEventFromPool(false);

		UE::Tasks::Async([Ran]() { Ran->store(true); })
			.Then([CompletionEvent]() { CompletionEvent->Trigger(); });

		TestFalse(TEXT("Did not run on the calling thread"), Ran->load());

		CompletionEvent->Wait();
		FPlatformProcess::ReturnSynchEventToPool(CompletionEvent);
		TestTrue(TEXT("Ran once dispatched"), Ran->load());
	});

	It("Nested inline continuations fall back to dispatch instead of overflowing", [this]()
	{
		const TSharedRef<std::atomic<int32>, ESPMode::ThreadSafe> Count =
			MakeShared<std::atomic<int32>, ESPMode::ThreadSafe>(0);
		FEvent* CompletionEvent = FPlatformProcess::GetSynchEventFromPool(false);

		// Far deeper than the inline cap, so this only completes if the chain stops recursing and
		// starts dispatching partway down.
		constexpr int32 ChainLength = 256;
		RunNestedChain(ChainLength, Count).Then([CompletionEvent]() { CompletionEvent->Trigger(); });

		TestTrue(TEXT("Chain completed"), CompletionEvent->Wait(FTimespan::FromSeconds(10.0)));
		FPlatformProcess::ReturnSynchEventToPool(CompletionEvent);
		TestEqual(TEXT("Every link ran"), Count->load(), ChainLength);
	});
}
