// Copyright Dominic Curry. All Rights Reserved.
#pragma once

//Engine Includes
#include "Templates/IntegralConstant.h"
#include "Traits/IsVoidType.h"

//Module Includes
#include "UnwrapTypes.h"
#include "Result.h"

namespace UE::Tasks
{
	template <class ResultType>
	class TAsyncFuture;

	namespace Private
	{
		struct ForbiddenFunc {};

		template <typename F>
		auto ReturnTypeHelper(F Func, int, ...) -> decltype(Func());

		template <typename F>
		auto ReturnTypeHelper(F Func, ...) -> ForbiddenFunc;

		//Aliases for supported specializations for functor return values (both initialization and continuation)
		template<typename ReturnType>
		struct TUnitTypeTraits
		{
			using IsVoid			= std::is_void<ReturnType>;
			using IsRealValue		= TIntegralConstant<bool, !std::is_void<ReturnType>::value && !TIsResult<ReturnType>::Value && !TIsFuture<ReturnType>::Value>;
			using IsFuture			= TIsFuture<ReturnType>;
			using IsResult			= TIsResult<ReturnType>;
		};

		//Aliases for supported specializations of initialization functors
		template<typename R>
		class TInitFunctionTraits
		{
		public:
			using ReturnsNonVoid = TEnableIf<TUnitTypeTraits<R>::IsRealValue || TUnitTypeTraits<R>::IsResult>;
			using ReturnsVoid = TEnableIf<TUnitTypeTraits<R>::IsVoid>;
		};

		template <typename F>
		struct TInitFunctionTypes
		{
			using ReturnType = decltype(ReturnTypeHelper(DeclVal<F>(), 0));
			static_assert(!std::is_same<ReturnType, ForbiddenFunc>::value, "Initial function cannot accept parameters.");
		
			using Traits = TInitFunctionTraits<ReturnType>;
		};

		template<typename P>
		TResult<P> ToResult(P p);

		TResult<void> ToResult();

		void ToVoid();

		//Continuations can either take a parameter of TResult<P>
		template <typename F, typename P>
		auto ContinuationType(F Func, P PrevType, int, int, ...) -> decltype(Func(ToResult(PrevType)), ToResult(PrevType));

		//Or a parameter type of P
		template <typename F, typename P>
		auto ContinuationType(F Func, P PrevType, int, ...) -> decltype(Func(PrevType), PrevType);

		//And no other type of parameter
		template <typename F, typename P>
		auto ContinuationType(F Func, P PrevType, ...)->ForbiddenFunc;

		//Continuations where the previous type was void can either take a parameter of TResult<void>
		template <typename F>
		auto ContinuationType_Void(F Func, int, int, ...) -> decltype(Func(ToResult()), ToResult());

		//Or a specifically void parameter type
		template <typename F>
		auto ContinuationType_Void(F Func, int, ...) -> decltype(Func(), ToVoid());

		//And no other type of parameter
		template <typename F>
		auto ContinuationType_Void(F Func, ...)->ForbiddenFunc;

		template<typename F, typename P, typename Enable = void>
		struct TFunctionTypes {};

		template<typename F, typename P>
		struct TFunctionTypes<F, P, typename TEnableIf<!std::is_void<P>::value>::Type>
		{
			using ReturnType = typename TInvokeResult_T<typename std::decay_t<F>, P>;
		};

		template<typename F, typename P>
		struct TFunctionTypes<F, P, typename TEnableIf<std::is_void<P>::value>::Type>
		{
			using ReturnType = typename TInvokeResult_T<typename std::decay_t<F>>;
		};

		//Aliases for supported specializations of continuation functors
		template<typename R, typename P>
		class TContinuationFunctionTraits
		{
		public:
			using IsVoidToVoid = TEnableIf<
				TIntegralConstant<bool, TUnitTypeTraits<R>::IsVoid::value &&
										TUnitTypeTraits<P>::IsVoid::value>::Value>;

			using IsRealValueToVoid = TEnableIf<
				TIntegralConstant<bool, TUnitTypeTraits<R>::IsVoid::value &&
										TUnitTypeTraits<P>::IsRealValue::Value>::Value>;

			using IsResultToVoid = TEnableIf<
				TIntegralConstant<bool, TUnitTypeTraits<R>::IsVoid::value &&
										TUnitTypeTraits<P>::IsResult::Value>::Value>;

			using IsVoidToResult = TEnableIf<
				TIntegralConstant<bool, TUnitTypeTraits<R>::IsResult::Value &&
										TUnitTypeTraits<P>::IsVoid::value>::Value>;

			using IsRealValueToResult = TEnableIf<
				TIntegralConstant<bool, TUnitTypeTraits<R>::IsResult::Value &&
										TUnitTypeTraits<P>::IsRealValue::Value>::Value>;

			using IsResultToResult = TEnableIf<
				TIntegralConstant<bool, TUnitTypeTraits<R>::IsResult::Value &&
										TUnitTypeTraits<P>::IsResult::Value>::Value>;

			using IsVoidToRealValue = TEnableIf<
				TIntegralConstant<bool, TUnitTypeTraits<R>::IsRealValue::Value &&
										TUnitTypeTraits<P>::IsVoid::value>::Value>;

			using IsRealValueToRealValue = TEnableIf<
				TIntegralConstant<bool, TUnitTypeTraits<R>::IsRealValue::Value &&
										TUnitTypeTraits<P>::IsRealValue::Value>::Value>;

			using IsResultToRealValue = TEnableIf<
				TIntegralConstant<bool, TUnitTypeTraits<R>::IsRealValue::Value &&
										TUnitTypeTraits<P>::IsResult::Value>::Value>;

			using IsVoidToFuture = TEnableIf<
				TIntegralConstant<bool, TUnitTypeTraits<R>::IsFuture::Value &&
										TUnitTypeTraits<P>::IsVoid::value>::Value>;

			using IsRealValueToFuture = TEnableIf<
				TIntegralConstant<bool, TUnitTypeTraits<R>::IsFuture::Value &&
										TUnitTypeTraits<P>::IsRealValue::Value>::Value>;

			using IsResultToFuture = TEnableIf<
				TIntegralConstant<bool, TUnitTypeTraits<R>::IsFuture::Value &&
										TUnitTypeTraits<P>::IsResult::Value>::Value>;
		};

		template <typename F, typename P>
		struct TContinuationTypes
		{
			using ParamType = decltype(ContinuationType(DeclVal<F>(), DeclVal<P>(), 0, 0));
			static_assert(!std::is_same<ParamType, ForbiddenFunc>::value, "Continuation function parameter can either be TResult<P> or P");

			using ReturnType = typename TFunctionTypes<F, ParamType>::ReturnType;
			using Traits = TContinuationFunctionTraits<ReturnType, ParamType>;
		};

		template <typename F>
		struct TContinuationTypes<F, void>
		{
			using ParamType = decltype(ContinuationType_Void(DeclVal<F>(), 0, 0));
			static_assert(!std::is_same<ParamType, ForbiddenFunc>::value, "Continuation function parameter can either be TResult<void> or void");

			using ReturnType = typename TFunctionTypes<F, ParamType>::ReturnType;
			using Traits = TContinuationFunctionTraits<ReturnType, ParamType>;
		};
	}
}
