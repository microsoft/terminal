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

#define DEFINE_ENUM_MAP(type, name)                                                          \
    winrt::Windows::Foundation::Collections::IMap<winrt::hstring, type> EnumMappings::name() \
    {                                                                                        \
        static IMap<winrt::hstring, type> enumMap = []() {                                   \
            auto map = single_threaded_map<winrt::hstring, type>();                          \
            for (auto [enumStr, enumVal] : JsonUtils::ConversionTrait<type>::mappings)       \
            {                                                                                \
                map.Insert(winrt::to_hstring(enumStr), enumVal);                             \
            }                                                                                \
            return map;                                                                      \
        }();                                                                                 \
        return enumMap;                                                                      \
    }

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    // Global Settings
    DEFINE_ENUM_MAP(winrt::Windows::UI::Xaml::ElementTheme, ElementTheme);
    DEFINE_ENUM_MAP(winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode, TabViewWidthMode);
    DEFINE_ENUM_MAP(Model::LaunchMode, LaunchMode);
    DEFINE_ENUM_MAP(Model::TabSwitcherMode, TabSwitcherMode);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Control::CopyFormat, CopyFormat);
    DEFINE_ENUM_MAP(Model::WindowingMode, WindowingMode);

    // Profile Settings
    DEFINE_ENUM_MAP(Model::CloseOnExitMode, CloseOnExitMode);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Control::ScrollbarState, ScrollbarState);
    DEFINE_ENUM_MAP(Windows::UI::Xaml::Media::Stretch, BackgroundImageStretchMode);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Control::TextAntialiasingMode, TextAntialiasingMode);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Core::CursorStyle, CursorStyle);

    // FontWeight is special because the JsonUtils::ConversionTrait for it
    // creates a FontWeight object, but we need to use the uint16_t value.
    winrt::Windows::Foundation::Collections::IMap<winrt::hstring, uint16_t> EnumMappings::FontWeight()
    {
        static IMap<winrt::hstring, uint16_t> enumMap = []() {
            auto map = single_threaded_map<winrt::hstring, uint16_t>();
            for (auto [enumStr, enumVal] : JsonUtils::ConversionTrait<Windows::UI::Text::FontWeight>::mappings)
            {
                map.Insert(winrt::to_hstring(enumStr), enumVal);
            }
            return map;
        }();
        return enumMap;
    }

    // BellStyle is special because we deprecated 'visual' for the settings UI but
    // want to allow it for legacy users
    winrt::Windows::Foundation::Collections::IMap<winrt::hstring, Model::BellStyle> EnumMappings::BellStyle()
    {
        static IMap<winrt::hstring, Model::BellStyle> enumMap = []() {
            auto map = single_threaded_map<winrt::hstring, Model::BellStyle>();
            for (auto [enumStr, enumVal] : JsonUtils::ConversionTrait<Model::BellStyle>::mappings)
            {
                const auto enumHStr = winrt::to_hstring(enumStr);
                // skip visual
                if (enumHStr != L"visual")
                {
                    map.Insert(enumHStr, enumVal);
                }
            }
            return map;
        }();
        return enumMap;
    }
}
