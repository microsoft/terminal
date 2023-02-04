/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- EnumMappings.h

Abstract:
- Contains mappings from enum name to enum value for the enum types used in our settings.
  These are mainly used in the settings UI for data binding so that we can display
  all possible choices in the UI for each setting/enum.

Author(s):
- Leon Liang - October 2020

--*/
#pragma once

#include "EnumMappings.g.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct EnumMappings : EnumMappingsT<EnumMappings>
    {
    public:
        EnumMappings() = default;

        // Global Settings
        static WFC::IMap<winrt::hstring, WUX::ElementTheme> ElementTheme();
        static WFC::IMap<winrt::hstring, NewTabPosition> NewTabPosition();
        static WFC::IMap<winrt::hstring, MUXC::TabViewWidthMode> TabViewWidthMode();
        static WFC::IMap<winrt::hstring, FirstWindowPreference> FirstWindowPreference();
        static WFC::IMap<winrt::hstring, LaunchMode> LaunchMode();
        static WFC::IMap<winrt::hstring, TabSwitcherMode> TabSwitcherMode();
        static WFC::IMap<winrt::hstring, MTControl::CopyFormat> CopyFormat();
        static WFC::IMap<winrt::hstring, WindowingMode> WindowingMode();
        static WFC::IMap<winrt::hstring, MTCore::MatchMode> MatchMode();

        // Profile Settings
        static WFC::IMap<winrt::hstring, CloseOnExitMode> CloseOnExitMode();
        static WFC::IMap<winrt::hstring, Microsoft::Terminal::Control::ScrollbarState> ScrollbarState();
        static WFC::IMap<winrt::hstring, WUXMedia::Stretch> BackgroundImageStretchMode();
        static WFC::IMap<winrt::hstring, MTControl::TextAntialiasingMode> TextAntialiasingMode();
        static WFC::IMap<winrt::hstring, MTCore::CursorStyle> CursorStyle();
        static WFC::IMap<winrt::hstring, uint16_t> FontWeight();
        static WFC::IMap<winrt::hstring, MTSM::IntenseStyle> IntenseTextStyle();
        static WFC::IMap<winrt::hstring, MTCore::AdjustTextMode> AdjustIndistinguishableColors();
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(EnumMappings);
}
