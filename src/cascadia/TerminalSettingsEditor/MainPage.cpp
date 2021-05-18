// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "MainPage.h"
#include "MainPage.g.cpp"
#include "Launch.h"
#include "Interaction.h"
#include "Rendering.h"
#include "Actions.h"
#include "Profiles.h"
#include "GlobalAppearance.h"
#include "ColorSchemes.h"
#include "AddProfile.h"
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
static const std::wstring_view actionsTag{ L"Actions_Nav" };
static const std::wstring_view addProfileTag{ L"AddProfile" };
static const std::wstring_view colorSchemesTag{ L"ColorSchemes_Nav" };
static const std::wstring_view globalAppearanceTag{ L"GlobalAppearance_Nav" };

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    static Editor::ProfileViewModel _viewModelForProfile(const Model::Profile& profile, const Model::CascadiaSettings& appSettings)
    {
        return winrt::make<implementation::ProfileViewModel>(profile, appSettings);
    }

    MainPage::MainPage(const CascadiaSettings& settings) :
        _settingsSource{ settings },
        _settingsClone{ settings.Copy() }
    {
        InitializeComponent();

        _InitializeProfilesList();

        _colorSchemesNavState = winrt::make<ColorSchemesPageNavigationState>(_settingsClone);

        Automation::AutomationProperties::SetHelpText(SaveButton(), RS_(L"Settings_SaveSettingsButton/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"));
        Automation::AutomationProperties::SetHelpText(ResetButton(), RS_(L"Settings_ResetSettingsButton/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"));
        Automation::AutomationProperties::SetHelpText(OpenJsonNavItem(), RS_(L"Nav_OpenJSON/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"));
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

        // Deduce information about the currently selected item
        IInspectable selectedItemTag;
        auto menuItems{ SettingsNav().MenuItems() };
        if (const auto& selectedItem{ SettingsNav().SelectedItem() })
        {
            if (const auto& navViewItem{ selectedItem.try_as<MUX::Controls::NavigationViewItem>() })
            {
                selectedItemTag = navViewItem.Tag();
            }
        }

        // remove all profile-related NavViewItems by populating a std::vector
        // with the ones we want to keep.
        // NOTE: menuItems.Remove() causes an out-of-bounds crash. Using ReplaceAll()
        //       gets around this crash.
        std::vector<IInspectable> menuItemsSTL;
        for (const auto& item : menuItems)
        {
            if (const auto& navViewItem{ item.try_as<MUX::Controls::NavigationViewItem>() })
            {
                if (const auto& tag{ navViewItem.Tag() })
                {
                    if (tag.try_as<Editor::ProfileViewModel>())
                    {
                        // don't add NavViewItem pointing to a Profile
                        continue;
                    }
                    else if (const auto& stringTag{ tag.try_as<hstring>() })
                    {
                        if (stringTag == addProfileTag)
                        {
                            // don't add the "Add Profile" item
                            continue;
                        }
                    }
                }
            }
            menuItemsSTL.emplace_back(item);
        }
        menuItems.ReplaceAll(menuItemsSTL);

        // Repopulate profile-related menu items
        _InitializeProfilesList();
        // Update the Nav State with the new version of the settings
        _colorSchemesNavState.Settings(_settingsClone);
        // We'll update the profile in the _profilesNavState whenever we actually navigate to one

        // now that the menuItems are repopulated,
        // refresh the current page using the SelectedItem data we collected before the refresh
        if (selectedItemTag)
        {
            for (const auto& item : menuItems)
            {
                if (const auto& menuItem{ item.try_as<MUX::Controls::NavigationViewItem>() })
                {
                    if (const auto& tag{ menuItem.Tag() })
                    {
                        if (const auto& stringTag{ tag.try_as<hstring>() })
                        {
                            if (const auto& selectedItemStringTag{ selectedItemTag.try_as<hstring>() })
                            {
                                if (stringTag == selectedItemStringTag)
                                {
                                    // found the one that was selected before the refresh
                                    SettingsNav().SelectedItem(item);
                                    _Navigate(*stringTag);
                                    co_return;
                                }
                            }
                        }
                        else if (const auto& profileTag{ tag.try_as<ProfileViewModel>() })
                        {
                            if (const auto& selectedItemProfileTag{ selectedItemTag.try_as<ProfileViewModel>() })
                            {
                                if (profileTag->Guid() == selectedItemProfileTag->Guid())
                                {
                                    // found the one that was selected before the refresh
                                    SettingsNav().SelectedItem(item);
                                    _Navigate(*profileTag);
                                    co_return;
                                }
                            }
                        }
                    }
                }
            }
        }

        // couldn't find the selected item,
        // fallback to first menu item
        const auto& firstItem{ menuItems.GetAt(0) };
        SettingsNav().SelectedItem(firstItem);
        if (const auto& tag{ SettingsNav().SelectedItem().try_as<MUX::Controls::NavigationViewItem>().Tag() })
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

    // Method Description:
    // - Creates a new profile and navigates to it in the Settings UI
    // Arguments:
    // - profileGuid: the guid of the profile we want to duplicate,
    //                can be empty to indicate that we should create a fresh profile
    void MainPage::_AddProfileHandler(winrt::guid profileGuid)
    {
        uint32_t insertIndex;
        auto selectedItem{ SettingsNav().SelectedItem() };
        auto menuItems{ SettingsNav().MenuItems() };
        menuItems.IndexOf(selectedItem, insertIndex);
        if (profileGuid != winrt::guid{})
        {
            // if we were given a non-empty guid, we want to duplicate the corresponding profile
            const auto profile = _settingsClone.FindProfile(profileGuid);
            if (profile)
            {
                const auto duplicated = _settingsClone.DuplicateProfile(profile);
                _CreateAndNavigateToNewProfile(insertIndex, duplicated);
            }
        }
        else
        {
            // we were given an empty guid, create a new profile
            _CreateAndNavigateToNewProfile(insertIndex, nullptr);
        }
    }

    uint64_t MainPage::GetHostingWindow() const noexcept
    {
        return reinterpret_cast<uint64_t>(_hostingHwnd.value_or(nullptr));
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
            if (clickedItemContainer.IsSelected())
            {
                // Clicked on the selected item.
                // Don't navigate to the same page again.
                return;
            }

            if (const auto navString = clickedItemContainer.Tag().try_as<hstring>())
            {
                _Navigate(*navString);
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
        else if (clickedItemTag == actionsTag)
        {
            contentFrame().Navigate(xaml_typename<Editor::Actions>(), winrt::make<ActionsPageNavigationState>(_settingsClone));
        }
        else if (clickedItemTag == colorSchemesTag)
        {
            contentFrame().Navigate(xaml_typename<Editor::ColorSchemes>(), _colorSchemesNavState);
        }
        else if (clickedItemTag == globalAppearanceTag)
        {
            contentFrame().Navigate(xaml_typename<Editor::GlobalAppearance>(), winrt::make<GlobalAppearancePageNavigationState>(_settingsClone.GlobalSettings()));
        }
        else if (clickedItemTag == addProfileTag)
        {
            auto addProfileState{ winrt::make<AddProfilePageNavigationState>(_settingsClone) };
            addProfileState.AddNew({ get_weak(), &MainPage::_AddProfileHandler });
            contentFrame().Navigate(xaml_typename<Editor::AddProfile>(), addProfileState);
        }
    }

    // Method Description:
    // - updates the content frame to present a view of the profile page
    // - NOTE: this does not update the selected item.
    // Arguments:
    // - profile - the profile object we are getting a view of
    void MainPage::_Navigate(const Editor::ProfileViewModel& profile)
    {
        _lastProfilesNavState = winrt::make<ProfilePageNavigationState>(profile,
                                                                        _settingsClone.GlobalSettings().ColorSchemes(),
                                                                        _lastProfilesNavState,
                                                                        *this);

        // Add an event handler for when the user wants to delete a profile.
        _lastProfilesNavState.DeleteProfile({ this, &MainPage::_DeleteProfile });

        contentFrame().Navigate(xaml_typename<Editor::Profiles>(), _lastProfilesNavState);
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
        UpdateSettings(_settingsSource);
    }

    void MainPage::_InitializeProfilesList()
    {
        // Manually create a NavigationViewItem for each profile
        // and keep a reference to them in a map so that we
        // can easily modify the correct one when the associated
        // profile changes.
        for (const auto& profile : _settingsClone.AllProfiles())
        {
            auto navItem = _CreateProfileNavViewItem(_viewModelForProfile(profile, _settingsClone));
            SettingsNav().MenuItems().Append(navItem);
        }

        // Top off (the end of the nav view) with the Add Profile item
        MUX::Controls::NavigationViewItem addProfileItem;
        addProfileItem.Content(box_value(RS_(L"Nav_AddNewProfile/Content")));
        addProfileItem.Tag(box_value(addProfileTag));

        FontIcon icon;
        // This is the "Add" symbol
        icon.Glyph(L"\xE710");
        addProfileItem.Icon(icon);

        SettingsNav().MenuItems().Append(addProfileItem);
    }

    void MainPage::_CreateAndNavigateToNewProfile(const uint32_t index, const Model::Profile& profile)
    {
        const auto newProfile{ profile ? profile : _settingsClone.CreateNewProfile() };
        const auto profileViewModel{ _viewModelForProfile(newProfile, _settingsClone) };
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

        // Update the menu item when the icon/name changes
        auto weakMenuItem{ make_weak(profileNavItem) };
        profile.PropertyChanged([weakMenuItem](const auto&, const WUX::Data::PropertyChangedEventArgs& args) {
            if (auto menuItem{ weakMenuItem.get() })
            {
                const auto& tag{ menuItem.Tag().as<Editor::ProfileViewModel>() };
                if (args.PropertyName() == L"Icon")
                {
                    const auto iconSource{ IconPathConverter::IconSourceWUX(tag.Icon()) };
                    WUX::Controls::IconSourceElement icon;
                    icon.IconSource(iconSource);
                    menuItem.Icon(icon);
                }
                else if (args.PropertyName() == L"Name")
                {
                    menuItem.Content(box_value(tag.Name()));
                }
            }
        });
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
