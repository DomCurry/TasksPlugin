# TasksPlugin
A Future/Promise style interface to the unreal taskgraph system, see the [Parallel Patterns Library](https://docs.microsoft.com/en-us/cpp/parallel/concrt/parallel-patterns-library-ppl?redirectedfrom=MSDN&view=vs-2019).

A promise, to software developers, is a type that guarantees its fulfillment at some later time, and a future is the potential value that the promise will hold. The Wiki on [Futures and Promises](https://en.wikipedia.org/wiki/Futures_and_promises) has some pretty good info for those curious.

To add to your project place in a `Tasks` folder in your `/plugins/` directory. 
To use add the `#include "AsyncFutures.h"` to your file.

## Contents
There's a few components in this plugin that all build together to produce a feature set to allow simple and parseable async programming patterns.
### Errors
Everyone wants to know why things go wrong, and so there's an `Error` type. This comes with the classic error code and error message fields that you can set to be whatever you like. And then in anticipation of using this plugin to wrap or integrate some 3rd party systems we added a `Context` for the errors so you can better avoid collisions between HTML codes and Platform codes and whichever other codes might want to pass through this plugin.
### Result
This plugin also has a new `TResult` type, but this is essentially Epic's `TValueOrError` with a bit of neatness and some type deduction wizardry. It does mean the Error type above isn't hot-swappable but that should be flexible enough.
### Promise
This is the first asynchronous component. Epic does have a `TPromise` type which does work well as a promise concept, but it didn't help to achieve the goals of this plugin. So this plugin adds `TAsyncPromise`, a copyable and thread safe version of a promise. 
### Future
This is the other side of the coin to the `TAsyncPromise` again with the threadsafe and copyable traits. 
### Continuations
A continuation is a key part of this plugin, allowing us to easily specify a unit of logic to be performed when - at some future time - the promise is fulfilled and the result delivered. This pattern establishes this through a `.Then` call on any `TAsyncFuture` which in turn will generate its own `TAsyncFuture` of the corresponding result of that chained future work.

**`.Then` on an already-resolved future runs synchronously**, before `.Then` returns, rather than costing a scheduler hop to reach a decision that is already made. This applies only when the continuation expressed no preference about where it runs — `EAsyncExecution::TaskGraph` with no thread named. Naming any thread (including a priority class like `AnyBackgroundThreadNormalTask`) or choosing another execution policy dispatches as before. `Async()` always dispatches: its upstream is an already-resolved future by construction, so it opts out explicitly via `FOptions::RequireAsync()` to stay true to its name. Deeply nested inline chains fall back to dispatching past a fixed depth, so a self-chaining continuation cannot exhaust the stack.
### Combinations
This plugin also supports splitting and converging chains of futures to better marshall the work required. This is achieved through `WhenAll` and `WhenAny` functions - each of which produce their own `TAsyncFuture`.
### Cancellation
There are cases where using these patterns is beneficial but not at the expense of the application crash resulting from a 'broken' or unfulfilled promise, in these cases we allow the cancellation of a `TAsyncPromise` allowing potential work to be abandoned with little overhead. This manifests as a specific error passed through the chain of results in the future values.

**A broken promise is fatal, by design.** Dropping a promise without fulfilling or cancelling it is a hard assert (`~TPromiseState`), on whatever thread releases the last reference. This is deliberate: client code is expected to manage its cancellations consciously at all times. An unfulfilled promise means a future that never completes and a continuation chain that silently stalls, which is worse to diagnose than an immediate assert. Always resolve a promise — call `Cancel()` on it directly, or bind an `FCancellationHandle` so it happens automatically.

Cancellation is also how an object owns its tasks: hold an `FCancellationHandle` as a member, pass it to every task you start via `FOptions`, and everything is cancelled when the object is collected — `~FCancellationState` cancels every promise ever bound to it. The handle is shared (`TSharedRef`) precisely so several objects can co-own a task's cancellation, but this means the handle must **outlive the work**. This cancels immediately and looks like nothing happened:

```cpp
{
    FCancellationHandle Handle;
    DoWork(FOptions().Set(Handle));
}   // Handle dies, DoWork is cancelled
```

Store it as a member, not a local.

**Cancelling runs continuations rather than skipping them.** `Cancel()` does not complete the bound promises itself; it runs each bound continuation immediately, handing it a cancelled `TResult` instead of whatever the upstream would have produced. The continuation is the only thing that completes the promise, which is what guarantees a downstream `.Then` never observes the result before the continuation that produced it has run. Two consequences worth knowing:

- A cancelled continuation does **not** wait for its upstream — it runs straight away, even if that upstream never resolves at all.
- A `TResult`-taking continuation sees `Result.IsCancelled()` and decides what to propagate. Returning the result passes the cancellation on; returning a plain value deliberately overrides it.

`Cancel()` therefore runs work on the calling thread where the continuation's `FOptions` allow it (`EAsyncExecution::TaskGraph` with no thread named). Name a thread, or pick another execution policy, if a continuation must not run on whichever thread calls `Cancel()`. Handle destruction always dispatches instead of running inline, so teardown never runs user code.
### Lifetime Monitoring
A common use for the cancellation of a promise is that the object that has initiated the work has since been destroyed. In those cases this plugin provides a neat conversion for `UObject*` and `TSharedFromThis` types, that will remove the boilerplate of the weak pointer capture and pinning of the owning object inside the continuation logic.

**Lifetime expiry is not cancellation.** When a monitored owner is destroyed, the continuation fails with `ERROR_LIFETIME` (code `2`), not `ERROR_CANCELLED` (code `1`), so `TResult::IsCancelled()` returns `false` for it. This distinction is deliberate: cancelling a handle is a decision to abandon work, whereas an owner being destroyed is the disappearance of one link in a chain whose other links may still be perfectly valid. Keeping the codes separate lets a downstream continuation tell "someone called `Cancel()`" apart from "the object that started this is gone" and react differently — use `TResult::IsOwnerExpired()` rather than hand-comparing error codes.
### FOptions
The structure to associate any task with the `FCancellationHandle`, `Thread` and execution mechanism (`EAsyncExecution`) it should run under. An explicit thread is only honoured when the execution policy is `EAsyncExecution::TaskGraph` — the other policies (`Thread`, `ThreadPool`, ...) choose their own thread and ignore it. Named setters make the valid combinations easy to reach for: `OnTaskGraph(Thread)` (the default), `OnGameThread()`, `OnDedicatedThread()` and `OnThreadPool()`; the lower-level `Set(...)` overloads remain available for less common cases. This plugin attempts to avoid redundancy by allowing an `FOptions` structure to be provided to each continuation, in the hope that this is enough to allow adaptation to any new async methodologies Epic may develop in the future.
### Tests
Included in this plugin are a suite of unit tests. These can be a good place to inspect functionality and the style of code produced by these structures. 
## Example
Here's what it looks like when a function returns a future value and how you can define work to do when it ends. The examples below assume `using namespace UE::Tasks;` and so drop that namespace's prefix.
```cpp
void ULoginWidget::OnLoginClicked()
{
  ShowSpinner(true);
  Backend->Login(Username, Password)
    .Then(this, [this] (const TResult<FProfile>& Result)
    {
      ShowSpinner(false);
      if (Result.HasError())
      {
        ShowErrorMessage(Result.GetError());
        return;
      }
      ShowProfile(Result.GetValue());
    });
}
```
If you're managing tasks you can chain them in this way - here we're hiding the initialization so we don't error if someone asks us to login before we're ready. We're also able to hide that we're doing a HTTP request to login from the frontend.
```cpp
TAsyncFuture<FProfile> UBackend::Login(const FString& Username, const FString& Password)
{
  return GetInitializationTask()
    .Then(this, [this, Username, Password] ()
    {
      return HTTP->SendHTTP(FString::Printf(TEXT("www.login.com?u=%s&p=%s"), *Username, *Password));
    }, FOptions().Set(ENamedThreads::AnyBackgroundThreadNormalTask))
    .Then(this, [this] (const FHTTPResponse& Response) -> TResult<FProfile>
    {
      if (Response.Code == 200) // OK
      {
        return FProfile(Response);
      }
      return HTTPError(Response);
    });
}
```
The HTTP system has its own call and response framework but we're easily able to wrap them in a TAsyncPromise and convert it to a consistent pattern expected by the rest of the codebase.
```cpp
TAsyncFuture<FHTTPResponse> HTTPRequester::SendHTTP(const FString& Address)
{
  TAsyncPromise<FHTTPResponse> Promise;
  HTTP.Send(Address).SetCallback([Promise] (FString& Response)
    { Promise.SetValue(FHTTPResponse(Response)); });
  return Promise.GetFuture();
}
```
