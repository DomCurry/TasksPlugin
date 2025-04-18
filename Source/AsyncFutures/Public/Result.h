// Copyright(c) Dominic Curry. All rights reserved.
#pragma once

// Engine Includes
#include "CoreTypes.h"

// Module Includes
#include "Error.h"

namespace UE::Tasks
{
	//Need a wrapper around TValueOrError to avoid exposing ValueType in TValueOrError
	template<typename T>
	class TResult
	{
	public:
		using ResultType = T;

		TResult(const ResultType& Value)	: ValueOrError(TValueOrError_ValueProxy(Forward<const ResultType>(Value))) {}
		TResult(ResultType&& Value)			: ValueOrError(TValueOrError_ValueProxy(MoveTemp(Value))) {}
		TResult(const Error& Result)		: ValueOrError(TValueOrError_ErrorProxy(Forward<const Error>(Result))) {}
		TResult(Error&& Result)				: ValueOrError(TValueOrError_ErrorProxy(MoveTemp(Result))) {}

		bool HasError() const			{ return ValueOrError.HasError(); }
		bool HasValue() const			{ return ValueOrError.HasValue(); }
		const Error& GetError() const	{ return ValueOrError.GetError(); }
		const ResultType& GetValue() const	{ return ValueOrError.GetValue(); }

		bool IsCancelled() const { return HasError() && GetError() == MakeCancelledError(); }

		template<typename TransformType>
		TResult<TransformType> Transform(TransformType&& Value = TransformType()) const
		{
			if (HasError())
			{
				return TResult<TransformType>(GetError());
			}

			if (HasValue())
			{
				return TResult<TransformType>(Value);
			}

			checkf(false, TEXT("Should only be called from complete results"));
			return TResult<TransformType>(Value);
		}

		TResult<void> Transform() const
		{
			if (HasError())
			{
				return TResult<void>(GetError());
			}

			if (HasValue())
			{
				return TResult<void>();
			}

			checkf(false, TEXT("Should only be called from complete results"));
			return TResult<void>();
		}

	private:
		TValueOrError<ResultType, const Error> ValueOrError;
	};

	template<>
	class TResult<void>
	{
	public:
		using ResultType = void;

		TResult() : ValueOrError(TValueOrError_ValueProxy()) {}
		TResult(const Error& Result) : ValueOrError(TValueOrError_ErrorProxy(Forward<const Error>(Result))) {}
		TResult(Error&& Result) : ValueOrError(TValueOrError_ErrorProxy(MoveTemp(Result))) {}

		bool HasError() const { return ValueOrError.HasError(); }
		bool HasValue() const { return ValueOrError.HasValue(); }
		const Error& GetError() const { return ValueOrError.GetError(); }
		//void GetValue() const { }

		bool IsCancelled() const { return HasError() && GetError() == MakeCancelledError(); }

		template<typename TransformType>
		TResult<TransformType> Transform(TransformType&& Value = TransformType()) const
		{
			if (HasError())
			{
				return TResult<TransformType>(GetError());
			}

			if (HasValue())
			{
				return TResult<TransformType>(Value);
			}

			checkf(false, TEXT("Should only be called from complete results"));
			return TResult<TransformType>(Value);
		}

		TResult<void> Transform() const
		{
			if (HasError())
			{
				return TResult<void>(GetError());
			}

			if (HasValue())
			{
				return TResult<void>();
			}

			checkf(false, TEXT("Should only be called from complete results"));
			return TResult<void>();
		}

	private:
		TValueOrError<ResultType, const Error> ValueOrError;
	};

	template<typename T>
	TResult<T> MakeCancelledResult()
	{
		return TResult<T>(MakeCancelledError());
	}

	TResult<void> MakeCancelledResult()
	{
		return TResult<void>(MakeCancelledError());
	}
}
