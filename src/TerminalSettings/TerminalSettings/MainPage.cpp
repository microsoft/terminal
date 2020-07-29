#include "pch.h"
#include "MainPage.h"
#include "MainPage.g.cpp"
#include "Home.h"
#include "Globals.h"
#include "Launch.h"
#include "Interaction.h"
#include "Rendering.h"
#include "Profiles.h"
#include "GlobalAppearance.h"
#include "ColorSchemes.h"
#include "Keybindings.h"
#include "AddProfile.h"

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
}

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::TerminalSettings::implementation;

namespace winrt::SettingsControl::implementation
{
    MainPage::MainPage()
    {
        InitializeComponent();

        // TODO: When we actually connect this to Windows Terminal,
        //       this section will clone the active AppSettings
        _settingsSource = AppSettings();
        _settingsClone = _settingsSource.Clone();

        SearchList.insert(std::pair<IInspectable, hstring>(Windows::Foundation::PropertyValue::CreateString(L"Add new profile"), L"AddNew_Nav"));
        SearchList.insert(std::pair<IInspectable, hstring>(Windows::Foundation::PropertyValue::CreateString(L"Always show tabs"), L"GlobalAppearance_Nav"));
        SearchList.insert(std::pair<IInspectable, hstring>(Windows::Foundation::PropertyValue::CreateString(L"Color scheme"), L"ColorSchemes_Nav"));
        SearchList.insert(std::pair<IInspectable, hstring>(Windows::Foundation::PropertyValue::CreateString(L"Columns on first launch"), L"Launch_Nav"));
        SearchList.insert(std::pair<IInspectable, hstring>(Windows::Foundation::PropertyValue::CreateString(L"Copy after selection is made"), L"Interaction_Nav"));
        SearchList.insert(std::pair<IInspectable, hstring>(Windows::Foundation::PropertyValue::CreateString(L"Copy formatting"), L"Interaction_Nav"));
        SearchList.insert(std::pair<IInspectable, hstring>(Windows::Foundation::PropertyValue::CreateString(L"Default profile"), L"Launch_Nav"));
        SearchList.insert(std::pair<IInspectable, hstring>(Windows::Foundation::PropertyValue::CreateString(L"Disable dynamic profiles"), L"Launch_Nav"));
        SearchList.insert(std::pair<IInspectable, hstring>(Windows::Foundation::PropertyValue::CreateString(L"Global appearance"), L"GlobalAppearance_Nav"));
        SearchList.insert(std::pair<IInspectable, hstring>(Windows::Foundation::PropertyValue::CreateString(L"Global profile settings"), L"GlobalProfile_Nav"));
        SearchList.insert(std::pair<IInspectable, hstring>(Windows::Foundation::PropertyValue::CreateString(L"Hide close all tabs popup"), L"GlobalAppearance_Nav"));
        SearchList.insert(std::pair<IInspectable, hstring>(Windows::Foundation::PropertyValue::CreateString(L"Interaction"), L"Interaction_Nav"));
        SearchList.insert(std::pair<IInspectable, hstring>(Windows::Foundation::PropertyValue::CreateString(L"Keyboard"), L"Keyboard_Nav"));
        SearchList.insert(std::pair<IInspectable, hstring>(Windows::Foundation::PropertyValue::CreateString(L"Launch"), L"Launch_Nav"));
        SearchList.insert(std::pair<IInspectable, hstring>(Windows::Foundation::PropertyValue::CreateString(L"Launch on startup"), L"Launch_Nav"));
        SearchList.insert(std::pair<IInspectable, hstring>(Windows::Foundation::PropertyValue::CreateString(L"Launch position"), L"Launch_Nav"));
        SearchList.insert(std::pair<IInspectable, hstring>(Windows::Foundation::PropertyValue::CreateString(L"Launch size"), L"Launch_Nav"));
        SearchList.insert(std::pair<IInspectable, hstring>(Windows::Foundation::PropertyValue::CreateString(L"Rendering"), L"Rendering_Nav"));
        SearchList.insert(std::pair<IInspectable, hstring>(Windows::Foundation::PropertyValue::CreateString(L"Rows on first launch"), L"Launch_Nav"));
        SearchList.insert(std::pair<IInspectable, hstring>(Windows::Foundation::PropertyValue::CreateString(L"Screen redrawing"), L"Rendering_Nav"));
        SearchList.insert(std::pair<IInspectable, hstring>(Windows::Foundation::PropertyValue::CreateString(L"Show terminal title in title bar"), L"GlobalAppearance_Nav"));
        SearchList.insert(std::pair<IInspectable, hstring>(Windows::Foundation::PropertyValue::CreateString(L"Show the title bar"), L"GlobalAppearance_Nav"));
        SearchList.insert(std::pair<IInspectable, hstring>(Windows::Foundation::PropertyValue::CreateString(L"Software rendering"), L"Rendering_Nav"));
        SearchList.insert(std::pair<IInspectable, hstring>(Windows::Foundation::PropertyValue::CreateString(L"Tab width mode"), L"GlobalAppearance_Nav"));
        SearchList.insert(std::pair<IInspectable, hstring>(Windows::Foundation::PropertyValue::CreateString(L"Theme"), L"GlobalAppearance_Nav"));
        SearchList.insert(std::pair<IInspectable, hstring>(Windows::Foundation::PropertyValue::CreateString(L"Window resize behavior"), L"Rendering_Nav"));
        SearchList.insert(std::pair<IInspectable, hstring>(Windows::Foundation::PropertyValue::CreateString(L"Word delimeters"), L"Interaction_Nav"));
    }

    void MainPage::ClickHandler(IInspectable const&, RoutedEventArgs const&)
    {
    }

    void MainPage::SettingsNav_SelectionChanged(const MUX::Controls::NavigationView, const MUX::Controls::NavigationViewSelectionChangedEventArgs)
    {
    }

    void MainPage::SettingsNav_Loaded(IInspectable const&, RoutedEventArgs const&)
    {
        //// set the initial selectedItem
        for (uint32_t i = 0; i < SettingsNav().MenuItems().Size(); i++)
        {
            const auto item = SettingsNav().MenuItems().GetAt(i).as<Controls::ContentControl>();
            const hstring homeNav = L"Home_Nav";
            const hstring itemTag = unbox_value<hstring>(item.Tag());

            if (itemTag == homeNav)
            {
                SettingsNav().SelectedItem(item);
                break;
            }
        }

        contentFrame().Navigate(xaml_typename<SettingsControl::Home>());
    }

    void MainPage::SettingsNav_ItemInvoked(MUX::Controls::NavigationView const&, MUX::Controls::NavigationViewItemInvokedEventArgs const& args)
    {
        auto clickedItemContainer = args.InvokedItemContainer();

        if (clickedItemContainer != NULL)
        {
            Navigate(contentFrame(), unbox_value<hstring>(clickedItemContainer.Tag()));
        }
    }

    void MainPage::AutoSuggestBox_TextChanged(IInspectable const &sender, const Controls::AutoSuggestBoxTextChangedEventArgs args)
    {
        Controls::AutoSuggestBox autoBox = sender.as<Controls::AutoSuggestBox>();
        auto query = autoBox.Text();
        SearchSettings(query, autoBox);
    }

    void MainPage::AutoSuggestBox_QuerySubmitted(const Controls::AutoSuggestBox sender, const Controls::AutoSuggestBoxQuerySubmittedEventArgs args)
    {
        auto value = args.QueryText();
    }

    void MainPage::AutoSuggestBox_SuggestionChosen(const Controls::AutoSuggestBox sender, const Controls::AutoSuggestBoxSuggestionChosenEventArgs args)
    {
        auto selectItem = args.SelectedItem().as<Windows::Foundation::IPropertyValue>().GetString();
        Controls::AutoSuggestBox autoBox = sender.as<Controls::AutoSuggestBox>();

        Navigate(contentFrame(), SearchList.at(args.SelectedItem()));
    }

    void MainPage::SearchSettings(hstring query, Controls::AutoSuggestBox &autoBox)
    {
        Windows::Foundation::Collections::IVector<IInspectable> suggestions = single_threaded_vector<IInspectable>();
        std::vector<IInspectable> rawSuggestions;

        for (auto& it = SearchList.begin(); it != SearchList.end(); ++it)
        {
            auto value = it->first;
            hstring item = value.as<Windows::Foundation::IPropertyValue>().GetString();

            std::string tmp = winrt::to_string(item);
            std::transform(tmp.begin(), tmp.end(), tmp.begin(), std::tolower);
            item = winrt::to_hstring(tmp);

            std::string tmp2 = winrt::to_string(query);
            std::transform(tmp2.begin(), tmp2.end(), tmp2.begin(), std::tolower);
            query = winrt::to_hstring(tmp2);

            if (std::wcsstr(item.c_str(), query.c_str()))
            {
                rawSuggestions.emplace_back(value);
            }
        }

        // perform sort comparing strings inside of IPropertyValues
        std::sort(rawSuggestions.begin(), rawSuggestions.end(), [](const IInspectable& a, const IInspectable& b) -> bool {
            return a.as<IPropertyValue>().GetString() < b.as<IPropertyValue>().GetString();
        });

        // Pass all elements from rawSuggestions to suggestions
        for (const auto& suggestion : rawSuggestions)
        {
            suggestions.Append(suggestion);
        }

        autoBox.ItemsSource(suggestions);
    }

    void MainPage::Navigate(Controls::Frame contentFrame, hstring clickedItemTag)
    {
        const hstring homePage = L"Home_Nav";
        const hstring generalPage = L"General_Nav";
        const hstring launchSubpage = L"Launch_Nav";
        const hstring interactionSubpage = L"Interaction_Nav";
        const hstring renderingSubpage = L"Rendering_Nav";

        const hstring profilesPage = L"Profiles_Nav";
        const hstring globalprofileSubpage = L"GlobalProfile_Nav";
        const hstring addnewSubpage = L"AddNew_Nav";

        const hstring appearancePage = L"Appearance_Nav";
        const hstring colorSchemesPage = L"ColorSchemes_Nav";
        const hstring globalAppearancePage = L"GlobalAppearance_Nav";

        const hstring keybindingsPage = L"Keyboard_Nav";

        if (clickedItemTag == homePage)
        {
            contentFrame.Navigate(xaml_typename<SettingsControl::Home>());
        }
        else if (clickedItemTag == launchSubpage)
        {
            contentFrame.Navigate(xaml_typename<SettingsControl::Launch>());
        }
        else if (clickedItemTag == interactionSubpage)
        {
            contentFrame.Navigate(xaml_typename<SettingsControl::Interaction>());
        }
        else if (clickedItemTag == renderingSubpage)
        {
            contentFrame.Navigate(xaml_typename<SettingsControl::Rendering>());
        }
        else if (clickedItemTag == globalprofileSubpage)
        {
            contentFrame.Navigate(xaml_typename<SettingsControl::Profiles>());
        }
        else if (clickedItemTag == addnewSubpage)
        {
            contentFrame.Navigate(xaml_typename<SettingsControl::AddProfile>());
        }
        else if (clickedItemTag == colorSchemesPage)
        {
            contentFrame.Navigate(xaml_typename<SettingsControl::ColorSchemes>());
        }
        else if (clickedItemTag == globalAppearancePage)
        {
            contentFrame.Navigate(xaml_typename<SettingsControl::GlobalAppearance>());
        }
        else if (clickedItemTag == keybindingsPage)
        {
            contentFrame.Navigate(xaml_typename<SettingsControl::Keybindings>());
        }
    }
}


