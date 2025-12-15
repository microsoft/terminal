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
    DEFINE_ENUM_MAP(Model::NewTabPosition, NewTabPosition);
    DEFINE_ENUM_MAP(winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode, TabViewWidthMode);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Control::DefaultInputScope, DefaultInputScope);
    DEFINE_ENUM_MAP(Model::LaunchMode, LaunchMode);
    DEFINE_ENUM_MAP(Model::TabSwitcherMode, TabSwitcherMode);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Control::CopyFormat, CopyFormat);
    DEFINE_ENUM_MAP(Model::WindowingMode, WindowingMode);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Core::MatchMode, MatchMode);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Control::GraphicsAPI, GraphicsAPI);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Control::TextMeasurement, TextMeasurement);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Control::WarnAboutMultiLinePaste, WarnAboutMultiLinePaste);

    // Profile Settings
    DEFINE_ENUM_MAP(Model::CloseOnExitMode, CloseOnExitMode);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Control::ScrollbarState, ScrollbarState);
    DEFINE_ENUM_MAP(Windows::UI::Xaml::Media::Stretch, BackgroundImageStretchMode);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Control::TextAntialiasingMode, TextAntialiasingMode);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Core::CursorStyle, CursorStyle);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Settings::Model::IntenseStyle, IntenseTextStyle);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Core::AdjustTextMode, AdjustIndistinguishableColors);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Control::PathTranslationStyle, PathTranslationStyle);

    // Actions
    DEFINE_ENUM_MAP(Microsoft::Terminal::Settings::Model::ResizeDirection, ResizeDirection);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Settings::Model::FocusDirection, FocusDirection);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Settings::Model::SplitDirection, SplitDirection);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Settings::Model::SplitType, SplitType);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Settings::Model::SettingsTarget, SettingsTarget);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Settings::Model::MoveTabDirection, MoveTabDirection);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Control::ScrollToMarkDirection, ScrollToMarkDirection);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Settings::Model::CommandPaletteLaunchMode, CommandPaletteLaunchMode);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Settings::Model::SuggestionsSource, SuggestionsSource);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Settings::Model::FindMatchDirection, FindMatchDirection);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Settings::Model::DesktopBehavior, DesktopBehavior);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Settings::Model::MonitorBehavior, MonitorBehavior);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Control::ClearBufferType, ClearBufferType);
    DEFINE_ENUM_MAP(Microsoft::Terminal::Settings::Model::SelectOutputDirection, SelectOutputDirection);

    // FontWeight is special because the JsonUtils::ConversionTrait for it
    // creates a FontWeight object, but we need to use the uint16_t value.
    winrt::Windows::Foundation::Collections::IMap<winrt::hstring, uint16_t> EnumMappings::FontWeight()
    {
        static auto enumMap = []() {
            auto map = single_threaded_map<winrt::hstring, uint16_t>();
            for (auto [enumStr, enumVal] : JsonUtils::ConversionTrait<Windows::UI::Text::FontWeight>::mappings)
            {
                map.Insert(winrt::to_hstring(enumStr), enumVal);
            }
            return map;
        }();
        return enumMap;
    }

    winrt::Windows::Foundation::Collections::IMap<winrt::hstring, Model::FirstWindowPreference> EnumMappings::FirstWindowPreference()
    {
        static auto enumMap = []() {
            auto map = single_threaded_map<winrt::hstring, Model::FirstWindowPreference>();
            for (auto [enumStr, enumVal] : JsonUtils::ConversionTrait<Model::FirstWindowPreference>::mappings)
            {
                // exclude legacy value from enum map
                if (enumStr != "persistedWindowLayout")
                {
                    map.Insert(winrt::to_hstring(enumStr), enumVal);
                }
            }
            return map;
        }();
        return enumMap;
    }
}
