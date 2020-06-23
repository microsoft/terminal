#pragma once
#include "ActionAndArgs.g.h"
#include "TerminalWarnings.h"
#include "..\inc\cppwinrt_utils.h"

namespace winrt::TerminalApp::implementation
{
    struct ActionAndArgs : public ActionAndArgsT<ActionAndArgs>
    {
        static const std::map<std::string_view, ShortcutAction, std::less<>> ActionNamesMap;
        static winrt::com_ptr<ActionAndArgs> FromJson(const Json::Value& json,
                                                      std::vector<::TerminalApp::SettingsLoadWarnings>& warnings);

        ActionAndArgs() = default;
        GETSET_PROPERTY(TerminalApp::ShortcutAction, Action, TerminalApp::ShortcutAction::Invalid);
        GETSET_PROPERTY(IActionArgs, Args, nullptr);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(ActionAndArgs);
}
