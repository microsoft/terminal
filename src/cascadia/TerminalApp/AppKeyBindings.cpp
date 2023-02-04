// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AppKeyBindings.h"

#include "AppKeyBindings.g.cpp"

using namespace winrt::Microsoft::Terminal;
using namespace MTApp;
using namespace MTControl;

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

    bool AppKeyBindings::IsKeyChordExplicitlyUnbound(const KeyChord& kc)
    {
        return _actionMap.IsKeyChordExplicitlyUnbound(kc);
    }

    void AppKeyBindings::SetDispatch(const MTApp::ShortcutActionDispatch& dispatch)
    {
        _dispatch = dispatch;
    }

    void AppKeyBindings::SetActionMap(const MTSM::IActionMapView& actionMap)
    {
        _actionMap = actionMap;
    }
}
