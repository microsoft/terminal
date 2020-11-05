// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "MainPage.h"
#include "MainPage.g.cpp"
#include "Launch.h"
#include "Interaction.h"
#include "Rendering.h"
#include "Profiles.h"
#include "GlobalAppearance.h"
#include "ColorSchemes.h"
#include "Keybindings.h"

#include <LibraryResources.h>

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
}

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::System;
using namespace winrt::Windows::UI::Xaml::Controls;

static const winrt::hstring launchTag{ L"Launch_Nav" };
static const winrt::hstring interactionTag{ L"Interaction_Nav" };
static const winrt::hstring renderingTag{ L"Rendering_Nav" };
static const winrt::hstring globalProfileTag{ L"GlobalProfile_Nav" };
static const winrt::hstring addProfileTag{ L"AddProfile" };
static const winrt::hstring colorSchemesTag{ L"ColorSchemes_Nav" };
static const winrt::hstring globalAppearanceTag{ L"GlobalAppearance_Nav" };

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    MainPage::MainPage(CascadiaSettings settings) :
        _settingsSource{ settings },
        _settingsClone{ settings },
        _profileToNavItemMap{ winrt::single_threaded_map<Model::Profile, MUX::Controls::NavigationViewItem>() }
    {
        InitializeComponent();

        _InitializeProfilesList();
    }

    // Function Description:
    // - Called when the NavigationView is loaded. Navigates to the first item in the NavigationView, if no item is selected
    // Arguments:
    // - <unused>
    // Return Value:
    // - <none>
    void MainPage::SettingsNav_Loaded(IInspectable const&, RoutedEventArgs const&)
    {
        if (SettingsNav().SelectedItem() == nullptr)
        {
            const auto initialItem = SettingsNav().MenuItems().GetAt(0);
            SettingsNav().SelectedItem(initialItem);

            // Manually navigate because setting the selected item programmatically doesn't trigger ItemInvoked.
            if (const auto tag = initialItem.as<MUX::Controls::NavigationViewItem>().Tag())
            {
                _Navigate(unbox_value<hstring>(tag));
            }
        }
    }

    // Function Description:
    // - Called when NavigationView items are invoked. Navigates to the corresponding page.
    // Arguments:
    // - args - additional event info from invoking the NavViewItem
    // Return Value:
    // - <none>
    void MainPage::SettingsNav_ItemInvoked(MUX::Controls::NavigationView const&, MUX::Controls::NavigationViewItemInvokedEventArgs const& args)
    {
        if (const auto clickedItemContainer = args.InvokedItemContainer())
        {
            if (const auto navString = clickedItemContainer.Tag().try_as<hstring>())
            {
                if (navString == addProfileTag)
                {
                    // "AddProfile" needs to create a new profile before we can navigate to it
                    uint32_t insertIndex;
                    SettingsNav().MenuItems().IndexOf(clickedItemContainer, insertIndex);
                    _CreateAndNavigateToNewProfile(insertIndex);
                }
                else
                {
                    // Otherwise, navigate to the page
                    _Navigate(*navString);
                }
            }
            else if (const auto profile = clickedItemContainer.Tag().try_as<Model::Profile>())
            {
                // Navigate to a page with the given profile
                const auto state{ winrt::make<ProfilePageNavigationState>(profile, _settingsClone.GlobalSettings().ColorSchemes()) };
                contentFrame().Navigate(xaml_typename<Editor::Profiles>(), state);
            }
        }
    }

    void MainPage::_Navigate(hstring clickedItemTag)
    {
        if (clickedItemTag == launchTag)
        {
            const auto state{ winrt::make<LaunchPageNavigationState>(_settingsClone) };
            contentFrame().Navigate(xaml_typename<Editor::Launch>(), state);
        }
        else if (clickedItemTag == interactionTag)
        {
            const auto state{ winrt::make<InteractionPageNavigationState>(_settingsClone.GlobalSettings()) };
            contentFrame().Navigate(xaml_typename<Editor::Interaction>(), state);
        }
        else if (clickedItemTag == renderingTag)
        {
            const auto state{ winrt::make<RenderingPageNavigationState>(_settingsClone.GlobalSettings()) };
            contentFrame().Navigate(xaml_typename<Editor::Rendering>(), state);
        }
        else if (clickedItemTag == globalProfileTag)
        {
            const auto state{ winrt::make<ProfilePageNavigationState>(_settingsClone.ProfileDefaults(), _settingsClone.GlobalSettings().ColorSchemes()) };
            contentFrame().Navigate(xaml_typename<Editor::Profiles>(), state);
        }
        else if (clickedItemTag == colorSchemesTag)
        {
            const auto state{ winrt::make<ColorSchemesPageNavigationState>(_settingsClone.GlobalSettings()) };
            contentFrame().Navigate(xaml_typename<Editor::ColorSchemes>(), state);
        }
        else if (clickedItemTag == globalAppearanceTag)
        {
            const auto state{ winrt::make<GlobalAppearancePageNavigationState>(_settingsClone.GlobalSettings()) };
            contentFrame().Navigate(xaml_typename<Editor::GlobalAppearance>(), state);
        }
    }

    void MainPage::SettingsNav_BackRequested(MUX::Controls::NavigationView const&, MUX::Controls::NavigationViewBackRequestedEventArgs const& /*args*/)
    {
        On_BackRequested();
    }

    bool MainPage::On_BackRequested()
    {
        if (!contentFrame().CanGoBack())
        {
            return false;
        }

        if (SettingsNav().IsPaneOpen() &&
            (SettingsNav().DisplayMode() == MUX::Controls::NavigationViewDisplayMode(1) ||
             SettingsNav().DisplayMode() == MUX::Controls::NavigationViewDisplayMode(0)))
        {
            return false;
        }

        contentFrame().GoBack();
        return true;
    }

    void MainPage::OpenJsonTapped(IInspectable const& /*sender*/, Windows::UI::Xaml::Input::TappedRoutedEventArgs const& /*args*/)
    {
        const CoreWindow window = CoreWindow::GetForCurrentThread();
        const auto rAltState = window.GetKeyState(VirtualKey::RightMenu);
        const auto lAltState = window.GetKeyState(VirtualKey::LeftMenu);
        const bool altPressed = WI_IsFlagSet(lAltState, CoreVirtualKeyStates::Down) ||
                                WI_IsFlagSet(rAltState, CoreVirtualKeyStates::Down);

        const auto target = altPressed ? SettingsTarget::DefaultsFile : SettingsTarget::SettingsFile;
        _OpenJsonHandlers(nullptr, target);
    }

    void MainPage::OpenJsonKeyDown(IInspectable const& /*sender*/, Windows::UI::Xaml::Input::KeyRoutedEventArgs const& args)
    {
        if (args.Key() == VirtualKey::Enter || args.Key() == VirtualKey::Space)
        {
            const auto target = args.KeyStatus().IsMenuKeyDown ? SettingsTarget::DefaultsFile : SettingsTarget::SettingsFile;
            _OpenJsonHandlers(nullptr, target);
        }
    }

    void MainPage::_InitializeProfilesList()
    {
        // Manually create a NavigationViewItem for each profile
        // and keep a reference to them in a map so that we
        // can easily modify the correct one when the associated
        // profile changes.
        for (const auto& profile : _settingsClone.AllProfiles())
        {
            auto navItem = _CreateProfileNavViewItem(profile);
            SettingsNav().MenuItems().Append(navItem);
        }

        // Top off (the end of the nav view) with the Add Profile item
        MUX::Controls::NavigationViewItem addProfileItem;
        addProfileItem.Content(box_value(RS_(L"Nav_AddNewProfile/Content")));
        addProfileItem.Tag(box_value(L"AddProfile"));
        addProfileItem.SelectsOnInvoked(false);

        FontIcon icon;
        // This is the "Add" symbol
        icon.Glyph(L"\xE710");
        addProfileItem.Icon(icon);

        SettingsNav().MenuItems().Append(addProfileItem);
    }

    void MainPage::_CreateAndNavigateToNewProfile(const uint32_t index)
    {
        auto newProfile = _settingsClone.CreateNewProfile();
        auto navItem = _CreateProfileNavViewItem(newProfile);
        SettingsNav().MenuItems().InsertAt(index, navItem);

        // Now nav to the new profile
        // TODO: Setting SelectedItem here doesn't seem to update
        // the NavigationView selected visual indicator, not sure where
        // it needs to be...
        contentFrame().Navigate(xaml_typename<Editor::Profiles>(), newProfile);
    }

    MUX::Controls::NavigationViewItem MainPage::_CreateProfileNavViewItem(const Profile& profile)
    {
        MUX::Controls::NavigationViewItem profileNavItem;
        profileNavItem.Content(box_value(profile.Name()));
        profileNavItem.Tag(box_value<Model::Profile>(profile));

        const auto iconSource{ IconPathConverter::IconSourceWUX(profile.Icon()) };
        WUX::Controls::IconSourceElement icon;
        icon.IconSource(iconSource);
        profileNavItem.Icon(icon);

        _profileToNavItemMap.Insert(profile, profileNavItem);

        return profileNavItem;
    }
}
