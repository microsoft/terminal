// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "TimeThrottle.h"

using namespace winrt::Windows::Foundation;

TimeThrottle::TimeThrottle(winrt::Windows::Foundation::TimeSpan throttleTime) :
    _throttleTime(throttleTime)
{
}

// Method Description:
// - Computes the time that the code should wait before doing the next action,
//   in order to throttle actions with the specified throttle time. If the code
//   should not wait, then an empty value is returned.
// Arguments:
// - <none>
// Return Value:
// - the time to wait before the next action or an empty value for no wait
std::optional<TimeSpan> TimeThrottle::GetNextWaitTime() noexcept
{
    if (!_lastAction.has_value())
    {
        return { std::nullopt };
    }
    else
    {
        const auto timeNow = std::chrono::high_resolution_clock::now();
        const auto sinceLastAction = timeNow - _lastAction.value();
        if (sinceLastAction > _throttleTime)
        {
            // Fortunately, time will not go backwards, so if we have
            // waited enough to start a new action, we will still have
            // waited enough to start a new action later (unless we
            // do an action in which case the `_lastAction` field will be
            // set).
            _lastAction = { std::nullopt };

            return { std::nullopt };
        }
        else
        {
            auto waitTime = std::chrono::duration_cast<TimeSpan>(_throttleTime - sinceLastAction);
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

// Method Description:
// - Register the fact than an action happened.
// - Next calls to `GetNextWaitTime` will take that in consideration.
// Arguments:
// - <none>
// Return Value:
// - <none>
void TimeThrottle::DidAction()
{
    _lastAction = std::chrono::high_resolution_clock::now();
}
