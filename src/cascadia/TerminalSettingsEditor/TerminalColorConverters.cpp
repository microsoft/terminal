// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TerminalColorConverters.h"
#include "TerminalColorToBrushConverter.g.cpp"
#include "TerminalColorToStringConverter.g.cpp"

using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{

    Windows::Foundation::IInspectable TerminalColorToBrushConverter::Convert(Windows::Foundation::IInspectable const& value, Windows::UI::Xaml::Interop::TypeName const& /*targetType*/, Windows::Foundation::IInspectable const& /*parameter*/, hstring const& /*language*/)
    {
        if (const auto termColor = value.as<Windows::Foundation::IReference<Microsoft::Terminal::Core::Color>>())
        {
            Windows::UI::Color color{
                .A = 255,
                .R = termColor.Value().R,
                .G = termColor.Value().G,
                .B = termColor.Value().B
            };
            return Microsoft::Terminal::UI::Converters::ColorToBrush(color);
        }
        return nullptr;
    }

    Windows::Foundation::IInspectable TerminalColorToBrushConverter::ConvertBack(Windows::Foundation::IInspectable const& /*value*/, Windows::UI::Xaml::Interop::TypeName const& /*targetType*/, Windows::Foundation::IInspectable const& /*parameter*/, hstring const& /*language*/)
    {
        throw hresult_not_implemented();
    }

    Windows::Foundation::IInspectable TerminalColorToStringConverter::Convert(Windows::Foundation::IInspectable const& value, Windows::UI::Xaml::Interop::TypeName const& /*targetType*/, Windows::Foundation::IInspectable const& /*parameter*/, hstring const& /*language*/)
    {
        if (const auto maybeColor = value.as<Windows::Foundation::IReference<Microsoft::Terminal::Core::Color>>())
        {
            const auto color = maybeColor.Value();
            return winrt::box_value(fmt::format(FMT_COMPILE(L"#{:02X}{:02X}{:02X}"), color.R, color.G, color.B));
        }
        return nullptr;
    }

    Windows::Foundation::IInspectable TerminalColorToStringConverter::ConvertBack(Windows::Foundation::IInspectable const& /*value*/, Windows::UI::Xaml::Interop::TypeName const& /*targetType*/, Windows::Foundation::IInspectable const& /*parameter*/, hstring const& /*language*/)
    {
        throw hresult_not_implemented();
    }
}
