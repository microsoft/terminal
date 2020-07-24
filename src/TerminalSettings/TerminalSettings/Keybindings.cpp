#include "pch.h"
#include "Keybindings.h"
#if __has_include("Keybindings.g.cpp")
#include "Keybindings.g.cpp"
#endif

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::SettingsControl::implementation
{
    Keybindings::Keybindings()
    {
        InitializeComponent();
    }

    int32_t Keybindings::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void Keybindings::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }

    void Keybindings::ClickHandler(IInspectable const&, RoutedEventArgs const&)
    {
        //Button().Content(box_value(L"Clicked"));
    }
}
