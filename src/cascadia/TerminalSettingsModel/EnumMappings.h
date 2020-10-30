// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#pragma once

#include "EnumMappings.g.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct EnumMappings : EnumMappingsT<EnumMappings>
    {
    public:
        EnumMappings() = default;

        // Global Settings
        static winrt::Windows::Foundation::Collections::IMap<winrt::hstring, winrt::Windows::UI::Xaml::ElementTheme> ElementTheme();
        static winrt::Windows::Foundation::Collections::IMap<winrt::hstring, winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode> TabViewWidthMode();
        static winrt::Windows::Foundation::Collections::IMap<winrt::hstring, LaunchMode> LaunchMode();

        // Profile Settings
        static winrt::Windows::Foundation::Collections::IMap<winrt::hstring, CloseOnExitMode> CloseOnExitMode();
        static winrt::Windows::Foundation::Collections::IMap<winrt::hstring, Microsoft::Terminal::TerminalControl::ScrollbarState> ScrollbarState();
        static winrt::Windows::Foundation::Collections::IMap<winrt::hstring, winrt::Windows::UI::Xaml::Media::Stretch> BackgroundImageStretchMode();
        static winrt::Windows::Foundation::Collections::IMap<winrt::hstring, winrt::Microsoft::Terminal::TerminalControl::TextAntialiasingMode> TextAntialiasingMode();
        static winrt::Windows::Foundation::Collections::IMap<winrt::hstring, winrt::Microsoft::Terminal::TerminalControl::CursorStyle> CursorStyle();
        static winrt::Windows::Foundation::Collections::IMap<winrt::hstring, BellStyle> BellStyle();
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(EnumMappings);
}
