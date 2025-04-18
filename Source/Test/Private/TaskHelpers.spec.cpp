// Copyright(c) Dominic Curry. All rights reserved.
#include <CoreMinimal.h>
#include <AsyncFutures.h>

BEGIN_DEFINE_SPEC(FAsyncFuturesSpec_TaskHelpers, "AsyncFutures.TaskHelpers", EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ServerContext)

END_DEFINE_SPEC(FAsyncFuturesSpec_TaskHelpers)

void FAsyncFuturesSpec_TaskHelpers::Define()
{
	LatentIt("Can Make Ready Future from value", [this](const auto& Done)
		{
			UE::Tasks::MakeReadyFuture(5).Then([this, Done](const UE::Tasks::TResult<int32>& Result)
				{
					TestTrue("Future is completed", Result.HasValue());
					TestEqual("Future value is correct", Result.GetValue(), 5);
					TestTrue("Future isn't error", !Result.HasError());
					Done.Execute();
				});
		});

	LatentIt("Can Make Ready Future from void value", [this](const auto& Done)
		{
			UE::Tasks::MakeReadyFuture().Then([this, Done](const UE::Tasks::TResult<void>& Result)
				{
					TestTrue("Future is completed", Result.HasValue());
					TestTrue("Future isn't error", !Result.HasError());
					Done.Execute();
				});
		});

	LatentIt("Can Make Ready Future from result", [this](const auto& Done)
		{
			UE::Tasks::MakeReadyFuture<int32>(UE::Tasks::TResult<int32>(5)).Then([this, Done](const UE::Tasks::TResult<int32>& Result)
				{
					TestTrue("Future is completed", Result.HasValue());
					TestEqual("Future value is correct", Result.GetValue(), 5);
					TestTrue("Future isn't error", !Result.HasError());
					Done.Execute();
				});
		});

	LatentIt("Can Make Ready Future from void result", [this](const auto& Done)
		{
			UE::Tasks::MakeReadyFuture<void>(UE::Tasks::TResult<void>()).Then([this, Done](const UE::Tasks::TResult<void>& Result)
				{
					TestTrue("Future is completed", Result.HasValue());
					TestTrue("Future isn't error", !Result.HasError());
					Done.Execute();
				});
		});

	LatentIt("Can Make Error Future from value", [this](const auto& Done)
		{
			UE::Tasks::MakeErrorFuture<int32>(UE::Tasks::Error(1, 2, TEXT("rvalue error"))).Then([this, Done](const UE::Tasks::TResult<int32>& Result)
				{
					TestTrue("Future is error", !Result.HasValue());
					TestTrue("Future has error", Result.HasError());
					TestEqual ("Future error is correct", Result.GetError(), UE::Tasks::Error(1, 2, TEXT("rvalue error")));
					Done.Execute();
				});
		});

	LatentIt("Can Make Error Future from void value", [this](const auto& Done)
		{
			UE::Tasks::Error Error = UE::Tasks::Error(1, 2, TEXT("lvalue error"));
			UE::Tasks::MakeErrorFuture<int32>(Error).Then([this, Done, Error](const UE::Tasks::TResult<int32>& Result)
				{
					TestTrue("Future is error", !Result.HasValue());
					TestTrue("Future has error", Result.HasError());
					TestEqual("Future error is correct", Result.GetError(), Error);
					Done.Execute();
				});
		});

	LatentIt("Can Make Error Future from result", [this](const auto& Done)
		{
			UE::Tasks::Error Error = UE::Tasks::Error(1, 2, TEXT("lvalue error"));
			UE::Tasks::MakeReadyFuture<int32>(UE::Tasks::TResult<int32>(Error)).Then([this, Done, Error](const UE::Tasks::TResult<int32>& Result)
				{
					TestTrue("Future is error", !Result.HasValue());
					TestTrue("Future has error", Result.HasError());
					TestEqual("Future error is correct", Result.GetError(), Error);
					Done.Execute();
				});
		});

	LatentIt("Can Make Error Future from void result", [this](const auto& Done)
		{
			UE::Tasks::MakeReadyFuture<void>(UE::Tasks::TResult<void>(UE::Tasks::Error(1, 2, TEXT("rvalue error")))).Then([this, Done](const UE::Tasks::TResult<void>& Result)
				{
					TestTrue("Future is completed", !Result.HasValue());
					TestTrue("Future has error", Result.HasError());
					TestEqual("Future error is correct", Result.GetError(), UE::Tasks::Error(1, 2, TEXT("rvalue error")));
					Done.Execute();
				});
		});
	
	LatentIt("Can Make Error Future from converted result", [this](const auto& Done)
		{
			UE::Tasks::Error Error = UE::Tasks::Error(1, 2, TEXT("lvalue error"));
			UE::Tasks::MakeErrorFuture<int32>(UE::Tasks::TResult<void>(Error)).Then([this, Done, Error](const UE::Tasks::TResult<int32>& Result)
				{
					TestTrue("Future is error", !Result.HasValue());
					TestTrue("Future has error", Result.HasError());
					TestEqual("Future error is correct", Result.GetError(), Error);
					Done.Execute();
				});
		});
	
	LatentIt("Can Make Error Future from void converted result", [this](const auto& Done)
		{
			UE::Tasks::MakeErrorFuture<void>(UE::Tasks::TResult<int32>(UE::Tasks::Error(1, 2, TEXT("rvalue error")))).Then([this, Done](const UE::Tasks::TResult<void>& Result)
				{
					TestTrue("Future is completed", !Result.HasValue());
					TestTrue("Future has error", Result.HasError());
					TestEqual("Future error is correct", Result.GetError(), UE::Tasks::Error(1, 2, TEXT("rvalue error")));
					Done.Execute();
				});
		});

	LatentIt("Can wait for future", [this](const auto& Done)
		{
			const FDateTime Start = FDateTime::UtcNow();
			UE::Tasks::WaitAsync(0.1f).Then([this, Done, Start](const UE::Tasks::TResult<void>& Result)
				{
					const FDateTime End = FDateTime::UtcNow();
					const FTimespan Duration = End - Start;

					TestTrue("Future is completed", Result.HasValue());
					TestTrue("Future has no error", !Result.HasError());
					TestGreaterEqual(TEXT("Duration is correct"), Duration.GetTotalSeconds(), 0.1, 0.03); //we only garentee that we'll wait that time, scheduling may take longer. Also account for FTSTicker drift.
					Done.Execute();
				});
		});
}
