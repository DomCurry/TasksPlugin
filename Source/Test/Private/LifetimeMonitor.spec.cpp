// Copyright Dominic Curry. All Rights Reserved.
#include <CoreMinimal.h>
#include <AsyncFutures.h>
#include "LifetimeMonitorTestObject.h"

namespace
{
	class FLifetimeMonitorTestOwner : public TSharedFromThis<FLifetimeMonitorTestOwner>
	{
	public:
		virtual ~FLifetimeMonitorTestOwner() = default;
	};

	class FLifetimeMonitorTestBaseOwner : public TSharedFromThis<FLifetimeMonitorTestBaseOwner>
	{
	public:
		virtual ~FLifetimeMonitorTestBaseOwner() = default;
	};

	class FLifetimeMonitorTestDerivedOwner : public FLifetimeMonitorTestBaseOwner
	{
	};
}

BEGIN_DEFINE_SPEC(FAsyncFuturesSpec_LifetimeMonitor, "AsyncFutures.LifetimeMonitor", EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ServerContext)

bool ContinuationCalled = false;

END_DEFINE_SPEC(FAsyncFuturesSpec_LifetimeMonitor)

void FAsyncFuturesSpec_LifetimeMonitor::Define()
{
	BeforeEach([this]()
	{
		ContinuationCalled = false;
	});

	Describe("No owner", [this]()
	{
		LatentIt("Continuation always runs", [this](const auto& Done)
		{
			UE::Tasks::TAsyncPromise<void> Gate;
			UE::Tasks::TAsyncFuture<int32> Future = Gate.GetFuture().Then([this]()
			{
				ContinuationCalled = true;
				return 5;
			});

			Gate.SetValue();
			Future.Then([this, Done](const UE::Tasks::TResult<int32>& Result)
			{
				TestTrue("Continuation body ran", ContinuationCalled);
				TestTrue("Result has value", Result.HasValue());
				TestEqual("Value", Result.GetValue(), 5);
				Done.Execute();
			}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
		});
	});

	Describe("UObject owner", [this]()
	{
		LatentIt("Alive - continuation runs", [this](const auto& Done)
		{
			ULifetimeMonitorTestObject* Owner = NewObject<ULifetimeMonitorTestObject>();
			Owner->AddToRoot();

			UE::Tasks::TAsyncPromise<void> Gate;
			UE::Tasks::TAsyncFuture<int32> Future = Gate.GetFuture().Then(Owner, [this]()
			{
				ContinuationCalled = true;
				return 5;
			});

			Gate.SetValue();
			Future.Then([this, Done, Owner](const UE::Tasks::TResult<int32>& Result)
			{
				Owner->RemoveFromRoot();
				TestTrue("Continuation body ran", ContinuationCalled);
				TestTrue("Result has value", Result.HasValue());
				TestEqual("Value", Result.GetValue(), 5);
				Done.Execute();
			}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
		});

		LatentIt("Destroyed before continuation - lifetime error propagates", [this](const auto& Done)
		{
			UE::Tasks::TAsyncPromise<void> Gate;
			ULifetimeMonitorTestObject* Owner = NewObject<ULifetimeMonitorTestObject>();
			Owner->AddToRoot();

			UE::Tasks::TAsyncFuture<int32> Future = Gate.GetFuture().Then(Owner, [this]()
			{
				ContinuationCalled = true;
				return 5;
			});

			//Simulate destruction: mark the object garbage so the weak pointer reports invalid,
			//exactly as would happen once the real owner is destroyed.
			Owner->MarkAsGarbage();
			Owner->RemoveFromRoot();

			Gate.SetValue();
			Future.Then([this, Done](const UE::Tasks::TResult<int32>& Result)
			{
				TestFalse("Continuation body did not run", ContinuationCalled);
				TestTrue("Error reported", Result.HasError());
				TestFalse("Lifetime error is not treated as cancellation", Result.IsCancelled());
				TestEqual("Lifetime error code", Result.GetError().GetCode(), UE::Tasks::ERROR_LIFETIME);
				Done.Execute();
			}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
		});
	});

	Describe("TSharedFromThis owner (direct)", [this]()
	{
		LatentIt("Alive - continuation runs", [this](const auto& Done)
		{
			TSharedPtr<FLifetimeMonitorTestOwner> Owner = MakeShared<FLifetimeMonitorTestOwner>();

			UE::Tasks::TAsyncPromise<void> Gate;
			UE::Tasks::TAsyncFuture<int32> Future = Gate.GetFuture().Then(Owner.Get(), [this]()
			{
				ContinuationCalled = true;
				return 5;
			});

			Gate.SetValue();
			Future.Then([this, Done](const UE::Tasks::TResult<int32>& Result)
			{
				TestTrue("Continuation body ran", ContinuationCalled);
				TestTrue("Result has value", Result.HasValue());
				TestEqual("Value", Result.GetValue(), 5);
				Done.Execute();
			}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
		});

		LatentIt("Destroyed before continuation - lifetime error propagates", [this](const auto& Done)
		{
			UE::Tasks::TAsyncPromise<void> Gate;
			UE::Tasks::TAsyncFuture<int32> Future;
			{
				TSharedPtr<FLifetimeMonitorTestOwner> Owner = MakeShared<FLifetimeMonitorTestOwner>();
				Future = Gate.GetFuture().Then(Owner.Get(), [this]()
				{
					ContinuationCalled = true;
					return 5;
				});
			} // Owner released here, before the gate opens

			Gate.SetValue();
			Future.Then([this, Done](const UE::Tasks::TResult<int32>& Result)
			{
				TestFalse("Continuation body did not run", ContinuationCalled);
				TestTrue("Error reported", Result.HasError());
				TestFalse("Lifetime error is not treated as cancellation", Result.IsCancelled());
				TestEqual("Lifetime error code", Result.GetError().GetCode(), UE::Tasks::ERROR_LIFETIME);
				Done.Execute();
			}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
		});
	});

	Describe("TSharedFromThis owner (via base class)", [this]()
	{
		LatentIt("Alive - continuation runs", [this](const auto& Done)
		{
			TSharedPtr<FLifetimeMonitorTestDerivedOwner> Owner = MakeShared<FLifetimeMonitorTestDerivedOwner>();

			UE::Tasks::TAsyncPromise<void> Gate;
			UE::Tasks::TAsyncFuture<int32> Future = Gate.GetFuture().Then(Owner.Get(), [this]()
			{
				ContinuationCalled = true;
				return 5;
			});

			Gate.SetValue();
			Future.Then([this, Done](const UE::Tasks::TResult<int32>& Result)
			{
				TestTrue("Continuation body ran", ContinuationCalled);
				TestTrue("Result has value", Result.HasValue());
				TestEqual("Value", Result.GetValue(), 5);
				Done.Execute();
			}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
		});

		LatentIt("Destroyed before continuation - lifetime error propagates", [this](const auto& Done)
		{
			UE::Tasks::TAsyncPromise<void> Gate;
			UE::Tasks::TAsyncFuture<int32> Future;
			{
				TSharedPtr<FLifetimeMonitorTestDerivedOwner> Owner = MakeShared<FLifetimeMonitorTestDerivedOwner>();
				Future = Gate.GetFuture().Then(Owner.Get(), [this]()
				{
					ContinuationCalled = true;
					return 5;
				});
			} // Owner released here, before the gate opens

			Gate.SetValue();
			Future.Then([this, Done](const UE::Tasks::TResult<int32>& Result)
			{
				TestFalse("Continuation body did not run", ContinuationCalled);
				TestTrue("Error reported", Result.HasError());
				TestFalse("Lifetime error is not treated as cancellation", Result.IsCancelled());
				TestEqual("Lifetime error code", Result.GetError().GetCode(), UE::Tasks::ERROR_LIFETIME);
				Done.Execute();
			}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
		});
	});
}
