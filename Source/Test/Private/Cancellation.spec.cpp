// Copyright(c) Dominic Curry. All rights reserved.
#include <CoreMinimal.h>
#include <AsyncFutures.h>

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
		}, UE::Tasks::FOptions(CancellationHandle));

		CancellationHandle.Cancel();

		Future.Then([this, Done](UE::Tasks::TResult<int32> Result)
		{
			//This is weird, but we *expect* cancellation to be a race condition. Cancellation is a best-effort process.
			//It may, or may not, cancel before the value is set. This test is here to make sure that we handle the race condition.
			TestTrue("Result result was cancelled or set", Result.HasValue() || Result.IsCancelled());
			Done.Execute();
		},UE::Tasks::FOptions(ENamedThreads::GameThread));
	});

	LatentIt("Can cancel a Future", [this](const auto& Done)
	{
		UE::Tasks::TAsyncPromise<void> Prm;
		UE::Tasks::TAsyncFuture<int32> Future = Prm.GetFuture().Then([]()
		{
			//Force a sleep here to avoid the race condition described above - we want to ensure a cancel.
			FPlatformProcess::Sleep(0.2f);
			return UE::Tasks::TResult(5);
		}, UE::Tasks::FOptions(CancellationHandle));

		CancellationHandle.Cancel();
		Prm.SetValue();

		Future.Then([this, Done](UE::Tasks::TResult<int32> Result)
		{
			TestTrue("Result result was cancelled", Result.IsCancelled());
			Done.Execute();
		}, UE::Tasks::FOptions(ENamedThreads::GameThread));
	});

	LatentIt("Can cancel before the Future is set", [this](const auto& Done)
	{
		CancellationHandle.Cancel();

		UE::Tasks::Async([]()
		{
			return UE::Tasks::TResult(5);
		}, UE::Tasks::FOptions(CancellationHandle))
		.Then([this, Done](UE::Tasks::TResult<int32> Result)
		{
			TestTrue("Result result was cancelled or set", Result.IsCancelled());
			Done.Execute();
		}, UE::Tasks::FOptions(ENamedThreads::GameThread));
	});

	LatentIt("Result Then is called after cancel", [this](const auto& Done)
	{
		UE::Tasks::TAsyncPromise<void> Prm;
		UE::Tasks::TAsyncFuture<void> Future = Prm.GetFuture().Then([]()
		{
			return UE::Tasks::TResult(5);
		}, UE::Tasks::FOptions(CancellationHandle))
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
		}, UE::Tasks::FOptions(ENamedThreads::GameThread));
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
		}, UE::Tasks::FOptions(CancellationHandle))
		.Then([this, Done](const UE::Tasks::TResult<void>& Result)
		{
			TestTrue("Result result was cancelled or set", Result.IsCancelled());
			TestFalse("Value-based continuation was called", ContinuationCalled);
			Done.Execute();
		}, UE::Tasks::FOptions(ENamedThreads::GameThread));
	});

	LatentIt("Result Then is called after cancelled Then", [this](const auto& Done)
	{
		CancellationHandle.Cancel();

		UE::Tasks::Async([]()
		{
			return UE::Tasks::TResult(5);
		})
		.Then([this](UE::Tasks::TResult<int32> Result)
		{
			return Result;
		}, UE::Tasks::FOptions(CancellationHandle))
		.Then([](UE::Tasks::TResult<int32> Result)
		{
			//Then result-based continuation is called with "Cancelled" status
			return UE::Tasks::TResult<bool>(Result.IsCancelled());
		})
		.Then([this, Done](UE::Tasks::TResult<bool> Result)
		{
			TestTrue("Result is completed", Result.HasValue());
			TestTrue("Then received was 'cancel' state", Result.GetValue());
			Done.Execute();
		}, UE::Tasks::FOptions(ENamedThreads::GameThread));
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
		}, UE::Tasks::FOptions(CancellationHandle))
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
		}, UE::Tasks::FOptions(ENamedThreads::GameThread));
	});
}
