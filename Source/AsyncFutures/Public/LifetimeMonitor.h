// Copyright Dominic Curry. All Rights Reserved.
#pragma once

// Engine Includes
#include "Templates/SharedPointer.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Object.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

namespace UE
{
	namespace Private
	{
		template <typename T>
		struct TSharedPtrTypes
		{
			using Type = std::decay_t<decltype(((std::remove_pointer_t<T>*)nullptr)->AsShared())>;
			using PtrType = typename Type::ElementType;
		};

		//forbidden base lifetime monitor, assert on construction
		template<typename T, typename Enabled = void>
		class TWeakObjectRefType
		{
		public:
			TWeakObjectRefType(T* Object) { static_assert(std::is_void<Enabled>::value == false, "TLifetimeMonitor: owner must have lifetime management (UObject or TSharedFromThis)"); }
			bool Pin() const { return false; }
		};

		//TSharedFromThis lifetime monitor (Thread Safe). Routes through TSharedPtrTypes so that both
		//direct derivation (class FThing : TSharedFromThis<FThing>) and derivation via a base class
		//(class FThing : FBase, where FBase : TSharedFromThis<FBase>) are handled by a single
		//specialization - PtrType resolves to T itself in the direct case. Do not reintroduce a
		//separate direct-derivation specialization: it would be identical to this one after
		//substitution (PtrType == T), making the two partial specializations ambiguous.
		template<typename T>
		class TWeakObjectRefType<T, typename TEnableIf<TIsDerivedFrom<T, TSharedFromThis<typename TSharedPtrTypes<T>::PtrType, ESPMode::ThreadSafe>>::IsDerived>::Type> : TWeakPtr<typename TSharedPtrTypes<T>::PtrType, ESPMode::ThreadSafe>
		{
		public:
			TWeakObjectRefType(T* Object) : TWeakPtr<typename TSharedPtrTypes<T>::PtrType, ESPMode::ThreadSafe>(Object ? TWeakPtr<typename TSharedPtrTypes<T>::PtrType, ESPMode::ThreadSafe>(Object->AsShared()) : nullptr) {}
			TSharedPtr<typename TSharedPtrTypes<T>::PtrType, ESPMode::ThreadSafe> Pin() const { return TWeakPtr<typename TSharedPtrTypes<T>::PtrType, ESPMode::ThreadSafe>::Pin(); }
		};

		//TSharedFromThis BaseClass lifetime monitor (Not Thread Safe)
		template<typename T>
		class TWeakObjectRefType<T, typename TEnableIf<TIsDerivedFrom<T, TSharedFromThis<typename TSharedPtrTypes<T>::PtrType, ESPMode::NotThreadSafe>>::IsDerived>::Type> : TWeakPtr<typename TSharedPtrTypes<T>::PtrType, ESPMode::NotThreadSafe>
		{
		public:
			TWeakObjectRefType(T* Object) : TWeakPtr<typename TSharedPtrTypes<T>::PtrType, ESPMode::NotThreadSafe>(Object ? TWeakPtr<typename TSharedPtrTypes<T>::PtrType, ESPMode::NotThreadSafe>(Object->AsShared()) : nullptr) {}
			TSharedPtr<typename TSharedPtrTypes<T>::PtrType, ESPMode::NotThreadSafe> Pin() const { return TWeakPtr<typename TSharedPtrTypes<T>::PtrType, ESPMode::NotThreadSafe>::Pin(); }
		};

		//UObject* lifetime monitor
		template<typename T>
		class TWeakObjectRefType<T, typename TEnableIf<TIsDerivedFrom<T, UObject>::IsDerived>::Type> : TWeakObjectPtr<T>
		{
		public:
			TWeakObjectRefType(T* Object) : TWeakObjectPtr<T>(Object) {}

			//Pin() runs inside the continuation, which may be scheduled on any worker thread (see
			//FOptions::GetDesiredThread) - not just the game thread. Reading a TWeakObjectPtr and
			//promoting it to a TStrongObjectPtr touches GC reference-tracking state that is not safe
			//to mutate concurrently with garbage collection. FGCScopeGuard blocks GC for the duration
			//of the promotion so this is safe from any thread; only the promotion itself is guarded,
			//not the continuation body, so user code does not run under the GC lock.
			TStrongObjectPtr<T> Pin() const
			{
				FGCScopeGuard ScopeGuard;
				return TStrongObjectPtr<T>(TWeakObjectPtr<T>::Get());
			}
		};
	}

	template<typename T>
	class TLifetimeMonitor
	{
	public:
		TLifetimeMonitor(T* Object) : WeakRef(Object) {}
		auto Pin() const { return WeakRef.Pin(); }
	private:
		Private::TWeakObjectRefType<T> WeakRef;
	};
	
	template<>
	class TLifetimeMonitor<void>
	{
	public:
		constexpr bool Pin() const { return true; }
	};
}
