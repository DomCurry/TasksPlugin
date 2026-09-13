// Copyright Dominic Curry. All Rights Reserved.
#include <CoreMinimal.h>
#include <AsyncFutures.h>

BEGIN_DEFINE_SPEC(FAsyncFuturesSpec_Combined, "AsyncFutures.Combined", EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ServerContext)

static constexpr int32 Context = 0x0000dead;
static constexpr int32 Code = 0xdead0000;

END_DEFINE_SPEC(FAsyncFuturesSpec_Combined)

void FAsyncFuturesSpec_Combined::Define()
{
	Describe("WhenAll", [this]()
	{
		LatentIt("Success", [this](const auto& Done)
		{
				UE::Tasks::WhenAll<int32>({ UE::Tasks::MakeReadyFuture<int32>(1), UE::Tasks::MakeReadyFuture<int32>(2), UE::Tasks::MakeReadyFuture<int32>(4) })
					.Then([this](const TArray<int32>& Result)
						{
							TestEqual("Num Results", Result.Num(), 3);
							int32 Total = 0;
							for (const int32 Value : Result)
							{
								Total += Value;
							}
							return Total;
						})
					.Then([this, Done](const UE::Tasks::TResult<int32>& Result)
						{
							TestFalse("Result is an error", Result.HasError());
							TestTrue("Result is completed", Result.HasValue());
							TestEqual("Total", Result.GetValue(), 7);
							Done.Execute();
						}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
			});

		LatentIt("Fail", [this](const auto& Done)
			{
				UE::Tasks::WhenAll({ UE::Tasks::MakeReadyFuture(), UE::Tasks::MakeReadyFuture(), UE::Tasks::MakeErrorFuture<void>(UE::Tasks::FError(Context, Code, TEXT("Error Message"))) })
					.Then([this, Done](const UE::Tasks::TResult<void>& Result)
						{
							TestTrue("Result is an error", Result.HasError());
							TestFalse("Result is completed", Result.HasValue());
							TestEqual("Error Code", Result.GetError().GetCode(), Code);
							TestEqual("Error Context", Result.GetError().GetContext(), Context);
							TestEqual("Captured String", *(Result.GetError().GetMessage()), TEXT("Error Message"));
							Done.Execute();
						}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
			});

			LatentIt("Preserves order with non-trivial values", [this](const auto& Done)
			{
				UE::Tasks::WhenAll<FString>({
					UE::Tasks::MakeReadyFuture<FString>(TEXT("first")),
					UE::Tasks::MakeReadyFuture<FString>(TEXT("second")),
					UE::Tasks::MakeReadyFuture<FString>(TEXT("third")) })
				.Then([this, Done](const UE::Tasks::TResult<TArray<FString>>& Result)
				{
					TestTrue("Completed", Result.HasValue());
					TestEqual("Count", Result.GetValue().Num(), 3);
					TestEqual("Order preserved", Result.GetValue()[1], FString(TEXT("second")));
					Done.Execute();
				}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
			});

			LatentIt("Does not crash destructing an unconstructed slot when a future fails", [this](const auto& Done)
			{
				struct FTrackedValue
				{
					FTrackedValue() : Value(MakeShared<int32, ESPMode::ThreadSafe>(0)) {}
					explicit FTrackedValue(int32 InValue) : Value(MakeShared<int32, ESPMode::ThreadSafe>(InValue)) {}
					FTrackedValue(const FTrackedValue&) = default;
					FTrackedValue& operator=(const FTrackedValue&) = default;
					~FTrackedValue() { *Value = -1; }

					TSharedPtr<int32, ESPMode::ThreadSafe> Value;
				};

				UE::Tasks::WhenAll<FTrackedValue>({
					UE::Tasks::MakeReadyFuture<FTrackedValue>(FTrackedValue(1)),
					UE::Tasks::MakeErrorFuture<FTrackedValue>(UE::Tasks::FError(Context, Code, TEXT("Error Message"))),
					UE::Tasks::MakeReadyFuture<FTrackedValue>(FTrackedValue(3)) })
				.Then([this, Done](const UE::Tasks::TResult<TArray<FTrackedValue>>& Result)
				{
					TestTrue("Result is an error", Result.HasError());
					TestEqual("Error Code", Result.GetError().GetCode(), Code);
					TestEqual("Error Context", Result.GetError().GetContext(), Context);
					Done.Execute();
				}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
			});

			LatentIt("EFailMode::Fast resolves without waiting for a slower future", [this](const auto& Done)
			{
				const TSharedRef<std::atomic<int32>, ESPMode::ThreadSafe> CompletedCount = MakeShared<std::atomic<int32>, ESPMode::ThreadSafe>(0);

				UE::Tasks::TAsyncPromise<int32> FailingPromise;
				UE::Tasks::TAsyncPromise<int32> SlowPromise;

				UE::Tasks::WhenAll<int32>({ FailingPromise.GetFuture(), SlowPromise.GetFuture() }, UE::Tasks::EFailMode::Fast)
					.Then([this, Done, CompletedCount](const UE::Tasks::TResult<TArray<int32>>& Result)
						{
							TestTrue("Result is an error", Result.HasError());
							TestEqual("Error Code", Result.GetError().GetCode(), Code);
							TestEqual("Error Context", Result.GetError().GetContext(), Context);
							TestEqual("Combined future did not wait for the slow future", CompletedCount->load(), 0);
							Done.Execute();
						}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));

				FailingPromise.SetValue(UE::Tasks::FError(Context, Code, TEXT("Error Message")));

				//kick this off in another thread so the sleep doesn't block the test thread
				UE::Tasks::Async([SlowPromise, CompletedCount]()
					{
						FPlatformProcess::Sleep(0.2f);
						CompletedCount->store(1);
						SlowPromise.SetValue(1);
					});
			});

			LatentIt("EFailMode::Full waits for every future before resolving", [this](const auto& Done)
			{
				const TSharedRef<std::atomic<int32>, ESPMode::ThreadSafe> CompletedCount = MakeShared<std::atomic<int32>, ESPMode::ThreadSafe>(0);

				UE::Tasks::TAsyncPromise<int32> FailingPromise;
				UE::Tasks::TAsyncPromise<int32> SlowPromise;

				UE::Tasks::WhenAll<int32>({ FailingPromise.GetFuture(), SlowPromise.GetFuture() }, UE::Tasks::EFailMode::Full)
					.Then([this, Done, CompletedCount](const UE::Tasks::TResult<TArray<int32>>& Result)
						{
							TestTrue("Result is an error", Result.HasError());
							TestEqual("Error Code", Result.GetError().GetCode(), Code);
							TestEqual("Error Context", Result.GetError().GetContext(), Context);
							TestEqual("Combined future waited for the slow future", CompletedCount->load(), 1);
							Done.Execute();
						}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));

				FailingPromise.SetValue(UE::Tasks::FError(Context, Code, TEXT("Error Message")));

				//kick this off in another thread so the sleep doesn't block the test thread
				UE::Tasks::Async([SlowPromise, CompletedCount]()
					{
						FPlatformProcess::Sleep(0.2f);
						CompletedCount->store(1);
						SlowPromise.SetValue(1);
					});
			});
		});

	Describe("WhenAny", [this]()
		{
		LatentIt("Success", [this](const auto& Done)
			{
				UE::Tasks::TAsyncPromise<int32> FirstPromise;
				UE::Tasks::TAsyncPromise<int32> SecondPromise;
				UE::Tasks::WhenAny<int32>({ FirstPromise.GetFuture(), SecondPromise.GetFuture() })
					.Then([this, Done](const UE::Tasks::TResult<int32>& Result)
						{
							TestFalse("Result is an error", Result.HasError());
							TestTrue("Result is completed", Result.HasValue());
							TestEqual("Total", Result.GetValue(), 1);
							Done.Execute();
						}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));

				//kick this off in another thread to avoid hitting the sleep on the test thread
				UE::Tasks::Async([FirstPromise, SecondPromise]()
					{
						FirstPromise.SetValue(1);

						//Force a sleep here to avoid the race condition.
						FPlatformProcess::Sleep(0.2f);

						SecondPromise.SetValue(50);
					});
			});

		LatentIt("Fail", [this](const auto& Done)
			{
				UE::Tasks::TAsyncPromise<int32> FirstPromise;
				UE::Tasks::TAsyncPromise<int32> SecondPromise;
				UE::Tasks::WhenAny<int32>({ FirstPromise.GetFuture(), SecondPromise.GetFuture() })
					.Then([this, Done](const UE::Tasks::TResult<int32>& Result)
						{
							TestTrue("Result is an error", Result.HasError());
							TestFalse("Result is completed", Result.HasValue());
							TestEqual("Error Code", Result.GetError().GetCode(), Code);
							TestEqual("Error Context", Result.GetError().GetContext(), Context);
							TestEqual("Captured String", *(Result.GetError().GetMessage()), TEXT("Error Message"));
							Done.Execute();
						}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));

				//kick this off in another thread to avoid hitting the sleep on the test thread
				UE::Tasks::Async([FirstPromise, SecondPromise]()
					{
						FirstPromise.SetValue(UE::Tasks::FError(Context, Code, TEXT("Error Message")));

						//Force a sleep here to avoid the race condition.
						FPlatformProcess::Sleep(0.2f);

						SecondPromise.SetValue(1);
					});
			});
	});
}
