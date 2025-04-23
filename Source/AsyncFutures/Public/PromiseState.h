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
			: ValueSet(false)
			, TriggeringTask(TGraphTask<FNullGraphTask>::CreateTask().ConstructAndHold(TStatId(), ENamedThreads::GameThread))
			, Value(TOptional<TResult<T>>())
		{ }

		~TPromiseState()
		{
			check(IsSet()); //TFutures are going out of scope and they're holding promises
			if (FTaskGraphInterface::IsRunning() && TriggeringTask->IsCompleted() == false)
			{
				//TriggeringTask->Unlock();
			}
		}

		bool IsSet() const { return ValueSet; }
		TResult<T> Get() const { check(IsSet() && Value.IsSet()); return Value.GetValue(); }

		void SetValue(TResult<T>&& Result)
		{
			if (IsSet() == false)
			{
				ValueSet = true;
				if (Triggered == false)
				{
					Value = Result;
					Trigger();
				}
			}
		}

		void SetValue(const TResult<T>& Result)
		{
			if (IsSet() == false)
			{
				ValueSet = true;
				if (Triggered == false)
				{
					Value = Result;
					Trigger();
				}
			}
		}

		FGraphEventRef GetCompletionEvent() const
		{
			return TriggeringTask->GetCompletionEvent();
		}

	private:
		void Trigger()
		{
			check(IsSet());
			GetCompletionEvent()->DispatchSubsequents();
			Triggered = true;
		}

	public:
		std::atomic_bool ValueSet = false;
		std::atomic_bool Triggered = false;
		TGraphTask<FNullGraphTask>* const TriggeringTask;

		TOptional<TResult<T>> Value;
	};

	template<>
	class TPromiseState<void>
	{
	public:
		TPromiseState() 
			: ValueSet(false)
			, TriggeringTask(TGraphTask<FNullGraphTask>::CreateTask().ConstructAndHold(TStatId(), ENamedThreads::GameThread))
			, Value(TOptional<TResult<void>>())
		{}

		~TPromiseState() 
		{
			check(IsSet()); //TFutures are going out of scope and they're holding promises
			if (FTaskGraphInterface::IsRunning() && TriggeringTask->IsCompleted() == false)
			{
				//TriggeringTask->Unlock();
			}
		} 

		bool IsSet() const { return ValueSet; }
		TResult<void> Get() const { check(IsSet() && Value.IsSet()); return Value.GetValue(); }

		void SetValue(TResult<void>&& Result)
		{
			if (IsSet() == false)
			{
				ValueSet = true;
				if (Triggered == false)
				{
					Value = Result;
					Trigger();
				}
			}
		}

		void SetValue(const TResult<void>& Result)
		{
			if (IsSet() == false)
			{
				ValueSet = true;
				if (Triggered == false)
				{
					Value = Result;
					Trigger();
				}
			}
		}

		FGraphEventRef GetCompletionEvent() const
		{
			return TriggeringTask->GetCompletionEvent();
		}

	private:
		void Trigger() 
		{ 
			check(IsSet());
			GetCompletionEvent()->DispatchSubsequents();
			Triggered = true;
		}

	public:
		std::atomic_bool ValueSet = false;
		std::atomic_bool Triggered = false;
		TGraphTask<FNullGraphTask>* const TriggeringTask;

		TOptional<TResult<void>> Value;
	};
}
