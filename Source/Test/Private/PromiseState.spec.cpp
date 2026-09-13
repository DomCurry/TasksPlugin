// Copyright Dominic Curry. All Rights Reserved.
#include <CoreMinimal.h>
#include <AsyncFutures.h>
#include "Async/TaskGraphInterfaces.h"
#include <atomic>

BEGIN_DEFINE_SPEC(FAsyncFuturesSpec_PromiseState, "AsyncFutures.PromiseState", EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ServerContext)

END_DEFINE_SPEC(FAsyncFuturesSpec_PromiseState)

void FAsyncFuturesSpec_PromiseState::Define()
{
	LatentIt("SetValue claims exactly once under concurrent callers", [this](const auto& Done)
	{
		static constexpr int32 NumSetters = 8;
		static constexpr int32 NumIterations = 300;

		for (int32 Iteration = 0; Iteration < NumIterations; ++Iteration)
		{
			UE::Tasks::TAsyncPromise<int32> Promise;

			std::atomic<int32> ContinuationCount{ 0 };
			std::atomic<int32> ObservedValue{ -1 };

			FEvent* CompletionSignal = FPlatformProcess::GetSynchEventFromPool();

			Promise.GetFuture().Then([&ContinuationCount, &ObservedValue, CompletionSignal](const UE::Tasks::TResult<int32>& Result)
			{
				ContinuationCount.fetch_add(1);
				if (Result.HasValue())
				{
					ObservedValue.store(Result.GetValue());
				}
				CompletionSignal->Trigger();
			});

			//Race NumSetters threads to claim the same promise with distinct values.
			FGraphEventArray SetterTasks;
			SetterTasks.Reserve(NumSetters);
			for (int32 SetterIndex = 0; SetterIndex < NumSetters; ++SetterIndex)
			{
				SetterTasks.Add(FFunctionGraphTask::CreateAndDispatchWhenReady([&Promise, SetterIndex]()
				{
					Promise.SetValue(SetterIndex);
				}, TStatId(), nullptr, ENamedThreads::AnyThread));
			}
			FTaskGraphInterface::Get().WaitUntilTasksComplete(SetterTasks);

			const bool bContinuationFired = CompletionSignal->Wait(FTimespan::FromSeconds(2.0));
			FPlatformProcess::ReturnSynchEventToPool(CompletionSignal);

			if (!TestTrue(TEXT("Continuation fired within timeout"), bContinuationFired))
			{
				break;
			}

			TestEqual(TEXT("Continuation ran exactly once, never double-dispatched"), ContinuationCount.load(), 1);

			const int32 Value = ObservedValue.load();
			TestTrue(TEXT("Observed value came from exactly one of the racing setters, not garbage"), Value >= 0 && Value < NumSetters);
		}

		Done.Execute();
	});

	LatentIt("IsReady/Get polling never observes a claimed-but-unpublished value", [this](const auto& Done)
	{
		static constexpr int32 NumIterations = 300;

		for (int32 Iteration = 0; Iteration < NumIterations; ++Iteration)
		{
			UE::Tasks::TAsyncPromise<int32> Promise;
			UE::Tasks::TAsyncFuture<int32> Future = Promise.GetFuture();

			FGraphEventRef SetterTask = FFunctionGraphTask::CreateAndDispatchWhenReady([&Promise, Iteration]()
			{
				Promise.SetValue(Iteration);
			}, TStatId(), nullptr, ENamedThreads::AnyThread);

			//Poll from this thread exactly the way the public API invites - if the value is
			//published before it's written, Get() below trips its internal check().
			while (!Future.IsReady())
			{
				FPlatformProcess::Sleep(0.0f);
			}

			const UE::Tasks::TResult<int32> Result = Future.Get();
			TestTrue(TEXT("Value observed"), Result.HasValue());
			if (Result.HasValue())
			{
				TestEqual(TEXT("Correct value observed"), Result.GetValue(), Iteration);
			}

			FTaskGraphInterface::Get().WaitUntilTaskCompletes(SetterTask);
		}

		Done.Execute();
	});
}
