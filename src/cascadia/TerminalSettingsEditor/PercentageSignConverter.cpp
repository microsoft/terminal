// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "PercentageSignConverter.h"
#include "PercentageSignConverter.g.cpp"

using namespace winrt::Windows;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Foundation::IInspectable PercentageSignConverter::Convert(const Foundation::IInspectable& value,
                                                              const Windows::UI::Xaml::Interop::TypeName& /* targetType */,
                                                              const Foundation::IInspectable& /* parameter */,
                                                              const hstring& /* language */)
    {
        const auto number{ winrt::unbox_value<double>(value) };
        return winrt::box_value(to_hstring((int)number) + L"%");
    }

    Foundation::IInspectable PercentageSignConverter::ConvertBack(const Foundation::IInspectable& /*value*/,
                                                                  const Windows::UI::Xaml::Interop::TypeName& /* targetType */,
                                                                  const Foundation::IInspectable& /*parameter*/,
                                                                  const hstring& /* language */)
    {
        throw hresult_not_implemented();
    }
}
