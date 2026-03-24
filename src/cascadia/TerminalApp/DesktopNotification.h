/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- DesktopNotification.h

Module Description:
- Helper for sending Windows desktop toast notifications. Used to surface
  terminal activity events to the user via the Windows notification center.
--*/

#pragma once
#include "pch.h"

namespace winrt::TerminalApp::implementation
{
    struct DesktopNotificationArgs
    {
        winrt::hstring Title;
        winrt::hstring Message;
        uint32_t TabIndex{ 0 };
    };

    class DesktopNotification
    {
    public:
        static bool ShouldSendNotification();
        static void SendNotification(const DesktopNotificationArgs& args, std::function<void(uint32_t tabIndex)> activatedFunc);

    private:
        static std::atomic<int64_t> _lastNotificationTime;

        // Minimum interval between notifications, in 100ns ticks (FILETIME units).
        // 5 seconds = 5 * 10,000,000
        static constexpr int64_t MinNotificationIntervalTicks = 50'000'000LL;
    };
}
