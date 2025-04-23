// Copyright Dominic Curry. All Rights Reserved.
#pragma once

// Engine Includes
#include "Async/Async.h"
#include "CoreTypes.h"
#include "Tasks/Task.h"
#include "Templates/SharedPointer.h"
#include "Misc/AssertionMacros.h"
#include "Misc/IQueuedWork.h"
#include "Misc/QueuedThreadPool.h"

// Module Includes
#include "Error.h"
#include "LifetimeMonitor.h"
#include "Result.h"
#include "PromiseState.h"

namespace UE::Tasks
{
	uint64 ERROR_LIFETIME = 2;
	class FOptions;

	namespace Private
	{
		template<typename Func, typename ResultType, typename Monitor>
		auto Then(
			Func&& Function,
			const TSharedRef<TPromiseState<ResultType>, ESPMode::ThreadSafe>& PreviousPromise,
			const FOptions& Options,
			Monitor LifetimeMonitor);
	}

	template<typename ResultType>
	class TAsyncFuture
	{
		using UnwrappedResultType = typename Private::TUnwrap<ResultType>::Type;
		using ExpectedResultType = TResult<UnwrappedResultType>;

	public:
		using TResult = ResultType;

		//Construction and copying
		TAsyncFuture() {}
		TAsyncFuture(TAsyncFuture<ResultType>&& Other) : Promise(MoveTemp(Other.Promise)) {}
		TAsyncFuture<ResultType>& operator= (TAsyncFuture<ResultType>&& Other)
		{
			Promise = MoveTemp(Other.Promise);
			return *this;
		}
		TAsyncFuture(const TAsyncFuture<ResultType>& Other) : Promise(Other.Promise) {}
		TAsyncFuture<ResultType>& operator= (const TAsyncFuture<ResultType>& Other)
		{
			Promise = Other.Promise;
			return *this;
		}
		TAsyncFuture(TSharedRef<Private::TPromiseState<ResultType>, ESPMode::ThreadSafe>&& Other) : Promise(MoveTemp(Other)) {}
		TAsyncFuture<ResultType>& operator= (TSharedRef<Private::TPromiseState<ResultType>, ESPMode::ThreadSafe>&& Other)
		{
			Promise = MoveTemp(Promise);
			return *this;
		}
		TAsyncFuture(const TSharedRef<Private::TPromiseState<ResultType>, ESPMode::ThreadSafe>& Other) : Promise(Other) {}
		TAsyncFuture<ResultType>& operator= (const TSharedRef<Private::TPromiseState<ResultType>, ESPMode::ThreadSafe>& Other)
		{
			Promise = Other;
			return *this;
		}

		//Getters
		bool IsValid() const { return Promise.IsValid(); }
		bool IsReady() const { return IsValid() && Promise->IsSet(); }
		ExpectedResultType Get() const { check(IsReady()); return Promise->Get(); }

		//Continuations
		template<typename Func>
		auto Then(Func&& Function, const FOptions& Options = FOptions()) const
		{
			check(IsValid())
			return Private::Then<Func, ResultType>(Forward<Func>(Function), Promise.ToSharedRef(), Options, TLifetimeMonitor<void>());
		}

		template<typename Func, typename TOwner>
		auto Then(TOwner* Owner, Func&& Function, const FOptions& Options = FOptions()) const
		{
			check(IsValid())
			return Private::Then<Func, ResultType>(Forward<Func>(Function), Promise.ToSharedRef(), Options, TLifetimeMonitor<TOwner>(Owner));
		}

	private:
		TSharedPtr<Private::TPromiseState<ResultType>, ESPMode::ThreadSafe> Promise;
	};

	//void Specialization
	template<>
	class TAsyncFuture<void>
	{
		using UnwrappedResultType = void;
		using ExpectedResultType = TResult<void>;

	public:
		using TResult = void;

		//Construction and copying
		TAsyncFuture() {}
		TAsyncFuture(TAsyncFuture<void>&& Other) : Promise(MoveTemp(Other.Promise)) {}
		TAsyncFuture<void>& operator= (TAsyncFuture<void>&& Other)
		{
			Promise = MoveTemp(Other.Promise);
			return *this;
		}
		TAsyncFuture(const TAsyncFuture<void>& Other) : Promise(Other.Promise) {}
		TAsyncFuture<void>& operator= (const TAsyncFuture<void>& Other)
		{
			Promise = Other.Promise;
			return *this;
		}
		TAsyncFuture(TSharedRef<Private::TPromiseState<void>, ESPMode::ThreadSafe>&& Other) : Promise(MoveTemp(Other)) {}
		TAsyncFuture<void>& operator= (TSharedRef<Private::TPromiseState<void>, ESPMode::ThreadSafe>&& Other)
		{
			Promise = MoveTemp(Promise);
			return *this;
		}
		TAsyncFuture(const TSharedRef<Private::TPromiseState<void>, ESPMode::ThreadSafe>& Other) : Promise(Other) {}
		TAsyncFuture<void>& operator= (const TSharedRef<Private::TPromiseState<void>, ESPMode::ThreadSafe>& Other)
		{
			Promise = Other;
			return *this;
		}

		//Getters
		bool IsValid() const { return Promise.IsValid(); }
		bool IsReady() const { return IsValid() && Promise->IsSet(); }
		ExpectedResultType Get() const { check(IsReady()); return Promise->Get(); }

		//Continuations
		template<typename Func>
		auto Then(Func&& Function, const FOptions& Options = FOptions()) const
		{
			check(IsValid());
			return Private::Then(MoveTemp(Function), Promise.ToSharedRef(), Options, TLifetimeMonitor<void>());
		}

		template<typename Func, typename TOwner>
		auto Then(TOwner* Owner, Func&& Function, const FOptions& Options = FOptions()) const
		{
			check(IsValid());
			return Private::Then(MoveTemp(Function), Promise.ToSharedRef(), Options, TLifetimeMonitor<TOwner>(Owner));
		}

	private:
		TSharedPtr<Private::TPromiseState<void>, ESPMode::ThreadSafe> Promise;
	};

	template<typename T>
	class TAsyncPromise
	{
	public:
		TAsyncPromise()
			: State(MakeShared<Private::TPromiseState<T>>())
		{}

		TAsyncPromise(const TAsyncPromise& Other) = default;
		TAsyncPromise& operator=(const TAsyncPromise& Other) = default;
		TAsyncPromise(TAsyncPromise&& Other) = default;
		TAsyncPromise& operator=(TAsyncPromise&& Other) = default;

		TAsyncFuture<T> GetFuture() { return TAsyncFuture<T>(State); }
		bool IsSet() const { return State->IsSet(); }
		TResult<T> Get() const { return State->Get(); }

		//fulfilling promise
		void SetValue(const TResult<T>& Result) const { State->SetValue(Result); }
		void SetValue(TResult<T>&& Result) const { State->SetValue(MoveTemp(Result)); }
		void SetValue(const T& Result) const { State->SetValue(Result); }
		void SetValue(T&& Result) const { State->SetValue(Result); }
		void SetValue(const FError& Result) const { State->SetValue(Result); }
		void SetValue(FError&& Result) const { State->SetValue(Result); }
		void Cancel() const { SetValue(MakeCancelledError()); }

	public:
		TSharedRef<Private::TPromiseState<T>, ESPMode::ThreadSafe> State;
	};

	//void Specialization
	template<>
	class TAsyncPromise<void>
	{
	public:
		TAsyncPromise()
			: State(MakeShared<Private::TPromiseState<void>>())
		{}

		TAsyncPromise(const TAsyncPromise& Other) = default;
		TAsyncPromise& operator=(const TAsyncPromise& Other) = default;
		TAsyncPromise(TAsyncPromise&& Other) = default;
		TAsyncPromise& operator=(TAsyncPromise&& Other) = default;

		TAsyncFuture<void> GetFuture() { return TAsyncFuture<void>(State); }
		bool IsSet() const { return State->IsSet(); }
		TResult<void> Get() const { return State->Get(); }

		//fulfilling promise
		void SetValue(const TResult<void>& Result) const { State->SetValue(Result); }
		void SetValue(TResult<void>&& Result) const { State->SetValue(MoveTemp(Result)); }
		void SetValue() const { State->SetValue(TResult<void>()); }
		void SetValue(const FError& Result) const { State->SetValue(TResult<void>(Result)); }
		void SetValue(FError&& Result) const { State->SetValue(TResult<void>(Result)); }
		void Cancel() const { SetValue(MakeCancelledError()); }

	public:
		TSharedRef<Private::TPromiseState<void>, ESPMode::ThreadSafe> State;
	};

	namespace Private
	{
		class IBoundPromise
		{
		public:
			virtual void Cancel() const = 0;
			virtual ~IBoundPromise() {}
		};

		template<typename TPromiseType>
		class TBoundPromise : public IBoundPromise
		{
		public:
			TBoundPromise(const TAsyncPromise<TPromiseType>& PromiseIn) : Promise(PromiseIn) {  }
			TBoundPromise(TAsyncPromise<TPromiseType>&& PromiseIn) : Promise(MoveTemp(PromiseIn)) {  }
			virtual void Cancel() const override { Promise.Cancel(); }
		private:
			TAsyncPromise<TPromiseType> Promise;
		};

		class FCancellationState
		{
		public:
			FCancellationState() : Cancelled(false) {}
			~FCancellationState() { Cancel(); }
			void Cancel()
			{
				Cancelled = true;
				for (const TSharedRef<IBoundPromise, ESPMode::ThreadSafe>& Promise : Promises)
				{
					Promise->Cancel();
				}
				Promises.Empty();
			}

			template<typename TPromiseType>
			void Bind(const TAsyncPromise<TPromiseType>& PromiseIn)
			{
				if (Cancelled)
				{
					PromiseIn.Cancel();
					return;
				}
				TSharedRef<TBoundPromise<TPromiseType>, ESPMode::ThreadSafe> BoundPromise = MakeShared<TBoundPromise<TPromiseType>>(PromiseIn);
				Promises.Emplace(MoveTemp(BoundPromise));
			}

		private:
			std::atomic_bool Cancelled = false;
			TArray<TSharedRef<IBoundPromise, ESPMode::ThreadSafe>> Promises;
		};
	}

	class FCancellationHandle
	{
	public:
		FCancellationHandle() : State(MakeShared<Private::FCancellationState>()) {}

		FCancellationHandle(const FCancellationHandle& Other) : State(Other.State) {}
		FCancellationHandle(FCancellationHandle&& Other) : State(MoveTemp(Other.State)) {}
		FCancellationHandle& operator= (const FCancellationHandle& Other) { State = Other.State; return *this; }
		FCancellationHandle& operator= (FCancellationHandle&& Other) { State = MoveTemp(Other.State); return *this; }

		template<typename TPromiseType>
		void Bind(const TAsyncPromise<TPromiseType>& PromiseIn) { State->Bind(PromiseIn); }
		void Cancel() { State->Cancel(); }
	private:
		TSharedRef<Private::FCancellationState, ESPMode::ThreadSafe> State;
		friend class FWeakCancellationHandle;
	};

	class FWeakCancellationHandle
	{
	public:
		FWeakCancellationHandle() : State(MakeShared<Private::FCancellationState>()) {}

		FWeakCancellationHandle(const FCancellationHandle& Other) : State(Other.State) {}
		FWeakCancellationHandle(FCancellationHandle&& Other) : State(MoveTemp(Other.State)) {}
		FWeakCancellationHandle& operator= (const FCancellationHandle& Other) { State = Other.State; return *this; }
		FWeakCancellationHandle& operator= (FCancellationHandle&& Other) { State = MoveTemp(Other.State); return *this; }


		FWeakCancellationHandle(const FWeakCancellationHandle& Other) : State(Other.State) {}
		FWeakCancellationHandle(FWeakCancellationHandle&& Other) : State(MoveTemp(Other.State)) {}
		FWeakCancellationHandle& operator= (const FWeakCancellationHandle& Other) { State = Other.State; return *this; }
		FWeakCancellationHandle& operator= (FWeakCancellationHandle&& Other) { State = MoveTemp(Other.State); return *this; }

	private:
		TWeakPtr<Private::FCancellationState, ESPMode::ThreadSafe> State;
	};

	class FOptions
	{
	public:
		FOptions()
			: Thread(TOptional<ENamedThreads::Type>())
			, CancellationHandle(TOptional<FCancellationHandle>())
			, Execution(TOptional<EAsyncExecution>())
		{
		}

		//FOptions& Set(ENamedThreads::Type&& ThreadIn) { Thread = MoveTemp(ThreadIn); return *this; }
		FOptions& Set(const ENamedThreads::Type ThreadIn) { Thread = ThreadIn; return *this; }
		//FOptions& Set(FCancellationHandle&& HandleIn) { CancellationHandle = MoveTemp(HandleIn); return *this; }
		FOptions& Set(const FCancellationHandle& HandleIn) { CancellationHandle = HandleIn; return *this; }
		//FOptions& Set(EAsyncExecution&& ExecutionIn) { Execution = MoveTemp(ExecutionIn); return *this; }
		FOptions& Set(const EAsyncExecution ExecutionIn) { Execution = ExecutionIn; return *this; }
		
		TOptional<FCancellationHandle> GetCancellation() const { return CancellationHandle; }
		ENamedThreads::Type GetDesiredThread() const {	return Thread.Get(ENamedThreads::AnyThread); }
		EAsyncExecution GetExecutionPolicy() const {	return Execution.Get(EAsyncExecution::TaskGraph); }

	private:
		TOptional<ENamedThreads::Type> Thread;
		TOptional<FCancellationHandle> CancellationHandle;
		TOptional<EAsyncExecution> Execution;
	};

	namespace Private
	{
		template<typename P, typename R, typename F,
			typename TContinuationTypes<F, R>::Traits::IsVoidToVoid::Type* = nullptr>
		void ExecuteContinuation(const TAsyncPromise<P>& Promise, const TResult<R>& Result, F&& Function)
		{
			if (Promise.IsSet())
			{
				return;
			}

			if (Result.HasError())
			{
				Promise.SetValue(Result.GetError());
			}
			else
			{
				Function();
				Promise.SetValue();
			}
		}

		template<typename P, typename R, typename F,
			typename TContinuationTypes<F, R>::Traits::IsRealValueToVoid::Type* = nullptr>
		void ExecuteContinuation(const TAsyncPromise<P>& Promise, const TResult<R>& Result, F&& Function)
		{
			if (Promise.IsSet())
			{
				return;
			}

			if (Result.HasError())
			{
				Promise.SetValue(Result.GetError());
			}
			else if (Result.HasValue())
			{
				Function(Result.GetValue());
				Promise.SetValue();
			}
			else
			{
				checkf(false, TEXT("Expected a set TResult"));
			}
		}

		template<typename P, typename R, typename F,
			typename TContinuationTypes<F, R>::Traits::IsResultToVoid::Type* = nullptr>
		void ExecuteContinuation(const TAsyncPromise<P>& Promise, const TResult<R>& Result, F&& Function)
		{
			Function(Result);
			Promise.SetValue();
		}

		template<typename P, typename R, typename F,
			typename TContinuationTypes<F, R>::Traits::IsVoidToResult::Type* = nullptr>
		void ExecuteContinuation(const TAsyncPromise<P>& Promise, const TResult<R>& Result, F&& Function)
		{
			if (Promise.IsSet())
			{
				return;
			}

			if (Result.HasError())
			{
				Promise.SetValue(Result.GetError());
			}
			else
			{
				Promise.SetValue(Function());
			}
		}

		template<typename P, typename R, typename F,
			typename TContinuationTypes<F, R>::Traits::IsRealValueToResult::Type* = nullptr>
		void ExecuteContinuation(const TAsyncPromise<P>& Promise, const TResult<R>& Result, F&& Function)
		{
			if (Promise.IsSet())
			{
				return;
			}

			if (Result.HasError())
			{
				Promise.SetValue(Result.GetError());
			}
			else if (Result.HasValue())
			{
				Promise.SetValue(Function(Result.GetValue()));
			}
			else
			{
				checkf(false, TEXT("Expected a set TResult"));
			}
		}

		template<typename P, typename R, typename F,
			typename TContinuationTypes<F, R>::Traits::IsResultToResult::Type* = nullptr>
		void ExecuteContinuation(const TAsyncPromise<P>& Promise, const TResult<R>& Result, F&& Function)
		{
			Promise.SetValue(Function(Result));
		}

		template<typename P, typename R, typename F,
			typename TContinuationTypes<F, R>::Traits::IsVoidToRealValue::Type* = nullptr>
		void ExecuteContinuation(const TAsyncPromise<P>& Promise, const TResult<R>& Result, F&& Function)
		{
			if (Promise.IsSet())
			{
				return;
			}

			if (Result.HasError())
			{
				Promise.SetValue(Result.GetError());
			}
			else
			{
				Promise.SetValue(Function());
			}
		}

		template<typename P, typename R, typename F,
			typename TContinuationTypes<F, R>::Traits::IsRealValueToRealValue::Type* = nullptr>
		void ExecuteContinuation(const TAsyncPromise<P>& Promise, const TResult<R>& Result, F&& Function)
		{
			if (Promise.IsSet())
			{
				return;
			}

			if (Result.HasError())
			{
				Promise.SetValue(Result.GetError());
			}
			else if (Result.HasValue())
			{
				Promise.SetValue(Function(Result.GetValue()));
			}
			else
			{
				checkf(false, TEXT("Expected a set TResult"));
			}
		}

		template<typename P, typename R, typename F,
			typename TContinuationTypes<F, R>::Traits::IsResultToRealValue::Type* = nullptr>
		void ExecuteContinuation(const TAsyncPromise<P>& Promise, const TResult<R>& Result, F&& Function)
		{
			Promise.SetValue(Function(Result));
		}

		template<typename P, typename R, typename F,
			typename TContinuationTypes<F, R>::Traits::IsVoidToFuture::Type* = nullptr>
		void ExecuteContinuation(const TAsyncPromise<P>& Promise, const TResult<R>& Result, F&& Function)
		{
			if (Promise.IsSet())
			{
				return;
			}

			if (Result.HasError())
			{
				Promise.SetValue(Result.GetError());
			}
			else
			{
				Function().Then([Promise](const TResult<P>& Value) { Promise.SetValue(Value); });
			}
		}

		template<typename P, typename R, typename F,
			typename TContinuationTypes<F, R>::Traits::IsRealValueToFuture::Type* = nullptr>
		void ExecuteContinuation(const TAsyncPromise<P>& Promise, const TResult<R>& Result, F&& Function)
		{
			if (Promise.IsSet())
			{
				return;
			}

			if (Result.HasError())
			{
				Promise.SetValue(Result.GetError());
			}
			else if (Result.HasValue())
			{
				Function(Result.GetValue()).Then([Promise](const TResult<P>& Value) { Promise.SetValue(Value); });
			}
			else
			{
				checkf(false, TEXT("Expected a set TResult"));
			}
		}

		template<typename P, typename R, typename F,
			typename TContinuationTypes<F, R>::Traits::IsResultToFuture::Type* = nullptr>
		void ExecuteContinuation(const TAsyncPromise<P>& Promise, const TResult<R>& Result, F&& Function)
		{
			Function(Result).Then([Promise](const TResult<P>& Value) { Promise.SetValue(Value); });
		}
	}

	namespace Private
	{
		//Wrapper task for the async call. When this executes we then schedule a free task on whatever thread/execution we've specified
		template<typename TFunctionType, typename TResultType, typename TPromiseType, typename TLifetimeMonitor>
		class TContinuationTask : public FAsyncGraphTaskBase
		{
			using TPromiseRef = TSharedRef<TAsyncPromise<TPromiseType>, ESPMode::ThreadSafe>;
			using TRootFunction = typename std::remove_cv_t<typename TRemoveReference<TFunctionType>::Type>;

		public:
			TContinuationTask(TFunctionType&& InFunction, 
				TPromiseRef&& InPromise,
				const TSharedRef<TPromiseState<TResultType>, ESPMode::ThreadSafe>& InPreviousPromise,
				TLifetimeMonitor&& InLifetimeMonitor,
				const FOptions& Options)
				: MyPromise(MoveTemp(InPromise))
				, PreviousPromise(InPreviousPromise)
				, ContinuationFunction(Forward<TFunctionType>(InFunction))
				, LifetimeMonitor(MoveTemp(InLifetimeMonitor))
				, DesiredThread(Options.GetDesiredThread())
				, Execution(Options.GetExecutionPolicy())
			{
				const TOptional<FCancellationHandle>& Cancellation = Options.GetCancellation();
				if (Cancellation.IsSet())
				{
					FCancellationHandle Handle = Cancellation.GetValue();
					Handle.Bind(*MyPromise);
				}
			}

			void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
			{
				check(PreviousPromise->IsSet());
				auto Function = [
					InPromise = MoveTemp(MyPromise),
					InPreviousPromise = MoveTemp(PreviousPromise), 
					InContinuationFunction = MoveTemp(ContinuationFunction),
					InLifetimeMonitor = MoveTemp(LifetimeMonitor),
					InThread = MoveTemp(DesiredThread)
				]() mutable -> int32
					{
						if (!InPromise->IsSet())
						{
							if (auto PinnedObject = InLifetimeMonitor.Pin())
							{
								check(InPreviousPromise->IsSet());
								ExecuteContinuation(*InPromise, InPreviousPromise->Get(), MoveTemp(InContinuationFunction));
							}
							else
							{
								InPromise->SetValue(FError(ERROR_CONTEXT_FUTURE, ERROR_LIFETIME, TEXT("Owner lifetime expired")));
							}
						}

						return 0;
					};
				TPromise<int32> Promise = TPromise<int32>();
				//Copied from Async.h to allow us to pass the thread to the task graph
				switch (Execution)
				{
				case EAsyncExecution::TaskGraphMainThread:
				{
					TGraphTask<TAsyncGraphTask<int32>>::CreateTask().
						ConstructAndDispatchWhenReady(
							MoveTemp(Function), 
							MoveTemp(Promise),
							ENamedThreads::GameThread);
				}
				break;
				case EAsyncExecution::TaskGraph:
				{
					TGraphTask<TAsyncGraphTask<int32>>::CreateTask().
						ConstructAndDispatchWhenReady(
							MoveTemp(Function), 
							MoveTemp(Promise),
							DesiredThread);
				}
				break;

				case EAsyncExecution::Thread:
					if (FPlatformProcess::SupportsMultithreading())
					{
						TPromise<FRunnableThread*> ThreadPromise = TPromise<FRunnableThread*>();
						TAsyncRunnable<int32>* Runnable = new TAsyncRunnable<int32>(
							MoveTemp(Function), 
							MoveTemp(Promise),
							ThreadPromise.GetFuture());

						const FString TAsyncThreadName = FString::Printf(TEXT("TAsync %d"), FAsyncThreadIndex::GetNext());
						FRunnableThread* RunnableThread = FRunnableThread::Create(Runnable, *TAsyncThreadName);

						check(RunnableThread != nullptr);
						check(RunnableThread->GetThreadType() == FRunnableThread::ThreadType::Real);

						ThreadPromise.SetValue(RunnableThread);
					}
					else
					{
						Function();
					}
					break;

				case EAsyncExecution::ThreadIfForkSafe:
					if (FPlatformProcess::SupportsMultithreading() || FForkProcessHelper::IsForkedMultithreadInstance())
					{
						TPromise<FRunnableThread*> ThreadPromise;
						TAsyncRunnable<int32>* Runnable = new TAsyncRunnable<int32>(
							MoveTemp(Function), 
							MoveTemp(Promise),
							ThreadPromise.GetFuture());

						const FString TAsyncThreadName = FString::Printf(TEXT("TAsync %d"), FAsyncThreadIndex::GetNext());
						FRunnableThread* RunnableThread = FForkProcessHelper::CreateForkableThread(Runnable, *TAsyncThreadName);

						check(RunnableThread != nullptr);
						check(RunnableThread->GetThreadType() == FRunnableThread::ThreadType::Real);

						ThreadPromise.SetValue(RunnableThread);
					}
					else
					{
						Function();
					}
					break;

				case EAsyncExecution::ThreadPool:
					if (FPlatformProcess::SupportsMultithreading())
					{
						check(GThreadPool != nullptr);
						GThreadPool->AddQueuedWork(new TAsyncQueuedWork<int32>(MoveTemp(Function), MoveTemp(Promise)));
					}
					else
					{
						Function();
					}
					break;

#if WITH_EDITOR
				case EAsyncExecution::LargeThreadPool:
					if (FPlatformProcess::SupportsMultithreading())
					{
						check(GLargeThreadPool != nullptr);
						GLargeThreadPool->AddQueuedWork(new TAsyncQueuedWork<int32>(MoveTemp(Function), MoveTemp(Promise)));
					}
					else
					{
						Function();
					}
					break;
#endif

				default:
					check(false); // not implemented!
				}
			}

			ENamedThreads::Type GetDesiredThread()
			{
				//This just schedules the unlock
				return ENamedThreads::AnyThread;
			}

		private:

			TPromiseRef MyPromise;
			TSharedRef<TPromiseState<TResultType>, ESPMode::ThreadSafe> PreviousPromise;

			TRootFunction ContinuationFunction;

			TLifetimeMonitor LifetimeMonitor;

			ENamedThreads::Type DesiredThread;
			EAsyncExecution Execution;
		};
	}

	namespace Private
	{
		template<typename Func, typename ResultType, typename Monitor>
		auto Then(
			Func&& Function,
			const TSharedRef<TPromiseState<ResultType>, ESPMode::ThreadSafe>& PreviousPromise,
			const FOptions& Options,
			Monitor LifetimeMonitor)
		{
			using ContinuationFunctionTraits = TContinuationTypes<Func, ResultType>;
			using TFutureType = TUnwrap_T<typename ContinuationFunctionTraits::ReturnType>;
			using TParamResultType = TUnwrap_T<typename ContinuationFunctionTraits::ParamType>;
			static_assert(std::is_same<ResultType, TParamResultType>::value, "Parameter of the continuation needs to have the same type as the previous return.");
			
			//Create promise
			TSharedRef<TAsyncPromise<TFutureType>> Promise = MakeShared<TAsyncPromise<TFutureType>>();
			TAsyncFuture<TFutureType> Future = Promise->GetFuture();

			//Make Scheduling task on the graphtask
			FGraphEventArray Triggers{ PreviousPromise->GetCompletionEvent() };
			TGraphTask<TContinuationTask<Func, ResultType, TFutureType, Monitor>>::CreateTask(&Triggers).ConstructAndDispatchWhenReady(
				Forward<Func>(Function), 
				MoveTemp(Promise), 
				PreviousPromise,
				MoveTemp(LifetimeMonitor),
				Options);

			//return future
			return Future;
		}
	}
}