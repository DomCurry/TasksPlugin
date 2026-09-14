// Copyright Dominic Curry. All Rights Reserved.
#pragma once

#include <atomic>

// Engine Includes
#include "Async/Async.h"
#include "CoreTypes.h"
#include "HAL/CriticalSection.h"
#include "Tasks/Task.h"
#include "Templates/SharedPointer.h"
#include "Misc/AssertionMacros.h"
#include "Misc/IQueuedWork.h"
#include "Misc/QueuedThreadPool.h"
#include "Misc/ScopeLock.h"

// Module Includes
#include "Error.h"
#include "LifetimeMonitor.h"
#include "Result.h"
#include "PromiseState.h"

namespace UE::Tasks
{
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
		using ValueType = ResultType;

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
			Promise = MoveTemp(Other);
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
			check(IsValid());
			return Private::Then<Func, ResultType>(Forward<Func>(Function), Promise.ToSharedRef(), Options, TLifetimeMonitor<void>());
		}

		template<typename Func, typename TOwner>
		auto Then(TOwner* Owner, Func&& Function, const FOptions& Options = FOptions()) const
		{
			check(IsValid());
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
		using ValueType = void;

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
			Promise = MoveTemp(Other);
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
			return Private::Then<Func, void>(Forward<Func>(Function), Promise.ToSharedRef(), Options, TLifetimeMonitor<void>());
		}

		template<typename Func, typename TOwner>
		auto Then(TOwner* Owner, Func&& Function, const FOptions& Options = FOptions()) const
		{
			check(IsValid());
			return Private::Then<Func, void>(Forward<Func>(Function), Promise.ToSharedRef(), Options, TLifetimeMonitor<TOwner>(Owner));
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
		bool IsSet() const { return State->IsClaimed(); }
		TResult<T> Get() const { return State->Get(); }

		//fulfilling promise
		void SetValue(const TResult<T>& Result) const { State->SetValue(Result); }
		void SetValue(TResult<T>&& Result) const { State->SetValue(MoveTemp(Result)); }
		void SetValue(const T& Result) const { State->SetValue(Result); }
		void SetValue(T&& Result) const { State->SetValue(Result); }
		void SetValue(const FError& Result) const { State->SetValue(Result); }
		void SetValue(FError&& Result) const { State->SetValue(Result); }
		void Cancel() const { SetValue(MakeCancelledError()); }

	private:
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
		bool IsSet() const { return State->IsClaimed(); }
		TResult<void> Get() const { return State->Get(); }

		//fulfilling promise
		void SetValue(const TResult<void>& Result) const { State->SetValue(Result); }
		void SetValue(TResult<void>&& Result) const { State->SetValue(MoveTemp(Result)); }
		void SetValue() const { State->SetValue(TResult<void>()); }
		void SetValue(const FError& Result) const { State->SetValue(TResult<void>(Result)); }
		void SetValue(FError&& Result) const { State->SetValue(TResult<void>(Result)); }
		void Cancel() const { SetValue(MakeCancelledError()); }

	private:
		TSharedRef<Private::TPromiseState<void>, ESPMode::ThreadSafe> State;
	};

	namespace Private
	{
		class ICancellable
		{
		public:
			virtual ~ICancellable() {}

			// Run the bound work with a cancelled input, unless some other path already claimed it.
			// bAllowInline is a ceiling, not a floor: false forbids running user code on the calling
			// thread, true only permits it if the work's own options allow.
			virtual void RunCancelled(bool bAllowInline) = 0;

			// "Cancellation can no longer reach this" - not the same question as "is the promise set".
			virtual bool IsResolved() const = 0;
		};

		template<typename TPromiseType>
		class TBoundPromise : public ICancellable
		{
		public:
			TBoundPromise(const TAsyncPromise<TPromiseType>& PromiseIn) : Promise(PromiseIn) {  }
			TBoundPromise(TAsyncPromise<TPromiseType>&& PromiseIn) : Promise(MoveTemp(PromiseIn)) {  }

			// A bare promise has no continuation to run, so cancelling it is just resolving it.
			virtual void RunCancelled(bool /*bAllowInline*/) override { Promise.Cancel(); }
			virtual bool IsResolved() const override { return Promise.IsSet(); }
		private:
			TAsyncPromise<TPromiseType> Promise;
		};

		class FCancellationState
		{
		public:
			FCancellationState() : Cancelled(false) {}
			// Destroying the last FCancellationHandle/FWeakCancellationHandle referencing this
			// state cancels everything ever bound to it. This is how an object owns its tasks:
			// hold the handle as a member (not a local!) and every task started with it is
			// cancelled when the object is collected.
			//
			// Teardown dispatches rather than running inline: the object that owned this handle is
			// usually mid-destruction, and a continuation re-entering it would run against
			// half-destroyed state.
			~FCancellationState() { Cancel(/*bAllowInline*/ false); }

			void Cancel() { Cancel(/*bAllowInline*/ true); }

			void Bind(const TSharedRef<ICancellable, ESPMode::ThreadSafe>& Cancellable)
			{
				{
					FScopeLock Lock(&CriticalSection);
					if (!Cancelled)
					{
						// Entries are otherwise append-only for the handle's whole life, so a
						// long-lived handle tagging many short-lived tasks retains every one of them.
						// Sweep here, under the lock Bind/Cancel already share - IsResolved() reads
						// only an atomic, so it cannot re-enter us. Gated on a doubling threshold
						// because sweeping on every Bind would be O(N) per call, i.e. O(N^2) for a
						// handle with N genuinely outstanding tasks - a worse trap than the leak.
						if (Cancellables.Num() >= SweepThreshold)
						{
							Cancellables.RemoveAllSwap(
								[](const TSharedRef<ICancellable, ESPMode::ThreadSafe>& Entry)
								{
									return Entry->IsResolved();
								}, EAllowShrinking::No);

							SweepThreshold = FMath::Max(MinSweepThreshold, Cancellables.Num() * 2);
						}

						Cancellables.Emplace(Cancellable);
						return;
					}
				}

				// Outside the lock - this now runs user code, not just a SetValue, and that code is
				// free to bind to or cancel this same handle.
				Cancellable->RunCancelled(/*bAllowInline*/ true);
			}

			template<typename TPromiseType>
			void Bind(const TAsyncPromise<TPromiseType>& PromiseIn)
			{
				const TSharedRef<ICancellable, ESPMode::ThreadSafe> Cancellable =
					MakeShared<TBoundPromise<TPromiseType>, ESPMode::ThreadSafe>(PromiseIn);
				Bind(Cancellable);
			}

			// Diagnostics: how many bound entries are still tracked. Exposed so the sweep above can
			// be tested without reaching into the array itself.
			int32 GetTrackedCount() const
			{
				FScopeLock Lock(&CriticalSection);
				return Cancellables.Num();
			}

		private:
			void Cancel(bool bAllowInline)
			{
				TArray<TSharedRef<ICancellable, ESPMode::ThreadSafe>> ToCancel;
				{
					FScopeLock Lock(&CriticalSection);
					Cancelled = true;
					ToCancel = MoveTemp(Cancellables);
					Cancellables.Empty();
					SweepThreshold = MinSweepThreshold;
				}

				for (const TSharedRef<ICancellable, ESPMode::ThreadSafe>& Entry : ToCancel)
				{
					Entry->RunCancelled(bAllowInline);
				}
			}

			static constexpr int32 MinSweepThreshold = 8;

			mutable FCriticalSection CriticalSection;
			bool Cancelled = false;
			int32 SweepThreshold = MinSweepThreshold;
			TArray<TSharedRef<ICancellable, ESPMode::ThreadSafe>> Cancellables;
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

		void Bind(const TSharedRef<Private::ICancellable, ESPMode::ThreadSafe>& Cancellable) { State->Bind(Cancellable); }
		// Diagnostics only - see FCancellationState::GetTrackedCount.
		int32 GetTrackedCount() const { return State->GetTrackedCount(); }
		template<typename TPromiseType>
		void Bind(const TAsyncPromise<TPromiseType>& PromiseIn) { State->Bind(PromiseIn); }
		void Cancel() { State->Cancel(); }
	private:
		TSharedRef<Private::FCancellationState, ESPMode::ThreadSafe> State;
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

		FOptions& Set(const ENamedThreads::Type ThreadIn) { Thread = ThreadIn; return *this; }
		FOptions& Set(const FCancellationHandle& HandleIn) { CancellationHandle = HandleIn; return *this; }
		FOptions& Set(const EAsyncExecution ExecutionIn)
		{
			Execution = ExecutionIn;
			if (ExecutionIn == EAsyncExecution::TaskGraphMainThread)
			{
				// TaskGraphMainThread means "TaskGraph, pinned to the game thread" - resolve the
				// thread here so GetDesiredThread() reports the truth instead of DoTask silently
				// overriding whatever thread was requested at dispatch time.
				Thread = ENamedThreads::GameThread;
			}
			return *this;
		}

		// Intent-revealing setters that keep Execution and Thread consistent, so the combination
		// cannot be got wrong the way the raw Set(...) overloads allow.
		FOptions& OnTaskGraph(ENamedThreads::Type ThreadIn = ENamedThreads::AnyThread) { Execution = EAsyncExecution::TaskGraph; Thread = ThreadIn; return *this; }
		FOptions& OnGameThread() { return OnTaskGraph(ENamedThreads::GameThread); }
		FOptions& OnDedicatedThread() { Execution = EAsyncExecution::Thread; return *this; }
		FOptions& OnThreadPool() { Execution = EAsyncExecution::ThreadPool; return *this; }

		// "Start this work - don't run it on my thread." Distinct from naming a thread: the caller
		// doesn't care which thread, only that it isn't this one, right now. Async() sets this, which
		// is what keeps it asynchronous even though its upstream (MakeReadyFuture) is always already
		// resolved and would otherwise qualify for inline execution.
		FOptions& RequireAsync() { bRequireAsync = true; return *this; }
		bool RequiresAsync() const { return bRequireAsync; }

		TOptional<FCancellationHandle> GetCancellation() const { return CancellationHandle; }
		ENamedThreads::Type GetDesiredThread() const {	return Thread.Get(ENamedThreads::AnyThread); }
		EAsyncExecution GetExecutionPolicy() const {	return Execution.Get(EAsyncExecution::TaskGraph); }
		bool HasExplicitThread() const {
			return Thread.IsSet() && ENamedThreads::GetThreadIndex(Thread.GetValue()) != ENamedThreads::AnyThread;
		}

		// True when a thread was named at all, including ENamedThreads::AnyThread and priority-only
		// values like AnyBackgroundThreadNormalTask whose thread index masks back to AnyThread.
		// Deliberately weaker than HasExplicitThread(): inline execution keys off this one, because
		// naming any thread - even a priority class - is a scheduling preference that running on the
		// calling thread would quietly violate.
		bool HasThreadPreference() const { return Thread.IsSet(); }

	private:
		TOptional<ENamedThreads::Type> Thread;
		TOptional<FCancellationHandle> CancellationHandle;
		TOptional<EAsyncExecution> Execution;
		bool bRequireAsync = false;
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
		// Inline continuations recurse: a continuation running on a ready future can call .Then()
		// again, directly or through the IsXxxToFuture overloads of ExecuteContinuation, which each
		// chain a .Then() onto whatever future the user returned. A self-chaining continuation over
		// ready futures would otherwise recurse until the stack ran out. Past the cap we fall back to
		// the task graph, which costs latency rather than correctness.
		//
		// A per-module copy of the counter is fine - the cap only has to be *a* bound, not a globally
		// shared one - so this deliberately does not reach for anything with stricter linkage.
		inline constexpr int32 MaxInlineContinuationDepth = 16;

		inline int32& GetInlineContinuationDepth()
		{
			static thread_local int32 Depth = 0;
			return Depth;
		}

		class FInlineContinuationScope
		{
		public:
			static bool CanEnter() { return GetInlineContinuationDepth() < MaxInlineContinuationDepth; }

			FInlineContinuationScope() { ++GetInlineContinuationDepth(); }
			~FInlineContinuationScope() { --GetInlineContinuationDepth(); }

			FInlineContinuationScope(const FInlineContinuationScope&) = delete;
			FInlineContinuationScope& operator=(const FInlineContinuationScope&) = delete;
		};

		// The continuation's payload, owned independently of the graph task that waits on the upstream
		// promise, so that cancellation can reach it directly: a cancelled continuation runs straight
		// away rather than waiting for - or ever reading - an upstream that may never resolve.
		//
		// Exactly one of RunFromUpstream()/RunCancelled() ever executes, decided by ExecutionClaimed.
		// The winner moves the payload out and is the only thing that completes the promise, which is
		// what keeps downstream continuations ordered behind this one.
		template<typename TFunctionType, typename TResultType, typename TPromiseType, typename TLifetimeMonitor>
		class TContinuationState : public ICancellable
		{
			using TPromiseRef = TSharedRef<TAsyncPromise<TPromiseType>, ESPMode::ThreadSafe>;
			using TPreviousRef = TSharedRef<TPromiseState<TResultType>, ESPMode::ThreadSafe>;
			using TRootFunction = typename std::remove_cv_t<typename TRemoveReference<TFunctionType>::Type>;

		public:
			TContinuationState(TFunctionType&& InFunction,
				TPromiseRef&& InPromise,
				const TPreviousRef& InPreviousPromise,
				TLifetimeMonitor&& InLifetimeMonitor,
				const FOptions& Options)
				: MyPromise(MoveTemp(InPromise))
				, PreviousPromise(InPreviousPromise)
				, ContinuationFunction(Forward<TFunctionType>(InFunction))
				, LifetimeMonitor(MoveTemp(InLifetimeMonitor))
				, DesiredThread(Options.GetDesiredThread())
				, Execution(Options.GetExecutionPolicy())
				// TaskGraph with no thread named at all is the only combination where "the thread
				// that called us" is indistinguishable from "the thread we were asked for".
				// HasThreadPreference(), not HasExplicitThread(): the latter masks the priority bits
				// off, so OnTaskGraph(AnyBackgroundThreadNormalTask) would look unpinned and get
				// inlined onto the calling thread, which is exactly what it asked us not to do.
				, bInlineEligible(Options.GetExecutionPolicy() == EAsyncExecution::TaskGraph
					&& !Options.HasThreadPreference()
					&& !Options.RequiresAsync())
			{
				ensureMsgf(!Options.HasExplicitThread() || Execution == EAsyncExecution::TaskGraph || Execution == EAsyncExecution::TaskGraphMainThread,
					TEXT("FOptions: an explicit thread is only honoured by EAsyncExecution::TaskGraph; ")
					TEXT("the requested thread will be ignored for this execution policy."));

				if (!IsSupportedExecutionPolicy(Execution))
				{
					// Claim execution here rather than carrying a flag both run paths would have to
					// remember to test: there is nothing to dispatch and nothing to cancel, so any
					// later RunFromUpstream()/RunCancelled() simply loses the CAS.
					ExecutionClaimed.store(true, std::memory_order_release);
					MyPromise->SetValue(FError(ERROR_CONTEXT_FUTURE, ERROR_UNSUPPORTED_EXECUTION,
						TEXT("Unsupported execution policy for this build configuration")));
				}
			}

			bool IsExecuted() const { return ExecutionClaimed.load(std::memory_order_acquire); }

			// Deliberately the atomic rather than MyPromise->IsSet(): MyPromise is moved-from once
			// execution has been claimed, so it is not safe to dereference here.
			virtual bool IsResolved() const override { return IsExecuted(); }

			void RunFromUpstream()
			{
				if (!TryClaimExecution())
				{
					return;
				}

				// Only valid on this path - the cancel path may run with the upstream unresolved.
				check(PreviousPromise->IsSet());
				ExecuteWith(PreviousPromise->Get(), /*bAllowInline*/ true);
			}

			// Note the input: a cancelled continuation is handed a cancelled TResult rather than the
			// upstream's. That is what lets a TResult-taking continuation observe the cancellation,
			// and what makes the value-taking overloads propagate the error without running the body.
			virtual void RunCancelled(bool bAllowInline) override
			{
				if (!TryClaimExecution())
				{
					return;
				}

				ExecuteWith(TResult<TResultType>(MakeCancelledError()), bAllowInline);
			}

		private:
			bool TryClaimExecution()
			{
				bool bExpected = false;
				return ExecutionClaimed.compare_exchange_strong(bExpected, true, std::memory_order_acq_rel);
			}

			void ExecuteWith(TResult<TResultType>&& Input, bool bAllowInline)
			{
				// Only the CAS winner reaches here, so moving the payload out of the still-shared
				// state is safe, and it releases the user functor and the upstream promise as soon as
				// the work is handed off. Nothing may read these members afterwards - hence
				// IsResolved() reading only the atomic.
				auto Function = [
					InPromise = MoveTemp(MyPromise),
					InPreviousPromise = MoveTemp(PreviousPromise),
					InContinuationFunction = MoveTemp(ContinuationFunction),
					InLifetimeMonitor = MoveTemp(LifetimeMonitor),
					InInput = MoveTemp(Input)
				]() mutable -> int32
					{
						if (auto PinnedObject = InLifetimeMonitor.Pin())
						{
							ExecuteContinuation(*InPromise, InInput, MoveTemp(InContinuationFunction));
						}
						else if (!InPromise->IsSet())
						{
							InPromise->SetValue(FError(ERROR_CONTEXT_FUTURE, ERROR_LIFETIME, TEXT("Owner lifetime expired")));
						}

						return 0;
					};

				// Dispatching once the graph has gone would strand the promise, and an unfulfilled
				// promise trips ~TPromiseState's check(IsClaimed()) - so during shutdown running here
				// is the only option left, whatever the options asked for.
				const bool bGraphUnavailable = !FTaskGraphInterface::IsRunning();
				if (bGraphUnavailable || (bAllowInline && bInlineEligible && FInlineContinuationScope::CanEnter()))
				{
					FInlineContinuationScope Scope;
					Function();
					return;
				}

				TPromise<int32> Promise = TPromise<int32>();
				//Copied from Async.h to allow us to pass the thread to the task graph
				switch (Execution)
				{
				case EAsyncExecution::TaskGraphMainThread:
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
						// ~TPromise asserts the promise was fulfilled, so the non-threaded
						// fallbacks have to set it rather than just running the work.
						Promise.SetValue(Function());
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
						// ~TPromise asserts the promise was fulfilled, so the non-threaded
						// fallbacks have to set it rather than just running the work.
						Promise.SetValue(Function());
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
						// ~TPromise asserts the promise was fulfilled, so the non-threaded
						// fallbacks have to set it rather than just running the work.
						Promise.SetValue(Function());
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
						// ~TPromise asserts the promise was fulfilled, so the non-threaded
						// fallbacks have to set it rather than just running the work.
						Promise.SetValue(Function());
					}
					break;
#endif

				default:
					// Unreachable: the constructor already fails the promise for any policy
					// IsSupportedExecutionPolicy() doesn't recognise, and DoTask returns before
					// reaching here in that case.
					checkf(false, TEXT("Unhandled EAsyncExecution policy %d"), static_cast<int32>(Execution));
				}
			}

			static bool IsSupportedExecutionPolicy(EAsyncExecution InExecution)
			{
				switch (InExecution)
				{
				case EAsyncExecution::TaskGraphMainThread:
				case EAsyncExecution::TaskGraph:
				case EAsyncExecution::Thread:
				case EAsyncExecution::ThreadIfForkSafe:
				case EAsyncExecution::ThreadPool:
#if WITH_EDITOR
				case EAsyncExecution::LargeThreadPool:
#endif
					return true;
				default:
					return false;
				}
			}

			TPromiseRef MyPromise;
			TPreviousRef PreviousPromise;

			TRootFunction ContinuationFunction;

			TLifetimeMonitor LifetimeMonitor;

			ENamedThreads::Type DesiredThread;
			EAsyncExecution Execution;
			bool bInlineEligible;
			std::atomic_bool ExecutionClaimed{ false };
		};

		// Waits on the upstream promise's completion event and hands off. Everything else - the
		// payload, the policy, the run-once decision - lives in TContinuationState, which cancellation
		// reaches without this task ever having to fire.
		template<typename TStateType>
		class TContinuationTask : public FAsyncGraphTaskBase
		{
		public:
			TContinuationTask(const TSharedRef<TStateType, ESPMode::ThreadSafe>& InState)
				: State(InState)
			{
			}

			void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
			{
				State->RunFromUpstream();
			}

			ENamedThreads::Type GetDesiredThread()
			{
				//This just schedules the unlock
				return ENamedThreads::AnyThread;
			}

		private:
			TSharedRef<TStateType, ESPMode::ThreadSafe> State;
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
			
			using TStateType = TContinuationState<Func, ResultType, TFutureType, Monitor>;

			//Create promise. Grab the future before anything else, because from the bind below
			//onwards the continuation may run and complete the promise synchronously.
			TSharedRef<TAsyncPromise<TFutureType>> Promise = MakeShared<TAsyncPromise<TFutureType>>();
			TAsyncFuture<TFutureType> Future = Promise->GetFuture();

			TSharedRef<TStateType, ESPMode::ThreadSafe> State = MakeShared<TStateType, ESPMode::ThreadSafe>(
				Forward<Func>(Function),
				MoveTemp(Promise),
				PreviousPromise,
				MoveTemp(LifetimeMonitor),
				Options);

			//Bind before scheduling. Bound afterwards, a task that won the race would run the body
			//with the upstream's live value instead of a cancelled one. If the handle is already
			//cancelled, Bind runs the cancelled continuation before it returns.
			if (!State->IsExecuted())
			{
				const TOptional<FCancellationHandle>& Cancellation = Options.GetCancellation();
				if (Cancellation.IsSet())
				{
					FCancellationHandle Handle = Cancellation.GetValue();
					Handle.Bind(State);
				}
			}

			if (!State->IsExecuted())
			{
				if (PreviousPromise->IsSet())
				{
					//Nothing to wait for, so skip the scheduling task entirely. Whether the work
					//actually runs here or gets dispatched is ExecuteWith's call, not ours.
					State->RunFromUpstream();
				}
				else
				{
					//Make Scheduling task on the graphtask
					FGraphEventArray Triggers{ PreviousPromise->GetCompletionEvent() };
					TGraphTask<TContinuationTask<TStateType>>::CreateTask(&Triggers).ConstructAndDispatchWhenReady(State);
				}
			}

			//return future
			return Future;
		}
	}
}