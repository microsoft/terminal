// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "BooleanToVisibilityConverter.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct BooleanToVisibilityConverter : BooleanToVisibilityConverterT<BooleanToVisibilityConverter>
    {
        BooleanToVisibilityConverter() = default;

        Windows::Foundation::IInspectable Convert(Windows::Foundation::IInspectable const& value,
                                                  Windows::UI::Xaml::Interop::TypeName const& targetType,
                                                  Windows::Foundation::IInspectable const& parameter,
                                                  hstring const& language);

        Windows::Foundation::IInspectable ConvertBack(Windows::Foundation::IInspectable const& value,
                                                      Windows::UI::Xaml::Interop::TypeName const& targetType,
                                                      Windows::Foundation::IInspectable const& parameter,
                                                      hstring const& language);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(BooleanToVisibilityConverter);
}
