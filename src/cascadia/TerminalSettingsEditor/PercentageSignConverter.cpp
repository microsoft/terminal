// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "PercentageSignConverter.h"
#include "PercentageSignConverter.g.cpp"

using namespace winrt::Windows;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Foundation::IInspectable PercentageSignConverter::Convert(Foundation::IInspectable const& value,
                                                              Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                              Foundation::IInspectable const& /* parameter */,
                                                              hstring const& /* language */)
    {
        const auto number{ winrt::unbox_value<double>(value) };
        return winrt::box_value(to_hstring((int)number) + L"%");
    }

    Foundation::IInspectable PercentageSignConverter::ConvertBack(Foundation::IInspectable const& /*value*/,
                                                                  Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                                  Foundation::IInspectable const& /*parameter*/,
                                                                  hstring const& /* language */)
    {
        throw hresult_not_implemented();
    }
}
