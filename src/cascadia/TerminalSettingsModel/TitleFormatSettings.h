#pragma once

#include "JsonUtils.h"
#include <optional>
#include <type_traits>

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    inline void SaveTitleSetting(Json::Value& json, const char* key, const std::optional<winrt::hstring>& value)
    {
        if (value)
        {
            json[key] = til::u16u8(std::wstring_view{ *value });
        }
    }

    inline void SaveTitleSetting(Json::Value& json, const char* key, const std::optional<std::optional<winrt::hstring>>& value)
    {
        if (value)
        {
            if (*value)
            {
                SaveTitleSetting(json, key, *value);
            }
            else
            {
                json[key] = Json::Value{ Json::nullValue };
            }
        }
    }

    template<typename Storage>
    bool LoadTitleSetting(const Json::Value& json, const char* key, Storage& target, const bool nullable)
    {
        if (!json.isMember(key))
        {
            return true;
        }
        const auto& value = json[key];
        if (!value.isString() && !(nullable && value.isNull()))
        {
            return false;
        }
        if constexpr (std::is_same_v<Storage, std::optional<std::optional<winrt::hstring>>>)
        {
            if (value.isNull())
            {
                target = std::optional<winrt::hstring>{};
                return true;
            }
        }
        ::Microsoft::Terminal::Settings::Model::JsonUtils::GetValueForKey(json, key, target);
        return true;
    }
}