/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- DesktopNotification.h

Module Description:
- Helper for sending Windows desktop toast notifications. Used by the
  `OutputNotificationStyle::Notification` flag to surface activity
  and prompt-return events to the user via the Windows notification center.

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
        // Sends a toast notification with the given title and message.
        // When the user clicks the toast, the `Activated` callback fires
        // with the tabIndex that was passed in, so the caller can switch
        // to the correct tab and summon the window.
        //
        // activated: A callback invoked on the background thread when the
        //            toast is clicked. The uint32_t parameter is the tab index.
        static void SendNotification(
            const DesktopNotificationArgs& args,
            std::function<void(uint32_t tabIndex)> activated);

        // Rate-limits toast notifications so we don't spam the user.
        // Returns true if a notification is allowed, false if too recent.
        static bool ShouldSendNotification();

    private:
        static std::atomic<int64_t> _lastNotificationTime;

        // Minimum interval between notifications, in 100ns ticks (FILETIME units).
        // 5 seconds = 5 * 10,000,000
        static constexpr int64_t MinNotificationIntervalTicks = 50'000'000LL;
    };
}
