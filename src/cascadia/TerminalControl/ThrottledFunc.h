/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ThrottledFunc.h

Abstract:
- This module defines a class to throttle function calls.
- You create an instance of a `ThrottledFunc` with a function and the delay
  between two function calls.
- The function takes an argument of type `T`, the template argument of
  `ThrottledFunc`.
- Use the `Run` method to wait and then call the function.
--*/

#pragma once
#include "pch.h"

#include "ThreadSafeOptional.h"

template<typename T>
class ThrottledFunc : public std::enable_shared_from_this<ThrottledFunc<T>>
{
public:
    using Func = std::function<void(T arg)>;

    ThrottledFunc(Func func, winrt::Windows::Foundation::TimeSpan delay) :
        _func{ func },
        _delay{ delay }
    {
    }

    // Method Description:
    // - Runs the function later with the specified argument, except if `Run`
    //   is called again before with a new argument, in which case the new
    //   argument will be instead.
    // - For more information, read the "Abstract" section in the header file.
    // Arguments:
    // - arg: the argument to pass to the function
    // Return Value:
    // - <none>
    winrt::fire_and_forget Run(T arg)
    {
        if (!_pendingCallArg.Emplace(arg))
        {
            // already pending
            return;
        }

        auto weakThis = this->weak_from_this();

        co_await winrt::resume_after(_delay);

        if (auto self{ weakThis.lock() })
        {
            auto arg = self->_pendingCallArg.Take();
            if (arg.has_value())
            {
                self->_func(arg.value());
            }
            else
            {
                // should not happen
            }
        }
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
        _pendingCallArg.ModifyValue(f);
    }

private:
    Func _func;
    winrt::Windows::Foundation::TimeSpan _delay;
    ThreadSafeOptional<T> _pendingCallArg;
};
