// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "MainPage.h"
#include "MainPage.g.cpp"
#include "Launch.h"
#include "Interaction.h"
#include "Rendering.h"
#include "RenderingViewModel.h"
#include "Actions.h"
#include "Profiles.h"
#include "GlobalAppearance.h"
#include "GlobalAppearanceViewModel.h"
#include "ColorSchemes.h"
#include "AddProfile.h"
#include "InteractionViewModel.h"
#include "LaunchViewModel.h"
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
using namespace winrt::Windows::Foundation::Collections;

static const std::wstring_view launchTag{ L"Launch_Nav" };
static const std::wstring_view interactionTag{ L"Interaction_Nav" };
static const std::wstring_view renderingTag{ L"Rendering_Nav" };
static const std::wstring_view actionsTag{ L"Actions_Nav" };
static const std::wstring_view globalProfileTag{ L"GlobalProfile_Nav" };
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

        _colorSchemesPageVM = winrt::make<ColorSchemesPageViewModel>(_settingsClone);

        Automation::AutomationProperties::SetHelpText(SaveButton(), RS_(L"Settings_SaveSettingsButton/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"));
        Automation::AutomationProperties::SetHelpText(ResetButton(), RS_(L"Settings_ResetSettingsButton/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"));
        Automation::AutomationProperties::SetHelpText(OpenJsonNavItem(), RS_(L"Nav_OpenJSON/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"));

        _breadcrumbs = single_threaded_observable_vector<IInspectable>();
    }

    // Method Description:
    // - Update the Settings UI with a new CascadiaSettings to bind to
    // Arguments:
    // - settings - the new settings source
    // Return value:
    // - <none>
    void MainPage::UpdateSettings(const Model::CascadiaSettings& settings)
    {
        _settingsSource = settings;
        _settingsClone = settings.Copy();

        // Deduce information about the currently selected item
        IInspectable lastBreadcrumb;
        const auto size = _breadcrumbs.Size();
        if (size > 0)
        {
            lastBreadcrumb = _breadcrumbs.GetAt(size - 1);
        }

        auto menuItems{ SettingsNav().MenuItems() };

        // We'll remove a bunch of items and iterate over it twice.
        // --> Copy it into an STL vector to simplify our code and reduce COM overhead.
        std::vector<IInspectable> menuItemsSTL(menuItems.Size(), nullptr);
        menuItems.GetMany(0, menuItemsSTL);

        // We want to refresh the list of profiles in the NavigationView.
        // In order to add profiles we can use _InitializeProfilesList();
        // But before we can do that we have to remove existing profiles first of course.
        // This "erase-remove" idiom will achieve just that.
        menuItemsSTL.erase(
            std::remove_if(
                menuItemsSTL.begin(),
                menuItemsSTL.end(),
                [](const auto& item) -> bool {
                    if (const auto& navViewItem{ item.try_as<MUX::Controls::NavigationViewItem>() })
                    {
                        if (const auto& tag{ navViewItem.Tag() })
                        {
                            if (tag.try_as<Editor::ProfileViewModel>())
                            {
                                // remove NavViewItem pointing to a Profile
                                return true;
                            }
                            if (const auto& stringTag{ tag.try_as<hstring>() })
                            {
                                if (stringTag == addProfileTag)
                                {
                                    // remove the "Add Profile" item
                                    return true;
                                }
                            }
                        }
                    }
                    return false;
                }),
            menuItemsSTL.end());

        menuItems.ReplaceAll(menuItemsSTL);

        // Repopulate profile-related menu items
        _InitializeProfilesList();
        // Update the Nav State with the new version of the settings
        _colorSchemesPageVM.UpdateSettings(_settingsClone);

        // We'll update the profile in the _profilesNavState whenever we actually navigate to one

        // now that the menuItems are repopulated,
        // refresh the current page using the breadcrumb data we collected before the refresh
        if (const auto& crumb{ lastBreadcrumb.try_as<Breadcrumb>() })
        {
            for (const auto& item : menuItems)
            {
                if (const auto& menuItem{ item.try_as<MUX::Controls::NavigationViewItem>() })
                {
                    if (const auto& tag{ menuItem.Tag() })
                    {
                        if (const auto& stringTag{ tag.try_as<hstring>() })
                        {
                            if (const auto& breadcrumbStringTag{ crumb->Tag().try_as<hstring>() })
                            {
                                if (stringTag == breadcrumbStringTag)
                                {
                                    // found the one that was selected before the refresh
                                    SettingsNav().SelectedItem(item);
                                    _Navigate(*stringTag, crumb->SubPage());
                                    return;
                                }
                            }
                        }
                        else if (const auto& profileTag{ tag.try_as<ProfileViewModel>() })
                        {
                            if (const auto& breadcrumbProfileTag{ crumb->Tag().try_as<ProfileViewModel>() })
                            {
                                if (profileTag->OriginalProfileGuid() == breadcrumbProfileTag->OriginalProfileGuid())
                                {
                                    // found the one that was selected before the refresh
                                    SettingsNav().SelectedItem(item);
                                    _Navigate(*profileTag, crumb->SubPage());
                                    return;
                                }
                            }
                        }
                    }
                }
            }
        }

        // Couldn't find the selected item, fallback to first menu item
        // This happens when the selected item was a profile which doesn't exist in the new configuration
        // We can use menuItemsSTL here because the only things they miss are profile entries.
        const auto& firstItem{ menuItemsSTL.at(0).as<MUX::Controls::NavigationViewItem>() };
        SettingsNav().SelectedItem(firstItem);
        _Navigate(unbox_value<hstring>(firstItem.Tag()), BreadcrumbSubPage::None);
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
    void MainPage::SettingsNav_Loaded(const IInspectable&, const RoutedEventArgs&)
    {
        if (SettingsNav().SelectedItem() == nullptr)
        {
            const auto initialItem = SettingsNav().MenuItems().GetAt(0);
            SettingsNav().SelectedItem(initialItem);

            // Manually navigate because setting the selected item programmatically doesn't trigger ItemInvoked.
            if (const auto tag = initialItem.as<MUX::Controls::NavigationViewItem>().Tag())
            {
                _Navigate(unbox_value<hstring>(tag), BreadcrumbSubPage::None);
            }
        }
    }

    // Function Description:
    // - Called when NavigationView items are invoked. Navigates to the corresponding page.
    // Arguments:
    // - args - additional event info from invoking the NavViewItem
    // Return Value:
    // - <none>
    void MainPage::SettingsNav_ItemInvoked(const MUX::Controls::NavigationView&, const MUX::Controls::NavigationViewItemInvokedEventArgs& args)
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
                _Navigate(*navString, BreadcrumbSubPage::None);
            }
            else if (const auto profile = clickedItemContainer.Tag().try_as<Editor::ProfileViewModel>())
            {
                // Navigate to a page with the given profile
                _Navigate(profile, BreadcrumbSubPage::None);
            }
        }
    }

    void MainPage::_PreNavigateHelper()
    {
        _profileViewModelChangedRevoker.revoke();
        _breadcrumbs.Clear();
    }

    void MainPage::_SetupProfileEventHandling(const Editor::ProfilePageNavigationState state)
    {
        // Add an event handler to navigate to Profiles_Appearance or Profiles_Advanced
        // Some notes on this:
        // - At first we tried putting another frame inside Profiles.xaml and having that
        //   frame default to showing Profiles_Base. This allowed the logic for navigation
        //   to Profiles_Advanced/Profiles_Appearance to live within Profiles.cpp.
        // - However, the header for the SUI lives in MainPage.xaml (because that's where
        //   the whole NavigationView is) and so the BreadcrumbBar needs to be in MainPage.xaml.
        //   We decided that it's better for the owner of the BreadcrumbBar to also be responsible
        //   for navigation, so the navigation to Profiles_Advanced/Profiles_Appearance from
        //   Profiles_Base got moved here.
        const auto profile = state.Profile();

        // If this is the base layer, the breadcrumb tag should be the globalProfileTag instead of the
        // ProfileViewModel, because the navigation menu item for this profile is the globalProfileTag.
        // See MainPage::UpdateSettings for why this matters
        const auto breadcrumbTag = profile.IsBaseLayer() ? box_value(globalProfileTag) : box_value(profile);
        const auto breadcrumbText = profile.IsBaseLayer() ? RS_(L"Nav_ProfileDefaults/Content") : profile.Name();
        _profileViewModelChangedRevoker = profile.PropertyChanged(winrt::auto_revoke, [=](auto&&, const PropertyChangedEventArgs& args) {
            const auto settingName{ args.PropertyName() };
            if (settingName == L"CurrentPage")
            {
                const auto currentPage = profile.CurrentPage();
                if (currentPage == ProfileSubPage::Base)
                {
                    contentFrame().Navigate(xaml_typename<Editor::Profiles_Base>(), state);
                    _breadcrumbs.Clear();
                    const auto crumb = winrt::make<Breadcrumb>(breadcrumbTag, breadcrumbText, BreadcrumbSubPage::None);
                    _breadcrumbs.Append(crumb);
                }
                else if (currentPage == ProfileSubPage::Appearance)
                {
                    contentFrame().Navigate(xaml_typename<Editor::Profiles_Appearance>(), state);
                    const auto crumb = winrt::make<Breadcrumb>(breadcrumbTag, RS_(L"Profile_Appearance/Header"), BreadcrumbSubPage::Profile_Appearance);
                    _breadcrumbs.Append(crumb);
                }
                else if (currentPage == ProfileSubPage::Advanced)
                {
                    contentFrame().Navigate(xaml_typename<Editor::Profiles_Advanced>(), state);
                    const auto crumb = winrt::make<Breadcrumb>(breadcrumbTag, RS_(L"Profile_Advanced/Header"), BreadcrumbSubPage::Profile_Advanced);
                    _breadcrumbs.Append(crumb);
                }
            }
        });
    }

    void MainPage::_Navigate(hstring clickedItemTag, BreadcrumbSubPage subPage)
    {
        _PreNavigateHelper();

        if (clickedItemTag == launchTag)
        {
            contentFrame().Navigate(xaml_typename<Editor::Launch>(), winrt::make<LaunchViewModel>(_settingsClone));
            const auto crumb = winrt::make<Breadcrumb>(box_value(clickedItemTag), RS_(L"Nav_Launch/Content"), BreadcrumbSubPage::None);
            _breadcrumbs.Append(crumb);
        }
        else if (clickedItemTag == interactionTag)
        {
            contentFrame().Navigate(xaml_typename<Editor::Interaction>(), winrt::make<InteractionViewModel>(_settingsClone.GlobalSettings()));
            const auto crumb = winrt::make<Breadcrumb>(box_value(clickedItemTag), RS_(L"Nav_Interaction/Content"), BreadcrumbSubPage::None);
            _breadcrumbs.Append(crumb);
        }
        else if (clickedItemTag == renderingTag)
        {
            contentFrame().Navigate(xaml_typename<Editor::Rendering>(), winrt::make<RenderingViewModel>(_settingsClone.GlobalSettings()));
            const auto crumb = winrt::make<Breadcrumb>(box_value(clickedItemTag), RS_(L"Nav_Rendering/Content"), BreadcrumbSubPage::None);
            _breadcrumbs.Append(crumb);
        }
        else if (clickedItemTag == actionsTag)
        {
            contentFrame().Navigate(xaml_typename<Editor::Actions>(), winrt::make<ActionsPageNavigationState>(_settingsClone));
            const auto crumb = winrt::make<Breadcrumb>(box_value(clickedItemTag), RS_(L"Nav_Actions/Content"), BreadcrumbSubPage::None);
            _breadcrumbs.Append(crumb);
        }
        else if (clickedItemTag == globalProfileTag)
        {
            auto profileVM{ _viewModelForProfile(_settingsClone.ProfileDefaults(), _settingsClone) };
            profileVM.IsBaseLayer(true);
            auto state{ winrt::make<ProfilePageNavigationState>(profileVM,
                                                                _settingsClone.GlobalSettings().ColorSchemes(),
                                                                *this) };

            _SetupProfileEventHandling(state);

            contentFrame().Navigate(xaml_typename<Editor::Profiles_Base>(), state);
            const auto crumb = winrt::make<Breadcrumb>(box_value(clickedItemTag), RS_(L"Nav_ProfileDefaults/Content"), BreadcrumbSubPage::None);
            _breadcrumbs.Append(crumb);

            // If we were given a label, make sure we are on the correct sub-page
            if (subPage == BreadcrumbSubPage::Profile_Appearance)
            {
                profileVM.CurrentPage(ProfileSubPage::Appearance);
            }
            else if (subPage == BreadcrumbSubPage::Profile_Advanced)
            {
                profileVM.CurrentPage(ProfileSubPage::Advanced);
            }
        }
        else if (clickedItemTag == colorSchemesTag)
        {
            contentFrame().Navigate(xaml_typename<Editor::ColorSchemes>(), _colorSchemesPageVM);
            const auto crumb = winrt::make<Breadcrumb>(box_value(clickedItemTag), RS_(L"Nav_ColorSchemes/Content"), BreadcrumbSubPage::None);
            _breadcrumbs.Append(crumb);
        }
        else if (clickedItemTag == globalAppearanceTag)
        {
            contentFrame().Navigate(xaml_typename<Editor::GlobalAppearance>(), winrt::make<GlobalAppearanceViewModel>(_settingsClone.GlobalSettings()));
            const auto crumb = winrt::make<Breadcrumb>(box_value(clickedItemTag), RS_(L"Nav_Appearance/Content"), BreadcrumbSubPage::None);
            _breadcrumbs.Append(crumb);
        }
        else if (clickedItemTag == addProfileTag)
        {
            auto addProfileState{ winrt::make<AddProfilePageNavigationState>(_settingsClone) };
            addProfileState.AddNew({ get_weak(), &MainPage::_AddProfileHandler });
            contentFrame().Navigate(xaml_typename<Editor::AddProfile>(), addProfileState);
            const auto crumb = winrt::make<Breadcrumb>(box_value(clickedItemTag), RS_(L"Nav_AddNewProfile/Content"), BreadcrumbSubPage::None);
            _breadcrumbs.Append(crumb);
        }
    }

    // Method Description:
    // - updates the content frame to present a view of the profile page
    // - NOTE: this does not update the selected item.
    // Arguments:
    // - profile - the profile object we are getting a view of
    void MainPage::_Navigate(const Editor::ProfileViewModel& profile, BreadcrumbSubPage subPage, const bool focusDeleteButton)
    {
        auto state{ winrt::make<ProfilePageNavigationState>(profile,
                                                            _settingsClone.GlobalSettings().ColorSchemes(),
                                                            *this) };
        state.FocusDeleteButton(focusDeleteButton);

        _PreNavigateHelper();

        _SetupProfileEventHandling(state);

        contentFrame().Navigate(xaml_typename<Editor::Profiles_Base>(), state);
        const auto crumb = winrt::make<Breadcrumb>(box_value(profile), profile.Name(), BreadcrumbSubPage::None);
        _breadcrumbs.Append(crumb);

        // Set the profile's 'CurrentPage' to the correct one, if this requires further navigation, the
        // event handler will do it
        if (subPage == BreadcrumbSubPage::None)
        {
            profile.CurrentPage(ProfileSubPage::Base);
        }
        else if (subPage == BreadcrumbSubPage::Profile_Appearance)
        {
            profile.CurrentPage(ProfileSubPage::Appearance);
        }
        else if (subPage == BreadcrumbSubPage::Profile_Advanced)
        {
            profile.CurrentPage(ProfileSubPage::Advanced);
        }
    }

    void MainPage::OpenJsonTapped(const IInspectable& /*sender*/, const Windows::UI::Xaml::Input::TappedRoutedEventArgs& /*args*/)
    {
        const auto window = CoreWindow::GetForCurrentThread();
        const auto rAltState = window.GetKeyState(VirtualKey::RightMenu);
        const auto lAltState = window.GetKeyState(VirtualKey::LeftMenu);
        const auto altPressed = WI_IsFlagSet(lAltState, CoreVirtualKeyStates::Down) ||
                                WI_IsFlagSet(rAltState, CoreVirtualKeyStates::Down);

        const auto target = altPressed ? SettingsTarget::DefaultsFile : SettingsTarget::SettingsFile;
        _OpenJsonHandlers(nullptr, target);
    }

    void MainPage::OpenJsonKeyDown(const IInspectable& /*sender*/, const Windows::UI::Xaml::Input::KeyRoutedEventArgs& args)
    {
        if (args.Key() == VirtualKey::Enter || args.Key() == VirtualKey::Space)
        {
            const auto target = args.KeyStatus().IsMenuKeyDown ? SettingsTarget::DefaultsFile : SettingsTarget::SettingsFile;
            _OpenJsonHandlers(nullptr, target);
        }
    }

    void MainPage::SaveButton_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*args*/)
    {
        _settingsClone.WriteSettingsToDisk();
    }

    void MainPage::ResetButton_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*args*/)
    {
        UpdateSettings(_settingsSource);
    }

    void MainPage::BreadcrumbBar_ItemClicked(const Microsoft::UI::Xaml::Controls::BreadcrumbBar& /*sender*/, const Microsoft::UI::Xaml::Controls::BreadcrumbBarItemClickedEventArgs& args)
    {
        if (gsl::narrow_cast<uint32_t>(args.Index()) < (_breadcrumbs.Size() - 1))
        {
            const auto tag = args.Item().as<Breadcrumb>()->Tag();
            const auto subPage = args.Item().as<Breadcrumb>()->SubPage();
            if (const auto profileViewModel = tag.try_as<ProfileViewModel>())
            {
                _Navigate(*profileViewModel, subPage);
            }
            else
            {
                _Navigate(tag.as<hstring>(), subPage);
            }
        }
    }

    void MainPage::_InitializeProfilesList()
    {
        const auto menuItems = SettingsNav().MenuItems();

        // Manually create a NavigationViewItem for each profile
        // and keep a reference to them in a map so that we
        // can easily modify the correct one when the associated
        // profile changes.
        for (const auto& profile : _settingsClone.AllProfiles())
        {
            if (!profile.Deleted())
            {
                auto navItem = _CreateProfileNavViewItem(_viewModelForProfile(profile, _settingsClone));
                Controls::ToolTipService::SetToolTip(navItem, box_value(profile.Name()));
                menuItems.Append(navItem);
            }
        }

        // Top off (the end of the nav view) with the Add Profile item
        MUX::Controls::NavigationViewItem addProfileItem;
        addProfileItem.Content(box_value(RS_(L"Nav_AddNewProfile/Content")));
        Controls::ToolTipService::SetToolTip(addProfileItem, box_value(RS_(L"Nav_AddNewProfile/Content")));
        addProfileItem.Tag(box_value(addProfileTag));

        FontIcon icon;
        // This is the "Add" symbol
        icon.Glyph(L"\xE710");
        addProfileItem.Icon(icon);

        menuItems.Append(addProfileItem);
    }

    void MainPage::_CreateAndNavigateToNewProfile(const uint32_t index, const Model::Profile& profile)
    {
        const auto newProfile{ profile ? profile : _settingsClone.CreateNewProfile() };
        const auto profileViewModel{ _viewModelForProfile(newProfile, _settingsClone) };
        const auto navItem{ _CreateProfileNavViewItem(profileViewModel) };
        SettingsNav().MenuItems().InsertAt(index, navItem);

        // Select and navigate to the new profile
        SettingsNav().SelectedItem(navItem);
        _Navigate(profileViewModel, BreadcrumbSubPage::None);
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

        // Add an event handler for when the user wants to delete a profile.
        profile.DeleteProfile({ this, &MainPage::_DeleteProfile });

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
        const auto newTag = newSelectedItem.as<MUX::Controls::NavigationViewItem>().Tag();
        if (const auto profileViewModel = newTag.try_as<ProfileViewModel>())
        {
            _Navigate(*profileViewModel, BreadcrumbSubPage::None);
        }
        else
        {
            _Navigate(newTag.as<hstring>(), BreadcrumbSubPage::None);
        }
    }

    IObservableVector<IInspectable> MainPage::Breadcrumbs() noexcept
    {
        return _breadcrumbs;
    }

    winrt::Windows::UI::Xaml::Media::Brush MainPage::BackgroundBrush()
    {
        return SettingsNav().Background();
    }

}
