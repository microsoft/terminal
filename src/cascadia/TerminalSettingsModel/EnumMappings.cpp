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

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    IMap<hstring, winrt::Windows::UI::Xaml::ElementTheme> EnumMappings::ElementTheme()
    {
        auto map = single_threaded_map<hstring, winrt::Windows::UI::Xaml::ElementTheme>();
        for (auto [enumStr, enumVal] : JsonUtils::ConversionTrait<winrt::Windows::UI::Xaml::ElementTheme>::mappings)
        {
            map.Insert(winrt::to_hstring(enumStr), enumVal);
        }
        return map;
    }
}
