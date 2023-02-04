// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "KeyChordSerialization.g.h"
#include "JsonUtils.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct KeyChordSerialization
    {
        KeyChordSerialization() = default;

        static MTControl::KeyChord FromString(const winrt::hstring& str);
        static winrt::hstring ToString(const MTControl::KeyChord& chord);
    };
}

template<>
struct Microsoft::Terminal::Settings::Model::JsonUtils::ConversionTrait<MTControl::KeyChord>
{
    MTControl::KeyChord FromJson(const Json::Value& json);
    bool CanConvert(const Json::Value& json);
    Json::Value ToJson(const MTControl::KeyChord& val);
    std::string TypeDescription() const;
};

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    // C++/WinRT generates a constructor even though one is not specified in the IDL
    BASIC_FACTORY(KeyChordSerialization);
}
