#include "pch.h"
#include "ColorSchemes.h"
#if __has_include("ColorSchemes.g.cpp")
#include "ColorSchemes.g.cpp"
#endif

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::SettingsControl::implementation
{
    ColorSchemes::ColorSchemes()
    {
        InitializeComponent();
    }

    int32_t ColorSchemes::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void ColorSchemes::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }

    void ColorSchemes::ClickHandler(IInspectable const&, RoutedEventArgs const&)
    {
        //Button().Content(box_value(L"Clicked"));
    }
}
