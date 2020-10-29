// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#pragma once

#include "EnumMappings.g.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct EnumMappings : EnumMappingsT<EnumMappings>
    {
    public:
        EnumMappings() = default;
        static winrt::Windows::Foundation::Collections::IMap<winrt::Windows::UI::Xaml::ElementTheme, winrt::hstring> ElementTheme();
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(EnumMappings);
}
