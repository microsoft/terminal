// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "StringIsEmptyConverter.h"
#include "StringIsEmptyConverter.g.cpp"

using namespace winrt::Windows;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Text;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Foundation::IInspectable StringIsEmptyConverter::Convert(Foundation::IInspectable const& value,
                                                             Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                             Foundation::IInspectable const& /* parameter */,
                                                             hstring const& /* language */)
    {
        const auto& name = winrt::unbox_value_or<hstring>(value, L"");
        return winrt::box_value(name.empty() ? Visibility::Collapsed : Visibility::Visible);
    }

    Foundation::IInspectable StringIsEmptyConverter::ConvertBack(Foundation::IInspectable const& /*value*/,
                                                                 Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                                 Foundation::IInspectable const& /*parameter*/,
                                                                 hstring const& /* language */)
    {
        throw hresult_not_implemented();
    }
}
