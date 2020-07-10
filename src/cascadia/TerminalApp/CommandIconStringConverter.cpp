#include "pch.h"
#include "Utils.h"
#include "CommandIconStringConverter.h"
#include "CommandIconStringConverter.g.cpp"

using namespace winrt::Windows;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::TerminalApp::implementation
{
    Foundation::IInspectable CommandIconStringConverter::Convert(Foundation::IInspectable const& value,
                                                                Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                                Foundation::IInspectable const& /* parameter */,
                                                                hstring const& /* language */)
    {
        const auto& str = winrt::unbox_value_or<hstring>(value, L"");

        auto icon = GetColoredIcon<Controls::IconSource>(str);

        if (!icon)
        {
            winrt::Windows::UI::Xaml::Controls::FontIconSource icon;
            icon.Glyph(L"\xE970");
            icon.FontFamily(winrt::Windows::UI::Xaml::Media::FontFamily{ L"Segoe MDL2 Assets" });
            icon.FontSize(10);

            return icon;
        }
        else
        {
            return GetColoredIcon<winrt::Windows::UI::Xaml::Controls::IconSource>(str);
        }
    }

    // unused for one-way bindings
    Foundation::IInspectable CommandIconStringConverter::ConvertBack(Foundation::IInspectable const& /* value */,
                                                                    Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                                    Foundation::IInspectable const& /* parameter */,
                                                                    hstring const& /* language */)
    {
        throw hresult_not_implemented();
    }
}
