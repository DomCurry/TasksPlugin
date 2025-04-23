// Copyright Dominic Curry. All Rights Reserved.
#include "TestModule.h"
#include <Modules/ModuleManager.h>

class FAsyncFuturesTest : public IAsyncFuturesTest
{

};

IMPLEMENT_MODULE(FAsyncFuturesTest, AsyncFutureTests)
