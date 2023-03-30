// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Converters.g.h"

namespace winrt::Microsoft::Terminal::UI::implementation
{
    struct Converters
    {
        Converters() = default;
        static winrt::hstring AppendPercentageSign(double value);
        static winrt::Windows::UI::Text::FontWeight DoubleToFontWeight(double value);
        static winrt::Windows::UI::Xaml::Media::SolidColorBrush ColorToBrush(const winrt::Windows::UI::Color& color);
        static double FontWeightToDouble(const winrt::Windows::UI::Text::FontWeight& fontWeight);
        static bool InvertBoolean(bool value);
        static winrt::Windows::UI::Xaml::Visibility InvertedBooleanToVisibility(bool value);
        static double MaxValueFromPaddingString(const winrt::hstring& paddingString);
        static int PercentageToPercentageValue(double value);
        static double PercentageValueToPercentage(double value);
        static bool StringsAreNotEqual(const winrt::hstring& expected, const winrt::hstring& actual);
        static winrt::Windows::UI::Xaml::Visibility StringNotEmptyToVisibility(const winrt::hstring& value);
        static winrt::hstring StringOrEmptyIfPlaceholder(const winrt::hstring& placeholder, const winrt::hstring& value);
    };
}

namespace winrt::Microsoft::Terminal::UI::factory_implementation
{
    struct Converters : ConvertersT<Converters, implementation::Converters>
    {
    };
}
