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
        winrt::hstring Tag;
    };

    class DesktopNotification
    {
    public:
        static bool ShouldSendNotification();
        static void SendNotification(const DesktopNotificationArgs& args, std::function<void()> activatedFunc);

    private:
        static std::atomic<uint64_t> _lastNotificationTime;

        // Minimum interval between notifications, in milliseconds (GetTickCount64 units).
        static constexpr uint64_t MinNotificationIntervalMs = 5'000;
    };
}
