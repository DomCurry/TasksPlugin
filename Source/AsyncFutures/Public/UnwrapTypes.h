// Copyright(c) Dominic Curry. All rights reserved.
#pragma once

#include "Result.h"

namespace UE::Tasks
{	
	template<typename T>
	class TAsyncFuture;

	namespace Private
	{
		template<typename T>
		class TIsFutureImpl { public: enum { Value = false }; };
		
		template<typename T>
		class TIsFutureImpl<TAsyncFuture<T>> { public: enum { Value = true }; };
		
		template<typename T>
		using TIsFuture = TIsFutureImpl<std::decay_t<T>>;

		template<typename T>
		class TIsResultImpl { public: enum { Value = false }; };
		
		template<typename T>
		class TIsResultImpl<TResult<T>> { public: enum { Value = true }; };
		
		template<typename T>
		using TIsResult = TIsResultImpl<std::decay_t<T>>;

		template<typename T, typename Enable = void>
		class TUnwrap;

		template<typename T>
		class TUnwrap<T, typename TEnableIf<TIsFuture<T>::Value>::Type>
		{
		public:
			using Type = typename TUnwrap<typename T::TResult>::Type;
		};

		template<typename T>
		class TUnwrap<T, typename TEnableIf<TIsResult<T>::Value>::Type>
		{
		public:
			using Type = typename TUnwrap<typename T::ResultType>::Type;
		};

		template<typename T>
		class TUnwrap<T, typename TEnableIf<!TIsFuture<T>::Value && !TIsResult<T>::Value>::Type>
		{
		public:
			using Type = T;
		};

		template <typename T>
		using TUnwrap_T = typename TUnwrap<T>::Type;
	}
}