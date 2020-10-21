// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AppKeyBindings.h"

#include "AppKeyBindings.g.cpp"

using namespace winrt::Microsoft::Terminal;
using namespace winrt::TerminalApp;
using namespace winrt::Microsoft::Terminal::TerminalControl;

namespace winrt::TerminalApp::implementation
{
    bool AppKeyBindings::TryKeyChord(const KeyChord& kc)
    {
        const auto actionAndArgs = _keymap.TryLookup(kc);
        if (actionAndArgs)
        {
            return _dispatch.DoAction(actionAndArgs);
        }
        return false;
    }

    void AppKeyBindings::SetDispatch(const winrt::TerminalApp::ShortcutActionDispatch& dispatch)
    {
        _dispatch = dispatch;
    }

    void AppKeyBindings::SetKeyMapping(const winrt::Microsoft::Terminal::Settings::Model::KeyMapping& keymap)
    {
        _keymap = keymap;
    }
}
