// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ColorToBrushConverter.h"
#include "ColorToBrushConverter.g.cpp"

using namespace winrt::Windows;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Foundation::IInspectable ColorToBrushConverter::Convert(Foundation::IInspectable const& value,
                                                            Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                            Foundation::IInspectable const& /* parameter */,
                                                            hstring const& /* language */)
    {
        return winrt::box_value(Windows::UI::Xaml::Media::SolidColorBrush(winrt::unbox_value<Windows::UI::Color>(value)));
    }

    Foundation::IInspectable ColorToBrushConverter::ConvertBack(Foundation::IInspectable const& /*value*/,
                                                                Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                                Foundation::IInspectable const& /*parameter*/,
                                                                hstring const& /* language */)
    {
        throw hresult_not_implemented();
    }
}
