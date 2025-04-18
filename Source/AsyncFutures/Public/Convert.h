// Copyright(c) Dominic Curry. All rights reserved.
#pragma once

// Module Includes
#include "Error.h"
#include "FunctionTypes.h"
#include "Result.h"
#include "UnwrapTypes.h"

namespace UE::Tasks
{
	template<typename TValueType, typename Func, typename TEnableIf<!std::is_void<Private::TUnwrap_T<typename Private::TContinuationTypes<Func, TValueType>::ReturnType>>::value && !std::is_void<TValueType>::value>::Type>
	extern auto Convert(const TResult<TValueType>& Result, Func&& Conversion);

	//void specializations
	template<typename Func, typename TEnableIf<!std::is_void<Private::TUnwrap_T<typename Private::TContinuationTypes<Func, void>::ReturnType>>::value>::Type>
	extern auto Convert(const TResult<void>& Result, Func&& Conversion);

	template<typename TValueType, typename Func, typename TEnableIf<std::is_void<Private::TUnwrap_T<typename Private::TContinuationTypes<Func, TValueType>::ReturnType>>::value && !std::is_void<TValueType>::value>::Type>
	extern TResult<void> Convert(const TResult<TValueType>& Result, Func&& Conversion);

	template<typename Func, typename TEnableIf<std::is_void<typename Private::TUnwrap<typename Private::TContinuationTypes<Func, void>::ReturnType>::Type>::value>::Type>
	extern TResult<void> Convert(const TResult<void>& Result, Func&& Conversion);
}
