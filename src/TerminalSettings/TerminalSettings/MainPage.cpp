#include "pch.h"
#include "MainPage.h"
#include "MainPage.g.cpp"
#include "Globals.h"
#include "Profiles.h"
#include "ColorSchemes.h"
#include "Keybindings.h"

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::TerminalSettings::implementation
{
    MainPage::MainPage()
    {
        InitializeComponent();
    }

    int32_t MainPage::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void MainPage::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }

    void MainPage::ClickHandler(IInspectable const&, RoutedEventArgs const&)
    {

    }

    void MainPage::SettingsNav_Loaded_1(IInspectable const& sender, RoutedEventArgs const& e)
    {
        //// set the initial selectedItem
        for (uint32_t i = 0; i < SettingsNav().MenuItems().Size(); i++)
        {
            const auto item0 = SettingsNav().MenuItems().GetAt(i);

            const auto item = item0.as<Controls::ContentControl>();
            const hstring globalsNav = L"Globals_Nav";
            const hstring itemTag = unbox_value<hstring>(item.Tag());

            if (itemTag == globalsNav)
            {
                // item.IsSelected(true); // have to investigate how to replace this
                SettingsNav().Header() = item.Tag();
                break;
            }
        }

        contentFrame().Navigate(xaml_typename<TerminalSettings::Globals>());
    }

    void MainPage::SettingsNav_SelectionChanged(Controls::NavigationView sender, Controls::NavigationViewSelectionChangedEventArgs args)
    {

    }

    void winrt::TerminalSettings::implementation::MainPage::SettingsNav_SelectionChanged_1(winrt::Microsoft::UI::Xaml::Controls::NavigationView const& sender, winrt::Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const& args)
    {
    }

    void MainPage::SettingsNav_ItemInvoked_1(winrt::Microsoft::UI::Xaml::Controls::NavigationView const& sender, winrt::Microsoft::UI::Xaml::Controls::NavigationViewItemInvokedEventArgs const& args)
    {
        auto clickedItemContainer = args.InvokedItemContainer();

        if (clickedItemContainer != NULL)
        {
            const hstring globalsPage = L"Globals_Page";
            const hstring profilesPage = L"Profiles_Page";
            const hstring colorSchemesPage = L"ColorSchemes_Page";
            const hstring keybindingsPage = L"Keybindings_Page";

            hstring clickedItemTag = unbox_value<hstring>(clickedItemContainer.Tag());

            if (clickedItemTag == globalsPage)
            {
                contentFrame().Navigate(xaml_typename<TerminalSettings::Globals>());
            }
            else if (clickedItemTag == profilesPage)
            {
                contentFrame().Navigate(xaml_typename<TerminalSettings::Profiles>());
            }
            else if (clickedItemTag == colorSchemesPage)
            {
                contentFrame().Navigate(xaml_typename<TerminalSettings::ColorSchemes>());
            }
            else if (clickedItemTag == keybindingsPage)
            {
                contentFrame().Navigate(xaml_typename<TerminalSettings::Keybindings>());
            }
        }
    }
}
