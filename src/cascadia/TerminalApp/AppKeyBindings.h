// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "AppKeyBindings.g.h"
#include "ShortcutActionDispatch.h"

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class SettingsTests;
}

namespace winrt::Microsoft::Terminal::App::implementation
{
    struct AppKeyBindings : AppKeyBindingsT<AppKeyBindings>
    {
        AppKeyBindings() = default;

        bool TryKeyChord(const winrt::Microsoft::Terminal::Control::KeyChord& kc);
        bool IsKeyChordExplicitlyUnbound(const winrt::Microsoft::Terminal::Control::KeyChord& kc);

        void SetDispatch(const winrt::Microsoft::Terminal::App::ShortcutActionDispatch& dispatch);
        void SetActionMap(const Microsoft::Terminal::Settings::Model::IActionMapView& actionMap);

    private:
        winrt::Microsoft::Terminal::Settings::Model::IActionMapView _actionMap{ nullptr };

        winrt::Microsoft::Terminal::App::ShortcutActionDispatch _dispatch{ nullptr };

        friend class TerminalAppLocalTests::SettingsTests;
    };
}

namespace winrt::Microsoft::Terminal::App::factory_implementation
{
    BASIC_FACTORY(AppKeyBindings);
}
