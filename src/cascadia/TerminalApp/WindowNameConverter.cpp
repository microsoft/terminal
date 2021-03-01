// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "WindowNameConverter.h"

#include "WindowIdConverter.g.cpp"
#include "WindowNameConverter.g.cpp"
#include <LibraryResources.h>

using namespace winrt::Windows;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Text;

namespace winrt::TerminalApp::implementation
{
    Foundation::IInspectable WindowIdConverter::Convert(Foundation::IInspectable const& value,
                                                        Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                        Foundation::IInspectable const& /* parameter */,
                                                        hstring const& /* language */)
    {
        // Returns Visible if the string is _not_ "desktopWallpaper", else returns Collapsed
        const auto& id = winrt::unbox_value_or<uint64_t>(value, 0);
        return winrt::box_value(fmt::format(std::wstring_view(RS_(L"WindowIdPrefix")), id));
    }

    Foundation::IInspectable WindowIdConverter::ConvertBack(Foundation::IInspectable const& /*value*/,
                                                            Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                            Foundation::IInspectable const& /*parameter*/,
                                                            hstring const& /* language */)
    {
        throw hresult_not_implemented();
    }

    Foundation::IInspectable WindowNameConverter::Convert(Foundation::IInspectable const& value,
                                                          Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                          Foundation::IInspectable const& /* parameter */,
                                                          hstring const& /* language */)
    {
        const auto& name = winrt::unbox_value_or<hstring>(value, L"");
        return winrt::box_value(name.empty() ? L"<unnamed-window>" : name);
    }

    Foundation::IInspectable WindowNameConverter::ConvertBack(Foundation::IInspectable const& /*value*/,
                                                              Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                              Foundation::IInspectable const& /*parameter*/,
                                                              hstring const& /* language */)
    {
        throw hresult_not_implemented();
    }
}
