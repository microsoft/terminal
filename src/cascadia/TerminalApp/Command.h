// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Command.g.h"
#include "..\inc\cppwinrt_utils.h"

namespace winrt::TerminalApp::implementation
{
    struct Command : CommandT<Command>
    {
        Command() = default;

        static winrt::com_ptr<Command> FromJson(const Json::Value& json);
        static void LayerJson(std::vector<winrt::TerminalApp::Command>& commands,
                              const Json::Value& json);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        OBSERVABLE_GETSET_PROPERTY(winrt::hstring, Name, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(winrt::hstring, IconPath, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(winrt::TerminalApp::ActionAndArgs, Action, _PropertyChangedHandlers);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(Command);
}
