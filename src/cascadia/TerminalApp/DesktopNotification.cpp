// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "DesktopNotification.h"

#include <WtExeUtils.h>

using namespace winrt::Windows::UI::Notifications;
using namespace winrt::Windows::Data::Xml::Dom;

namespace winrt::TerminalApp::implementation
{
    std::atomic<uint64_t> DesktopNotification::_lastNotificationTime{ 0 };

    // Method Description:
    // - Rate-limits toast notifications so we don't spam the user.
    // Return Value:
    // - Returns true if a notification is allowed, false if too recent.
    bool DesktopNotification::ShouldSendNotification()
    {
        const auto now = GetTickCount64();
        auto last = _lastNotificationTime.load(std::memory_order_relaxed);

        // Subtraction wraps cleanly modulo 2^64, so the delta is correct even
        // across the (~584 million year) GetTickCount64 rollover.
        if (now - last < MinNotificationIntervalMs)
        {
            return false;
        }

        // Attempt to update; if another thread beat us, that's fine — we'll skip this one.
        return _lastNotificationTime.compare_exchange_strong(last, now, std::memory_order_relaxed);
    }

    // Method Description:
    // - Sends a toast notification with the given title and message.
    // - When the user clicks the toast, the `Activated` callback fires
    //   with the tabIndex that was passed in, so the caller can switch
    //   to the correct tab and summon the window.
    // Arguments:
    // - args: The title, message, and tab index to include in the notification.
    // - activated: A callback invoked on the background thread when the
    //              toast is clicked. The uint32_t parameter is the tab index.
    void DesktopNotification::SendNotification(const DesktopNotificationArgs& args, std::function<void()> activatedFunc)
    {
        try
        {
            if (!ShouldSendNotification())
            {
                return;
            }

            // Build the toast XML. We use a simple template with a title and body text.
            //
            // <toast launch="--from-toast">
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

            auto toastElement = toastXml.DocumentElement();

            // When a toast is clicked, Windows launches a new instance of the app
            // with the "launch" attribute as command-line arguments. We handle
            // toast activation in-process via the Activated event below, so the
            // new instance should do nothing. "--from-toast" is recognized by
            // AppCommandlineArgs::ParseArgs as a no-op sentinel.
            toastElement.SetAttribute(L"launch", L"--from-toast");

            toastElement.SetAttribute(L"scenario", L"default");

            auto toast = ToastNotification{ toastXml };

            // Set the tag and group to enable notification replacement.
            // Repeated notifications with the same tag replace the previous one
            // rather than stacking in the notification center.
            toast.Tag(args.Tag);
            toast.Group(L"WindowsTerminal");

            // When the user activates (clicks) the toast, fire the callback.
            if (activatedFunc)
            {
                toast.Activated([activatedFunc](const auto& /*sender*/, const auto& /*eventArgs*/) {
                    activatedFunc();
                });
            }

            // For packaged apps, CreateToastNotifier() uses the package identity automatically.
            // For unpackaged apps, we must pass the explicit AUMID that was registered
            // at startup via SetCurrentProcessExplicitAppUserModelID.
            winrt::Windows::UI::Notifications::ToastNotifier notifier{ nullptr };
            if (IsPackaged())
            {
                notifier = ToastNotificationManager::CreateToastNotifier();
            }
            else
            {
                // Retrieve the AUMID that was set by WindowEmperor at startup.
                wil::unique_cotaskmem_string aumid;
                if (SUCCEEDED(GetCurrentProcessExplicitAppUserModelID(&aumid)))
                {
                    notifier = ToastNotificationManager::CreateToastNotifier(aumid.get());
                }
            }
            if (notifier)
            {
                notifier.Show(toast);
            }
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
