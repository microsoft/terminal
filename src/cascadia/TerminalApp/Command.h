// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Command.g.h"
#include "TerminalWarnings.h"
#include "Profile.h"
#include "..\inc\cppwinrt_utils.h"

namespace winrt::TerminalApp::implementation
{
    enum class ExpandCommandType : uint32_t
    {
        None = 0,
        Profiles
    };

    struct Command : CommandT<Command>
    {
        Command() = default;

        static winrt::com_ptr<Command> FromJson(const Json::Value& json, std::vector<::TerminalApp::SettingsLoadWarnings>& warnings);
        static std::vector<::TerminalApp::SettingsLoadWarnings> LayerJson(std::map<winrt::hstring, winrt::TerminalApp::Command>& commands,
                                                                          const Json::Value& json);

        static std::vector<winrt::TerminalApp::Command> ExpandCommand(winrt::com_ptr<Command> expandable,
                                                                      const std::vector<::TerminalApp::Profile>& profiles,
                                                                      std::vector<::TerminalApp::SettingsLoadWarnings>& warnings);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        OBSERVABLE_GETSET_PROPERTY(winrt::hstring, Name, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(winrt::hstring, IconPath, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(winrt::TerminalApp::ActionAndArgs, Action, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(winrt::hstring, KeyChordText, _PropertyChangedHandlers);

        GETSET_PROPERTY(ExpandCommandType, IterateOn, ExpandCommandType::None);

    private:
        Json::Value _originalJson;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(Command);
}
