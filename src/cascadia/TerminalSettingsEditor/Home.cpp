// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Home.h"
#include "Home.g.cpp"
#include "MainPage.h"

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::SettingsControl::implementation
{
    Home::Home()
    {
        m_homeViewModel = winrt::make<SettingsControl::implementation::SettingsControlViewModel>();
        InitializeComponent();

        HomeViewModel().HomeGridItems().Append(winrt::make<SettingsControl::implementation::HomeGridItem>(L"Startup", L"Launch_Nav"));
        HomeViewModel().HomeGridItems().Append(winrt::make<SettingsControl::implementation::HomeGridItem>(L"Interaction", L"Interaction_Nav"));
        HomeViewModel().HomeGridItems().Append(winrt::make<SettingsControl::implementation::HomeGridItem>(L"Rendering", L"Rendering_Nav"));
        HomeViewModel().HomeGridItems().Append(winrt::make<SettingsControl::implementation::HomeGridItem>(L"Global appearance", L"GlobalAppearance_Nav"));
        HomeViewModel().HomeGridItems().Append(winrt::make<SettingsControl::implementation::HomeGridItem>(L"Color schemes", L"ColorSchemes_Nav"));
        HomeViewModel().HomeGridItems().Append(winrt::make<SettingsControl::implementation::HomeGridItem>(L"Global profile settings", L"GlobalProfile_Nav"));
        HomeViewModel().HomeGridItems().Append(winrt::make<SettingsControl::implementation::HomeGridItem>(L"Keyboard", L"Keyboard_Nav"));
    }

    void Home::HomeGridItemClickHandler(IInspectable const&, Controls::ItemClickEventArgs const& args)
    {
        auto clickedItemContainer = args.ClickedItem().as<HomeGridItem>();
        hstring tag = clickedItemContainer->PageTag();
        MainPage mainPage;
        mainPage.Navigate(frame(), tag);
    }

    SettingsControl::SettingsControlViewModel Home::HomeViewModel()
    {
        return m_homeViewModel;
    }

}
