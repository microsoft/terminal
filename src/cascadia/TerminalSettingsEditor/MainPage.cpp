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
#include "ProfileViewModel.h"
#include "GlobalAppearance.h"
#include "GlobalAppearanceViewModel.h"
#include "ColorSchemes.h"
#include "AddProfile.h"
#include "InteractionViewModel.h"
#include "LaunchViewModel.h"
#include "..\types\inc\utils.hpp"
#include <..\WinRTUtils\inc\Utils.h>

#include <LibraryResources.h>
#include <dwmapi.h>

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
        _UpdateBackgroundForMica();

        _colorSchemesPageVM = winrt::make<ColorSchemesPageViewModel>(_settingsClone);
        _colorSchemesPageViewModelChangedRevoker = _colorSchemesPageVM.PropertyChanged(winrt::auto_revoke, [=](auto&&, const PropertyChangedEventArgs& args) {
            const auto settingName{ args.PropertyName() };
            if (settingName == L"CurrentPage")
            {
                const auto currentScheme = _colorSchemesPageVM.CurrentScheme();
                if (_colorSchemesPageVM.CurrentPage() == ColorSchemesSubPage::EditColorScheme && currentScheme)
                {
                    contentFrame().Navigate(xaml_typename<Editor::EditColorScheme>(), currentScheme);
                    const auto crumb = winrt::make<Breadcrumb>(box_value(colorSchemesTag), currentScheme.Name(), BreadcrumbSubPage::ColorSchemes_Edit);
                    _breadcrumbs.Append(crumb);
                }
                else if (_colorSchemesPageVM.CurrentPage() == ColorSchemesSubPage::Base)
                {
                    _Navigate(winrt::hstring{ colorSchemesTag }, BreadcrumbSubPage::None);
                }
            }
            else if (settingName == L"CurrentSchemeName")
            {
                // this is not technically a setting, it is the ColorSchemesPageVM telling us the breadcrumb item needs to be updated because of a rename
                _breadcrumbs.RemoveAtEnd();
                const auto crumb = winrt::make<Breadcrumb>(box_value(colorSchemesTag), _colorSchemesPageVM.CurrentScheme().Name(), BreadcrumbSubPage::ColorSchemes_Edit);
                _breadcrumbs.Append(crumb);
            }
        });

        // Make sure to initialize the profiles _after_ we have initialized the color schemes page VM, because we pass
        // that VM into the appearance VMs within the profiles
        _InitializeProfilesList();

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

        _UpdateBackgroundForMica();

        // Deduce information about the currently selected item
        IInspectable lastBreadcrumb;
        const auto size = _breadcrumbs.Size();
        if (size > 0)
        {
            lastBreadcrumb = _breadcrumbs.GetAt(size - 1);
        }

        // Collect only the first items out of the menu item source, the static
        // ones that we don't want to regenerate.
        //
        // By manipulating a MenuItemsSource this way, rather than manipulating the
        // MenuItems directly, we avoid a crash in WinUI.
        //
        // By making the vector only _originalNumItems big to start, GetMany
        // will only fill that number of elements out of the current source.
        std::vector<IInspectable> menuItemsSTL(_originalNumItems, nullptr);
        _menuItemSource.GetMany(0, menuItemsSTL);
        // now, just stick them back in.
        _menuItemSource.ReplaceAll(menuItemsSTL);

        // Repopulate profile-related menu items
        _InitializeProfilesList();
        // Update the Nav State with the new version of the settings
        _colorSchemesPageVM.UpdateSettings(_settingsClone);

        // We'll update the profile in the _profilesNavState whenever we actually navigate to one

        // now that the menuItems are repopulated,
        // refresh the current page using the breadcrumb data we collected before the refresh
        if (const auto& crumb{ lastBreadcrumb.try_as<Breadcrumb>() })
        {
            for (const auto& item : _menuItemSource)
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
        const auto& firstItem{ _menuItemSource.GetAt(0).as<MUX::Controls::NavigationViewItem>() };
        SettingsNav().SelectedItem(firstItem);
        _Navigate(unbox_value<hstring>(firstItem.Tag()), BreadcrumbSubPage::None);
    }

    void MainPage::SetHostingWindow(uint64_t hostingWindow) noexcept
    {
        _hostingHwnd.emplace(reinterpret_cast<HWND>(hostingWindow));
        // Now that we have a HWND, update our own BG to account for if that
        // window is using mica or not.
        _UpdateBackgroundForMica();
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
        if (_menuItemSource)
        {
            _menuItemSource.IndexOf(selectedItem, insertIndex);
        }
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
            else
            {
                // If we are navigating to a new page, scroll to the top
                SettingsMainPage_ScrollViewer().ScrollToVerticalOffset(0);
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

    void MainPage::_SetupProfileEventHandling(const Editor::ProfileViewModel profile)
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
                    contentFrame().Navigate(xaml_typename<Editor::Profiles_Base>(), winrt::make<implementation::NavigateToProfileArgs>(profile, *this));
                    _breadcrumbs.Clear();
                    const auto crumb = winrt::make<Breadcrumb>(breadcrumbTag, breadcrumbText, BreadcrumbSubPage::None);
                    _breadcrumbs.Append(crumb);
                }
                else if (currentPage == ProfileSubPage::Appearance)
                {
                    contentFrame().Navigate(xaml_typename<Editor::Profiles_Appearance>(), winrt::make<implementation::NavigateToProfileArgs>(profile, *this));
                    const auto crumb = winrt::make<Breadcrumb>(breadcrumbTag, RS_(L"Profile_Appearance/Header"), BreadcrumbSubPage::Profile_Appearance);
                    _breadcrumbs.Append(crumb);
                    SettingsMainPage_ScrollViewer().ScrollToVerticalOffset(0);
                }
                else if (currentPage == ProfileSubPage::Advanced)
                {
                    contentFrame().Navigate(xaml_typename<Editor::Profiles_Advanced>(), profile);
                    const auto crumb = winrt::make<Breadcrumb>(breadcrumbTag, RS_(L"Profile_Advanced/Header"), BreadcrumbSubPage::Profile_Advanced);
                    _breadcrumbs.Append(crumb);
                    SettingsMainPage_ScrollViewer().ScrollToVerticalOffset(0);
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
            contentFrame().Navigate(xaml_typename<Editor::Rendering>(), winrt::make<RenderingViewModel>(_settingsClone));
            const auto crumb = winrt::make<Breadcrumb>(box_value(clickedItemTag), RS_(L"Nav_Rendering/Content"), BreadcrumbSubPage::None);
            _breadcrumbs.Append(crumb);
        }
        else if (clickedItemTag == actionsTag)
        {
            contentFrame().Navigate(xaml_typename<Editor::Actions>(), winrt::make<ActionsViewModel>(_settingsClone));
            const auto crumb = winrt::make<Breadcrumb>(box_value(clickedItemTag), RS_(L"Nav_Actions/Content"), BreadcrumbSubPage::None);
            _breadcrumbs.Append(crumb);
        }
        else if (clickedItemTag == globalProfileTag)
        {
            auto profileVM{ _viewModelForProfile(_settingsClone.ProfileDefaults(), _settingsClone) };
            profileVM.SetupAppearances(_colorSchemesPageVM.AllColorSchemes());
            profileVM.IsBaseLayer(true);

            _SetupProfileEventHandling(profileVM);

            contentFrame().Navigate(xaml_typename<Editor::Profiles_Base>(), winrt::make<implementation::NavigateToProfileArgs>(profileVM, *this));
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
            const auto crumb = winrt::make<Breadcrumb>(box_value(clickedItemTag), RS_(L"Nav_ColorSchemes/Content"), BreadcrumbSubPage::None);
            _breadcrumbs.Append(crumb);
            contentFrame().Navigate(xaml_typename<Editor::ColorSchemes>(), _colorSchemesPageVM);

            if (subPage == BreadcrumbSubPage::ColorSchemes_Edit)
            {
                _colorSchemesPageVM.CurrentPage(ColorSchemesSubPage::EditColorScheme);
            }
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
    void MainPage::_Navigate(const Editor::ProfileViewModel& profile, BreadcrumbSubPage subPage)
    {
        _PreNavigateHelper();

        _SetupProfileEventHandling(profile);

        contentFrame().Navigate(xaml_typename<Editor::Profiles_Base>(), winrt::make<implementation::NavigateToProfileArgs>(profile, *this));
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
        const auto& itemSource{ SettingsNav().MenuItemsSource() };
        if (!itemSource)
        {
            // There wasn't a MenuItemsSource set yet? The only way that's
            // possible is if we haven't used
            // _MoveXamlParsedNavItemsIntoItemSource to move the hardcoded menu
            // entries from XAML into our runtime menu item source. Do that now.

            _MoveXamlParsedNavItemsIntoItemSource();
        }

        // Manually create a NavigationViewItem for each profile
        // and keep a reference to them in a map so that we
        // can easily modify the correct one when the associated
        // profile changes.
        for (const auto& profile : _settingsClone.AllProfiles())
        {
            if (!profile.Deleted())
            {
                auto profileVM = _viewModelForProfile(profile, _settingsClone);
                profileVM.SetupAppearances(_colorSchemesPageVM.AllColorSchemes());
                auto navItem = _CreateProfileNavViewItem(profileVM);
                _menuItemSource.Append(navItem);
            }
        }

        // Top off (the end of the nav view) with the Add Profile item
        MUX::Controls::NavigationViewItem addProfileItem;
        addProfileItem.Content(box_value(RS_(L"Nav_AddNewProfile/Content")));
        addProfileItem.Tag(box_value(addProfileTag));

        FontIcon icon;
        // This is the "Add" symbol
        icon.Glyph(L"\xE710");
        addProfileItem.Icon(icon);

        _menuItemSource.Append(addProfileItem);
    }

    // BODGY
    // Does the very wacky business of moving all our MenuItems that we
    // hardcoded in XAML into a runtime MenuItemsSource. We'll then use _that_
    // MenuItemsSource as the source for our nav view entries instead. This
    // lets us hardcode the initial entries in precompiled XAML, but then adjust
    // the items at runtime. Without using a MenuItemsSource, the NavView just
    // crashes when items are removed (see GH#13673)
    void MainPage::_MoveXamlParsedNavItemsIntoItemSource()
    {
        if (SettingsNav().MenuItemsSource())
        {
            // We've already copied over the original items to a source. We can
            // just skip this now.
            return;
        }

        auto menuItems{ SettingsNav().MenuItems() };
        _originalNumItems = menuItems.Size();
        // Remove all the existing items, and move them to a separate vector
        // that we'll use as a MenuItemsSource. By doing this, we avoid a WinUI
        // bug (MUX#6302) where modifying the NavView.Items() directly causes a
        // crash. By leaving these static entries in XAML, we maintain the
        // benefit of instantiating them from the XBF, rather than at runtime.
        //
        // --> Copy it into an STL vector to simplify our code and reduce COM overhead.
        auto original = std::vector<IInspectable>{ _originalNumItems, nullptr };
        menuItems.GetMany(0, original);

        _menuItemSource = winrt::single_threaded_observable_vector<IInspectable>(std::move(original));

        SettingsNav().MenuItemsSource(_menuItemSource);
    }

    void MainPage::_CreateAndNavigateToNewProfile(const uint32_t index, const Model::Profile& profile)
    {
        const auto newProfile{ profile ? profile : _settingsClone.CreateNewProfile() };
        const auto profileViewModel{ _viewModelForProfile(newProfile, _settingsClone) };
        profileViewModel.SetupAppearances(_colorSchemesPageVM.AllColorSchemes());
        const auto navItem{ _CreateProfileNavViewItem(profileViewModel) };

        if (_menuItemSource)
        {
            _menuItemSource.InsertAt(index, navItem);
        }

        // Select and navigate to the new profile
        SettingsNav().SelectedItem(navItem);
        _Navigate(profileViewModel, BreadcrumbSubPage::None);
    }

    MUX::Controls::NavigationViewItem MainPage::_CreateProfileNavViewItem(const Editor::ProfileViewModel& profile)
    {
        MUX::Controls::NavigationViewItem profileNavItem;
        profileNavItem.Content(box_value(profile.Name()));
        profileNavItem.Tag(box_value<Editor::ProfileViewModel>(profile));
        profileNavItem.Icon(UI::IconPathConverter::IconWUX(profile.EvaluatedIcon()));

        // Update the menu item when the icon/name changes
        auto weakMenuItem{ make_weak(profileNavItem) };
        profile.PropertyChanged([weakMenuItem](const auto&, const WUX::Data::PropertyChangedEventArgs& args) {
            if (auto menuItem{ weakMenuItem.get() })
            {
                const auto& tag{ menuItem.Tag().as<Editor::ProfileViewModel>() };
                if (args.PropertyName() == L"Icon")
                {
                    menuItem.Icon(UI::IconPathConverter::IconWUX(tag.Icon()));
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
        if (_menuItemSource)
        {
            _menuItemSource.IndexOf(selectedItem, index);
            _menuItemSource.RemoveAt(index);

            // navigate to the profile next to this one
            const auto newSelectedItem{ _menuItemSource.GetAt(index < _menuItemSource.Size() - 1 ? index : index - 1) };
            SettingsNav().SelectedItem(newSelectedItem);
            const auto newTag = newSelectedItem.as<MUX::Controls::NavigationViewItem>().Tag();
            if (const auto profileViewModel = newTag.try_as<ProfileViewModel>())
            {
                profileViewModel->FocusDeleteButton(true);
                _Navigate(*profileViewModel, BreadcrumbSubPage::None);
            }
            else
            {
                _Navigate(newTag.as<hstring>(), BreadcrumbSubPage::None);
            }
            // Since we are navigating to a new profile after deletion, scroll up to the top
            SettingsMainPage_ScrollViewer().ChangeView(nullptr, 0.0, nullptr);
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

    // If the theme asks for Mica, then drop out our background, so that we
    // can have mica too.
    void MainPage::_UpdateBackgroundForMica()
    {
        bool isMicaAvailable = false;

        // Check to see if our hosting window supports Mica at all. We'll check
        // to see if the window has Mica enabled - if it does, then we can
        // assume that it supports Mica.
        //
        // We're doing this instead of checking if we're on Windows build 22621
        // or higher.
        if (_hostingHwnd.has_value())
        {
            int attribute = DWMSBT_NONE;
            const auto hr = DwmGetWindowAttribute(*_hostingHwnd, DWMWA_SYSTEMBACKDROP_TYPE, &attribute, sizeof(attribute));
            if (SUCCEEDED(hr))
            {
                isMicaAvailable = attribute == DWMSBT_MAINWINDOW;
            }
        }

        if (!isMicaAvailable)
        {
            return;
        }

        const auto& theme = _settingsSource.GlobalSettings().CurrentTheme();
        const auto& requestedTheme = _settingsSource.GlobalSettings().CurrentTheme().RequestedTheme();

        RequestedTheme(requestedTheme);

        const auto bgKey = (theme.Window() != nullptr && theme.Window().UseMica()) ?
                               L"SettingsPageMicaBackground" :
                               L"SettingsPageBackground";

        // remember to use ThemeLookup to get the actual correct color for the
        // currently requested theme.
        if (const auto bgColor = ThemeLookup(Resources(), requestedTheme, winrt::box_value(bgKey)))
        {
            SettingsNav().Background(winrt::WUX::Media::SolidColorBrush(winrt::unbox_value<Windows::UI::Color>(bgColor)));
        }
    }

}
