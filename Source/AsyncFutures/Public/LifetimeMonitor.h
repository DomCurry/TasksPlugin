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
		// Detects whether T has a callable AsShared() member without hard-erroring for types
		// that don't (e.g. plain UObject). TSharedPtrTypes below dispatches on this before ever
		// naming ->AsShared() in a template body, because a class template's own instantiation
		// is not "immediate context" for SFINAE - the compiler will not silently discard a
		// failure inside TSharedPtrTypes<T>'s member typedefs the way it discards one in the
		// signature of the alias/function that names TSharedPtrTypes<T>::PtrType. Route any
		// change here through the same two-step (detect, then dispatch) shape or the
		// TSharedFromThis partial specializations below will hard-error for UObject types again.
		template <typename T, typename = void>
		struct THasAsShared : std::false_type {};

		template <typename T>
		struct THasAsShared<T, decltype(void(std::declval<std::remove_pointer_t<T>&>().AsShared()))> : std::true_type {};

		template <typename T, bool = THasAsShared<T>::value>
		struct TSharedPtrTypes
		{
		};

		template <typename T>
		struct TSharedPtrTypes<T, true>
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
