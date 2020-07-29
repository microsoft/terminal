#include "pch.h"
#include "Home.h"
#include "Home.g.cpp"

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::SettingsControl::implementation
{
    Home::Home()
    {
        m_homeViewModel = winrt::make<SettingsControl::implementation::SettingsControlViewModel>();
        InitializeComponent();

        HomeViewModel().HomeGridItems().Append(winrt::make<SettingsControl::implementation::HomeGridItem>(L"Launch"));
        HomeViewModel().HomeGridItems().Append(winrt::make<SettingsControl::implementation::HomeGridItem>(L"Interaction"));
        HomeViewModel().HomeGridItems().Append(winrt::make<SettingsControl::implementation::HomeGridItem>(L"Rendering"));
        HomeViewModel().HomeGridItems().Append(winrt::make<SettingsControl::implementation::HomeGridItem>(L"Global appearance"));
        HomeViewModel().HomeGridItems().Append(winrt::make<SettingsControl::implementation::HomeGridItem>(L"Color Schemes"));
        HomeViewModel().HomeGridItems().Append(winrt::make<SettingsControl::implementation::HomeGridItem>(L"Global profile settings"));
        HomeViewModel().HomeGridItems().Append(winrt::make<SettingsControl::implementation::HomeGridItem>(L"Keyboard"));
    }

    void Home::ClickHandler(IInspectable const&, RoutedEventArgs const&)
    {

    }

    void Home::OnHomeGridItemClick(IInspectable const& sender, RoutedEventArgs const& args)
    {
        // This doesn't work
        auto gridView = HomeGridView();
        HomeViewModel().HomeGridItem().Title(L"CLICKED");
    }

    SettingsControl::SettingsControlViewModel Home::HomeViewModel()
    {
        return m_homeViewModel;
    }

}
