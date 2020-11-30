// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AppearanceConfig.h"
#include "AppearanceConfig.g.cpp"
#include "TerminalSettingsSerializationHelpers.h"
#include "JsonUtils.h"

using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;

static constexpr std::string_view ForegroundKey{ "foreground" };
static constexpr std::string_view BackgroundKey{ "background" };
static constexpr std::string_view SelectionBackgroundKey{ "selectionBackground" };
static constexpr std::string_view CursorColorKey{ "cursorColor" };
static constexpr std::string_view CursorShapeKey{ "cursorShape" };
static constexpr std::string_view BackgroundImageKey{ "backgroundImage" };
static constexpr std::string_view ColorSchemeKey{ "colorScheme" };

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
}
