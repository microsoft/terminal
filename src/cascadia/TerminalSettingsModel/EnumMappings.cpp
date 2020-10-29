// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"
#include "ActionAndArgs.h"
#include "JsonUtils.h"
#include "TerminalSettingsSerializationHelpers.h"

#include "EnumMappings.h"
#include "EnumMappings.g.cpp"

using namespace winrt;
using namespace winrt::Windows::Foundation::Collections;
using namespace ::Microsoft::Terminal::Settings::Model;

#define DEFINE_ENUM_MAP(type, name)                                                \
    IMap<hstring, type> EnumMappings::name()                                       \
    {                                                                              \
        auto map = single_threaded_map<hstring, type>();                           \
        for (auto [enumStr, enumVal] : JsonUtils::ConversionTrait<type>::mappings) \
        {                                                                          \
            map.Insert(winrt::to_hstring(enumStr), enumVal);                       \
        }                                                                          \
        return map;                                                                \
    }

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    // Global Settings
    DEFINE_ENUM_MAP(winrt::Windows::UI::Xaml::ElementTheme, ElementTheme);
    DEFINE_ENUM_MAP(winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode, TabWidthMode);
    DEFINE_ENUM_MAP(Model::LaunchMode, LaunchMode);

    // Profile Settings
    DEFINE_ENUM_MAP(Model::CloseOnExitMode, CloseOnExit);
    DEFINE_ENUM_MAP(Microsoft::Terminal::TerminalControl::ScrollbarState, ScrollState);
    DEFINE_ENUM_MAP(Windows::UI::Xaml::Media::Stretch, BackgroundImageStretchMode);
    DEFINE_ENUM_MAP(Microsoft::Terminal::TerminalControl::TextAntialiasingMode, AntialiasingMode);
    DEFINE_ENUM_MAP(Microsoft::Terminal::TerminalControl::CursorStyle, CursorShape);
    DEFINE_ENUM_MAP(Model::BellStyle, BellStyle);
}
