// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "PercentageConverter.h"
#include "PercentageConverter.g.cpp"

using namespace winrt::Windows;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Foundation::IInspectable PercentageConverter::Convert(Foundation::IInspectable const& value,
                                                          Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                          Foundation::IInspectable const& /* parameter */,
                                                          hstring const& /* language */)
    {
        const auto decimal{ winrt::unbox_value<double>(value) };
        const unsigned int number{ base::ClampMul(decimal, 100u) };
        return winrt::box_value<double>(number);
    }

    Foundation::IInspectable PercentageConverter::ConvertBack(Foundation::IInspectable const& value,
                                                              Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                              Foundation::IInspectable const& /*parameter*/,
                                                              hstring const& /* language */)
    {
        const auto number{ winrt::unbox_value<double>(value) };
        const auto decimal{ base::ClampDiv<double, double>(number, 100) };
        return winrt::box_value<double>(decimal);
    }
}
