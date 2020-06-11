/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ThrottledFunc.h

Abstract:
- This module is a helper to throttle actions.
- The action is defined by the user of that helper. It could be updating a
  a file, fetching something from the disk, etc.
- You give it a minimum delay between two actions and every time you want to
  start a new action, you call the `Run` method with an argument.
- Then the function give in the constructor will be called, but only when
  enough time passed since the last call.
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
    // - For more information, read the Abstract section in the header file.
    // Arguments:
    // - arg: the argument
    // Return Value:
    // - <none>
    winrt::fire_and_forget Run(T arg)
    {
        if (!_nextArg.Emplace(arg))
        {
            // already pending
            return;
        }

        auto weakThis = this->weak_from_this();

        co_await winrt::resume_after(_delay);

        if (auto that{ weakThis.lock() })
        {
            auto arg = that->_nextArg.Take();
            if (arg.has_value())
            {
                that->_func(arg.value());
            }
            else
            {
                // should not happen
            }
        }
    }

    template<typename F>
    void ModifyPending(F f)
    {
        _nextArg.ModifyValue(f);
    }

private:
    Func _func;
    winrt::Windows::Foundation::TimeSpan _delay;
    ThreadSafeOptional<T> _nextArg;
};
