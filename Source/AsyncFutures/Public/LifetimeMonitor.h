// Copyright Dominic Curry. All Rights Reserved.
#pragma once

// Engine Includes
#include "Templates/SharedPointer.h"
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
			TWeakObjectRefType(T* Object) { static_assert(std::is_void<Enabled>::value == false, __FUNCTION__ ": Need to use an object that has lifetime management (UObject, TSharedFromThis)"); }
			bool Pin() const { return false; }
		};

		//TSharedFromThis lifetime monitor (Thread Safe)
		template<typename T>
		class TWeakObjectRefType<T, typename TEnableIf<TIsDerivedFrom<T, TSharedFromThis<T, ESPMode::ThreadSafe>>::IsDerived>::Type> : TWeakPtr<T, ESPMode::ThreadSafe>
		{
		public:
			TWeakObjectRefType(T* Object) : TWeakPtr<T, ESPMode::ThreadSafe>(Object ? TWeakPtr<T, ESPMode::ThreadSafe>(Object->AsShared()) : nullptr) {}
			TSharedPtr<T, ESPMode::ThreadSafe> Pin() const { return TWeakPtr<T, ESPMode::ThreadSafe>::Pin(); }
		};

		//TSharedFromThis lifetime monitor (Not Thread Safe)
		template<typename T>
		class TWeakObjectRefType<T, typename TEnableIf<TIsDerivedFrom<T, TSharedFromThis<T, ESPMode::NotThreadSafe>>::IsDerived>::Type> : TWeakPtr<T, ESPMode::NotThreadSafe>
		{
		public:
			TWeakObjectRefType(T* Object) : TWeakPtr<T, ESPMode::NotThreadSafe>(Object ? TWeakPtr<T, ESPMode::NotThreadSafe>(Object->AsShared()) : nullptr) {}
			TSharedPtr<T, ESPMode::NotThreadSafe> Pin() const { return TWeakPtr<T, ESPMode::NotThreadSafe>::Pin(); }
		};

		//TSharedFromThis BaseClass lifetime monitor (Thread Safe)
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
			TStrongObjectPtr<T> Pin() const { return TStrongObjectPtr<T>(TWeakObjectPtr<T>::Get()); }
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
