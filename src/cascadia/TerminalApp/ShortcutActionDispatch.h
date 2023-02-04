// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ShortcutActionDispatch.g.h"
#include "../TerminalSettingsModel/AllShortcutActions.h"

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class SettingsTests;
    class KeyBindingsTests;
}

#define DECLARE_ACTION(action) TYPED_EVENT(action, MTApp::ShortcutActionDispatch, MTSM::ActionEventArgs);

namespace winrt::TerminalApp::implementation
{
    struct ShortcutActionDispatch : ShortcutActionDispatchT<ShortcutActionDispatch>
    {
        ShortcutActionDispatch() = default;

        bool DoAction(const MTSM::ActionAndArgs& actionAndArgs);

#define ON_ALL_ACTIONS(action) DECLARE_ACTION(action);
        ALL_SHORTCUT_ACTIONS
#undef ON_ALL_ACTIONS

    private:
        friend class TerminalAppLocalTests::SettingsTests;
        friend class TerminalAppLocalTests::KeyBindingsTests;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(ShortcutActionDispatch);
}
