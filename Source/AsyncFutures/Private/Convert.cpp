// Copyright(c) Dominic Curry. All rights reserved.
#include "Convert.h"
// Module Includes
#include "Error.h"
#include "FunctionTypes.h"
#include "Result.h"
#include "UnwrapTypes.h"

namespace UE::Tasks
{
	template<typename TValueType, typename Func, typename TEnableIf<!std::is_void<Private::TUnwrap_T<typename Private::TContinuationTypes<Func, TValueType>::ReturnType>>::value && !std::is_void<TValueType>::value>::Type>
	auto Convert(const TResult<TValueType>& Result, Func&& Conversion)
	{
		using ResultType = Private::TUnwrap_T<typename Private::TContinuationTypes<Func, TValueType>::ReturnType>;

		if (Result.HasValue())
		{
			return TResult<ResultType>(Conversion(Result.GetValue()));
		}

		if (Result.HasError())
		{
			return TResult<ResultType>(Result.GetError());
		}

		checkf(false, TEXT("Should only be called from complete results"));
		return TResult<ResultType>();
	}

	//void specializations
	template<typename Func, typename TEnableIf<!std::is_void<Private::TUnwrap_T<typename Private::TContinuationTypes<Func, void>::ReturnType>>::value>::Type>
	auto Convert(const TResult<void>& Result, Func&& Conversion)
	{
		using ResultType = Private::TUnwrap_T<typename Private::TContinuationTypes<Func, void>::ReturnType>;

		if (Result.HasValue())
		{
			return TResult<ResultType>(Conversion());
		}

		if (Result.HasError())
		{
			return TResult<ResultType>(Result.GetError());
		}

		checkf(false, TEXT("Should only be called from complete results"));
		return TResult<ResultType>();
	}

	template<typename TValueType, typename Func, typename TEnableIf<std::is_void<Private::TUnwrap_T<typename Private::TContinuationTypes<Func, TValueType>::ReturnType>>::value && !std::is_void<TValueType>::value>::Type>
	TResult<void> Convert(const TResult<TValueType>& Result, Func&& Conversion)
	{
		if (Result.HasValue())
		{
			Conversion(Result.GetValue());
			return TResult<void>();
		}

		if (Result.HasError())
		{
			return TResult<void>(Result.GetError());
		}

		checkf(false, TEXT("Should only be called from complete results"));
		return TResult<void>();
	}

	template<typename Func, typename TEnableIf<std::is_void<typename Private::TUnwrap<typename Private::TContinuationTypes<Func, void>::ReturnType>::Type>::value>::Type>
	TResult<void> Convert(const TResult<void>& Result, Func&& Conversion)
	{
		if (Result.HasValue())
		{
			Conversion();
			return TResult<void>();
		}

		if (Result.HasError())
		{
			return TResult<void>(Result.GetError());
		}

		checkf(false, TEXT("Should only be called from complete results"));
		return TResult<void>();
	}
}
