// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ProfileIcon.h"
#include "JsonUtils.h"
#include "Command.h"
#include "ProfileIcon.g.cpp"

using namespace winrt::Microsoft::Terminal::Control;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    ProfileIcon::ProfileIcon() noexcept
    {
    }

    com_ptr<ProfileIcon> ProfileIcon::FromJson(const Json::Value& json)
    {
        auto result = make_self<ProfileIcon>();
        return result->_layerJson(json) ? result : nullptr;
    }

    bool ProfileIcon::_layerJson(const Json::Value& json)
    {
        const auto FoundDarkMode = JsonUtils::GetValueForKey(json, DarkModeKey, _Dark);
        const auto FoundLightMode = JsonUtils::GetValueForKey(json, LightModeKey, _Light);
        // If one is found we can default to that else fail json
        return FoundDarkMode || FoundLightMode;
    }

    Json::Value ProfileIcon::ToJson(const Model::ProfileIcon& val)
    {
        Json::Value json{ Json::ValueType::objectValue };
        JsonUtils::SetValueForKey(json, DarkModeKey, val.Dark());
        JsonUtils::SetValueForKey(json, LightModeKey, val.Light());
        return json;
    }
}
