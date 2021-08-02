// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ShortcutActionDispatch.h"

#include "ShortcutActionDispatch.g.cpp"

using namespace winrt::Microsoft::Terminal;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::TerminalApp;

#define ACTION_CASE(action)                    \
    case ShortcutAction::action:               \
    {                                          \
        _##action##Handlers(*this, eventArgs); \
        break;                                 \
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
    // - true if we handled the event was handled, else false.
    bool ShortcutActionDispatch::DoAction(const ActionAndArgs& actionAndArgs)
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
#undef ON_ALL_ACTIONS
        default:
            return false;
        }
        return eventArgs.Handled();
    }

}
