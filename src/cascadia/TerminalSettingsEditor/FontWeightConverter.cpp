// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "FontWeightConverter.h"
#include "FontWeightConverter.g.cpp"

using namespace winrt::Windows;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Text;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Foundation::IInspectable FontWeightConverter::Convert(Foundation::IInspectable const& value,
                                                          Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                          Foundation::IInspectable const& /* parameter */,
                                                          hstring const& /* language */)
    {
        const auto weight{ winrt::unbox_value<FontWeight>(value) };
        return winrt::box_value<double>(weight.Weight);
    }

    Foundation::IInspectable FontWeightConverter::ConvertBack(Foundation::IInspectable const& value,
                                                              Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                              Foundation::IInspectable const& /*parameter*/,
                                                              hstring const& /* language */)
    {
        const auto sliderVal{ winrt::unbox_value<double>(value) };
        FontWeight weight{ base::ClampedNumeric<uint16_t>(sliderVal) };
        return winrt::box_value<FontWeight>(weight);
    }
}
