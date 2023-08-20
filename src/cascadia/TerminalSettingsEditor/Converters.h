// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Converters.g.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct Converters : ConvertersT<Converters>
    {
        static winrt::hstring AppendPercentageSign(double value);
        static winrt::Windows::UI::Text::FontWeight DoubleToFontWeight(double value);
        static winrt::Windows::UI::Xaml::Media::SolidColorBrush ColorToBrush(winrt::Windows::UI::Color color);
        static double FontWeightToDouble(winrt::Windows::UI::Text::FontWeight fontWeight);
        static bool InvertBoolean(bool value);
        static winrt::Windows::UI::Xaml::Visibility InvertedBooleanToVisibility(bool value);
        static double MaxValueFromPaddingString(winrt::hstring paddingString);
        static int PercentageToPercentageValue(double value);
        static double PercentageValueToPercentage(double value);
        static bool StringsAreNotEqual(winrt::hstring expected, winrt::hstring actual);
        static winrt::Windows::UI::Xaml::Visibility StringNotEmptyToVisibility(winrt::hstring value);
        static winrt::hstring StringOrEmptyIfPlaceholder(winrt::hstring placeholder, winrt::hstring value);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Converters);
}
