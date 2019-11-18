#pragma once
#include "ActionAndArgs.g.h"
#include "..\inc\cppwinrt_utils.h"

namespace winrt::TerminalApp::implementation
{
    struct ActionAndArgs : public ActionAndArgsT<ActionAndArgs>
    {
        ActionAndArgs() = default;
        GETSET_PROPERTY(TerminalApp::ShortcutAction, Action, TerminalApp::ShortcutAction::Invalid);
        GETSET_PROPERTY(IActionArgs, Args, nullptr);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(ActionAndArgs);
}
