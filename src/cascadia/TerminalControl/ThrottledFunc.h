/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- TimeThrottle.h

Abstract:
- This module is a helper to throttle actions.
- The action is defined by the user of that helper. It could be updating a
  a file, fetching something from the disk, etc.
- You give it a minimum delay between two actions and every time you want to
  start a new action, you call the `GetNextWaitTime` method. If it returns a
  non empty value, then you have to wait for that amount of time before you
  can actually do the action.
- When you complete an action, you call the `DidAction` method.
- This is not thread safe.
--*/

#pragma once
#include "pch.h"

#include "ThreadSafeOptional.h"

template<typename T>
class ThrottledFunc : public std::enable_shared_from_this<ThrottledFunc<T>>
{
public:
    using Func = std::function<void(T arg)>;

    ThrottledFunc(Func func, winrt::Windows::Foundation::TimeSpan minimumDelay) :
        _func{ func },
        _minimumDelay{ minimumDelay }
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

        const auto waitTime = GetNextWaitTime();
        if (waitTime.has_value())
        {
            co_await winrt::resume_after(waitTime.value());
        }

        if (auto that{ weakThis.lock() })
        {
            // No need to lock because there cannot be another thread that is
            // reading/writing to the value because `_nextArg` has a value so
            // other calls to `Run` early return.
            that->_lastRun = std::chrono::high_resolution_clock::now();

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
    // Method Description:
    // - Computes the time that the code should wait before doing the next action,
    //   in order to throttle actions with the specified throttle time. If the code
    //   should not wait, then an empty value is returned.
    // Arguments:
    // - <none>
    // Return Value:
    // - the time to wait before the next action or an empty value for no wait
    std::optional<winrt::Windows::Foundation::TimeSpan> GetNextWaitTime()
    {
        if (!_lastRun.has_value())
        {
            return { std::nullopt };
        }
        else
        {
            const auto timeNow = std::chrono::high_resolution_clock::now();
            const auto sinceLastAction = timeNow - _lastRun.value();
            if (sinceLastAction > _minimumDelay)
            {
                // Fortunately, time will not go backwards, so if we have
                // waited enough to start a new action, we will still have
                // waited enough to start a new action later (unless we
                // do an action in which case the `_lastAction` field will be
                // set).
                _lastRun = { std::nullopt };

                return { std::nullopt };
            }
            else
            {
                auto waitTime = std::chrono::duration_cast<winrt::Windows::Foundation::TimeSpan>(_minimumDelay - sinceLastAction);
                if (waitTime.count() > 0)
                {
                    return { waitTime };
                }
                else
                {
                    return { std::nullopt };
                }
            }
        }
    }

    Func _func;
    winrt::Windows::Foundation::TimeSpan _minimumDelay;
    std::optional<std::chrono::high_resolution_clock::time_point> _lastRun;
    ThreadSafeOptional<T> _nextArg;
};
