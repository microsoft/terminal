#pragma once

#include "MainPage.g.h"
#include "winrt/Microsoft.UI.Xaml.Controls.h"
#include "ObjectModel/AppSettings.h"

namespace winrt::SettingsControl::implementation
{
    struct MainPage : MainPageT<MainPage>
    {
        MainPage();

        void ClickHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
        void SettingsNav_Loaded(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
        void SettingsNav_SelectionChanged(const Microsoft::UI::Xaml::Controls::NavigationView sender, const Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs args);
        void SettingsNav_ItemInvoked(Microsoft::UI::Xaml::Controls::NavigationView const& sender, Microsoft::UI::Xaml::Controls::NavigationViewItemInvokedEventArgs const& args);
        void Navigate(hstring clickedItemTag);

        std::map<IInspectable, hstring> SearchList;

        void AutoSuggestBox_TextChanged(Windows::Foundation::IInspectable const& sender, const winrt::Windows::UI::Xaml::Controls::AutoSuggestBoxTextChangedEventArgs args);
        void AutoSuggestBox_QuerySubmitted(const winrt::Windows::UI::Xaml::Controls::AutoSuggestBox sender, const winrt::Windows::UI::Xaml::Controls::AutoSuggestBoxQuerySubmittedEventArgs args);
        void AutoSuggestBox_SuggestionChosen(const winrt::Windows::UI::Xaml::Controls::AutoSuggestBox sender, const winrt::Windows::UI::Xaml::Controls::AutoSuggestBoxSuggestionChosenEventArgs args);
        void SearchSettings(hstring query, winrt::Windows::UI::Xaml::Controls::AutoSuggestBox& autoBox);

    private:
        // XAML should data-bind to the _settingsClone
        // When "save" is pressed, _settingsSource = _settingsClone
        winrt::TerminalSettings::implementation::AppSettings _settingsSource;
        winrt::TerminalSettings::implementation::AppSettings _settingsClone;
    };
}

namespace winrt::SettingsControl::factory_implementation
{
    struct MainPage : MainPageT<MainPage, implementation::MainPage>
    {
    };
}
