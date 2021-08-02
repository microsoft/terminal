// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "InvertedBooleanConverter.h"
#include "InvertedBooleanConverter.g.cpp"

using namespace winrt::Windows;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Foundation::IInspectable InvertedBooleanConverter::Convert(Foundation::IInspectable const& value,
                                                               Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                               Foundation::IInspectable const& /* parameter */,
                                                               hstring const& /* language */)
    {
        return winrt::box_value(!winrt::unbox_value<bool>(value));
    }

    Foundation::IInspectable InvertedBooleanConverter::ConvertBack(Foundation::IInspectable const& value,
                                                                   Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                                   Foundation::IInspectable const& /*parameter*/,
                                                                   hstring const& /* language */)
    {
        return winrt::box_value(!winrt::unbox_value<bool>(value));
    }
}
