#include "pch.h"
#include "Home.h"
#include "Home.g.cpp"

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::SettingsControl::implementation
{
    Home::Home()
    {
        InitializeComponent();
    }

    void Home::ClickHandler(IInspectable const&, RoutedEventArgs const&)
    {
    }

}
