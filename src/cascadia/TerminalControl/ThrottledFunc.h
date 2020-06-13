/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ThrottledFunc.h
--*/

#pragma once
#include "pch.h"

#include "ThreadSafeOptional.h"

// Class Description:
// - Represents a function whose invokation is delayed by a specified duration
//   and rate-limited such that if the code tries to run the function while a
//   call to the function is already pending, the request will be ignored.
// - The function will be run on the the specified dispatcher.
class ThrottledFunc : public std::enable_shared_from_this<ThrottledFunc>
{
public:
    using Func = std::function<void()>;

    ThrottledFunc(Func func, winrt::Windows::Foundation::TimeSpan delay, winrt::Windows::UI::Core::CoreDispatcher dispatcher);

    void Run();

private:
    Func _func;
    winrt::Windows::Foundation::TimeSpan _delay;
    winrt::Windows::UI::Core::CoreDispatcher _dispatcher;
    std::atomic_flag _isRunPending;
};

// Class Description:
// - Represents a function that takes an argument and whose invokation is
//   delayed by a specified duration and rate-limited such that if the code
//   tries to run the function while a call to the function is already
//   pending, then the previous call with the previous argument will be
//   cancelled and the call will be made with the new argument instead.
// - The function will be run on the the specified dispatcher.
template<typename T>
class ThrottledArgFunc : public std::enable_shared_from_this<ThrottledArgFunc<T>>
{
public:
    using Func = std::function<void(T arg)>;

    ThrottledArgFunc(Func func, winrt::Windows::Foundation::TimeSpan delay, winrt::Windows::UI::Core::CoreDispatcher dispatcher) :
        _func{ func },
        _delay{ delay },
        _dispatcher{ dispatcher }
    {
    }

    // Method Description:
    // - Runs the function later with the specified argument, except if `Run`
    //   is called again before with a new argument, in which case the new
    //   argument will be instead.
    // - For more information, read the class' documentation.
    // - This method is always thread-safe. It can be called multiple times on
    //   different threads.
    // Arguments:
    // - arg: the argument to pass to the function
    // Return Value:
    // - <none>
    void Run(T arg)
    {
        if (!_pendingRunArg.Emplace(arg))
        {
            // already pending
            return;
        }

        _dispatcher.RunAsync(CoreDispatcherPriority::Low, [weakThis = this->weak_from_this()]() {
            if (auto self{ weakThis.lock() })
            {
                DispatcherTimer timer;
                timer.Interval(self->_delay);
                timer.Tick([=](auto&&...) {
                    if (auto self{ weakThis.lock() })
                    {
                        timer.Stop();
                        self->_func(self->_pendingRunArg.Take().value());
                    }
                });
                timer.Start();
            }
        });
    }

    // Method Description:
    // - Modifies the pending argument for the next function invocation, if
    //   there is one pending currently.
    // - Let's say that you just called the `Run` method with argument A.
    //   After the delay specified in the constructor, the function R
    //   specified in the constructor will be called with argument A.
    // - By using this method, you can modify argument A before the function R
    //   is called with argument A.
    // - You pass a function to this method which will take a reference to
    //   argument A and will modify it.
    // - When there is no pending invocation of function R, this method will
    //   not do anything.
    // - This method is always thread-safe. It can be called multiple times on
    //   different threads.
    // Arguments:
    // - f: the function to call with a reference to the argument
    // Return Value:
    // - <none>
    template<typename F>
    void ModifyPending(F f)
    {
        _pendingRunArg.ModifyValue(f);
    }

private:
    Func _func;
    winrt::Windows::Foundation::TimeSpan _delay;
    winrt::Windows::UI::Core::CoreDispatcher _dispatcher;
    ThreadSafeOptional<T> _pendingRunArg;
};
