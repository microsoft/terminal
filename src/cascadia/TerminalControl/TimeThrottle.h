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

class TimeThrottle
{
public:
    TimeThrottle(winrt::Windows::Foundation::TimeSpan throttleTime);

    std::optional<winrt::Windows::Foundation::TimeSpan> GetNextWaitTime() noexcept;
    void DidAction();

private:
    std::optional<std::chrono::high_resolution_clock::time_point> _lastAction;
    winrt::Windows::Foundation::TimeSpan _throttleTime;
};
