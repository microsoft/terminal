// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AppearanceConfig.h"
#include "AppearanceConfig.g.cpp"
#include "TerminalSettingsSerializationHelpers.h"
#include "JsonUtils.h"

using namespace winrt::Microsoft::Terminal::Control;
using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;

static constexpr std::string_view ForegroundKey{ "foreground" };
static constexpr std::string_view BackgroundKey{ "background" };
static constexpr std::string_view SelectionBackgroundKey{ "selectionBackground" };
static constexpr std::string_view CursorColorKey{ "cursorColor" };
static constexpr std::string_view CursorShapeKey{ "cursorShape" };
static constexpr std::string_view CursorHeightKey{ "cursorHeight" };
static constexpr std::string_view BackgroundImageKey{ "backgroundImage" };
static constexpr std::string_view ColorSchemeKey{ "colorScheme" };
static constexpr std::string_view BackgroundImageOpacityKey{ "backgroundImageOpacity" };
static constexpr std::string_view BackgroundImageStretchModeKey{ "backgroundImageStretchMode" };
static constexpr std::string_view BackgroundImageAlignmentKey{ "backgroundImageAlignment" };
static constexpr std::string_view RetroTerminalEffectKey{ "experimental.retroTerminalEffect" };
static constexpr std::string_view PixelShaderPathKey{ "experimental.pixelShaderPath" };

winrt::Microsoft::Terminal::Settings::Model::implementation::AppearanceConfig::AppearanceConfig(const winrt::weak_ref<Profile> sourceProfile) :
    _sourceProfile(sourceProfile)
{
}

winrt::com_ptr<AppearanceConfig> AppearanceConfig::CopyAppearance(const winrt::com_ptr<AppearanceConfig> source, const winrt::weak_ref<Profile> sourceProfile)
{
    auto appearance{ winrt::make_self<AppearanceConfig>(sourceProfile) };
    auto const sourceAppearance = source.try_as<AppearanceConfig>();
    appearance->_BackgroundImagePath = sourceAppearance->_BackgroundImagePath;
    appearance->_BackgroundImageOpacity = sourceAppearance->_BackgroundImageOpacity;
    appearance->_BackgroundImageStretchMode = sourceAppearance->_BackgroundImageStretchMode;
    appearance->_ColorSchemeName = sourceAppearance->_ColorSchemeName;
    appearance->_Foreground = sourceAppearance->_Foreground;
    appearance->_Background = sourceAppearance->_Background;
    appearance->_SelectionBackground = sourceAppearance->_SelectionBackground;
    appearance->_CursorColor = sourceAppearance->_CursorColor;
    appearance->_CursorShape = sourceAppearance->_CursorShape;
    appearance->_CursorHeight = sourceAppearance->_CursorHeight;
    appearance->_BackgroundImageAlignment = sourceAppearance->_BackgroundImageAlignment;
    appearance->_RetroTerminalEffect = sourceAppearance->_RetroTerminalEffect;
    appearance->_PixelShaderPath = sourceAppearance->_PixelShaderPath;
    return appearance;
}

Json::Value AppearanceConfig::ToJson() const
{
    Json::Value json{ Json::ValueType::objectValue };

    JsonUtils::SetValueForKey(json, ForegroundKey, _Foreground);
    JsonUtils::SetValueForKey(json, BackgroundKey, _Background);
    JsonUtils::SetValueForKey(json, SelectionBackgroundKey, _SelectionBackground);
    JsonUtils::SetValueForKey(json, CursorColorKey, _CursorColor);
    JsonUtils::SetValueForKey(json, ColorSchemeKey, _ColorSchemeName);
    JsonUtils::SetValueForKey(json, CursorHeightKey, _CursorHeight);
    JsonUtils::SetValueForKey(json, CursorShapeKey, _CursorShape);
    JsonUtils::SetValueForKey(json, BackgroundImageKey, _BackgroundImagePath);
    JsonUtils::SetValueForKey(json, BackgroundImageOpacityKey, _BackgroundImageOpacity);
    JsonUtils::SetValueForKey(json, BackgroundImageStretchModeKey, _BackgroundImageStretchMode);
    JsonUtils::SetValueForKey(json, BackgroundImageAlignmentKey, _BackgroundImageAlignment);
    JsonUtils::SetValueForKey(json, RetroTerminalEffectKey, _RetroTerminalEffect);
    JsonUtils::SetValueForKey(json, PixelShaderPathKey, _PixelShaderPath);

    return json;
}

// Method Description:
// - Layer values from the given json object on top of the existing properties
//   of this object. For any keys we're expecting to be able to parse in the
//   given object, we'll parse them and replace our settings with values from
//   the new json object. Properties that _aren't_ in the json object will _not_
//   be replaced.
// - Optional values that are set to `null` in the json object
//   will be set to nullopt.
// - This is similar to Profile::LayerJson but for AppearanceConfig
// Arguments:
// - json: an object which should be a partial serialization of an AppearanceConfig object.
void AppearanceConfig::LayerJson(const Json::Value& json)
{
    JsonUtils::GetValueForKey(json, ForegroundKey, _Foreground);
    JsonUtils::GetValueForKey(json, BackgroundKey, _Background);
    JsonUtils::GetValueForKey(json, SelectionBackgroundKey, _SelectionBackground);
    JsonUtils::GetValueForKey(json, CursorColorKey, _CursorColor);
    JsonUtils::GetValueForKey(json, CursorHeightKey, _CursorHeight);
    JsonUtils::GetValueForKey(json, ColorSchemeKey, _ColorSchemeName);
    JsonUtils::GetValueForKey(json, CursorShapeKey, _CursorShape);
    JsonUtils::GetValueForKey(json, BackgroundImageKey, _BackgroundImagePath);
    JsonUtils::GetValueForKey(json, BackgroundImageOpacityKey, _BackgroundImageOpacity);
    JsonUtils::GetValueForKey(json, BackgroundImageStretchModeKey, _BackgroundImageStretchMode);
    JsonUtils::GetValueForKey(json, BackgroundImageAlignmentKey, _BackgroundImageAlignment);
    JsonUtils::GetValueForKey(json, RetroTerminalEffectKey, _RetroTerminalEffect);
    JsonUtils::GetValueForKey(json, PixelShaderPathKey, _PixelShaderPath);
}

winrt::Microsoft::Terminal::Settings::Model::Profile AppearanceConfig::SourceProfile()
{
    return _sourceProfile.get();
}

// Method Description:
// - Returns this AppearanceConfig's background image path, if one is set, expanding
//   any environment variables in the path, if there are any.
// - Or if "DesktopWallpaper" is set, then gets the path to the desktops wallpaper.
// - This is the same as Profile::ExpandedBackgroundImagePath, but for AppearanceConfig
// - NOTE: This is just placeholder for now, eventually the path will no longer be expanded in the settings model
// Return Value:
// - This profile's expanded background image path / desktops's wallpaper path /the empty string.
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
