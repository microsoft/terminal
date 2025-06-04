// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ShortcutActionDispatch.h"
#include "WtExeUtils.h"

#include "ShortcutActionDispatch.g.cpp"

using namespace winrt::Microsoft::Terminal;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::TerminalApp;

#define ACTION_CASE(action)              \
    case ShortcutAction::action:         \
    {                                    \
        action.raise(sender, eventArgs); \
        break;                           \
    }

namespace winrt::TerminalApp::implementation
{
    // Method Description:
    // - Dispatch the appropriate event for the given ActionAndArgs. Constructs
    //   an ActionEventArgs to hold the IActionArgs payload for the event, and
    //   calls the matching handlers for that event.
    // Arguments:
    // - actionAndArgs: the ShortcutAction and associated args to raise an event for.
    // Return Value:
    // - true if the event was handled, else false.
    bool ShortcutActionDispatch::DoAction(const winrt::Windows::Foundation::IInspectable& sender,
                                          const ActionAndArgs& actionAndArgs)
    {
        if (!actionAndArgs)
        {
            return false;
        }

        const auto& action = actionAndArgs.Action();
        const auto& args = actionAndArgs.Args();
        auto eventArgs = args ? ActionEventArgs{ args } :
                                ActionEventArgs{};

        switch (action)
        {
#define ON_ALL_ACTIONS(id) ACTION_CASE(id);
            ALL_SHORTCUT_ACTIONS
            INTERNAL_SHORTCUT_ACTIONS
#undef ON_ALL_ACTIONS
        default:
            return false;
        }
        const auto handled = eventArgs.Handled();

        if (handled)
        {
#if defined(WT_BRANDING_RELEASE)
            constexpr uint8_t branding = 3;
#elif defined(WT_BRANDING_PREVIEW)
            constexpr uint8_t branding = 2;
#elif defined(WT_BRANDING_CANARY)
            constexpr uint8_t branding = 1;
#else
            constexpr uint8_t branding = 0;
#endif

            TraceLoggingWrite(
                g_hTerminalAppProvider,
                "ActionDispatched",
                TraceLoggingDescription("Event emitted when an action was successfully performed"),
                TraceLoggingValue(static_cast<int>(actionAndArgs.Action()), "Action"),
                TraceLoggingValue(branding, "Branding"),
                TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
                TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
        }

        return handled;
    }

    bool ShortcutActionDispatch::DoAction(const ActionAndArgs& actionAndArgs)
    {
        return DoAction(nullptr, actionAndArgs);
    }
}
