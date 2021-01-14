// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "StringIsNotDesktopConverter.h"
#include "StringIsNotDesktopConverter.g.cpp"
#include "DesktopWallpaperToEmptyStringConverter.g.cpp"

using namespace winrt::Windows;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Text;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Foundation::IInspectable StringIsNotDesktopConverter::Convert(Foundation::IInspectable const& value,
                                                                  Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                                  Foundation::IInspectable const& /* parameter */,
                                                                  hstring const& /* language */)
    {
        // Returns Visible if the string is _not_ "desktopWallpaper", else returns Collapsed
        const auto& name = winrt::unbox_value_or<hstring>(value, L"");
        return winrt::box_value(name != L"desktopWallpaper");
    }

    Foundation::IInspectable StringIsNotDesktopConverter::ConvertBack(Foundation::IInspectable const& /*value*/,
                                                                      Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                                      Foundation::IInspectable const& /*parameter*/,
                                                                      hstring const& /* language */)
    {
        throw hresult_not_implemented();
    }

    Foundation::IInspectable DesktopWallpaperToEmptyStringConverter::Convert(Foundation::IInspectable const& value,
                                                                             Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                                             Foundation::IInspectable const& /* parameter */,
                                                                             hstring const& /* language */)
    {
        // Returns the empty string if the string is "desktopWallpaper", else returns the original value.
        const auto& name = winrt::unbox_value_or<hstring>(value, L"");
        return winrt::box_value(name == L"desktopWallpaper" ? L"" : name);
    }

    Foundation::IInspectable DesktopWallpaperToEmptyStringConverter::ConvertBack(Foundation::IInspectable const& value,
                                                                                 Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                                                 Foundation::IInspectable const& /*parameter*/,
                                                                                 hstring const& /* language */)
    {
        return value;
    }
}
