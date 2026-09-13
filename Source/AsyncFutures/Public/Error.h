// Copyright Dominic Curry. All Rights Reserved.
#pragma once

// Engine Includes
#include "CoreTypes.h"

namespace UE::Tasks
{
	struct FError
	{
	public:
		FError(uint64 ErrorContext, uint64 ErrorCode, const FString& ErrorMessage)
			: Context(ErrorContext)
			, Code(ErrorCode)
			, Message(ErrorMessage)
		{}
		
		FError(uint64 ErrorContext, uint64 ErrorCode, FString&& ErrorMessage)
			: Context(ErrorContext)
			, Code(ErrorCode)
			, Message(MoveTemp(ErrorMessage))
		{}

		FError(uint64 ErrorContext, uint64 ErrorCode)
			: FError(ErrorContext, ErrorCode, FString())
		{}

		inline bool operator==(const FError& Other) const { return Context == Other.Context && Code == Other.Code; }

		uint64 GetContext() const { return Context;}
		uint64 GetCode() const { return Code;}
		FString GetMessage() const { return Message;}

	private:
		uint64 Context;
		uint64 Code;
		FString Message;
	};

	inline constexpr uint64 ERROR_CONTEXT_FUTURE = 1;
	inline constexpr uint64 ERROR_CANCELLED = 1;
	// A monitored owner (UObject/TSharedFromThis) was destroyed before its continuation ran.
	// Deliberately distinct from ERROR_CANCELLED: nobody asked to abandon the work, one link
	// in the chain just disappeared. See TResult::IsOwnerExpired().
	inline constexpr uint64 ERROR_LIFETIME = 2;

	inline FError MakeCancelledError()
	{
		return FError(ERROR_CONTEXT_FUTURE, ERROR_CANCELLED, TEXT("Cancelled Result"));
	}
}
