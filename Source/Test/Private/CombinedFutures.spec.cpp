// Copyright(c) Dominic Curry. All rights reserved.
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
						}, UE::Tasks::FOptions(ENamedThreads::GameThread));
			});

		LatentIt("Fail", [this](const auto& Done)
			{
				UE::Tasks::WhenAll({ UE::Tasks::MakeReadyFuture(), UE::Tasks::MakeReadyFuture(), UE::Tasks::MakeErrorFuture<void>(UE::Tasks::Error(Code, Context, TEXT("Error Message"))) })
					.Then([this, Done](const UE::Tasks::TResult<void>& Result)
						{
							TestTrue("Result is an error", Result.HasError());
							TestFalse("Result is completed", Result.HasValue());
							TestEqual("Captured String", *(Result.GetError().GetMessage()), TEXT("Error Message"));
							Done.Execute();
						}, UE::Tasks::FOptions(ENamedThreads::GameThread));
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
						}, UE::Tasks::FOptions(ENamedThreads::GameThread));

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
							TestEqual("Captured String", *(Result.GetError().GetMessage()), TEXT("Error Message"));
							Done.Execute();
						}, UE::Tasks::FOptions(ENamedThreads::GameThread));

				//kick this off in another thread to avoid hitting the sleep on the test thread
				UE::Tasks::Async([FirstPromise, SecondPromise]()
					{
						FirstPromise.SetValue(UE::Tasks::Error(Code, Context, TEXT("Error Message")));

						//Force a sleep here to avoid the race condition.
						FPlatformProcess::Sleep(0.2f);

						SecondPromise.SetValue(1);
					});
			});
	});
}
