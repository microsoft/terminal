// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "AppKeyBindings.g.h"
#include "ShortcutActionDispatch.h"
#include "../inc/cppwinrt_utils.h"

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class SettingsTests;
}

namespace winrt::TerminalApp::implementation
{
    struct AppKeyBindings : AppKeyBindingsT<AppKeyBindings>
    {
        AppKeyBindings() = default;

        bool TryKeyChord(winrt::Microsoft::Terminal::Control::KeyChord const& kc);

        void SetDispatch(const winrt::TerminalApp::ShortcutActionDispatch& dispatch);
        void SetActionMap(const Microsoft::Terminal::Settings::Model::IActionMapView& actionMap);

    private:
        winrt::Microsoft::Terminal::Settings::Model::IActionMapView _actionMap{ nullptr };

        winrt::TerminalApp::ShortcutActionDispatch _dispatch{ nullptr };

        friend class TerminalAppLocalTests::SettingsTests;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(AppKeyBindings);
}
