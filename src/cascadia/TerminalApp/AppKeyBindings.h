// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "AppKeyBindings.g.h"
#include "ShortcutActionDispatch.h"
#include "..\inc\cppwinrt_utils.h"

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class SettingsTests;
    class KeyBindingsTests;
    class TestUtils;
}

namespace winrt::TerminalApp::implementation
{
    struct AppKeyBindings : AppKeyBindingsT<AppKeyBindings>
    {
        AppKeyBindings() = default;

        bool TryKeyChord(winrt::Microsoft::Terminal::TerminalControl::KeyChord const& kc);

        void SetDispatch(const winrt::TerminalApp::ShortcutActionDispatch& dispatch);
        void SetKeyMapping(const Microsoft::Terminal::Settings::Model::KeyMapping& keymap);

    private:
        winrt::Microsoft::Terminal::Settings::Model::KeyMapping _keymap{ nullptr };

        winrt::TerminalApp::ShortcutActionDispatch _dispatch{ nullptr };

        friend class TerminalAppLocalTests::SettingsTests;
        friend class TerminalAppLocalTests::KeyBindingsTests;
        friend class TerminalAppLocalTests::TestUtils;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(AppKeyBindings);
}
