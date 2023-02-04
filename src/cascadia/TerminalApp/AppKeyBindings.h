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

namespace winrt::TerminalApp::implementation
{
    struct AppKeyBindings : AppKeyBindingsT<AppKeyBindings>
    {
        AppKeyBindings() = default;

        bool TryKeyChord(const MTControl::KeyChord& kc);
        bool IsKeyChordExplicitlyUnbound(const MTControl::KeyChord& kc);

        void SetDispatch(const MTApp::ShortcutActionDispatch& dispatch);
        void SetActionMap(const MTSM::IActionMapView& actionMap);

    private:
        MTSM::IActionMapView _actionMap{ nullptr };

        MTApp::ShortcutActionDispatch _dispatch{ nullptr };

        friend class TerminalAppLocalTests::SettingsTests;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(AppKeyBindings);
}
