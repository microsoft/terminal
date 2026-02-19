// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "MainPage.g.h"
#include "Breadcrumb.g.h"
#include "NavigateToPageArgs.g.h"
#include "Utils.h"
#include "SearchIndex.h"

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

    struct NavigateToPageArgs : NavigateToPageArgsT<NavigateToPageArgs>
    {
    public:
        NavigateToPageArgs(Windows::Foundation::IInspectable viewModel, Editor::IHostedInWindow windowRoot, const hstring& elementToFocus = {}) :
            _ViewModel(viewModel),
            _WeakWindowRoot(windowRoot),
            _ElementToFocus(elementToFocus) {}

        Editor::IHostedInWindow WindowRoot() const noexcept { return _WeakWindowRoot.get(); }
        Windows::Foundation::IInspectable ViewModel() const noexcept { return _ViewModel; }
        hstring ElementToFocus() const noexcept { return _ElementToFocus; }

    private:
        winrt::weak_ref<Editor::IHostedInWindow> _WeakWindowRoot;
        Windows::Foundation::IInspectable _ViewModel{ nullptr };
        hstring _ElementToFocus{};
    };

    struct MainPage : MainPageT<MainPage>
    {
        MainPage() = delete;
        MainPage(const Model::CascadiaSettings& settings);

        void UpdateSettings(const Model::CascadiaSettings& settings);

        safe_void_coroutine SettingsSearchBox_TextChanged(const Windows::UI::Xaml::Controls::AutoSuggestBox& sender, const Windows::UI::Xaml::Controls::AutoSuggestBoxTextChangedEventArgs& args);
        void SettingsSearchBox_QuerySubmitted(const Windows::UI::Xaml::Controls::AutoSuggestBox& sender, const Windows::UI::Xaml::Controls::AutoSuggestBoxQuerySubmittedEventArgs& args);
        void SettingsSearchBox_SuggestionChosen(const Windows::UI::Xaml::Controls::AutoSuggestBox& sender, const Windows::UI::Xaml::Controls::AutoSuggestBoxSuggestionChosenEventArgs& args);

        void SettingsNav_Loaded(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);
        void SettingsNav_ItemInvoked(const Microsoft::UI::Xaml::Controls::NavigationView& sender, const Microsoft::UI::Xaml::Controls::NavigationViewItemInvokedEventArgs& args);
        void SaveButton_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);
        void ResetButton_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);
        void BreadcrumbBar_ItemClicked(const Microsoft::UI::Xaml::Controls::BreadcrumbBar& sender, const Microsoft::UI::Xaml::Controls::BreadcrumbBarItemClickedEventArgs& args);

        void SetHostingWindow(uint64_t hostingWindow) noexcept;
        bool TryPropagateHostingWindow(IInspectable object) noexcept;
        uint64_t GetHostingWindow() const noexcept;

        winrt::Windows::UI::Xaml::Media::Brush BackgroundBrush();

        Windows::Foundation::Collections::IObservableVector<IInspectable> Breadcrumbs() noexcept;
        Editor::ExtensionsViewModel ExtensionsVM() const noexcept { return _extensionsVM; }
        Editor::ActionsViewModel ActionsVM() const noexcept { return _actionsVM; }

        til::typed_event<Windows::Foundation::IInspectable, Model::SettingsTarget> OpenJson;
        til::typed_event<Windows::Foundation::IInspectable, Windows::Foundation::Collections::IVectorView<Model::SettingsLoadWarnings>> ShowLoadWarningsDialog;

    private:
        Windows::Foundation::Collections::IObservableVector<IInspectable> _breadcrumbs;
        Windows::Foundation::Collections::IObservableVector<IInspectable> _menuItemSource;
        size_t _originalNumItems = 0u;

        Model::CascadiaSettings _settingsSource;
        Model::CascadiaSettings _settingsClone;

        std::optional<HWND> _hostingHwnd;

        void _InitializeProfilesList();
        void _CreateAndNavigateToNewProfile(const uint32_t index, const Model::Profile& profile);
        winrt::Microsoft::UI::Xaml::Controls::NavigationViewItem _CreateProfileNavViewItem(const Editor::ProfileViewModel& profile);
        void _DeleteProfile(const Windows::Foundation::IInspectable sender, const Editor::DeleteProfileEventArgs& args);
        void _AddProfileHandler(const winrt::guid profileGuid);

        void _SetupProfileEventHandling(const winrt::Microsoft::Terminal::Settings::Editor::ProfileViewModel profile);
        void _SetupColorSchemesEventHandling();
        void _NavigateToProfileSubPage(const Editor::ProfileViewModel& profile, ProfileSubPage page, const IInspectable& breadcrumbTag, const hstring& elementToFocus);

        void _PreNavigateHelper();
        void _Navigate(const IInspectable& vm, BreadcrumbSubPage subPage, hstring elementToFocus = {});
        void _NavigateToProfileHandler(const IInspectable& sender, winrt::guid profileGuid);
        void _NavigateToColorSchemeHandler(const IInspectable& sender, const IInspectable& args);
        Microsoft::UI::Xaml::Controls::NavigationViewItem _FindProfileNavItem(winrt::guid profileGuid) const;

        void _UpdateBackgroundForMica();
        void _MoveXamlParsedNavItemsIntoItemSource();

        safe_void_coroutine _UpdateSearchIndex();

        winrt::Microsoft::Terminal::Settings::Editor::ProfileViewModel _profileDefaultsVM{ nullptr };
        Windows::Foundation::Collections::IVector<winrt::Microsoft::Terminal::Settings::Editor::ProfileViewModel> _profileVMs{ nullptr };
        winrt::Microsoft::Terminal::Settings::Editor::ColorSchemesPageViewModel _colorSchemesPageVM{ nullptr };
        winrt::Microsoft::Terminal::Settings::Editor::ActionsViewModel _actionsVM{ nullptr };
        winrt::Microsoft::Terminal::Settings::Editor::NewTabMenuViewModel _newTabMenuPageVM{ nullptr };
        winrt::Microsoft::Terminal::Settings::Editor::ExtensionsViewModel _extensionsVM{ nullptr };

        Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IObservableVector<Windows::Foundation::IInspectable>> _currentSearch{ nullptr };

        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _profileViewModelChangedRevoker;
        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _colorSchemesPageViewModelChangedRevoker;
        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _actionsViewModelChangedRevoker;
        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _ntmViewModelChangedRevoker;
        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _extensionsViewModelChangedRevoker;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(MainPage);
}
