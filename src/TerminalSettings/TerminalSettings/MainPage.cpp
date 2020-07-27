#include "pch.h"
#include "MainPage.h"
#include "MainPage.g.cpp"
#include "Globals.h"
#include "Launch.h"
#include "Interaction.h"
#include "Rendering.h"
#include "Profiles.h"
#include "GlobalAppearance.h"
#include "ColorSchemes.h"
#include "Keybindings.h"

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
}

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
            const hstring launchNav = L"Launch_Nav";
            const hstring itemTag = unbox_value<hstring>(item.Tag());

            if (itemTag == launchNav)
            {
                // item.IsSelected(true); // have to investigate how to replace this
                SettingsNav().Header() = item.Tag();
                break;
            }
        }

        contentFrame().Navigate(xaml_typename<SettingsControl::Launch>());
    }

    void MainPage::SettingsNav_ItemInvoked(MUX::Controls::NavigationView const&, MUX::Controls::NavigationViewItemInvokedEventArgs const& args)
    {
        auto clickedItemContainer = args.InvokedItemContainer();

        if (clickedItemContainer != NULL)
        {
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
            

            hstring clickedItemTag = unbox_value<hstring>(clickedItemContainer.Tag());

            if (clickedItemTag == launchSubpage)
            {
                contentFrame().Navigate(xaml_typename<SettingsControl::Launch>());
            }
            else if (clickedItemTag == interactionSubpage)
            {
                contentFrame().Navigate(xaml_typename<SettingsControl::Interaction>());
            }
            else if (clickedItemTag == renderingSubpage)
            {
                contentFrame().Navigate(xaml_typename<SettingsControl::Rendering>());
            }
            else if (clickedItemTag == globalprofileSubpage)
            {
                contentFrame().Navigate(xaml_typename<SettingsControl::Profiles>());
            }
            else if (clickedItemTag == addnewSubpage)
            {
                contentFrame().Navigate(xaml_typename<SettingsControl::Profiles>());
            }
            else if (clickedItemTag == colorSchemesPage)
            {
                contentFrame().Navigate(xaml_typename<SettingsControl::ColorSchemes>());
            }
            else if (clickedItemTag == globalAppearancePage)
            {
                contentFrame().Navigate(xaml_typename<SettingsControl::GlobalAppearance>());
            }
            else if (clickedItemTag == keybindingsPage)
            {
                contentFrame().Navigate(xaml_typename<SettingsControl::Keybindings>());
            }
        }
    }
}
