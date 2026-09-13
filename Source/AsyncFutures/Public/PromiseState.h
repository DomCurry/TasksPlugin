// Copyright Dominic Curry. All Rights Reserved.
#pragma once

// Module Includes
#include "Error.h"
#include "FunctionTypes.h"
#include "Result.h"
#include "UnwrapTypes.h"

namespace UE::Tasks::Private
{
	template<typename T>
	class TPromiseState
	{
	public:
		TPromiseState()
			: CompletionEvent(FGraphEvent::CreateGraphEvent())
			, Value()
		{ }

		~TPromiseState()
		{
			check(IsClaimed()); //TFutures are going out of scope and they're holding promises
		}

		bool IsSet() const { return ValuePublished.load(std::memory_order_acquire); }
		bool IsClaimed() const { return ValueClaimed.load(std::memory_order_acquire); }

		TResult<T> Get() const { check(IsSet() && Value.IsSet()); return Value.GetValue(); }

		void SetValue(TResult<T>&& Result)
		{
			bool bExpected = false;
			if (!ValueClaimed.compare_exchange_strong(bExpected, true, std::memory_order_acq_rel))
			{
				return; // someone else already owns this promise
			}

			Value.Emplace(MoveTemp(Result));
			ValuePublished.store(true, std::memory_order_release);
			CompletionEvent->DispatchSubsequents();
		}

		void SetValue(const TResult<T>& Result)
		{
			bool bExpected = false;
			if (!ValueClaimed.compare_exchange_strong(bExpected, true, std::memory_order_acq_rel))
			{
				return; // someone else already owns this promise
			}

			Value.Emplace(Result);
			ValuePublished.store(true, std::memory_order_release);
			CompletionEvent->DispatchSubsequents();
		}

		FGraphEventRef GetCompletionEvent() const
		{
			return CompletionEvent;
		}

	private:
		std::atomic_bool ValueClaimed{ false };
		std::atomic_bool ValuePublished{ false };
		FGraphEventRef CompletionEvent;

		TOptional<TResult<T>> Value;
	};

	template<>
	class TPromiseState<void>
	{
	public:
		TPromiseState()
			: CompletionEvent(FGraphEvent::CreateGraphEvent())
			, Value()
		{ }

		~TPromiseState()
		{
			check(IsClaimed()); //TFutures are going out of scope and they're holding promises
		}

		bool IsSet() const { return ValuePublished.load(std::memory_order_acquire); }
		bool IsClaimed() const { return ValueClaimed.load(std::memory_order_acquire); }

		TResult<void> Get() const { check(IsSet() && Value.IsSet()); return Value.GetValue(); }

		void SetValue(TResult<void>&& Result)
		{
			bool bExpected = false;
			if (!ValueClaimed.compare_exchange_strong(bExpected, true, std::memory_order_acq_rel))
			{
				return; // someone else already owns this promise
			}

			Value.Emplace(MoveTemp(Result));
			ValuePublished.store(true, std::memory_order_release);
			CompletionEvent->DispatchSubsequents();
		}

		void SetValue(const TResult<void>& Result)
		{
			bool bExpected = false;
			if (!ValueClaimed.compare_exchange_strong(bExpected, true, std::memory_order_acq_rel))
			{
				return; // someone else already owns this promise
			}

			Value.Emplace(Result);
			ValuePublished.store(true, std::memory_order_release);
			CompletionEvent->DispatchSubsequents();
		}

		FGraphEventRef GetCompletionEvent() const
		{
			return CompletionEvent;
		}

	private:
		std::atomic_bool ValueClaimed{ false };
		std::atomic_bool ValuePublished{ false };
		FGraphEventRef CompletionEvent;

		TOptional<TResult<void>> Value;
	};
}
