// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Utils.h"
#include "JsonUtils.h"
#include "../../types/inc/Utils.hpp"

void TerminalApp::JsonUtils::GetOptionalColor(const Json::Value& json,
                                              std::string_view key,
                                              std::optional<uint32_t>& target)
{
    const auto conversionFn = [](const Json::Value& value) -> uint32_t {
        return ::Microsoft::Console::Utils::ColorFromHexString(value.asString());
    };
    GetOptionalValue(json,
                     key,
                     target,
                     conversionFn);
}

void TerminalApp::JsonUtils::GetOptionalString(const Json::Value& json,
                                               std::string_view key,
                                               std::optional<std::wstring>& target)
{
    const auto conversionFn = [](const Json::Value& value) -> std::wstring {
        return GetWstringFromJson(value);
    };
    GetOptionalValue(json,
                     key,
                     target,
                     conversionFn);
}

void TerminalApp::JsonUtils::GetOptionalGuid(const Json::Value& json,
                                             std::string_view key,
                                             std::optional<GUID>& target)
{
    const auto conversionFn = [](const Json::Value& value) -> GUID {
        return ::Microsoft::Console::Utils::GuidFromString(GetWstringFromJson(value));
    };
    GetOptionalValue(json,
                     key,
                     target,
                     conversionFn);
}

void TerminalApp::JsonUtils::GetOptionalDouble(const Json::Value& json,
                                               std::string_view key,
                                               std::optional<double>& target)
{
    const auto conversionFn = [](const Json::Value& value) -> double {
        return value.asFloat();
    };
    GetOptionalValue(json,
                     key,
                     target,
                     conversionFn);
}

void TerminalApp::JsonUtils::GetInt(const Json::Value& json,
                                    std::string_view key,
                                    int& target)
{
    if (json.isMember(JsonKey(key)))
    {
        if (auto jsonVal{ json[JsonKey(key)] })
        {
            if (jsonVal.isInt())
            {
                target = jsonVal.asInt();
            }
        }
    }
}

void TerminalApp::JsonUtils::GetUInt(const Json::Value& json,
                                     std::string_view key,
                                     uint32_t& target)
{
    if (json.isMember(JsonKey(key)))
    {
        if (auto jsonVal{ json[JsonKey(key)] })
        {
            if (jsonVal.isUInt())
            {
                target = jsonVal.asUInt();
            }
        }
    }
}

void TerminalApp::JsonUtils::GetDouble(const Json::Value& json,
                                       std::string_view key,
                                       double& target)
{
    if (json.isMember(JsonKey(key)))
    {
        if (auto jsonVal{ json[JsonKey(key)] })
        {
            if (jsonVal.isNumeric())
            {
                target = jsonVal.asFloat();
            }
        }
    }
}

void TerminalApp::JsonUtils::GetBool(const Json::Value& json,
                                     std::string_view key,
                                     bool& target)
{
    if (json.isMember(JsonKey(key)))
    {
        if (auto jsonVal{ json[JsonKey(key)] })
        {
            if (jsonVal.isBool())
            {
                target = jsonVal.asBool();
            }
        }
    }
}
