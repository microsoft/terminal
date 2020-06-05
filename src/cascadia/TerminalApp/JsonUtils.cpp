// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Utils.h"
#include "JsonUtils.h"
#include "../../types/inc/Utils.hpp"

void TerminalApp::JsonUtils::GetOptionalColor(const Json::Value& json,
                                              std::string_view key,
                                              std::optional<til::color>& target)
{
    const auto conversionFn = [](const Json::Value& value) -> til::color {
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
    const auto validationFn = [](const Json::Value& value) -> bool {
        return value.isNumeric();
    };
    GetOptionalValue(json,
                     key,
                     target,
                     conversionFn,
                     validationFn);
}

void TerminalApp::JsonUtils::GetInt(const Json::Value& json,
                                    std::string_view key,
                                    int& target)
{
    const auto conversionFn = [](const Json::Value& value) -> int {
        return value.asInt();
    };
    const auto validationFn = [](const Json::Value& value) -> bool {
        return value.isInt();
    };
    GetValue(json, key, target, conversionFn, validationFn);
}

void TerminalApp::JsonUtils::GetUInt(const Json::Value& json,
                                     std::string_view key,
                                     uint32_t& target)
{
    const auto conversionFn = [](const Json::Value& value) -> uint32_t {
        return value.asUInt();
    };
    const auto validationFn = [](const Json::Value& value) -> bool {
        return value.isUInt();
    };
    GetValue(json, key, target, conversionFn, validationFn);
}

void TerminalApp::JsonUtils::GetDouble(const Json::Value& json,
                                       std::string_view key,
                                       double& target)
{
    const auto conversionFn = [](const Json::Value& value) -> double {
        return value.asFloat();
    };
    const auto validationFn = [](const Json::Value& value) -> bool {
        return value.isNumeric();
    };
    GetValue(json, key, target, conversionFn, validationFn);
}

void TerminalApp::JsonUtils::GetBool(const Json::Value& json,
                                     std::string_view key,
                                     bool& target)
{
    const auto conversionFn = [](const Json::Value& value) -> bool {
        return value.asBool();
    };
    const auto validationFn = [](const Json::Value& value) -> bool {
        return value.isBool();
    };
    GetValue(json, key, target, conversionFn, validationFn);
}

void TerminalApp::JsonUtils::GetWstring(const Json::Value& json,
                                        std::string_view key,
                                        std::wstring& target)
{
    const auto conversionFn = [](const Json::Value& value) -> std::wstring {
        return GetWstringFromJson(value);
    };
    GetValue(json, key, target, conversionFn, nullptr);
}
