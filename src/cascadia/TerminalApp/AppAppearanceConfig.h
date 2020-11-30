// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "AppAppearanceConfig.g.h"
#include "..\inc\cppwinrt_utils.h"
#include <DefaultSettings.h>

namespace winrt::TerminalApp::implementation
{
    struct AppAppearanceConfig : AppAppearanceConfigT<AppAppearanceConfig>
    {
        AppAppearanceConfig() = default;

        GETSET_PROPERTY(hstring, ColorSchemeName);
        GETSET_PROPERTY(uint32_t, DefaultForeground, DEFAULT_FOREGROUND_WITH_ALPHA);
        GETSET_PROPERTY(uint32_t, DefaultBackground, DEFAULT_BACKGROUND_WITH_ALPHA);
        GETSET_PROPERTY(uint32_t, SelectionBackground, DEFAULT_FOREGROUND);
        GETSET_PROPERTY(uint32_t, CursorColor, DEFAULT_CURSOR_COLOR);
        GETSET_PROPERTY(Microsoft::Terminal::TerminalControl::CursorStyle, CursorShape, Microsoft::Terminal::TerminalControl::CursorStyle::Vintage);
        GETSET_PROPERTY(hstring, BackgroundImage);

    private:
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(AppAppearanceConfig);
}
