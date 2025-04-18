// Copyright(c) Dominic Curry. All rights reserved.
#pragma once

// Engine Includes
#include "Async/Async.h"
#include "Tasks/Task.h"
#include "Templates/SharedPointer.h"

// Module Includes
#include "Error.h"
#include "LifetimeMonitor.h"
#include "Result.h"
#include "PromiseState.h"
#include "Error.h"

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
		void SetValue(const Error& Result) const { State->SetValue(Result); }
		void SetValue(Error&& Result) const { State->SetValue(Result); }
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
		void SetValue(const Error& Result) const { State->SetValue(TResult<void>(Result)); }
		void SetValue(Error&& Result) const { State->SetValue(TResult<void>(Result)); }
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
		FOptions(TOptional<FCancellationHandle>&& OptionalHandle, TOptional<ENamedThreads::Type>&& OptionalThread)
			: Thread(MoveTemp(OptionalThread))
			, CancellationHandle(MoveTemp(OptionalHandle))
		{
		}

		FOptions()
			: FOptions(
				TOptional<FCancellationHandle>(),
				TOptional<ENamedThreads::Type>()) 
		{
		}

		FOptions(TOptional<FCancellationHandle>&& OptionalHandle)
			: FOptions(
				MoveTemp(OptionalHandle),
				TOptional<ENamedThreads::Type>())
		{
		}

		FOptions(TOptional<ENamedThreads::Type>&& OptionalThread)
			: FOptions(
				TOptional<FCancellationHandle>(),
				MoveTemp(OptionalThread))
		{
		}

		
		TOptional<FCancellationHandle> GetCancellation() const { return CancellationHandle; }
		ENamedThreads::Type GetDesiredThread() const
		{
			return Thread.Get(ENamedThreads::AnyThread);
		}

	private:
		TOptional<ENamedThreads::Type> Thread;
		TOptional<FCancellationHandle> CancellationHandle;
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
		template<typename TFunctionType, typename TResultType, typename TPromise, typename TLifetimeMonitor>
		class TContinuationTask : public FAsyncGraphTaskBase
		{
			using TPromiseRef = TSharedRef<TAsyncPromise<TPromise>, ESPMode::ThreadSafe>;
			using TRootFunction = typename std::remove_cv_t<typename TRemoveReference<TFunctionType>::Type>;

		public:
			TContinuationTask(TFunctionType&& InFunction, 
				TPromiseRef&& InPromise,
				const TSharedRef<TPromiseState<TResultType>, ESPMode::ThreadSafe>& InPreviousPromise,
				TLifetimeMonitor&& InLifetimeMonitor,
				const FOptions& Options)
				: Promise(MoveTemp(InPromise))
				, PreviousPromise(InPreviousPromise)
				, ContinuationFunction(Forward<TFunctionType>(InFunction))
				, LifetimeMonitor(MoveTemp(InLifetimeMonitor))
				, DesiredThread(Options.GetDesiredThread())
			{
				const TOptional<FCancellationHandle>& Cancellation = Options.GetCancellation();
				if (Cancellation.IsSet())
				{
					FCancellationHandle Handle = Cancellation.GetValue();
					Handle.Bind(*Promise);
				}
			}

			void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
			{
				if (!Promise->IsSet())
				{
					if (auto PinnedObject = LifetimeMonitor.Pin())
					{
						check(PreviousPromise->IsSet());
						ExecuteContinuation(*Promise, PreviousPromise->Get(), MoveTemp(ContinuationFunction));
					}
					else
					{
						Promise->SetValue(Error(ERROR_CONTEXT_FUTURE, ERROR_LIFETIME, TEXT("Owner lifetime expired")));
					}
				}
			}

			ENamedThreads::Type GetDesiredThread()
			{
				return DesiredThread;
			}

		private:

			TPromiseRef Promise;
			TSharedRef<TPromiseState<TResultType>, ESPMode::ThreadSafe> PreviousPromise;

			TRootFunction ContinuationFunction;

			TLifetimeMonitor LifetimeMonitor;

			ENamedThreads::Type DesiredThread;
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