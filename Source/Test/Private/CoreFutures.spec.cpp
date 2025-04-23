// Copyright Dominic Curry. All Rights Reserved.
#include <CoreMinimal.h>
#include <AsyncFutures.h>

BEGIN_DEFINE_SPEC(FAsyncFuturesSpec_Core, "AsyncFutures.Core", EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ServerContext)

static constexpr int32 Context = 0x0000dead;
static constexpr int32 Code = 0xdead0000;

bool ContinuationCalled = false;
int32 MemberInt = 0;

END_DEFINE_SPEC(FAsyncFuturesSpec_Core)

UE::Tasks::TAsyncFuture<FString> IncrementAndStringify(int32 Value)
{
	return UE::Tasks::Async([Value]() { return FString::FromInt(Value + 1); });
}

void FAsyncFuturesSpec_Core::Define()
{
	LatentIt("Can run future", [this](const auto& Done)
	{
		UE::Tasks::TAsyncFuture<void> TestFuture = UE::Tasks::MakeReadyFuture();
		TestTrue("Future is ready", TestFuture.IsReady());

		TestFuture.Then([this, Done](UE::Tasks::TResult<void> Result)
		{
			TestTrue("Future is completed", Result.HasValue());
			Done.Execute();
		}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
	});

	LatentIt("Will run future on Then and not before", [this](const auto& Done)
	{
		UE::Tasks::TAsyncFuture<int32> TestFuture = UE::Tasks::Async([]()
		{
			return 10;
		});

		TestFuture.Then([this, Done](int32 Result)
		{
			TestEqual("Value", Result, 10);
			Done.Execute();
		}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
	});

	LatentIt("Can pass result directly to next step", [this](const auto& Done)
	{
		UE::Tasks::Async([]()
		{
			return 10;
		})
		.Then([](UE::Tasks::TResult<int32> Result)
		{
			//Pass initial result straight through
			return Result;
		})
		.Then([this, Done](UE::Tasks::TResult<int32> Result)
		{
			TestTrue("Result was completed", Result.HasValue());
			TestTrue("Result result TOptional is set", Result.HasValue());
			TestEqual("Value", Result.GetValue(), 10);
			Done.Execute();
		}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
	});

	LatentIt("Task with specified thread doesn't block that thread", [this](const auto& Done)
		{
			UE::Tasks::TAsyncFuture<FDateTime> SlowTask = UE::Tasks::Async([]()
			{
				FPlatformProcess::Sleep(0.1f);
			}, UE::Tasks::FOptions().Set(ENamedThreads::AnyBackgroundThreadNormalTask))
			.Then([]()
			{
				return FDateTime::UtcNow();
			}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));

			UE::Tasks::TAsyncFuture<FDateTime> FastTask = UE::Tasks::Async([]()
			{
				return FDateTime::UtcNow();
			}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));

			UE::Tasks::WhenAll<FDateTime>({ FastTask, SlowTask }).Then([this, Done](const UE::Tasks::TResult<TArray<FDateTime>>& Result)
			{
				TestTrue(TEXT("Fast and Slow task finished successfully"), Result.HasValue());
				const TArray<FDateTime>& Values = Result.GetValue();
				TestEqual(TEXT("Correct number of results returned"),Values.Num(), 2);
				const FDateTime& FastTime = Values[0];
				const FDateTime& SlowTime = Values[1];
				TestLessThan(TEXT("Task Finish Time"), FastTime.GetTicks(), SlowTime.GetTicks());
				Done.Execute();
			}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
		});

	Describe("Continuations", [this]()
		{
			Describe("void", [this]()
				{
					LatentIt("to void", [this](const auto& Done)
						{
							UE::Tasks::MakeReadyFuture().Then([]() {})
								.Then([this, Done]()
									{
										Done.Execute();
									}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
						});
					LatentIt("to value", [this](const auto& Done)
						{
							UE::Tasks::MakeReadyFuture().Then([]() { return 5; })
								.Then([this, Done](int32 Value)
									{
										TestEqual("Value", Value, 5);
										Done.Execute();
									}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
						});
					LatentIt("to result", [this](const auto& Done)
						{
							UE::Tasks::MakeReadyFuture().Then([]() { return UE::Tasks::TResult<int32>(5); })
								.Then([this, Done](int32 Value)
									{
										TestEqual("Value", Value, 5);
										Done.Execute();
									}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
						});
					LatentIt("to future", [this](const auto& Done)
						{
							UE::Tasks::MakeReadyFuture().Then([]() { return UE::Tasks::MakeReadyFuture<int32>(5); })
								.Then([this, Done](int32 Value)
									{
										TestEqual("Value", Value, 5);
										Done.Execute();
									}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
						});
				});
			Describe("value", [this]()
				{
					LatentIt("to void", [this](const auto& Done)
						{
							UE::Tasks::MakeReadyFuture<int32>(5).Then([](int32 Value) {})
								.Then([this, Done]()
									{
										Done.Execute();
									}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
						});
					LatentIt("to value", [this](const auto& Done)
						{
							UE::Tasks::MakeReadyFuture<int32>(5).Then([](int32 Value) { return Value; })
								.Then([this, Done](int32 Value)
									{
										TestEqual("Value", Value, 5);
										Done.Execute();
									}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
						});
					LatentIt("to result", [this](const auto& Done)
						{
							UE::Tasks::MakeReadyFuture<int32>(5).Then([](int32 Value) { return UE::Tasks::TResult<int32>(Value); })
								.Then([this, Done](int32 Value)
									{
										TestEqual("Value", Value, 5);
										Done.Execute();
									}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
						});
					LatentIt("to future", [this](const auto& Done)
						{
							UE::Tasks::MakeReadyFuture<int32>(5).Then([](int32 Value) { return UE::Tasks::MakeReadyFuture<int32>(Value); })
								.Then([this, Done](int32 Value)
									{
										TestEqual("Value", Value, 5);
										Done.Execute();
									}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
						});
				});
			Describe("result", [this]()
				{
					LatentIt("to void", [this](const auto& Done)
						{
							UE::Tasks::MakeReadyFuture<int32>(5).Then([](UE::Tasks::TResult<int32> Result) {})
								.Then([this, Done]()
									{
										Done.Execute();
									}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
						});
					LatentIt("to value", [this](const auto& Done)
						{
							UE::Tasks::MakeReadyFuture<int32>(5).Then([](UE::Tasks::TResult<int32> Result) { return Result.GetValue(); })
								.Then([this, Done](int32 Value)
									{
										TestEqual("Value", Value, 5);
										Done.Execute();
									}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
						});
					LatentIt("to result", [this](const auto& Done)
						{
							UE::Tasks::MakeReadyFuture<int32>(5).Then([](UE::Tasks::TResult<int32> Result) { return UE::Tasks::TResult<int32>(Result.GetValue()); })
								.Then([this, Done](int32 Value)
									{
										TestEqual("Value", Value, 5);
										Done.Execute();
									}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
						});
					LatentIt("to future", [this](const auto& Done)
						{
							UE::Tasks::MakeReadyFuture<int32>(5).Then([](UE::Tasks::TResult<int32> Result) { return UE::Tasks::MakeReadyFuture<int32>(Result.GetValue()); })
								.Then([this, Done](int32 Value)
									{
										TestEqual("Value", Value, 5);
										Done.Execute();
									}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
						});
				});
		});

	Describe("Raw value lambdas", [this]()
	{
		LatentIt("Can capture and return the value", [this](const auto& Done)
		{
			UE::Tasks::MakeReadyFuture<int32>(1)
			.Then([this, Done](int32 Value)
			{
				TestEqual("Value", Value, 1);
				Done.Execute();
			}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
		});

		LatentIt("Can concatenate Then", [this](const auto& Done)
		{
			UE::Tasks::MakeReadyFuture<int32>(1)
			.Then([this](int32 Result)
			{
				TestEqual("Value on first Then", Result, 1);
				return Result + 2;
			})
			.Then([this, Done](int32 Result)
			{
				TestEqual("Value on second Then", Result, 3);
				Done.Execute();
			}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
		});

		LatentIt("Can capture initial value and then concatenate void", [this](const auto& Done)
		{
			UE::Tasks::MakeReadyFuture<int32>(4)
			.Then([this](int32 Result)
			{
				TestEqual("Value on first Then", Result, 4);
			})
			.Then([this, Done]()
			{
				Done.Execute();
			}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
		});

		LatentIt("Can execute with no capture and then concatenate with capture", [this](const auto& Done)
		{
			UE::Tasks::MakeReadyFuture()
			.Then([this]()
			{
				return 5;
			})
			.Then([this, Done](int32 Result)
			{
				TestEqual("Value on second Then", Result, 5);
				Done.Execute();
			}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
		});
	});

	Describe("Value Or FError lambdas", [this]()
	{
		LatentIt("Can capture and return the value", [this](const auto& Done)
		{
			UE::Tasks::MakeReadyFuture<int32>(1)
			.Then([this, Done](UE::Tasks::TResult<int32> Result)
			{
				TestTrue("Future is completed", Result.HasValue());
				TestEqual("Value", Result.GetValue(), 1);
				Done.Execute();
			}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
		});

		LatentIt("Can concatenate Then", [this](const auto& Done)
		{
			UE::Tasks::MakeReadyFuture<int32>(1)
			.Then([this](UE::Tasks::TResult<int32> Result)
			{
				TestEqual("Value on first Then", Result.GetValue(), 1);
				return Result.GetValue() + 2;
			})
			.Then([this, Done](UE::Tasks::TResult<int32> Result)
			{
				TestTrue("Future is completed", Result.HasValue());
				TestEqual("Value on second Then", Result.GetValue(), 3);
				Done.Execute();
			}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
		});

		LatentIt("Can capture initial value and then concatenate void", [this](const auto& Done)
		{
			UE::Tasks::MakeReadyFuture<int32>(1)
			.Then([this](UE::Tasks::TResult<int32> Result)
			{
				TestEqual("Value on first Then", Result.GetValue(), 1);
			})
			.Then([this, Done](UE::Tasks::TResult<void> Result)
			{
				TestTrue("Future is completed", Result.HasValue());
				Done.Execute();
			}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
		});

		LatentIt("Can execute with no capture and then concatenate with capture", [this](const auto& Done)
		{
			UE::Tasks::MakeReadyFuture()
			.Then([]()
			{
				return 1;
			})
			.Then([this, Done](UE::Tasks::TResult<int32> Result)
			{
				TestTrue("Future is completed", Result.HasValue());
				TestEqual("Value on second Then", Result.GetValue(), 1);
				Done.Execute();
			}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
		});

		LatentIt("Can change future captured type", [this](const auto& Done)
		{
			UE::Tasks::MakeReadyFuture().Then([]()
			{
				return UE::Tasks::TResult<int32>(10);
			})
			.Then([this, Done](UE::Tasks::TResult<int32> Result)
			{
				TestTrue("Future is completed", Result.HasValue());
				TestEqual("Value on second Then", Result.GetValue(), 10);
				Done.Execute();
			}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
		});
	});

	Describe("Async", [this]()
	{
		LatentIt("Can execute Async without return", [this](const auto& Done)
		{
			MemberInt = 0;
			UE::Tasks::Async([this]()
			{
				MemberInt = 2;
			})
			.Then([this, Done]()
			{
				TestEqual("Value", MemberInt, 2);
				Done.Execute();
			}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
		});

		LatentIt("Can execute Async with return", [this](const auto& Done)
		{
			UE::Tasks::Async([]()
			{
				return 4;
			})
			.Then([this, Done](int32 Result)
			{
				TestEqual("Value", Result, 4);
				Done.Execute();
			}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
		});

		LatentIt("Can unwrap Async inside Then", [this](const auto& Done)
		{
			UE::Tasks::MakeReadyFuture()
			.Then([this]()
			{
				return IncrementAndStringify(10);
			})
			.Then([this, Done](const FString& Result)
			{
				TestEqual("Value on second Then", Result, TEXT("11"));
				Done.Execute();
			}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
		});

		LatentIt("Can unwrap inside an unwrapped Async inside Then", [this](const auto& Done)
		{
			UE::Tasks::MakeReadyFuture()
			.Then([]()
			{
				return UE::Tasks::Async([]()
				{
					return UE::Tasks::TResult<int32>(5);
				});
			})
			.Then([this, Done](int32 Result)
			{
				TestEqual("Value on second Then", Result, 5);
				Done.Execute();
			}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
		});

		LatentIt("Can unwrap Async inside Async", [this](const auto& Done)
		{
			UE::Tasks::Async([this]()
			{
				return IncrementAndStringify(20);
			})
			.Then([this, Done](const FString& Result)
			{
				TestEqual("Value on second Then", Result, TEXT("21"));
				Done.Execute();
			}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
		});

		LatentIt("Captured lambda value can be changed", [this](const auto& Done)
		{
			MemberInt = 2;
			UE::Tasks::Async([this]()
			{
				MemberInt = 4;
			})
			.Then([this, Done](UE::Tasks::TResult<void> Result)
			{
				TestTrue("Result was completed", Result.HasValue());
				TestEqual("Captured value", MemberInt, 4);
				Done.Execute();
			}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
		});
	});

	Describe("Errors", [this]()
	{
		LatentIt("Can return an error and be received in Result then", [this](const auto& Done)
		{
			UE::Tasks::Async([this]()
			{
				return UE::Tasks::TResult<int32>(UE::Tasks::FError(Context, Code, TEXT("Error Message")));
			})
			.Then([this, Done](UE::Tasks::TResult<int32> Result)
			{
				TestTrue("Result is an error", Result.HasError());
				TestEqual("Error Code", Result.GetError().GetCode(), Code);
				TestEqual("Error Context", Result.GetError().GetContext(), Context);
				TestEqual("Error Message", *Result.GetError().GetMessage(), TEXT("Error Message"));
				Done.Execute();
			}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
		});

		LatentIt("Value Then wont be called on error", [this](const auto& Done)
		{
			ContinuationCalled = false;

			UE::Tasks::Async([this]()
			{
				return UE::Tasks::TResult<int32>(UE::Tasks::FError(Context, Code));
			})
			.Then([this](int32 Result)
			{
				ContinuationCalled = true;
				return Result;
			})
			.Then([this, Done](UE::Tasks::TResult<int32> Result)
			{
				TestTrue("Result is an error", Result.HasError());
				TestEqual("Error Code", Result.GetError().GetCode(), Code);
				TestEqual("Error Context", Result.GetError().GetContext(), Context);

				TestFalse("Then Called", ContinuationCalled);
				Done.Execute();
			}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
		});

		LatentIt("FError can be passed through captured type change", [this](const auto& Done)
		{
			UE::Tasks::Async([this]()
			{
				return UE::Tasks::TResult<int32>(UE::Tasks::FError(Context, Code));
			})
			.Then([this](int32 Result)
			{
				return FString{};
			})
			.Then([this, Done](UE::Tasks::TResult<FString> Result)
			{
				TestTrue("Result is an error", Result.HasError());
				TestEqual("Error Code", Result.GetError().GetCode(), Code);
				TestEqual("Error Context", Result.GetError().GetContext(), Context);
				Done.Execute();
			}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
		});

		LatentIt("Can handle error", [this](const auto& Done)
		{
			UE::Tasks::Async([this]()
			{
				return UE::Tasks::TResult<int32>(UE::Tasks::FError(Context, Code, TEXT("Error Message")));
			})
			.Then([](UE::Tasks::TResult<int32> Result)
			{
				if (Result.HasError())
				{
					return Result.GetError().GetMessage();
				}
				return FString{};
			})
			.Then([this, Done](UE::Tasks::TResult<FString> Result)
			{
				TestFalse("Result is an error", Result.HasError());
				TestTrue("Result is completed", Result.HasValue());
				TestEqual("Captured String", Result.GetValue(), TEXT("Error Message"));
				Done.Execute();
			}, UE::Tasks::FOptions().Set(ENamedThreads::GameThread));
		});
	});
}
