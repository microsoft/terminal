// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ColorToHexConverter.h"
#include "ColorToHexConverter.g.cpp"

using namespace winrt::Windows;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Foundation::IInspectable ColorToHexConverter::Convert(Foundation::IInspectable const& value,
                                                          Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                          Foundation::IInspectable const& /* parameter */,
                                                          hstring const& /* language */)
    {
        til::color color{ winrt::unbox_value<winrt::Windows::UI::Color>(value) };
        auto hex = winrt::to_hstring(color.ToHexString().data());
        return winrt::box_value(hex);
    }

    Foundation::IInspectable ColorToHexConverter::ConvertBack(Foundation::IInspectable const& value,
                                                              Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                              Foundation::IInspectable const& /* parameter */,
                                                              hstring const& /* language */)
    {
        auto str = winrt::unbox_value<winrt::hstring>(value);
        std::wstring hex{ str.data() };
        Windows::UI::Color newColor;
        newColor.A = base::checked_cast<uint8_t>(std::stoi(hex.substr(1, 2), nullptr, 16));
        newColor.R = base::checked_cast<uint8_t>(std::stoi(hex.substr(3, 2), nullptr, 16));
        newColor.G = base::checked_cast<uint8_t>(std::stoi(hex.substr(5, 2), nullptr, 16));
        newColor.B = base::checked_cast<uint8_t>(std::stoi(hex.substr(7, 2), nullptr, 16));
        return winrt::box_value(newColor);
    }
}
