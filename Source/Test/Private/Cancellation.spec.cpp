// Copyright Dominic Curry. All Rights Reserved.
#include <CoreMinimal.h>
#include <AsyncFutures.h>

namespace
{
	class FCancellationTestOwner : public TSharedFromThis<FCancellationTestOwner>
	{
	public:
		virtual ~FCancellationTestOwner() = default;
	};
}

BEGIN_DEFINE_SPEC(FAsyncFuturesSpec_Cancelling, "AsyncFutures.Cancelling", EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ServerContext)

bool ContinuationCalled = false;

UE::Tasks::FCancellationHandle CancellationHandle = UE::Tasks::FCancellationHandle();

END_DEFINE_SPEC(FAsyncFuturesSpec_Cancelling)

void FAsyncFuturesSpec_Cancelling::Define()
{
	BeforeEach([this]()
	{
		CancellationHandle = UE::Tasks::FCancellationHandle();
		ContinuationCalled = false;
	});

	LatentIt("Can deal with a cancel race condition", [this](const auto& Done)
	{
		UE::Tasks::TAsyncFuture<int32> Future = UE::Tasks::Async([]() {
			return UE::Tasks::TResult<int32>(5);
		}, UE::Tasks::FOptions().Set(CancellationHandle));

		CancellationHandle.Cancel();

		Future.Then([this, Done](UE::Tasks::TResult<int32> Result)
		{
			//This is weird, but we *expect* cancellation to be a race condition. Cancellation is a best-effort process.
			//It may, or may not, cancel before the value is set. This test is here to make sure that we handle the race condition.
			TestTrue("Result result was cancelled or set", Result.HasValue() || Result.IsCancelled());
			Done.Execute();
		}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
	});

	LatentIt("Can cancel a Future", [this](const auto& Done)
	{
		UE::Tasks::TAsyncPromise<void> Prm;
		UE::Tasks::TAsyncFuture<int32> Future = Prm.GetFuture().Then([]()
		{
			//Force a sleep here to avoid the race condition described above - we want to ensure a cancel.
			FPlatformProcess::Sleep(0.2f);
			return UE::Tasks::TResult(5);
		}, UE::Tasks::FOptions().Set(CancellationHandle));

		CancellationHandle.Cancel();
		Prm.SetValue();

		Future.Then([this, Done](UE::Tasks::TResult<int32> Result)
		{
			TestTrue("Result result was cancelled", Result.IsCancelled());
			Done.Execute();
		}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
	});

	LatentIt("Can cancel before the Future is set", [this](const auto& Done)
	{
		CancellationHandle.Cancel();

		UE::Tasks::Async([this]()
		{
			ContinuationCalled = true;
			return UE::Tasks::TResult(5);
		}, UE::Tasks::FOptions().Set(CancellationHandle))
		.Then([this, Done](UE::Tasks::TResult<int32> Result)
		{
			TestTrue("Result result was cancelled or set", Result.IsCancelled());
			TestFalse("Continuation body was not run after cancel", ContinuationCalled);
			Done.Execute();
		}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
	});

	It("Bind racing Cancel never leaves a promise unset", [this]()
	{
		constexpr int32 NumIterations = 2000;
		for (int32 Index = 0; Index < NumIterations; ++Index)
		{
			UE::Tasks::FCancellationHandle Handle;
			UE::Tasks::TAsyncPromise<void> Prm;

			TFuture<void> CancelThread = ::Async(EAsyncExecution::ThreadPool, [Handle]() mutable
			{
				Handle.Cancel();
			});

			UE::Tasks::TAsyncFuture<int32> Future = Prm.GetFuture().Then([]()
			{
				return UE::Tasks::TResult<int32>(5);
			}, UE::Tasks::FOptions().Set(Handle));

			Prm.SetValue();

			FEvent* CompletionEvent = FPlatformProcess::GetSynchEventFromPool(false);
			Future.Then([this, CompletionEvent](UE::Tasks::TResult<int32> Result)
			{
				TestTrue("Result was cancelled or set, never left unset", Result.HasValue() || Result.IsCancelled());
				CompletionEvent->Trigger();
			});

			CompletionEvent->Wait();
			FPlatformProcess::ReturnSynchEventToPool(CompletionEvent);
			CancelThread.Wait();
		}
	});

	LatentIt("Destroying the last handle cancels bound work", [this](const auto& Done)
	{
		UE::Tasks::TAsyncPromise<void> Gate;
		UE::Tasks::TAsyncFuture<int32> Future;
		{
			UE::Tasks::FCancellationHandle ScopedHandle;
			Future = Gate.GetFuture().Then([]() { return UE::Tasks::TResult<int32>(5); },
				UE::Tasks::FOptions().Set(ScopedHandle));
		}   // ScopedHandle dies here

		Gate.SetValue();
		Future.Then([this, Done](const UE::Tasks::TResult<int32>& Result)
		{
			TestTrue("Cancelled by handle destruction", Result.IsCancelled());
			Done.Execute();
		}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
	});

	LatentIt("Result Then is called after cancel", [this](const auto& Done)
	{
		UE::Tasks::TAsyncPromise<void> Prm;
		UE::Tasks::TAsyncFuture<void> Future = Prm.GetFuture().Then([]()
		{
			return UE::Tasks::TResult(5);
		}, UE::Tasks::FOptions().Set(CancellationHandle))
		.Then([this](UE::Tasks::TResult<int32> Result)
		{
			ContinuationCalled = true;
			return Result.Transform();
		});

		CancellationHandle.Cancel();
		Prm.SetValue();

		Future.Then([this, Done](UE::Tasks::TResult<void> Result)
		{
			TestTrue("Result result was cancelled or set", Result.IsCancelled());
			TestTrue("Result-based continuation was called", ContinuationCalled);
			Done.Execute();
		}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
	});

	LatentIt("Cancelled Then wont be called", [this](const auto& Done)
	{
		CancellationHandle.Cancel();

		UE::Tasks::Async([this]()
		{
			TestTrue("First Called", true);
			return 5;
		})
		.Then([this](int32 Value)
		{
			ContinuationCalled = true;
			TestTrue("Cancelled Not called", false);
		}, UE::Tasks::FOptions().Set(CancellationHandle))
		.Then([this, Done](const UE::Tasks::TResult<void>& Result)
		{
			TestTrue("Result result was cancelled or set", Result.IsCancelled());
			TestFalse("Value-based continuation was called", ContinuationCalled);
			Done.Execute();
		}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
	});

	LatentIt("Result Then runs even when its own promise is cancelled", [this](const auto& Done)
	{
		CancellationHandle.Cancel();

		UE::Tasks::Async([]() { return UE::Tasks::TResult(5); })
		.Then([this](UE::Tasks::TResult<int32> Result)
		{
			ContinuationCalled = true;
			TestTrue("Continuation observed the cancellation", Result.IsCancelled());
			return Result;
		}, UE::Tasks::FOptions().Set(CancellationHandle))
		.Then([this, Done](UE::Tasks::TResult<int32> Result)
		{
			TestTrue("Result-taking continuation was called", ContinuationCalled);
			TestTrue("Cancellation propagated", Result.IsCancelled());
			Done.Execute();
		}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
	});

	LatentIt("Then is not called with raw value after Cancellation", [this](const auto& Done)
	{
		CancellationHandle.Cancel();

		UE::Tasks::Async([]()
		{
			return 5;
		})
		.Then([](UE::Tasks::TResult<int32> Result)
		{
			return Result;
		}, UE::Tasks::FOptions().Set(CancellationHandle))
		.Then([this](int32 Result)
		{
			ContinuationCalled = true;
			return true;
		})
		.Then([this, Done](UE::Tasks::TResult<bool> Result)
		{
			TestTrue("Result is completed", Result.IsCancelled());
			TestFalse("Then with raw value executed", ContinuationCalled);
			Done.Execute();
		}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
	});

	It("Bind does not retain continuations that have already run", [this]()
	{
		UE::Tasks::FCancellationHandle Handle;

		// Each gate is resolved before Then is called, so the continuation runs inline and the entry
		// is resolved by the time the next Bind sweeps.
		constexpr int32 NumBound = 64;
		for (int32 Index = 0; Index < NumBound; ++Index)
		{
			UE::Tasks::TAsyncPromise<void> Gate;
			Gate.SetValue();
			Gate.GetFuture().Then([]() { return 5; }, UE::Tasks::FOptions().Set(Handle));
		}

		TestTrue("Resolved entries were swept rather than retained", Handle.GetTrackedCount() < NumBound);
	});

	LatentIt("A dead owner reports a lifetime error rather than a cancellation", [this](const auto& Done)
	{
		// Both conditions apply at once. Pin() is the precondition for touching the owner at all, so
		// lifetime wins: handing a cancelled result to a continuation whose owner is gone would mean
		// running it against a dangling this.
		UE::Tasks::TAsyncPromise<void> Gate;
		UE::Tasks::TAsyncFuture<int32> Future;
		{
			TSharedPtr<FCancellationTestOwner> Owner = MakeShared<FCancellationTestOwner>();
			Future = Gate.GetFuture().Then(Owner.Get(), [this]()
			{
				ContinuationCalled = true;
				return 5;
			}, UE::Tasks::FOptions().Set(CancellationHandle));
		}   // Owner dies here, while the handle is still live

		CancellationHandle.Cancel();

		Future.Then([this, Done](const UE::Tasks::TResult<int32>& Result)
		{
			TestFalse("Continuation body did not run", ContinuationCalled);
			TestTrue("Error reported", Result.HasError());
			TestFalse("Not reported as a cancellation", Result.IsCancelled());
			TestEqual("Lifetime error code", Result.GetError().GetCode(), UE::Tasks::ERROR_LIFETIME);
			Done.Execute();
		}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
	});

	LatentIt("A continuation can cancel its own handle while running", [this](const auto& Done)
	{
		// Cancel invokes continuations outside its own lock precisely so this is safe - a
		// continuation is free to re-enter Cancel()/Bind() on the handle that just triggered it.
		UE::Tasks::TAsyncPromise<void> Gate;
		UE::Tasks::TAsyncFuture<void> Future = Gate.GetFuture().Then([this](UE::Tasks::TResult<void> Result)
		{
			ContinuationCalled = true;
			CancellationHandle.Cancel();
			// Propagated deliberately: the continuation is now the only thing that completes this
			// promise, so returning a plain value here would override the cancellation instead.
			return Result;
		}, UE::Tasks::FOptions().Set(CancellationHandle));

		CancellationHandle.Cancel();

		Future.Then([this, Done](const UE::Tasks::TResult<void>& Result)
		{
			TestTrue("Continuation ran", ContinuationCalled);
			TestTrue("Result was cancelled", Result.IsCancelled());
			Done.Execute();
		}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
	});
}
