// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ActionAndArgs.g.h"
#include "ActionArgs.h"
#include "TerminalWarnings.h"
#include "../inc/cppwinrt_utils.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct ActionAndArgs : public ActionAndArgsT<ActionAndArgs>
    {
        static const std::map<std::string_view, ShortcutAction, std::less<>> ActionKeyNamesMap;
        static winrt::com_ptr<ActionAndArgs> FromJson(const Json::Value& json,
                                                      std::vector<SettingsLoadWarnings>& warnings);

        ActionAndArgs() = default;
        ActionAndArgs(ShortcutAction action, IActionArgs args) :
            _Action{ action },
            _Args{ args } {};
        com_ptr<ActionAndArgs> Copy() const;

        hstring GenerateName() const;

        WINRT_PROPERTY(ShortcutAction, Action, ShortcutAction::Invalid);
        WINRT_PROPERTY(IActionArgs, Args, nullptr);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(ActionAndArgs);
}
