// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "AppCustomConfig.g.h"
#include "..\inc\cppwinrt_utils.h"
#include <DefaultSettings.h>

namespace winrt::TerminalApp::implementation
{
    struct AppCustomConfig : AppCustomConfigT<AppCustomConfig>
    {
        AppCustomConfig() = default;

        GETSET_PROPERTY(hstring, ColorSchemeName);
        GETSET_PROPERTY(uint32_t, DefaultForeground, DEFAULT_FOREGROUND_WITH_ALPHA);
        GETSET_PROPERTY(uint32_t, DefaultBackground, DEFAULT_BACKGROUND_WITH_ALPHA);
        GETSET_PROPERTY(uint32_t, SelectionBackground, DEFAULT_FOREGROUND);
        GETSET_PROPERTY(uint32_t, CursorColor, DEFAULT_CURSOR_COLOR);
        GETSET_PROPERTY(hstring, BackgroundImage);

    private:
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(AppCustomConfig);
}
