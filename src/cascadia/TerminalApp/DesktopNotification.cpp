// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "DesktopNotification.h"

using namespace winrt::Windows::UI::Notifications;
using namespace winrt::Windows::Data::Xml::Dom;

namespace winrt::TerminalApp::implementation
{
    std::atomic<int64_t> DesktopNotification::_lastNotificationTime{ 0 };

    bool DesktopNotification::ShouldSendNotification()
    {
        FILETIME ft{};
        GetSystemTimeAsFileTime(&ft);
        const auto now = (static_cast<int64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
        auto last = _lastNotificationTime.load(std::memory_order_relaxed);

        if (now - last < MinNotificationIntervalTicks)
        {
            return false;
        }

        // Attempt to update; if another thread beat us, that's fine — we'll skip this one.
        _lastNotificationTime.compare_exchange_strong(last, now, std::memory_order_relaxed);
        return true;
    }

    void DesktopNotification::SendNotification(
        const DesktopNotificationArgs& args,
        std::function<void(uint32_t tabIndex)> activated)
    {
        try
        {
            if (!ShouldSendNotification())
            {
                return;
            }

            // Build the toast XML. We use a simple template with a title and body text.
            //
            // <toast launch="tabIndex=N">
            //   <visual>
            //     <binding template="ToastGeneric">
            //       <text>Title</text>
            //       <text>Message</text>
            //     </binding>
            //   </visual>
            // </toast>
            auto toastXml = ToastNotificationManager::GetTemplateContent(ToastTemplateType::ToastText02);
            auto textNodes = toastXml.GetElementsByTagName(L"text");

            // First <text> is the title
            textNodes.Item(0).InnerText(args.Title);
            // Second <text> is the body
            textNodes.Item(1).InnerText(args.Message);

            // Set launch args so we can identify which tab to activate.
            auto toastElement = toastXml.DocumentElement();
            toastElement.SetAttribute(L"launch", fmt::format(FMT_COMPILE(L"tabIndex={}"), args.TabIndex));

            // Set the scenario to "reminder" to ensure the toast shows even in DND,
            // and the group/tag to allow replacement of repeated notifications.
            toastElement.SetAttribute(L"scenario", L"default");

            auto toast = ToastNotification{ toastXml };

            // Set the tag and group to enable notification replacement.
            // Using the tab index as a tag means repeated output from the same tab
            // replaces the previous notification rather than stacking.
            toast.Tag(fmt::format(FMT_COMPILE(L"wt-tab-{}"), args.TabIndex));
            toast.Group(L"WindowsTerminal");

            // When the user activates (clicks) the toast, fire the callback.
            if (activated)
            {
                const auto tabIndex = args.TabIndex;
                toast.Activated([activated, tabIndex](const auto& /*sender*/, const auto& /*eventArgs*/) {
                    activated(tabIndex);
                });
            }

            // For packaged apps, CreateToastNotifier() uses the package identity automatically.
            // For unpackaged apps, we need to provide an AUMID, but that case is less common
            // and toast notifications may not be supported without additional setup.
            auto notifier = ToastNotificationManager::CreateToastNotifier();
            notifier.Show(toast);
        }
        catch (...)
        {
            // Toast notification is a best-effort feature. If it fails (e.g., notifications
            // are disabled, or the app is unpackaged without proper AUMID setup), we silently
            // ignore the error.
            LOG_CAUGHT_EXCEPTION();
        }
    }
}
