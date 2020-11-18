// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "BooleanToVisibilityConverter.h"
#include "BooleanToVisibilityConverter.g.cpp"

using namespace winrt::Windows;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Foundation::IInspectable BooleanToVisibilityConverter::Convert(Foundation::IInspectable const& value,
                                                          Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                          Foundation::IInspectable const& /* parameter */,
                                                          hstring const& /* language */)
    {
        const auto isChecked{ winrt::unbox_value<bool>(value) };
        return winrt::box_value(isChecked ? Visibility::Visible : Visibility::Collapsed);
    }

    Foundation::IInspectable BooleanToVisibilityConverter::ConvertBack(Foundation::IInspectable const& /*value*/,
                                                              Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                              Foundation::IInspectable const& /*parameter*/,
                                                              hstring const& /* language */)
    {
        throw hresult_not_implemented();
    }
}
