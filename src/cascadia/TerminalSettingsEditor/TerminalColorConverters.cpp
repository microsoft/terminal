// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TerminalColorConverters.h"
#include "ColorToBrushConverter.g.cpp"
#include "ColorToStringConverter.g.cpp"

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
}
