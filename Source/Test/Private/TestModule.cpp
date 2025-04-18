// Copyright(c) Dominic Curry. All rights reserved.
#include "TestModule.h"
#include <Modules/ModuleManager.h>

class FAsyncFuturesTest : public IAsyncFuturesTest
{

};

IMPLEMENT_MODULE(FAsyncFuturesTest, AsyncFutureTests)
