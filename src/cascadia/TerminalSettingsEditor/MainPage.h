// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "MainPage.g.h"
#include "Breadcrumb.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct Breadcrumb : BreadcrumbT<Breadcrumb>
    {
        Breadcrumb(IInspectable tag, winrt::hstring label, BreadcrumbSubPage subPage) :
            _Tag{ tag },
            _Label{ label },
            _SubPage{ subPage } {}

        hstring ToString() { return _Label; }

        WINRT_PROPERTY(IInspectable, Tag);
        WINRT_PROPERTY(winrt::hstring, Label);
        WINRT_PROPERTY(BreadcrumbSubPage, SubPage);
    };

    struct MainPage : MainPageT<MainPage>
    {
        MainPage() = delete;
        MainPage(const MTSM::CascadiaSettings& settings);

        void UpdateSettings(const MTSM::CascadiaSettings& settings);

        void OpenJsonKeyDown(const WF::IInspectable& sender, const WUX::Input::KeyRoutedEventArgs& args);
        void OpenJsonTapped(const WF::IInspectable& sender, const WUX::Input::TappedRoutedEventArgs& args);
        void SettingsNav_Loaded(const WF::IInspectable& sender, const WUX::RoutedEventArgs& args);
        void SettingsNav_ItemInvoked(const MUXC::NavigationView& sender, const MUXC::NavigationViewItemInvokedEventArgs& args);
        void SaveButton_Click(const WF::IInspectable& sender, const WUX::RoutedEventArgs& args);
        void ResetButton_Click(const WF::IInspectable& sender, const WUX::RoutedEventArgs& args);
        void BreadcrumbBar_ItemClicked(const MUXC::BreadcrumbBar& sender, const MUXC::BreadcrumbBarItemClickedEventArgs& args);

        void SetHostingWindow(uint64_t hostingWindow) noexcept;
        bool TryPropagateHostingWindow(IInspectable object) noexcept;
        uint64_t GetHostingWindow() const noexcept;

        WUXMedia::Brush BackgroundBrush();

        WFC::IObservableVector<IInspectable> Breadcrumbs() noexcept;

        TYPED_EVENT(OpenJson, WF::IInspectable, MTSM::SettingsTarget);

    private:
        WFC::IObservableVector<IInspectable> _breadcrumbs;
        MTSM::CascadiaSettings _settingsSource;
        MTSM::CascadiaSettings _settingsClone;

        std::optional<HWND> _hostingHwnd;

        void _InitializeProfilesList();
        void _CreateAndNavigateToNewProfile(const uint32_t index, const MTSM::Profile& profile);
        MUXC::NavigationViewItem _CreateProfileNavViewItem(const Editor::ProfileViewModel& profile);
        void _DeleteProfile(const WF::IInspectable sender, const Editor::DeleteProfileEventArgs& args);
        void _AddProfileHandler(const winrt::guid profileGuid);

        void _SetupProfileEventHandling(const winrt::Microsoft::Terminal::Settings::Editor::ProfileViewModel profile);

        void _PreNavigateHelper();
        void _Navigate(hstring clickedItemTag, BreadcrumbSubPage subPage);
        void _Navigate(const Editor::ProfileViewModel& profile, BreadcrumbSubPage subPage);

        void _UpdateBackgroundForMica();

        winrt::Microsoft::Terminal::Settings::Editor::ColorSchemesPageViewModel _colorSchemesPageVM{ nullptr };

        WUX::Data::INotifyPropertyChanged::PropertyChanged_revoker _profileViewModelChangedRevoker;
        WUX::Data::INotifyPropertyChanged::PropertyChanged_revoker _colorSchemesPageViewModelChangedRevoker;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(MainPage);
}
