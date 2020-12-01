// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AppAppearanceConfig.h"

#include "AppAppearanceConfig.g.cpp"

using namespace winrt::Microsoft::Terminal;
using namespace winrt::TerminalApp;
using namespace winrt::Microsoft::Terminal::TerminalControl;

namespace winrt::TerminalApp::implementation
{
    void AppAppearanceConfig::ApplyColorScheme(const Microsoft::Terminal::Settings::Model::ColorScheme& scheme)
    {
        _DefaultForeground = til::color{ scheme.Foreground() };
        _DefaultBackground = til::color{ scheme.Background() };
        _SelectionBackground = til::color{ scheme.SelectionBackground() };
        _CursorColor = til::color{ scheme.CursorColor() };
    }
}
