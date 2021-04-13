// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "NullCheckConverter.h"
#include "NullCheckToVisibilityConverter.g.cpp"

using namespace winrt::Windows;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Text;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Foundation::IInspectable NullCheckToVisibilityConverter::Convert(Foundation::IInspectable const& value,
                                                                     Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                                     Foundation::IInspectable const& /* parameter */,
                                                                     hstring const& /* language */)
    {
        return box_value(value ? Visibility::Visible : Visibility::Collapsed);
    }

    Foundation::IInspectable NullCheckToVisibilityConverter::ConvertBack(Foundation::IInspectable const& /*value*/,
                                                                         Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                                         Foundation::IInspectable const& /*parameter*/,
                                                                         hstring const& /* language */)
    {
        throw hresult_not_implemented();
    }
}
