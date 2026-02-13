// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "MainPage.h"
#include "MainPage.g.cpp"
#include "Launch.h"
#include "Interaction.h"
#include "Compatibility.h"
#include "Rendering.h"
#include "RenderingViewModel.h"
#include "Extensions.h"
#include "Actions.h"
#include "ProfileViewModel.h"
#include "GlobalAppearance.h"
#include "GlobalAppearanceViewModel.h"
#include "ColorSchemes.h"
#include "EditColorScheme.h"
#include "AddProfile.h"
#include "InteractionViewModel.h"
#include "LaunchViewModel.h"
#include "NewTabMenuViewModel.h"
#include "NewTabMenu.h"
#include "NavConstants.h"
#include "..\types\inc\utils.hpp"
#include <..\WinRTUtils\inc\Utils.h>

#include <dwmapi.h>
#include <fmt/compile.h>

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

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    static WUX::Controls::FontIcon _fontIconForNavTag(const std::wstring_view navTag)
    {
        WUX::Controls::FontIcon icon{};
        icon.Glyph(NavTagIconMap[navTag]);
        icon.FontFamily(Media::FontFamily{ L"Segoe Fluent Icons, Segoe MDL2 Assets" });
        icon.FontSize(16);
        return icon;
    }

    static Editor::ProfileViewModel _viewModelForProfile(const Model::Profile& profile, const Model::CascadiaSettings& appSettings, const Windows::UI::Core::CoreDispatcher& dispatcher)
    {
        return winrt::make<implementation::ProfileViewModel>(profile, appSettings, dispatcher);
    }

    static ProfileSubPage ProfileSubPageFromBreadcrumb(BreadcrumbSubPage subPage)
    {
        switch (subPage)
        {
        case BreadcrumbSubPage::None:
            return ProfileSubPage::Base;
        case BreadcrumbSubPage::Profile_Appearance:
            return ProfileSubPage::Appearance;
        case BreadcrumbSubPage::Profile_Terminal:
            return ProfileSubPage::Terminal;
        case BreadcrumbSubPage::Profile_Advanced:
            return ProfileSubPage::Advanced;
        default:
            // This should never happen
            assert(false);
            return ProfileSubPage::Base;
        }
    }

    MainPage::MainPage(const CascadiaSettings& settings) :
        _settingsSource{ settings },
        _settingsClone{ settings.Copy() },
        _profileVMs{ single_threaded_observable_vector<Editor::ProfileViewModel>() }
    {
        InitializeComponent();
        _UpdateBackgroundForMica();

        _newTabMenuPageVM = winrt::make<NewTabMenuViewModel>(_settingsClone);
        _ntmViewModelChangedRevoker = _newTabMenuPageVM.PropertyChanged(winrt::auto_revoke, [this](auto&&, const PropertyChangedEventArgs& args) {
            const auto settingName{ args.PropertyName() };
            if (settingName == L"CurrentFolder")
            {
                if (const auto& currentFolder = _newTabMenuPageVM.CurrentFolder())
                {
                    const auto crumb = winrt::make<Breadcrumb>(box_value(currentFolder), currentFolder.Name(), BreadcrumbSubPage::NewTabMenu_Folder);
                    _breadcrumbs.Append(crumb);
                    SettingsMainPage_ScrollViewer().ScrollToVerticalOffset(0);
                }
                else
                {
                    // If we don't have a current folder, we're at the root of the NTM
                    _breadcrumbs.Clear();
                    const auto crumb = winrt::make<Breadcrumb>(box_value(newTabMenuTag), RS_(L"Nav_NewTabMenu/Content"), BreadcrumbSubPage::None);
                    _breadcrumbs.Append(crumb);
                }
                contentFrame().Navigate(xaml_typename<Editor::NewTabMenu>(), winrt::make<NavigateToPageArgs>(_newTabMenuPageVM, *this));
            }
        });

        _colorSchemesPageVM = winrt::make<ColorSchemesPageViewModel>(_settingsClone);
        _colorSchemesPageViewModelChangedRevoker = _colorSchemesPageVM.PropertyChanged(winrt::auto_revoke, [=](auto&&, const PropertyChangedEventArgs& args) {
            const auto settingName{ args.PropertyName() };
            if (settingName == L"CurrentPage")
            {
                // extract ElementToFocus and clear it; we only want to use it once
                auto vmImpl = get_self<ColorSchemesPageViewModel>(_colorSchemesPageVM);
                const auto elementToFocus = vmImpl->ElementToFocus();
                vmImpl->ElementToFocus({});

                const auto currentScheme = _colorSchemesPageVM.CurrentScheme();
                if (_colorSchemesPageVM.CurrentPage() == ColorSchemesSubPage::EditColorScheme && currentScheme)
                {
                    contentFrame().Navigate(xaml_typename<Editor::EditColorScheme>(), winrt::make<NavigateToPageArgs>(currentScheme, *this, elementToFocus));
                    const auto crumb = winrt::make<Breadcrumb>(box_value(colorSchemesTag), currentScheme.Name(), BreadcrumbSubPage::ColorSchemes_Edit);
                    _breadcrumbs.Append(crumb);
                }
                else if (_colorSchemesPageVM.CurrentPage() == ColorSchemesSubPage::Base)
                {
                    _Navigate(winrt::hstring{ colorSchemesTag }, BreadcrumbSubPage::None, elementToFocus);
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

        _actionsVM = winrt::make<ActionsViewModel>(_settingsClone);
        _actionsViewModelChangedRevoker = _actionsVM.PropertyChanged(winrt::auto_revoke, [=](auto&&, const PropertyChangedEventArgs& args) {
            const auto settingName{ args.PropertyName() };
            if (settingName == L"CurrentPage")
            {
                if (_actionsVM.CurrentPage() == ActionsSubPage::Edit)
                {
                    contentFrame().Navigate(xaml_typename<Editor::EditAction>(), winrt::make<NavigateToPageArgs>(_actionsVM.CurrentCommand(), *this));
                    const auto crumb = winrt::make<Breadcrumb>(box_value(actionsTag), RS_(L"Nav_EditAction/Content"), BreadcrumbSubPage::Actions_Edit);
                    _breadcrumbs.Append(crumb);
                }
                else if (_actionsVM.CurrentPage() == ActionsSubPage::Base)
                {
                    _Navigate(winrt::hstring{ actionsTag }, BreadcrumbSubPage::None);
                }
            }
        });

        auto extensionsVMImpl = winrt::make_self<ExtensionsViewModel>(_settingsClone, _colorSchemesPageVM);
        extensionsVMImpl->NavigateToProfileRequested({ this, &MainPage::_NavigateToProfileHandler });
        extensionsVMImpl->NavigateToColorSchemeRequested({ this, &MainPage::_NavigateToColorSchemeHandler });
        _extensionsVM = *extensionsVMImpl;
        _extensionsViewModelChangedRevoker = _extensionsVM.PropertyChanged(winrt::auto_revoke, [=](auto&&, const PropertyChangedEventArgs& args) {
            const auto settingName{ args.PropertyName() };
            if (settingName == L"CurrentExtensionPackage")
            {
                if (const auto& currentExtensionPackage = _extensionsVM.CurrentExtensionPackage())
                {
                    const auto crumb = winrt::make<Breadcrumb>(box_value(currentExtensionPackage), currentExtensionPackage.DisplayName(), BreadcrumbSubPage::Extensions_Extension);
                    _breadcrumbs.Append(crumb);
                    SettingsMainPage_ScrollViewer().ScrollToVerticalOffset(0);
                }
                else
                {
                    // If we don't have a current extension package, we're at the root of the Extensions page
                    _breadcrumbs.Clear();
                    const auto crumb = winrt::make<Breadcrumb>(box_value(extensionsTag), RS_(L"Nav_Extensions/Content"), BreadcrumbSubPage::None);
                    _breadcrumbs.Append(crumb);
                }
                contentFrame().Navigate(xaml_typename<Editor::Extensions>(), winrt::make<NavigateToPageArgs>(_extensionsVM, *this));
            }
        });

        // Make sure to initialize the profiles _after_ we have initialized the color schemes page VM, because we pass
        // that VM into the appearance VMs within the profiles
        _InitializeProfilesList();

        // Apply icons to static nav items
        LaunchNavItem().Icon(_fontIconForNavTag(launchTag));
        InteractionNavItem().Icon(_fontIconForNavTag(interactionTag));
        AppearanceNavItem().Icon(_fontIconForNavTag(globalAppearanceTag));
        ColorSchemesNavItem().Icon(_fontIconForNavTag(colorSchemesTag));
        RenderingNavItem().Icon(_fontIconForNavTag(renderingTag));
        CompatibilityNavItem().Icon(_fontIconForNavTag(compatibilityTag));
        ActionsNavItem().Icon(_fontIconForNavTag(actionsTag));
        NewTabMenuNavItem().Icon(_fontIconForNavTag(newTabMenuTag));
        ExtensionsNavItem().Icon(_fontIconForNavTag(extensionsTag));
        BaseLayerMenuItem().Icon(_fontIconForNavTag(globalProfileTag));
        OpenJsonNavItem().Icon(_fontIconForNavTag(openJsonTag));

        Automation::AutomationProperties::SetHelpText(SaveButton(), RS_(L"Settings_SaveSettingsButton/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"));
        Automation::AutomationProperties::SetHelpText(ResetButton(), RS_(L"Settings_ResetSettingsButton/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"));
        Automation::AutomationProperties::SetHelpText(OpenJsonNavItem(), RS_(L"Nav_OpenJSON/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"));

        // GH#19688: the nav item text may be truncated, so we need to set tooltips for all nav items. Reuse the displayed text resources.
        WUX::Controls::ToolTipService::SetToolTip(LaunchNavItem(), box_value(RS_(L"Nav_Launch/Content")));
        WUX::Controls::ToolTipService::SetToolTip(InteractionNavItem(), box_value(RS_(L"Nav_Interaction/Content")));
        WUX::Controls::ToolTipService::SetToolTip(AppearanceNavItem(), box_value(RS_(L"Nav_Appearance/Content")));
        WUX::Controls::ToolTipService::SetToolTip(ColorSchemesNavItem(), box_value(RS_(L"Nav_ColorSchemes/Content")));
        WUX::Controls::ToolTipService::SetToolTip(RenderingNavItem(), box_value(RS_(L"Nav_Rendering/Content")));
        WUX::Controls::ToolTipService::SetToolTip(CompatibilityNavItem(), box_value(RS_(L"Nav_Compatibility/Content")));
        WUX::Controls::ToolTipService::SetToolTip(ActionsNavItem(), box_value(RS_(L"Nav_Actions/Content")));
        WUX::Controls::ToolTipService::SetToolTip(NewTabMenuNavItem(), box_value(RS_(L"Nav_NewTabMenu/Content")));
        WUX::Controls::ToolTipService::SetToolTip(ExtensionsNavItem(), box_value(RS_(L"Nav_Extensions/Content")));
        WUX::Controls::ToolTipService::SetToolTip(BaseLayerMenuItem(), box_value(RS_(L"Nav_ProfileDefaults/Content")));
        WUX::Controls::ToolTipService::SetToolTip(OpenJsonNavItem(), box_value(RS_(L"Nav_OpenJSON/Content")));

        _breadcrumbs = single_threaded_observable_vector<IInspectable>();
        _UpdateSearchIndex();
        extensionsVMImpl->LazyLoadExtensions();
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
        _actionsVM.UpdateSettings(_settingsClone);
        _newTabMenuPageVM.UpdateSettings(_settingsClone);
        _extensionsVM.UpdateSettings(_settingsClone, _colorSchemesPageVM);
        _profileDefaultsVM = nullptr; // Lazy-loaded upon navigation

        // We'll update the profile in the _profilesNavState whenever we actually navigate to one

        // now that the menuItems are repopulated,
        // refresh the current page using the breadcrumb data we collected before the refresh
        if (const auto& crumb{ lastBreadcrumb.try_as<Breadcrumb>() }; crumb && crumb->Tag())
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
                                    _Navigate(*breadcrumbStringTag, crumb->SubPage());
                                    SettingsNav().SelectedItem(item);
                                    return;
                                }
                            }
                            else if (const auto& breadcrumbFolderEntry{ crumb->Tag().try_as<Editor::FolderEntryViewModel>() })
                            {
                                if (stringTag == newTabMenuTag)
                                {
                                    // navigate to the NewTabMenu page,
                                    // _Navigate() will handle trying to find the right subpage
                                    _Navigate(breadcrumbFolderEntry, BreadcrumbSubPage::NewTabMenu_Folder);
                                    SettingsNav().SelectedItem(item);
                                    return;
                                }
                            }
                            else if (const auto& breadcrumbExtensionPackage{ crumb->Tag().try_as<Editor::ExtensionPackageViewModel>() })
                            {
                                if (stringTag == extensionsTag)
                                {
                                    // navigate to the Extensions page,
                                    // _Navigate() will handle trying to find the right subpage
                                    _Navigate(breadcrumbExtensionPackage, BreadcrumbSubPage::Extensions_Extension);
                                    SettingsNav().SelectedItem(item);
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
                                    _Navigate(*profileTag, crumb->SubPage());
                                    SettingsNav().SelectedItem(item);
                                    return;
                                }
                            }
                        }
                    }
                }
            }
        }

        // Couldn't find the selected item, fall back to first menu item
        // This happens when the selected item was a profile which doesn't exist in the new configuration
        // We can use menuItemsSTL here because the only things they miss are profile entries.
        const auto& firstItem{ _menuItemSource.GetAt(0).as<MUX::Controls::NavigationViewItem>() };
        _Navigate(unbox_value<hstring>(firstItem.Tag()), BreadcrumbSubPage::None);
        SettingsNav().SelectedItem(firstItem);

        _UpdateSearchIndex();
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
                if (*navString == openJsonTag)
                {
                    const auto window = CoreWindow::GetForCurrentThread();
                    const auto rAltState = window.GetKeyState(VirtualKey::RightMenu);
                    const auto lAltState = window.GetKeyState(VirtualKey::LeftMenu);
                    const auto altPressed = WI_IsFlagSet(lAltState, CoreVirtualKeyStates::Down) ||
                                            WI_IsFlagSet(rAltState, CoreVirtualKeyStates::Down);
                    const auto target = altPressed ? SettingsTarget::DefaultsFile : SettingsTarget::SettingsFile;

                    TraceLoggingWrite(
                        g_hTerminalSettingsEditorProvider,
                        "OpenJson",
                        TraceLoggingDescription("Event emitted when the user clicks the Open JSON button in the settings UI"),
                        TraceLoggingValue(target == SettingsTarget::DefaultsFile ? "DefaultsFile" : "SettingsFile", "SettingsTarget", "The target settings file"),
                        TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
                        TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));

                    OpenJson.raise(nullptr, target);
                    return;
                }
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
                // extract ElementToFocus and clear it; we only want to use it once
                auto vmImpl = get_self<ProfileViewModel>(profile);
                const auto elementToFocus = vmImpl->ElementToFocus();
                vmImpl->ElementToFocus({});

                const auto currentPage = profile.CurrentPage();
                if (currentPage == ProfileSubPage::Base)
                {
                    contentFrame().Navigate(xaml_typename<Editor::Profiles_Base>(), winrt::make<NavigateToPageArgs>(profile, *this, elementToFocus));
                    _breadcrumbs.Clear();
                    const auto crumb = winrt::make<Breadcrumb>(breadcrumbTag, breadcrumbText, BreadcrumbSubPage::None);
                    _breadcrumbs.Append(crumb);
                    SettingsMainPage_ScrollViewer().ScrollToVerticalOffset(0);
                }
                else if (currentPage == ProfileSubPage::Appearance)
                {
                    contentFrame().Navigate(xaml_typename<Editor::Profiles_Appearance>(), winrt::make<NavigateToPageArgs>(profile, *this, elementToFocus));
                    const auto crumb = winrt::make<Breadcrumb>(breadcrumbTag, RS_(L"Profile_Appearance/Header"), BreadcrumbSubPage::Profile_Appearance);
                    _breadcrumbs.Append(crumb);
                    SettingsMainPage_ScrollViewer().ScrollToVerticalOffset(0);
                }
                else if (currentPage == ProfileSubPage::Terminal)
                {
                    contentFrame().Navigate(xaml_typename<Editor::Profiles_Terminal>(), winrt::make<NavigateToPageArgs>(profile, *this, elementToFocus));
                    const auto crumb = winrt::make<Breadcrumb>(breadcrumbTag, RS_(L"Profile_Terminal/Header"), BreadcrumbSubPage::Profile_Terminal);
                    _breadcrumbs.Append(crumb);
                    SettingsMainPage_ScrollViewer().ScrollToVerticalOffset(0);
                }
                else if (currentPage == ProfileSubPage::Advanced)
                {
                    contentFrame().Navigate(xaml_typename<Editor::Profiles_Advanced>(), winrt::make<NavigateToPageArgs>(profile, *this, elementToFocus));
                    const auto crumb = winrt::make<Breadcrumb>(breadcrumbTag, RS_(L"Profile_Advanced/Header"), BreadcrumbSubPage::Profile_Advanced);
                    _breadcrumbs.Append(crumb);
                    SettingsMainPage_ScrollViewer().ScrollToVerticalOffset(0);
                }
            }
        });
    }

    void MainPage::_Navigate(hstring clickedItemTag, BreadcrumbSubPage subPage, hstring elementToFocus)
    {
        _PreNavigateHelper();

        if (clickedItemTag == launchTag)
        {
            contentFrame().Navigate(xaml_typename<Editor::Launch>(), winrt::make<NavigateToPageArgs>(winrt::make<LaunchViewModel>(_settingsClone), *this, elementToFocus));
            const auto crumb = winrt::make<Breadcrumb>(box_value(clickedItemTag), RS_(L"Nav_Launch/Content"), BreadcrumbSubPage::None);
            _breadcrumbs.Append(crumb);
            SettingsNav().SelectedItem(LaunchNavItem());
        }
        else if (clickedItemTag == interactionTag)
        {
            contentFrame().Navigate(xaml_typename<Editor::Interaction>(), winrt::make<NavigateToPageArgs>(winrt::make<InteractionViewModel>(_settingsClone.GlobalSettings()), *this, elementToFocus));
            const auto crumb = winrt::make<Breadcrumb>(box_value(clickedItemTag), RS_(L"Nav_Interaction/Content"), BreadcrumbSubPage::None);
            _breadcrumbs.Append(crumb);
            SettingsNav().SelectedItem(InteractionNavItem());
        }
        else if (clickedItemTag == renderingTag)
        {
            contentFrame().Navigate(xaml_typename<Editor::Rendering>(), winrt::make<NavigateToPageArgs>(winrt::make<RenderingViewModel>(_settingsClone), *this, elementToFocus));
            const auto crumb = winrt::make<Breadcrumb>(box_value(clickedItemTag), RS_(L"Nav_Rendering/Content"), BreadcrumbSubPage::None);
            _breadcrumbs.Append(crumb);
            SettingsNav().SelectedItem(RenderingNavItem());
        }
        else if (clickedItemTag == compatibilityTag)
        {
            contentFrame().Navigate(xaml_typename<Editor::Compatibility>(), winrt::make<NavigateToPageArgs>(winrt::make<CompatibilityViewModel>(_settingsClone), *this, elementToFocus));
            const auto crumb = winrt::make<Breadcrumb>(box_value(clickedItemTag), RS_(L"Nav_Compatibility/Content"), BreadcrumbSubPage::None);
            _breadcrumbs.Append(crumb);
            SettingsNav().SelectedItem(CompatibilityNavItem());
        }
        else if (clickedItemTag == actionsTag)
        {
            const auto crumb = winrt::make<Breadcrumb>(box_value(clickedItemTag), RS_(L"Nav_Actions/Content"), BreadcrumbSubPage::None);
            _breadcrumbs.Append(crumb);
            contentFrame().Navigate(xaml_typename<Editor::Actions>(), winrt::make<NavigateToPageArgs>(_actionsVM, *this, elementToFocus));
            SettingsNav().SelectedItem(ActionsNavItem());

            if (subPage == BreadcrumbSubPage::Actions_Edit && _actionsVM.CurrentCommand() != nullptr)
            {
                _actionsVM.CurrentPage(ActionsSubPage::Edit);
            }
        }
        else if (clickedItemTag == newTabMenuTag)
        {
            if (_newTabMenuPageVM.CurrentFolder())
            {
                // Setting CurrentFolder triggers the PropertyChanged event,
                // which will navigate to the correct page and update the breadcrumbs appropriately
                _newTabMenuPageVM.CurrentFolder(nullptr);
            }
            else
            {
                // Navigate to the NewTabMenu page
                contentFrame().Navigate(xaml_typename<Editor::NewTabMenu>(), winrt::make<NavigateToPageArgs>(_newTabMenuPageVM, *this, elementToFocus));
                const auto crumb = winrt::make<Breadcrumb>(box_value(clickedItemTag), RS_(L"Nav_NewTabMenu/Content"), BreadcrumbSubPage::None);
                _breadcrumbs.Append(crumb);
            }
            SettingsNav().SelectedItem(NewTabMenuNavItem());
        }
        else if (clickedItemTag == extensionsTag)
        {
            if (_extensionsVM.CurrentExtensionPackage())
            {
                // Setting CurrentExtensionPackage triggers the PropertyChanged event,
                // which will navigate to the correct page and update the breadcrumbs appropriately
                _extensionsVM.CurrentExtensionPackage(nullptr);
            }
            else
            {
                contentFrame().Navigate(xaml_typename<Editor::Extensions>(), winrt::make<NavigateToPageArgs>(_extensionsVM, *this, elementToFocus));
                const auto crumb = winrt::make<Breadcrumb>(box_value(clickedItemTag), RS_(L"Nav_Extensions/Content"), BreadcrumbSubPage::None);
                _breadcrumbs.Append(crumb);
            }
            SettingsNav().SelectedItem(ExtensionsNavItem());
        }
        else if (clickedItemTag == globalProfileTag)
        {
            // lazy load profile defaults VM
            if (!_profileDefaultsVM)
            {
                _profileDefaultsVM = _viewModelForProfile(_settingsClone.ProfileDefaults(), _settingsClone, Dispatcher());
                _profileDefaultsVM.SetupAppearances(_colorSchemesPageVM.AllColorSchemes());
                _profileDefaultsVM.IsBaseLayer(true);
            }

            _SetupProfileEventHandling(_profileDefaultsVM);

            contentFrame().Navigate(xaml_typename<Editor::Profiles_Base>(), winrt::make<NavigateToPageArgs>(_profileDefaultsVM, *this, elementToFocus));
            const auto crumb = winrt::make<Breadcrumb>(box_value(clickedItemTag), RS_(L"Nav_ProfileDefaults/Content"), BreadcrumbSubPage::None);
            _breadcrumbs.Append(crumb);
            SettingsNav().SelectedItem(BaseLayerMenuItem());

            // Pass along the element to focus to the ProfileViewModel.
            // This will work as a staging area before we navigate to the correct sub-page
            auto profileVMImpl = get_self<ProfileViewModel>(_profileDefaultsVM);
            profileVMImpl->ElementToFocus(elementToFocus);

            // Set the profile's 'CurrentPage' to the correct one, if this requires further navigation, the
            // event handler will do it
            const ProfileSubPage profileSubPage = ProfileSubPageFromBreadcrumb(subPage);
            const bool needsForcedRefresh = _profileDefaultsVM.CurrentPage() == profileSubPage;
            _profileDefaultsVM.CurrentPage(profileSubPage);
            if (needsForcedRefresh)
            {
                // If we're already on the correct sub-page, the PropertyChanged event won't fire.
                // However, we still need to pass along the ElementToFocus, so we need to force a refresh.
                profileVMImpl->ForceRefreshCurrentPage();
            }
        }
        else if (clickedItemTag == colorSchemesTag)
        {
            const auto crumb = winrt::make<Breadcrumb>(box_value(clickedItemTag), RS_(L"Nav_ColorSchemes/Content"), BreadcrumbSubPage::None);
            _breadcrumbs.Append(crumb);
            contentFrame().Navigate(xaml_typename<Editor::ColorSchemes>(), winrt::make<NavigateToPageArgs>(_colorSchemesPageVM, *this, elementToFocus));
            SettingsNav().SelectedItem(ColorSchemesNavItem());

            if (subPage == BreadcrumbSubPage::ColorSchemes_Edit)
            {
                _colorSchemesPageVM.CurrentPage(ColorSchemesSubPage::EditColorScheme);
            }
        }
        else if (clickedItemTag == globalAppearanceTag)
        {
            contentFrame().Navigate(xaml_typename<Editor::GlobalAppearance>(), winrt::make<NavigateToPageArgs>(winrt::make<GlobalAppearanceViewModel>(_settingsClone.GlobalSettings()), *this, elementToFocus));
            const auto crumb = winrt::make<Breadcrumb>(box_value(clickedItemTag), RS_(L"Nav_Appearance/Content"), BreadcrumbSubPage::None);
            _breadcrumbs.Append(crumb);
            SettingsNav().SelectedItem(AppearanceNavItem());
        }
        else if (clickedItemTag == addProfileTag)
        {
            auto addProfileState{ winrt::make<AddProfilePageNavigationState>(_settingsClone, elementToFocus) };
            addProfileState.AddNew({ get_weak(), &MainPage::_AddProfileHandler });
            contentFrame().Navigate(xaml_typename<Editor::AddProfile>(), winrt::make<NavigateToPageArgs>(addProfileState, *this));
            const auto crumb = winrt::make<Breadcrumb>(box_value(clickedItemTag), RS_(L"Nav_AddNewProfile/Content"), BreadcrumbSubPage::None);
            _breadcrumbs.Append(crumb);

            // Find the "Add new profile" menu item and select it
            // It's likely at the very end of the list, so we'll search backwards
            for (auto i = _menuItemSource.Size() - 1; i > 0; --i)
            {
                const auto& item = _menuItemSource.GetAt(i);
                if (const auto& menuItem{ item.try_as<MUX::Controls::NavigationViewItem>() })
                {
                    if (const auto& tag{ menuItem.Tag() })
                    {
                        if (const auto& stringTag{ tag.try_as<hstring>() })
                        {
                            if (*stringTag == addProfileTag)
                            {
                                SettingsNav().SelectedItem(item);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    // Method Description:
    // - updates the content frame to present a view of the profile page
    // - NOTE: this does not update the selected item.
    // Arguments:
    // - profile - the profile object we are getting a view of
    void MainPage::_Navigate(const Editor::ProfileViewModel& profile, BreadcrumbSubPage subPage, hstring elementToFocus)
    {
        _PreNavigateHelper();

        _SetupProfileEventHandling(profile);

        if (profile.Orphaned())
        {
            contentFrame().Navigate(xaml_typename<Editor::Profiles_Base_Orphaned>(), winrt::make<NavigateToPageArgs>(profile, *this, elementToFocus));
            const auto crumb = winrt::make<Breadcrumb>(box_value(profile), profile.Name(), BreadcrumbSubPage::None);
            _breadcrumbs.Append(crumb);
            profile.CurrentPage(ProfileSubPage::Base);
            return;
        }

        contentFrame().Navigate(xaml_typename<Editor::Profiles_Base>(), winrt::make<NavigateToPageArgs>(profile, *this, elementToFocus));
        const auto crumb = winrt::make<Breadcrumb>(box_value(profile), profile.Name(), BreadcrumbSubPage::None);
        _breadcrumbs.Append(crumb);
        if (const auto profileNavItem = _FindProfileNavItem(profile.OriginalProfileGuid()))
        {
            SettingsNav().SelectedItem(profileNavItem);
        }

        // Pass along the element to focus to the ProfileViewModel.
        // This will work as a staging area before we navigate to the correct sub-page
        auto profileVMImpl = get_self<ProfileViewModel>(profile);
        profileVMImpl->ElementToFocus(elementToFocus);

        // Set the profile's 'CurrentPage' to the correct one, if this requires further navigation, the
        // event handler will do it
        const ProfileSubPage profileSubPage = ProfileSubPageFromBreadcrumb(subPage);
        const bool needsForcedRefresh = profile.CurrentPage() == profileSubPage;
        profile.CurrentPage(profileSubPage);
        if (needsForcedRefresh)
        {
            // If we're already on the correct sub-page, the PropertyChanged event won't fire.
            // However, we still need to pass along the ElementToFocus, so we need to force a refresh.
            profileVMImpl->ForceRefreshCurrentPage();
        }
    }

    void MainPage::_Navigate(const Editor::ColorSchemeViewModel& colorSchemeVM, BreadcrumbSubPage subPage, hstring elementToFocus)
    {
        _PreNavigateHelper();

        const auto crumb = winrt::make<Breadcrumb>(box_value(colorSchemesTag), RS_(L"Nav_ColorSchemes/Content"), BreadcrumbSubPage::None);
        _breadcrumbs.Append(crumb);
        contentFrame().Navigate(xaml_typename<Editor::ColorSchemes>(), winrt::make<NavigateToPageArgs>(_colorSchemesPageVM, *this, elementToFocus));
        SettingsNav().SelectedItem(ColorSchemesNavItem());

        // Pass along the element to focus to the ColorSchemesPageViewModel.
        // This will work as a staging area before we navigate to EditColorScheme
        get_self<ColorSchemesPageViewModel>(_colorSchemesPageVM)->ElementToFocus(elementToFocus);

        // Set CurrentScheme BEFORE the CurrentPage!
        // Doing so triggers the PropertyChanged event which performs the navigation to EditColorScheme
        if (subPage == BreadcrumbSubPage::None)
        {
            _colorSchemesPageVM.CurrentScheme(nullptr);
            _colorSchemesPageVM.CurrentPage(ColorSchemesSubPage::Base);
        }
        else
        {
            _colorSchemesPageVM.CurrentScheme(colorSchemeVM);
            _colorSchemesPageVM.CurrentPage(ColorSchemesSubPage::EditColorScheme);
        }
    }

    void MainPage::_Navigate(const Editor::NewTabMenuEntryViewModel& ntmEntryVM, BreadcrumbSubPage subPage, hstring elementToFocus)
    {
        _PreNavigateHelper();

        contentFrame().Navigate(xaml_typename<Editor::NewTabMenu>(), winrt::make<NavigateToPageArgs>(_newTabMenuPageVM, *this, elementToFocus));
        const auto crumb = winrt::make<Breadcrumb>(box_value(newTabMenuTag), RS_(L"Nav_NewTabMenu/Content"), BreadcrumbSubPage::None);
        _breadcrumbs.Append(crumb);
        SettingsNav().SelectedItem(NewTabMenuNavItem());

        if (subPage == BreadcrumbSubPage::None)
        {
            _newTabMenuPageVM.CurrentFolder(nullptr);
        }
        else if (const auto& folderEntryVM = ntmEntryVM.try_as<Editor::FolderEntryViewModel>(); subPage == BreadcrumbSubPage::NewTabMenu_Folder && folderEntryVM)
        {
            // The given ntmEntryVM doesn't exist anymore since the whole tree had to be recreated.
            // Instead, let's look for a match by name and navigate to it.
            if (const auto& folderPath = _newTabMenuPageVM.FindFolderPathByName(folderEntryVM.Name()); folderPath.Size() > 0)
            {
                for (const auto& step : folderPath)
                {
                    // Take advantage of the PropertyChanged event to navigate
                    // to the correct folder and build the breadcrumbs as we go
                    _newTabMenuPageVM.CurrentFolder(step);
                }
            }
            else
            {
                // If we couldn't find a reasonable match, just go back to the root
                _newTabMenuPageVM.CurrentFolder(nullptr);
            }
        }
    }

    void MainPage::_Navigate(const Editor::ExtensionPackageViewModel& extPkgVM, BreadcrumbSubPage subPage, hstring elementToFocus)
    {
        _PreNavigateHelper();

        contentFrame().Navigate(xaml_typename<Editor::Extensions>(), winrt::make<NavigateToPageArgs>(_extensionsVM, *this, elementToFocus));
        const auto crumb = winrt::make<Breadcrumb>(box_value(extensionsTag), RS_(L"Nav_Extensions/Content"), BreadcrumbSubPage::None);
        _breadcrumbs.Append(crumb);
        SettingsNav().SelectedItem(ExtensionsNavItem());

        if (subPage == BreadcrumbSubPage::None)
        {
            _extensionsVM.CurrentExtensionPackage(nullptr);
        }
        else
        {
            bool found = false;
            for (const auto& pkgVM : _extensionsVM.ExtensionPackages())
            {
                if (pkgVM.Package().Source() == extPkgVM.Package().Source())
                {
                    // Take advantage of the PropertyChanged event to navigate
                    // to the correct extension package and build the breadcrumbs as we go.
                    const auto wasAlreadyOnExtension = (_extensionsVM.CurrentExtensionPackage() == pkgVM);
                    _extensionsVM.CurrentExtensionPackage(pkgVM);
                    found = true;

                    // If CurrentExtensionPackage was already this extension, PropertyChanged won't fire,
                    // so we add the breadcrumb manually.
                    if (wasAlreadyOnExtension)
                    {
                        const auto extCrumb = winrt::make<Breadcrumb>(box_value(pkgVM), pkgVM.DisplayName(), BreadcrumbSubPage::Extensions_Extension);
                        _breadcrumbs.Append(extCrumb);
                    }
                    break;
                }
            }
            if (!found)
            {
                // If we couldn't find a reasonable match, just go back to the root
                _extensionsVM.CurrentExtensionPackage(nullptr);
            }
        }
    }

    void MainPage::_Navigate(const Editor::CommandViewModel& commandVM, BreadcrumbSubPage subPage, hstring elementToFocus)
    {
        _PreNavigateHelper();

        contentFrame().Navigate(xaml_typename<Editor::Actions>(), winrt::make<NavigateToPageArgs>(_actionsVM, *this, elementToFocus));
        const auto crumb = winrt::make<Breadcrumb>(box_value(actionsTag), RS_(L"Nav_Actions/Content"), BreadcrumbSubPage::None);
        _breadcrumbs.Append(crumb);
        SettingsNav().SelectedItem(ActionsNavItem());

        if (subPage == BreadcrumbSubPage::None || !commandVM)
        {
            _actionsVM.CurrentCommand(nullptr);
        }
        else
        {
            // Take advantage of the PropertyChanged event to navigate
            // to EditAction and build the breadcrumbs as we go.
            const auto wasAlreadyEdit = (_actionsVM.CurrentPage() == ActionsSubPage::Edit);
            _actionsVM.CurrentCommand(commandVM);
            _actionsVM.CurrentPage(ActionsSubPage::Edit);

            // If CurrentPage was already Edit, PropertyChanged won't fire,
            // so we navigate and add breadcrumb manually.
            if (wasAlreadyEdit)
            {
                contentFrame().Navigate(xaml_typename<Editor::EditAction>(), winrt::make<implementation::NavigateToPageArgs>(commandVM, *this, elementToFocus));
                const auto editCrumb = winrt::make<Breadcrumb>(box_value(actionsTag), RS_(L"Nav_EditAction/Content"), BreadcrumbSubPage::Actions_Edit);
                _breadcrumbs.Append(editCrumb);
            }
        }
    }

    void MainPage::SaveButton_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*args*/)
    {
        _settingsClone.LogSettingChanges(false);
        if (!_settingsClone.WriteSettingsToDisk())
        {
            ShowLoadWarningsDialog.raise(*this, _settingsClone.Warnings());
        }
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
            else if (const auto ntmEntryViewModel = tag.try_as<NewTabMenuEntryViewModel>())
            {
                _Navigate(*ntmEntryViewModel, subPage);
            }
            else if (const auto extPkgViewModel = tag.try_as<ExtensionPackageViewModel>())
            {
                _Navigate(*extPkgViewModel, subPage);
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

        // Manually create a NavigationViewItem and view model for each profile
        // and keep a reference to them in a map so that we
        // can easily modify the correct one when the associated
        // profile changes.
        _profileVMs.Clear();
        for (const auto& profile : _settingsClone.AllProfiles())
        {
            if (!profile.Deleted())
            {
                auto profileVM = _viewModelForProfile(profile, _settingsClone, Dispatcher());
                profileVM.SetupAppearances(_colorSchemesPageVM.AllColorSchemes());
                auto navItem = _CreateProfileNavViewItem(profileVM);
                _menuItemSource.Append(navItem);
            }
        }

        // Top off (the end of the nav view) with the Add Profile item
        MUX::Controls::NavigationViewItem addProfileItem;
        const auto addProfileText = RS_(L"Nav_AddNewProfile/Content");
        addProfileItem.Content(box_value(addProfileText));
        addProfileItem.Tag(box_value(addProfileTag));
        WUX::Controls::ToolTipService::SetToolTip(addProfileItem, box_value(addProfileText));

        FontIcon icon;
        // This is the "Add" symbol
        icon.Glyph(NavTagIconMap[addProfileTag]);
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
        const auto profileViewModel{ _viewModelForProfile(newProfile, _settingsClone, Dispatcher()) };
        profileViewModel.SetupAppearances(_colorSchemesPageVM.AllColorSchemes());
        const auto navItem{ _CreateProfileNavViewItem(profileViewModel) };

        if (_menuItemSource)
        {
            _menuItemSource.InsertAt(index, navItem);
        }

        // Select and navigate to the new profile
        _Navigate(profileViewModel, BreadcrumbSubPage::None);
        SettingsNav().SelectedItem(navItem);
    }

    static MUX::Controls::InfoBadge _createGlyphIconBadge(wil::zwstring_view glyph)
    {
        MUX::Controls::InfoBadge badge;
        MUX::Controls::FontIconSource icon;
        icon.FontFamily(winrt::Windows::UI::Xaml::Media::FontFamily{ L"Segoe Fluent Icons, Segoe MDL2 Assets" });
        icon.FontSize(12);
        icon.Glyph(glyph);
        badge.IconSource(icon);
        return badge;
    }

    MUX::Controls::NavigationViewItem MainPage::_CreateProfileNavViewItem(const Editor::ProfileViewModel& profile)
    {
        MUX::Controls::NavigationViewItem profileNavItem;
        profileNavItem.Content(box_value(profile.Name()));
        profileNavItem.Tag(box_value<Editor::ProfileViewModel>(profile));
        profileNavItem.Icon(UI::IconPathConverter::IconWUX(profile.EvaluatedIcon()));
        WUX::Controls::ToolTipService::SetToolTip(profileNavItem, box_value(profile.Name()));

        if (profile.Orphaned())
        {
            profileNavItem.InfoBadge(_createGlyphIconBadge(L"\xE7BA") /* Warning Triangle */);
        }
        else if (profile.Hidden())
        {
            profileNavItem.InfoBadge(_createGlyphIconBadge(L"\xED1A") /* Hide */);
        }

        // Update the menu item when the icon/name changes
        auto weakMenuItem{ make_weak(profileNavItem) };
        profile.PropertyChanged([weakMenuItem](const auto&, const WUX::Data::PropertyChangedEventArgs& args) {
            if (auto menuItem{ weakMenuItem.get() })
            {
                const auto& tag{ menuItem.Tag().as<Editor::ProfileViewModel>() };
                if (args.PropertyName() == L"Icon")
                {
                    menuItem.Icon(UI::IconPathConverter::IconWUX(tag.EvaluatedIcon()));
                }
                else if (args.PropertyName() == L"Name")
                {
                    menuItem.Content(box_value(tag.Name()));
                    WUX::Controls::ToolTipService::SetToolTip(menuItem, box_value(tag.Name()));
                }
                else if (args.PropertyName() == L"Hidden")
                {
                    menuItem.InfoBadge(tag.Hidden() ? _createGlyphIconBadge(L"\xED1A") /* Hide */ : nullptr);
                }
            }
        });

        // Add an event handler for when the user wants to delete a profile.
        profile.DeleteProfileRequested({ this, &MainPage::_DeleteProfile });

        // Register the VM so that it appears in the search index
        _profileVMs.Append(profile);

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

            // Remove it from the list of VMs
            auto profileVM = selectedItem.as<MUX::Controls::NavigationViewItem>().Tag().as<Editor::ProfileViewModel>();
            uint32_t vmIndex;
            if (_menuItemSource.IndexOf(profileVM, vmIndex))
            {
                _profileVMs.RemoveAt(vmIndex);
            }

            // navigate to the profile next to this one
            const auto newSelectedItem{ _menuItemSource.GetAt(index < _menuItemSource.Size() - 1 ? index : index - 1) };
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
            SettingsNav().SelectedItem(newSelectedItem);
        }
    }

    IObservableVector<IInspectable> MainPage::Breadcrumbs() noexcept
    {
        return _breadcrumbs;
    }

    void MainPage::_NavigateToProfileHandler(const IInspectable& /*sender*/, winrt::guid profileGuid)
    {
        if (const auto profileNavItem = _FindProfileNavItem(profileGuid))
        {
            _Navigate(profileNavItem.Tag().as<Editor::ProfileViewModel>(), BreadcrumbSubPage::None);
        }
        // Silently fail if the profile wasn't found
    }

    MUX::Controls::NavigationViewItem MainPage::_FindProfileNavItem(winrt::guid profileGuid) const
    {
        for (auto&& menuItem : _menuItemSource)
        {
            if (const auto& navViewItem{ menuItem.try_as<MUX::Controls::NavigationViewItem>() })
            {
                if (const auto& tag{ navViewItem.Tag() })
                {
                    if (const auto& profileTag{ tag.try_as<ProfileViewModel>() })
                    {
                        if (profileTag->OriginalProfileGuid() == profileGuid)
                        {
                            return navViewItem;
                        }
                    }
                }
            }
        }
        return nullptr;
    }

    void MainPage::_NavigateToColorSchemeHandler(const IInspectable& /*sender*/, const IInspectable& /*args*/)
    {
        _Navigate(hstring{ colorSchemesTag }, BreadcrumbSubPage::ColorSchemes_Edit);
        SettingsNav().SelectedItem(ColorSchemesNavItem());
    }

    winrt::Windows::UI::Xaml::Media::Brush MainPage::BackgroundBrush()
    {
        return SettingsNav().Background();
    }

    // If the theme asks for Mica, then drop out our background, so that we
    // can have mica too.
    void MainPage::_UpdateBackgroundForMica()
    {
        // If we're in high contrast mode, don't override the theme.
        if (Windows::UI::ViewManagement::AccessibilitySettings accessibilitySettings; accessibilitySettings.HighContrast())
        {
            return;
        }

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

        const auto& theme = _settingsSource.GlobalSettings().CurrentTheme();
        const bool hasThemeForSettings{ theme.Settings() != nullptr };
        const auto& appTheme = theme.RequestedTheme();
        const auto& requestedTheme = (hasThemeForSettings) ? theme.Settings().RequestedTheme() : appTheme;

        RequestedTheme(requestedTheme);

        // Mica gets it's appearance from the app's theme, not necessarily the
        // Page's theme. In the case of dark app, light settings, mica will be a
        // dark color, and the text will also be dark, making the UI _very_ hard
        // to read. (and similarly in the inverse situation).
        //
        // To mitigate this, don't set the transparent background in the case
        // that our theme is different than the app's.
        const bool actuallyUseMica = isMicaAvailable && (appTheme == requestedTheme);

        const auto bgKey = (theme.Window() != nullptr && theme.Window().UseMica()) && actuallyUseMica ?
                               L"SettingsPageMicaBackground" :
                               L"SettingsPageBackground";

        // remember to use ThemeLookup to get the actual correct color for the
        // currently requested theme.
        if (const auto bgColor = ThemeLookup(Resources(), requestedTheme, winrt::box_value(bgKey)))
        {
            SettingsNav().Background(winrt::WUX::Media::SolidColorBrush(winrt::unbox_value<Windows::UI::Color>(bgColor)));
        }
    }

    safe_void_coroutine MainPage::SettingsSearchBox_TextChanged(const AutoSuggestBox& sender, const AutoSuggestBoxTextChangedEventArgs& args)
    {
        if (args.Reason() != AutoSuggestionBoxTextChangeReason::UserInput)
        {
            // Only respond to user input, not programmatic text changes
            co_return;
        }

        // remove leading spaces
        std::wstring queryW{ sender.Text() };
        const auto firstNonSpace{ queryW.find_first_not_of(L' ') };
        if (firstNonSpace == std::wstring::npos)
        {
            // only spaces
            const auto& searchBox = SettingsSearchBox();
            searchBox.ItemsSource(nullptr);
            searchBox.IsSuggestionListOpen(false);
            co_return;
        }

        const hstring sanitizedQuery{ queryW.substr(firstNonSpace) };
        if (sanitizedQuery.empty())
        {
            // empty query
            const auto& searchBox = SettingsSearchBox();
            searchBox.ItemsSource(nullptr);
            searchBox.IsSuggestionListOpen(false);
            co_return;
        }

        if (_currentSearch)
        {
            // a newer search has started, abandon this one
            _currentSearch.Cancel();
            co_return;
        }
        _currentSearch = SearchIndex::Instance().SearchAsync(sanitizedQuery,
                                                             _profileVMs.GetView(),
                                                             get_self<implementation::NewTabMenuViewModel>(_newTabMenuPageVM)->FolderTreeFlatList().GetView(),
                                                             _colorSchemesPageVM.AllColorSchemes().GetView(),
                                                             _extensionsVM.ExtensionPackages().GetView(),
                                                             _actionsVM.CommandList().GetView());
        const auto results = co_await _currentSearch;
        _currentSearch = nullptr;

        // Update the UI with the results
        const auto& searchBox = SettingsSearchBox();
        searchBox.ItemsSource(results);
        searchBox.IsSuggestionListOpen(true);
    }

    void MainPage::SettingsSearchBox_QuerySubmitted(const AutoSuggestBox& /*sender*/, const AutoSuggestBoxQuerySubmittedEventArgs& args)
    {
        if (args.ChosenSuggestion())
        {
            const auto& chosenResult{ args.ChosenSuggestion().as<FilteredSearchResult>() };
            if (chosenResult->IsNoResultsPlaceholder())
            {
                // don't navigate anywhere
                return;
            }

            // Navigate to the target page
            const auto& indexEntry{ chosenResult->SearchIndexEntry() };
            const auto& navigationArg{ chosenResult->NavigationArg() };
            const auto& subpage{ indexEntry.Entry->SubPage };
            const auto& elementToFocus{ indexEntry.Entry->ElementName };
            if (const auto navArgString = navigationArg.try_as<hstring>())
            {
                _Navigate(*navArgString, subpage, elementToFocus);
            }
            else if (const auto& profileVM = navigationArg.try_as<Editor::ProfileViewModel>())
            {
                _Navigate(profileVM, subpage, elementToFocus);
            }
            else if (const auto& colorSchemeVM = navigationArg.try_as<Editor::ColorSchemeViewModel>())
            {
                _Navigate(colorSchemeVM, subpage, elementToFocus);
            }
            else if (const auto& ntmEntryVM = navigationArg.try_as<Editor::NewTabMenuEntryViewModel>())
            {
                _Navigate(ntmEntryVM, subpage, elementToFocus);
            }
            else if (const auto& extPkgVM = navigationArg.try_as<Editor::ExtensionPackageViewModel>())
            {
                _Navigate(extPkgVM, subpage, elementToFocus);
            }
            else if (const auto& commandVM = navigationArg.try_as<Editor::CommandViewModel>())
            {
                _Navigate(commandVM, subpage, elementToFocus);
            }
            SettingsSearchBox().Text(L"");
        }
    }

    void MainPage::SettingsSearchBox_SuggestionChosen(const AutoSuggestBox&, const AutoSuggestBoxSuggestionChosenEventArgs&)
    {
        // Don't navigate on arrow keys
        // Handle Enter/Click with QuerySubmitted() to instead
        // AutoSuggestBox will pass the chosen item to QuerySubmitted() via args.ChosenSuggestion()
    }

    safe_void_coroutine MainPage::_UpdateSearchIndex()
    {
        auto weakThis = get_weak();

        co_await winrt::resume_background();

        if (auto strongThis = weakThis.get())
        {
            if (strongThis->_currentSearch && strongThis->_currentSearch.Status() == AsyncStatus::Started)
            {
                strongThis->_currentSearch.Cancel();
            }
            SearchIndex::Instance().Reset();
        }
    }
}
