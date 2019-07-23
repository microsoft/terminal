// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Action.g.h"
#include "..\inc\cppwinrt_utils.h"

namespace winrt::TerminalApp::implementation
{
    struct Action : ActionT<Action>
    {
        Action() = default;

        // DECLARE_GETSET_PROPERTY(winrt::hstring, Name);
        DECLARE_GETSET_PROPERTY(winrt::hstring, IconPath);
        DECLARE_GETSET_PROPERTY(winrt::TerminalApp::ShortcutAction, Command);

        DECLARE_EVENT(PropertyChanged, _propertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        DEFINE_OBSERVABLE_GETSET_PROPERTY(winrt::hstring, Name, _propertyChanged);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct Action : ActionT<Action, implementation::Action>
    {
    };
}
