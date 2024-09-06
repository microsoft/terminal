// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TerminalColorConverters.h"
#include "ColorToBrushConverter.g.cpp"
#include "ColorToStringConverter.g.cpp"
#include "BooleanToVisibilityConverter.g.cpp"

using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{

    Windows::Foundation::IInspectable ColorToBrushConverter::Convert(Windows::Foundation::IInspectable const& value, Windows::UI::Xaml::Interop::TypeName const& /*targetType*/, Windows::Foundation::IInspectable const& /*parameter*/, hstring const& /*language*/)
    {
        const auto color = value.as<Windows::UI::Color>();
        return Microsoft::Terminal::UI::Converters::ColorToBrush(color);
    }

    Windows::Foundation::IInspectable ColorToBrushConverter::ConvertBack(Windows::Foundation::IInspectable const& /*value*/, Windows::UI::Xaml::Interop::TypeName const& /*targetType*/, Windows::Foundation::IInspectable const& /*parameter*/, hstring const& /*language*/)
    {
        throw hresult_not_implemented();
    }

    Windows::Foundation::IInspectable ColorToStringConverter::Convert(Windows::Foundation::IInspectable const& value, Windows::UI::Xaml::Interop::TypeName const& /*targetType*/, Windows::Foundation::IInspectable const& /*parameter*/, hstring const& /*language*/)
    {
        const auto color = value.as<Windows::UI::Color>();
        return winrt::box_value(fmt::format(FMT_COMPILE(L"#{:02X}{:02X}{:02X}"), color.R, color.G, color.B));
    }

    Windows::Foundation::IInspectable ColorToStringConverter::ConvertBack(Windows::Foundation::IInspectable const& /*value*/, Windows::UI::Xaml::Interop::TypeName const& /*targetType*/, Windows::Foundation::IInspectable const& /*parameter*/, hstring const& /*language*/)
    {
        throw hresult_not_implemented();
    }

    Windows::Foundation::IInspectable BooleanToVisibilityConverter::Convert(Windows::Foundation::IInspectable const& value, Windows::UI::Xaml::Interop::TypeName const& /*targetType*/, Windows::Foundation::IInspectable const& /*parameter*/, hstring const& /*language*/)
    {
        return winrt::box_value(winrt::unbox_value<bool>(value) ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed);
    }

    Windows::Foundation::IInspectable BooleanToVisibilityConverter::ConvertBack(Windows::Foundation::IInspectable const& value, Windows::UI::Xaml::Interop::TypeName const& /*targetType*/, Windows::Foundation::IInspectable const& /*parameter*/, hstring const& /*language*/)
    {
        switch (value.as<Windows::UI::Xaml::Visibility>())
        {
        case Windows::UI::Xaml::Visibility::Collapsed:
            return winrt::box_value(false);
        case Windows::UI::Xaml::Visibility::Visible:
        default:
            return winrt::box_value(true);
        }
    }
}
