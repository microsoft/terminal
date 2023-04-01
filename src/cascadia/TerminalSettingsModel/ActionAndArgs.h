// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ActionAndArgs.g.h"
#include "ActionArgs.h"
#include "TerminalWarnings.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct ActionAndArgs : public ActionAndArgsT<ActionAndArgs>
    {
        static const std::map<std::string_view, ShortcutAction, std::less<>> ActionKeyNamesMap;
        static winrt::com_ptr<ActionAndArgs> FromJson(const Json::Value& json,
                                                      std::vector<SettingsLoadWarnings>& warnings);
        static Json::Value ToJson(const Model::ActionAndArgs& val);

        static winrt::hstring Serialize(const winrt::Windows::Foundation::Collections::IVector<Model::ActionAndArgs>& args);
        static winrt::Windows::Foundation::Collections::IVector<Model::ActionAndArgs> Deserialize(winrt::hstring content);

        ActionAndArgs() = default;
        ActionAndArgs(ShortcutAction action);
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

namespace Microsoft::Terminal::Settings::Model::JsonUtils
{
    using namespace winrt::Microsoft::Terminal::Settings::Model;

    template<>
    struct ConversionTrait<ActionAndArgs>
    {
        ActionAndArgs FromJson(const Json::Value& json)
        {
            std::vector<SettingsLoadWarnings> v;
            return *implementation::ActionAndArgs::FromJson(json, v);
        }

        bool CanConvert(const Json::Value& json) const
        {
            // commands without args might just be a string
            return json.isString() || json.isObject();
        }

        Json::Value ToJson(const ActionAndArgs& val)
        {
            return implementation::ActionAndArgs::ToJson(val);
        }

        std::string TypeDescription() const
        {
            return "ActionAndArgs";
        }
    };
}
