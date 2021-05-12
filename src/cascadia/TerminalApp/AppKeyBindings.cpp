// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AppKeyBindings.h"

#include "AppKeyBindings.g.cpp"

using namespace winrt::Microsoft::Terminal;
using namespace winrt::TerminalApp;
using namespace winrt::Microsoft::Terminal::Control;

namespace winrt::TerminalApp::implementation
{
    bool AppKeyBindings::TryKeyChord(const KeyChord& kc)
    {
        if (const auto cmd{ _actionMap.GetActionByKeyChord(kc) })
        {
            return _dispatch.DoAction(cmd.ActionAndArgs());
        }
        return false;
    }

    void AppKeyBindings::SetDispatch(const winrt::TerminalApp::ShortcutActionDispatch& dispatch)
    {
        _dispatch = dispatch;
    }

    void AppKeyBindings::SetActionMap(const winrt::Microsoft::Terminal::Settings::Model::IActionMapView& actionMap)
    {
        _actionMap = actionMap;
    }
}
