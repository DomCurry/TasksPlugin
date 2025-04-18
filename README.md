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
### Combinations
This plugin also supports splitting and converging chains of futures to better marshall the work required. This is achieved through `WhenAll` and `WhenAny` functions - each of which produce their own `TAsyncFuture`.
### Cancellation
There are cases where using these patterns is beneficial but not at the expense of the application crash resulting from a 'broken' or unfulfilled promise, in these cases we allow the cancellation of a `TAsyncPromise` allowing potential work to be abandoned with little overhead. This manifests as a specific error passed through the chain of results in the future values.
### Lifetime Monitoring
A common use for the cancellation of a promise is that the object that has initiated the work has since been destroyed. In those cases this plugin provides a neat conversion for `UObject*` and `TSharedFromThis` types, that will remove the boilerplate of the weak pointer capture and pinning of the owning object inside the continuation logic.
### FOptions
The structure to associate any task with the `CancellationHandle` associated with it and the `Thread` it should run on. This plugin uses the `TaskGraph` system and while this currently only exposes the setting of the `Thread` to run the task on, this plugin attempts to avoid redundancy by allowing an `FOptions` structure to be provided to each continuation. Hopefully, this would be enough to allow the adaptation to any new async methodologies that Epic may develop in the future.
### Tests
Included in this plugin are a suite of unit tests. These can be a good place to inspect functionality and the style of code produced by these structures. 
## Example
Here's what it looks like when a function returns a future value and how you can define work to do when it ends.
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
UE::Tasks::TAsyncFuture<FProfile> UBackend::Login(const FString& Username, const FString& Password)
{
  GetInitializationTask().Then(this, [this, Username, Password] ()
  {
    return HTTP->SendHTTP("www.login.com?u=%s&p=%s", Username, Password);
  }, UE::Tasks::FOptions(ENamedThreads:AnyBackgroundNormalPri))
  .Then(this, [this] (const FHTTPResponse Response)
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
UE::Tasks::TAsyncFuture<FHTTPResponse> HTTPRequester::SendHTTP(const FString& Address)
{
  UE::Tasks::TAsyncPromise<FHTTPResponse> Promise;
  HTTP.Send(Address).SetCallback([Promise] (FString& Response)
    { Promise.SetValue(FHTTPResponse(Response)); });
  return Promise.GetFuture();
}
```
