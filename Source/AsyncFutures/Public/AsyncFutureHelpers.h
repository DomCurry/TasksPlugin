// Copyright(c) Dominic Curry. All rights reserved.
#pragma once
#include <atomic>

#include "AsyncFuture.h"
#include "Result.h"

#include "Containers/Ticker.h"

namespace UE::Tasks
{
	uint64 ERROR_INVALID_ARGUMENT = 3;

	enum class EFailMode
	{
		Full,
		Fast
	};

	template <class T>
	TAsyncFuture<T> MakeReadyFuture(T&& Value)
	{
		TAsyncPromise<T> Promise = TAsyncPromise<T>();
		Promise.SetValue(Forward<T>(Value));
		return Promise.GetFuture();
	}

	inline TAsyncFuture<void> MakeReadyFuture()
	{
		TAsyncPromise<void> Promise = TAsyncPromise<void>();
		Promise.SetValue();
		return Promise.GetFuture();
	}

	template <typename T>
	TAsyncFuture<T> MakeReadyFuture(const TResult<T>& Value)
	{
		TAsyncPromise<T> Promise = TAsyncPromise<T>();
		Promise.SetValue(Value);
		return Promise.GetFuture();
	}
	
	template <typename T>
	TAsyncFuture<T> MakeReadyFuture(TResult<T>&& Value)
	{
		TAsyncPromise<T> Promise = TAsyncPromise<T>();
		Promise.SetValue(Value);
		return Promise.GetFuture();
	}
	
	template <typename T, typename R>
	TAsyncFuture<T> MakeErrorFuture(const TResult<R>& Value)
	{
		TAsyncPromise<T> ErrorPromise = TAsyncPromise<T>();
		ErrorPromise.SetValue(Value.GetError());
		return ErrorPromise.GetFuture();
	}

	template <typename T, typename R>
	TAsyncFuture<T> MakeErrorFuture(TResult<R>&& Value)
	{
		TAsyncPromise<T> ErrorPromise = TAsyncPromise<T>();
		ErrorPromise.SetValue(Value.GetError());
		return ErrorPromise.GetFuture();
	}

	template <typename T>
	TAsyncFuture<T> MakeErrorFuture(const Error& Value)
	{
		TAsyncPromise<T> ErrorPromise = TAsyncPromise<T>();
		ErrorPromise.SetValue(Value);
		return ErrorPromise.GetFuture();
	}

	template <typename T>
	TAsyncFuture<T> MakeErrorFuture(Error&& Value)
	{
		TAsyncPromise<T> ErrorPromise = TAsyncPromise<T>();
		ErrorPromise.SetValue(MoveTemp(Value));
		return ErrorPromise.GetFuture();
	}

	template<typename F>
	auto Async(F&& Function, const FOptions& FutureOptions = FOptions())
	{
		return MakeReadyFuture().Then(MoveTemp(Function), FutureOptions);
	}

	template<typename T, typename F>
	auto Async(T* Owner, F&& Function, const FOptions& FutureOptions = FOptions())
	{
		return MakeReadyFuture().Then(Owner, MoveTemp(Function), FutureOptions);
	}

	template<typename T>
	TAsyncFuture<TArray<T>> WhenAll(const TArray<TAsyncFuture<T>>& Futures, const EFailMode FailMode)
	{
		if (Futures.Num() == 0)
		{
			return MakeReadyFuture<TArray<T>>(TArray<T>());
		}

		const int32 Count = Futures.Num();
		const TSharedRef<std::atomic<int32>, ESPMode::ThreadSafe> CounterRef = MakeShared<std::atomic<int32>, ESPMode::ThreadSafe>(Count);
		const TSharedRef<TAsyncPromise<TArray<T>>, ESPMode::ThreadSafe> PromiseRef = MakeShared<TAsyncPromise<TArray<T>>, ESPMode::ThreadSafe>();
		const TSharedRef<TArray<T>, ESPMode::ThreadSafe> ValueRef = MakeShared<TArray<T>, ESPMode::ThreadSafe>();
		const TSharedRef<TAsyncPromise<TArray<T>>, ESPMode::ThreadSafe> FirstErrorRef = MakeShared<TAsyncPromise<TArray<T>>, ESPMode::ThreadSafe>();
		ValueRef->AddZeroed(Count); //allow us to preserve the order 

		const auto SetPromise = [FirstErrorRef, PromiseRef]()
		{
			FirstErrorRef->GetFuture().Then([PromiseRef](TResult<TArray<T>> Result) { PromiseRef->SetValue(MoveTemp(Result)); });
		};

		if (FailMode == EFailMode::Fast)
		{
			SetPromise();
		}

		for (int32 i = 0; i < Count; ++i)
		{
			const auto& Future = Futures[i];
			Future.Then([CounterRef, ValueRef, FirstErrorRef, SetPromise, FailMode, i](const TResult<T>& Result)
				{
					if (Result.HasValue())
					{
						(*ValueRef)[i] = Result.GetValue();
					}
					else
					{
						FirstErrorRef->SetValue(Result.GetError());
					}

					if (--(CounterRef.Get()) == 0)
					{
						//TArray<T> Result;
						//for (int32 j = 0; j < Count; ++j)
						//{
						//	Result.Add(MoveTemp(*ValueRef->Find(j)));
						//}
						FirstErrorRef->SetValue(MoveTemp(ValueRef.Get()));

						if (FailMode != EFailMode::Fast)
						{
							SetPromise();
						}
					}
				});
		}
		return PromiseRef->GetFuture();
	}

	template<typename T>
	TAsyncFuture<TArray<T>> WhenAll(const TArray<TAsyncFuture<T>>& Futures) { return WhenAll<T>(Futures, EFailMode::Full); }

	TAsyncFuture<void> WhenAll(const TArray<TAsyncFuture<void>>& Futures, const EFailMode FailMode)

	{
		if (Futures.Num() == 0)
		{
			return UE::Tasks::MakeReadyFuture();
		}

		const auto CounterRef = MakeShared<std::atomic<int32>, ESPMode::ThreadSafe>(Futures.Num());
		const auto PromiseRef = MakeShared<TAsyncPromise<void>, ESPMode::ThreadSafe>();
		const auto FirstErrorRef = MakeShared<TAsyncPromise<void>, ESPMode::ThreadSafe>();

		const auto SetPromise = [FirstErrorRef, PromiseRef]()
			{
				FirstErrorRef->GetFuture().Then([PromiseRef](const TResult<void>& Result) { PromiseRef->SetValue(Result); });
			};

		if (FailMode == EFailMode::Fast)
		{
			SetPromise();
		}

		for (const auto& Future : Futures)
		{
			Future.Then([CounterRef, FirstErrorRef, SetPromise, FailMode](const TResult<void>& Result)
				{
					if (Result.HasError())
					{
						FirstErrorRef->SetValue(Result);
					}

					if (--(CounterRef.Get()) == 0)
					{
						FirstErrorRef->SetValue();

						if (FailMode != EFailMode::Fast)
						{
							SetPromise();
						}
					}
				});
		}
		return PromiseRef->GetFuture();
	}

	TAsyncFuture<void> WhenAll(const TArray<TAsyncFuture<void>>& Futures)
	{
		return WhenAll(Futures, EFailMode::Full);
	}

	template<typename T>
	TAsyncFuture<T> WhenAny(const TArray<TAsyncFuture<T>>& Futures)
	{
		if (Futures.Num() == 0)
		{
			return UE::Tasks::MakeErrorFuture<T>(Error(ERROR_CONTEXT_FUTURE, ERROR_INVALID_ARGUMENT, TEXT("UE::Tasks::WhenAny - Must have at least one element in the array.")));
		}
		auto PromiseRef = MakeShared<TAsyncPromise<T>, ESPMode::ThreadSafe>();
		for (auto& Future : Futures)
		{
			Future.Then([PromiseRef](const TResult<T>& Result)
				{
					PromiseRef->SetValue(TResult<T>(Result));
				});
		}
		return PromiseRef->GetFuture();
	}

	TAsyncFuture<void> WaitAsync(const float DelayInSeconds)
	{
		const TSharedRef<TAsyncPromise<void>, ESPMode::ThreadSafe> Promise = MakeShared<TAsyncPromise<void>>();

		FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda([Promise](const float Delta)
				{
					Promise->SetValue();

					static constexpr bool ExecuteAgain = false;
					return ExecuteAgain;
				}), DelayInSeconds);

		return Promise->GetFuture();
	}
}
