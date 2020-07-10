// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "JsonUtilsNew.h"
#include <winrt/Microsoft.Terminal.Settings.h>

DEFINE_ENUM_FLAG_OPERATORS(winrt::Microsoft::Terminal::Settings::CopyFormat);

JSON_FLAG_MAPPER(winrt::Microsoft::Terminal::Settings::CopyFormat)
{
    JSON_MAPPINGS(5) = {
        pair_type{ "none", AllClear },
        pair_type{ "plain", winrt::Microsoft::Terminal::Settings::CopyFormat::Plain },
        pair_type{ "html", winrt::Microsoft::Terminal::Settings::CopyFormat::HTML },
        pair_type{ "rtf", winrt::Microsoft::Terminal::Settings::CopyFormat::RTF },
        pair_type{ "all", AllSet },
    };

    winrt::Microsoft::Terminal::Settings::CopyFormat FromJson(const Json::Value& json)
    {
        if (json.isBool())
        {
            return json.asBool() ? AllSet : winrt::Microsoft::Terminal::Settings::CopyFormat::Plain;
        }
        return BaseFlagMapper::FromJson(json);
    }

    bool CanConvert(const Json::Value& json)
    {
        return BaseFlagMapper::CanConvert(json) || json.isBool();
    }
};
