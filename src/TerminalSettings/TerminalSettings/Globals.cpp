#include "pch.h"
#include "Globals.h"
#include "Globals.g.cpp"

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::SettingsControl::implementation
{
    Globals::Globals()
    {
        InitializeComponent();
    }

    int32_t Globals::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void Globals::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }

    void Globals::ClickHandler(IInspectable const&, RoutedEventArgs const&)
    {

    }
}
