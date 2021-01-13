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
#include "..\types\inc\utils.hpp"

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

static const std::wstring_view launchTag{ L"Launch_Nav" };
static const std::wstring_view interactionTag{ L"Interaction_Nav" };
static const std::wstring_view renderingTag{ L"Rendering_Nav" };
static const std::wstring_view globalProfileTag{ L"GlobalProfile_Nav" };
static const std::wstring_view addProfileTag{ L"AddProfile" };
static const std::wstring_view colorSchemesTag{ L"ColorSchemes_Nav" };
static const std::wstring_view globalAppearanceTag{ L"GlobalAppearance_Nav" };

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    static Editor::ProfileViewModel _viewModelForProfile(const Model::Profile& profile)
    {
        return winrt::make<implementation::ProfileViewModel>(profile);
    }

    MainPage::MainPage(const CascadiaSettings& settings) :
        _settingsSource{ settings },
        _settingsClone{ settings.Copy() }
    {
        InitializeComponent();

        _InitializeProfilesList();

        _colorSchemesNavState = winrt::make<ColorSchemesPageNavigationState>(_settingsClone.GlobalSettings());
    }

    // Method Description:
    // - Update the Settings UI with a new CascadiaSettings to bind to
    // Arguments:
    // - settings - the new settings source
    // Return value:
    // - <none>
    fire_and_forget MainPage::UpdateSettings(Model::CascadiaSettings settings)
    {
        _settingsSource = settings;
        _settingsClone = settings.Copy();

        co_await winrt::resume_foreground(Dispatcher());

        // "remove" all profile-related NavViewItems
        // LOAD-BEARING: use Visibility here, instead of menuItems.Remove().
        //               Remove() works fine on NavViewItems with an hstring tag,
        //               but causes an out-of-bounds error with Profile tagged items.
        //               The cause of this error is unknown.
        auto menuItems{ SettingsNav().MenuItems() };
        for (auto i = menuItems.Size() - 1; i > 0; --i)
        {
            if (const auto navViewItem{ menuItems.GetAt(i).try_as<MUX::Controls::NavigationViewItem>() })
            {
                if (const auto tag{ navViewItem.Tag() })
                {
                    if (tag.try_as<Editor::ProfileViewModel>())
                    {
                        // hide NavViewItem pointing to a Profile
                        navViewItem.Visibility(Visibility::Collapsed);
                    }
                    else if (const auto stringTag{ tag.try_as<hstring>() })
                    {
                        if (stringTag == addProfileTag)
                        {
                            // hide NavViewItem pointing to "Add Profile"
                            navViewItem.Visibility(Visibility::Collapsed);
                        }
                    }
                }
            }
        }
        _InitializeProfilesList();

        // Update the Nav State with the new version of the settings
        _colorSchemesNavState.Globals(_settingsClone.GlobalSettings());

        _RefreshCurrentPage();
    }

    void MainPage::_RefreshCurrentPage()
    {
        auto navigationMenu{ SettingsNav() };
        if (const auto selectedItem{ navigationMenu.SelectedItem() })
        {
            if (const auto tag{ selectedItem.as<MUX::Controls::NavigationViewItem>().Tag() })
            {
                if (const auto oldProfile{ tag.try_as<Editor::ProfileViewModel>() })
                {
                    // check if the profile still exists
                    if (const auto profile{ _settingsClone.FindProfile(oldProfile.Guid()) })
                    {
                        // Navigate to the page with the given profile
                        _Navigate(_viewModelForProfile(profile));
                        return;
                    }
                }
                else if (const auto stringTag{ tag.try_as<hstring>() })
                {
                    // navigate to the page with this tag
                    _Navigate(*stringTag);
                    return;
                }
            }
        }

        // could not find the page we were on, fallback to first menu item
        const auto firstItem{ navigationMenu.MenuItems().GetAt(0) };
        navigationMenu.SelectedItem(firstItem);
        if (const auto tag{ navigationMenu.SelectedItem().as<MUX::Controls::NavigationViewItem>().Tag() })
        {
            _Navigate(unbox_value<hstring>(tag));
        }
    }

    void MainPage::SetHostingWindow(uint64_t hostingWindow) noexcept
    {
        _hostingHwnd.emplace(reinterpret_cast<HWND>(hostingWindow));
    }

    bool MainPage::TryPropagateHostingWindow(IInspectable object) noexcept
    {
        if (_hostingHwnd)
        {
            if (auto initializeWithWindow{ object.try_as<IInitializeWithWindow>() })
            {
                return SUCCEEDED(initializeWithWindow->Initialize(*_hostingHwnd));
            }
        }
        return false;
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
            else if (const auto profile = clickedItemContainer.Tag().try_as<Editor::ProfileViewModel>())
            {
                // Navigate to a page with the given profile
                _Navigate(profile);
            }
        }
    }

    void MainPage::_Navigate(hstring clickedItemTag)
    {
        if (clickedItemTag == launchTag)
        {
            contentFrame().Navigate(xaml_typename<Editor::Launch>(), winrt::make<LaunchPageNavigationState>(_settingsClone));
        }
        else if (clickedItemTag == interactionTag)
        {
            contentFrame().Navigate(xaml_typename<Editor::Interaction>(), winrt::make<InteractionPageNavigationState>(_settingsClone.GlobalSettings()));
        }
        else if (clickedItemTag == renderingTag)
        {
            contentFrame().Navigate(xaml_typename<Editor::Rendering>(), winrt::make<RenderingPageNavigationState>(_settingsClone.GlobalSettings()));
        }
        else if (clickedItemTag == globalProfileTag)
        {
            auto profileVM{ _viewModelForProfile(_settingsClone.ProfileDefaults()) };
            profileVM.IsBaseLayer(true);
            contentFrame().Navigate(xaml_typename<Editor::Profiles>(), winrt::make<ProfilePageNavigationState>(profileVM, _settingsClone.GlobalSettings().ColorSchemes(), *this));
        }
        else if (clickedItemTag == colorSchemesTag)
        {
            contentFrame().Navigate(xaml_typename<Editor::ColorSchemes>(), _colorSchemesNavState);
        }
        else if (clickedItemTag == globalAppearanceTag)
        {
            contentFrame().Navigate(xaml_typename<Editor::GlobalAppearance>(), winrt::make<GlobalAppearancePageNavigationState>(_settingsClone.GlobalSettings()));
        }
    }

    void MainPage::_Navigate(const Editor::ProfileViewModel& profile)
    {
        auto state{ winrt::make<ProfilePageNavigationState>(profile, _settingsClone.GlobalSettings().ColorSchemes(), *this) };

        // Add an event handler for when the user wants to delete a profile.
        state.DeleteProfile({ this, &MainPage::_DeleteProfile });

        contentFrame().Navigate(xaml_typename<Editor::Profiles>(), state);
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

    void MainPage::SaveButton_Click(IInspectable const& /*sender*/, RoutedEventArgs const& /*args*/)
    {
        _settingsClone.WriteSettingsToDisk();
    }

    void MainPage::ResetButton_Click(IInspectable const& /*sender*/, RoutedEventArgs const& /*args*/)
    {
        _settingsClone = _settingsSource.Copy();
        _RefreshCurrentPage();
    }

    void MainPage::_InitializeProfilesList()
    {
        // Manually create a NavigationViewItem for each profile
        // and keep a reference to them in a map so that we
        // can easily modify the correct one when the associated
        // profile changes.
        for (const auto& profile : _settingsClone.AllProfiles())
        {
            auto navItem = _CreateProfileNavViewItem(_viewModelForProfile(profile));
            SettingsNav().MenuItems().Append(navItem);
        }

        // Top off (the end of the nav view) with the Add Profile item
        MUX::Controls::NavigationViewItem addProfileItem;
        addProfileItem.Content(box_value(RS_(L"Nav_AddNewProfile/Content")));
        addProfileItem.Tag(box_value(addProfileTag));
        addProfileItem.SelectsOnInvoked(false);

        FontIcon icon;
        // This is the "Add" symbol
        icon.Glyph(L"\xE710");
        addProfileItem.Icon(icon);

        SettingsNav().MenuItems().Append(addProfileItem);
    }

    void MainPage::_CreateAndNavigateToNewProfile(const uint32_t index)
    {
        const auto newProfile{ _settingsClone.CreateNewProfile() };
        const auto profileViewModel{ _viewModelForProfile(newProfile) };
        const auto navItem{ _CreateProfileNavViewItem(profileViewModel) };
        SettingsNav().MenuItems().InsertAt(index, navItem);

        // Select and navigate to the new profile
        SettingsNav().SelectedItem(navItem);
        _Navigate(profileViewModel);
    }

    MUX::Controls::NavigationViewItem MainPage::_CreateProfileNavViewItem(const Editor::ProfileViewModel& profile)
    {
        MUX::Controls::NavigationViewItem profileNavItem;
        profileNavItem.Content(box_value(profile.Name()));
        profileNavItem.Tag(box_value<Editor::ProfileViewModel>(profile));

        const auto iconSource{ IconPathConverter::IconSourceWUX(profile.Icon()) };
        WUX::Controls::IconSourceElement icon;
        icon.IconSource(iconSource);
        profileNavItem.Icon(icon);

        return profileNavItem;
    }

    void MainPage::_DeleteProfile(const IInspectable /*sender*/, const Editor::DeleteProfileEventArgs& args)
    {
        // Delete profile from settings model
        const auto guid{ args.ProfileGuid() };
        auto profileList{ _settingsClone.AllProfiles() };
        for (uint32_t i = 0; i < profileList.Size(); ++i)
        {
            if (profileList.GetAt(i).Guid() == guid)
            {
                profileList.RemoveAt(i);
                break;
            }
        }

        // remove selected item
        uint32_t index;
        auto selectedItem{ SettingsNav().SelectedItem() };
        auto menuItems{ SettingsNav().MenuItems() };
        menuItems.IndexOf(selectedItem, index);
        menuItems.RemoveAt(index);

        // navigate to the profile next to this one
        const auto newSelectedItem{ menuItems.GetAt(index < menuItems.Size() - 1 ? index : index - 1) };
        SettingsNav().SelectedItem(newSelectedItem);
        _Navigate(newSelectedItem.try_as<MUX::Controls::NavigationViewItem>().Tag().try_as<Editor::ProfileViewModel>());
    }
}
