// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"
#include "ActionAndArgs.h"
#include "JsonUtils.h"
#include "TerminalSettingsSerializationHelpers.h"

#include "EnumMappings.h"
#include "EnumMappings.g.cpp"

using namespace winrt;
using namespace WFC;
using namespace ::Microsoft::Terminal::Settings::Model;

#define DEFINE_ENUM_MAP(type, name)                                                    \
    WFC::IMap<winrt::hstring, type> EnumMappings::name()                               \
    {                                                                                  \
        static IMap<winrt::hstring, type> enumMap = []() {                             \
            auto map = single_threaded_map<winrt::hstring, type>();                    \
            for (auto [enumStr, enumVal] : JsonUtils::ConversionTrait<type>::mappings) \
            {                                                                          \
                map.Insert(winrt::to_hstring(enumStr), enumVal);                       \
            }                                                                          \
            return map;                                                                \
        }();                                                                           \
        return enumMap;                                                                \
    }

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    // Global Settings
    DEFINE_ENUM_MAP(WUX::ElementTheme, ElementTheme);
    DEFINE_ENUM_MAP(Model::NewTabPosition, NewTabPosition);
    DEFINE_ENUM_MAP(MUXC::TabViewWidthMode, TabViewWidthMode);
    DEFINE_ENUM_MAP(Model::FirstWindowPreference, FirstWindowPreference);
    DEFINE_ENUM_MAP(Model::LaunchMode, LaunchMode);
    DEFINE_ENUM_MAP(Model::TabSwitcherMode, TabSwitcherMode);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Control::CopyFormat, CopyFormat);
    DEFINE_ENUM_MAP(Model::WindowingMode, WindowingMode);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Core::MatchMode, MatchMode);

    // Profile Settings
    DEFINE_ENUM_MAP(Model::CloseOnExitMode, CloseOnExitMode);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Control::ScrollbarState, ScrollbarState);
    DEFINE_ENUM_MAP(WUXMedia::Stretch, BackgroundImageStretchMode);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Control::TextAntialiasingMode, TextAntialiasingMode);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Core::CursorStyle, CursorStyle);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Settings::Model::IntenseStyle, IntenseTextStyle);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Core::AdjustTextMode, AdjustIndistinguishableColors);

    // FontWeight is special because the JsonUtils::ConversionTrait for it
    // creates a FontWeight object, but we need to use the uint16_t value.
    WFC::IMap<winrt::hstring, uint16_t> EnumMappings::FontWeight()
    {
        static auto enumMap = []() {
            auto map = single_threaded_map<winrt::hstring, uint16_t>();
            for (auto [enumStr, enumVal] : JsonUtils::ConversionTrait<WUT::FontWeight>::mappings)
            {
                map.Insert(winrt::to_hstring(enumStr), enumVal);
            }
            return map;
        }();
        return enumMap;
    }
}
