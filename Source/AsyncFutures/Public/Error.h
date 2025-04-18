// Copyright(c) Dominic Curry. All rights reserved.
#pragma once

// Engine Includes
#include "CoreTypes.h"

namespace UE::Tasks
{
	struct Error
	{
	public:
		Error(uint64 ErrorContext, uint64 ErrorCode, const FString& ErrorMessage)
			: Context(ErrorContext)
			, Code(ErrorCode)
			, Message(ErrorMessage)
		{}
		
		Error(uint64 ErrorContext, uint64 ErrorCode, FString&& ErrorMessage)
			: Context(ErrorContext)
			, Code(ErrorCode)
			, Message(MoveTemp(ErrorMessage))
		{}

		Error(uint64 ErrorContext, uint64 ErrorCode)
			: Error(ErrorContext, ErrorCode, FString())
		{}

		inline bool operator==(const Error& Other) const { return Context == Other.Context && Code == Other.Code; }

		uint64 GetContext() const { return Context;}
		uint64 GetCode() const { return Code;}
		FString GetMessage() const { return Message;}

	private:
		uint64 Context;
		uint64 Code;
		FString Message;
	};

	uint64 ERROR_CONTEXT_FUTURE = 1;
	uint64 ERROR_CANCELLED = 1;

	Error MakeCancelledError()
	{
		return Error(ERROR_CONTEXT_FUTURE, ERROR_CANCELLED, TEXT("Cancelled Result"));
	}
}
