// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AppearanceConfig.h"
#include "AppearanceConfig.g.cpp"
#include "TerminalSettingsSerializationHelpers.h"
#include "JsonUtils.h"

using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;

static constexpr std::string_view ForegroundKey{ "foreground" };
static constexpr std::string_view BackgroundKey{ "background" };
static constexpr std::string_view SelectionBackgroundKey{ "selectionBackground" };
static constexpr std::string_view CursorColorKey{ "cursorColor" };
static constexpr std::string_view CursorShapeKey{ "cursorShape" };
static constexpr std::string_view BackgroundImageKey{ "backgroundImage" };
static constexpr std::string_view ColorSchemeKey{ "colorScheme" };
static constexpr std::string_view BackgroundImageOpacityKey{ "backgroundImageOpacity" };
static constexpr std::string_view BackgroundImageStretchModeKey{ "backgroundImageStretchMode" };
static constexpr std::string_view BackgroundImageAlignmentKey{ "backgroundImageAlignment" };

AppearanceConfig::AppearanceConfig()
{
}

void AppearanceConfig::LayerJson(const Json::Value& json)
{
    JsonUtils::GetValueForKey(json, ForegroundKey, _Foreground);
    JsonUtils::GetValueForKey(json, BackgroundKey, _Background);
    JsonUtils::GetValueForKey(json, SelectionBackgroundKey, _SelectionBackground);
    JsonUtils::GetValueForKey(json, CursorColorKey, _CursorColor);
    JsonUtils::GetValueForKey(json, ColorSchemeKey, _ColorSchemeName);
    JsonUtils::GetValueForKey(json, CursorShapeKey, _CursorShape);
    JsonUtils::GetValueForKey(json, BackgroundImageKey, _BackgroundImagePath);
    JsonUtils::GetValueForKey(json, BackgroundImageOpacityKey, _BackgroundImageOpacity);
    JsonUtils::GetValueForKey(json, BackgroundImageStretchModeKey, _BackgroundImageStretchMode);
    JsonUtils::GetValueForKey(json, BackgroundImageAlignmentKey, _BackgroundImageAlignment);
}

// NOTE: This is just placeholder for now, eventually the path will no longer be expanded in the settings model
winrt::hstring AppearanceConfig::ExpandedBackgroundImagePath()
{
    const auto path{ BackgroundImagePath() };
    if (path.empty())
    {
        return path;
    }
    // checks if the user would like to copy their desktop wallpaper
    // if so, replaces the path with the desktop wallpaper's path
    else if (path == L"desktopWallpaper")
    {
        WCHAR desktopWallpaper[MAX_PATH];

        // "The returned string will not exceed MAX_PATH characters" as of 2020
        if (SystemParametersInfo(SPI_GETDESKWALLPAPER, MAX_PATH, desktopWallpaper, SPIF_UPDATEINIFILE))
        {
            return winrt::hstring{ (desktopWallpaper) };
        }
        else
        {
            return winrt::hstring{ L"" };
        }
    }
    else
    {
        return winrt::hstring{ wil::ExpandEnvironmentStringsW<std::wstring>(path.c_str()) };
    }
}
