#include "pch.h"
#include "Rendering.h"
#include "Rendering.g.cpp"

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::SettingsControl::implementation
{
    Rendering::Rendering()
    {
        InitializeComponent();
    }

    void Rendering::ClickHandler(IInspectable const&, RoutedEventArgs const&)
    {
    }
}
