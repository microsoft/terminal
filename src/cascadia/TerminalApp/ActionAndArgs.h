#pragma once
#include "ActionAndArgs.g.h"
#include "..\inc\cppwinrt_utils.h"

namespace winrt::TerminalApp::implementation
{
    struct ActionAndArgs : public ActionAndArgsT<ActionAndArgs>
    {
        ActionAndArgs() = default;

        // TerminalApp::IActionArgs Args();
        // void Args(TerminalApp::IActionArgs const& value);
        // TerminalApp::ShortcutAction Action();
        // void Action(TerminalApp::ShortcutAction const& value);

        GETSET_PROPERTY(TerminalApp::ShortcutAction, Action, TerminalApp::ShortcutAction::Invalid);
        GETSET_PROPERTY(IActionArgs, Args, nullptr);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(ActionAndArgs);
}

// BASIC_FACTORY(ActionAndArgs);
