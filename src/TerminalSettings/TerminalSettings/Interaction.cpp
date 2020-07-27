#include "pch.h"
#include "Interaction.h"
#include "Interaction.g.cpp"

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::SettingsControl::implementation
{
    Interaction::Interaction()
    {
        InitializeComponent();
    }

    void Interaction::ClickHandler(IInspectable const&, RoutedEventArgs const&)
    {
    }
}
