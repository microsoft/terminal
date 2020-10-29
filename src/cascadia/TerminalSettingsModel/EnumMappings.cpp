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
    IMap<winrt::Windows::UI::Xaml::ElementTheme, hstring> EnumMappings::ElementTheme()
    {
        // use C++11 magic statics to make sure we only do this once.
        static IMap<winrt::Windows::UI::Xaml::ElementTheme, hstring> enumMap = []() {
            auto map = std::map<winrt::Windows::UI::Xaml::ElementTheme, hstring>();
            for (auto [enumStr, enumVal] : JsonUtils::ConversionTrait<winrt::Windows::UI::Xaml::ElementTheme>::mappings)
            {
                map.emplace(enumVal, winrt::to_hstring(enumStr));
            }
            return winrt::single_threaded_map(std::move(map));
        }();

        return enumMap;
    }
}
