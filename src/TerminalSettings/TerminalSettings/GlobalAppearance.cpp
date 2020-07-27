#include "pch.h"
#include "GlobalAppearance.h"
#include "GlobalAppearance.g.cpp"

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::SettingsControl::implementation
{
    GlobalAppearance::GlobalAppearance()
    {
        InitializeComponent();
    }

    void GlobalAppearance::ClickHandler(IInspectable const&, RoutedEventArgs const&)
    {
    }
}
