#include "pch.h"
#include "Launch.h"
#include "Launch.g.cpp"

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::SettingsControl::implementation
{
    Launch::Launch()
    {
        InitializeComponent();
    }

    int32_t Launch::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void Launch::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }

    void Launch::ClickHandler(IInspectable const&, RoutedEventArgs const&)
    {
    }
}
