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
#include "IconPathConverter.h"

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

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings MainPage::_settingsSource{ nullptr };

    MainPage::MainPage(CascadiaSettings settings) :
        _profileToNavItemMap{ winrt::single_threaded_map<guid, MUX::Controls::NavigationViewItem>() }
    {
        InitializeComponent();

        // TODO GH#1564: When we actually connect this to Windows Terminal,
        //       this section will clone the active AppSettings
        MainPage::_settingsSource = settings;
        _settingsClone = nullptr;

        _InitializeProfilesList();
    }

    CascadiaSettings MainPage::Settings()
    {
        return _settingsSource;
    }

    void MainPage::SettingsNav_Loaded(IInspectable const&, RoutedEventArgs const&)
    {
<<<<<<< HEAD
        contentFrame().Navigate(xaml_typename<Editor::Launch>());
=======
        auto initialItem = SettingsNav().MenuItems().GetAt(0);
        SettingsNav().SelectedItem(initialItem);

        // Manually navigate because setting the selected item programmatically doesn't trigger ItemInvoked.
        if (auto tag = initialItem.as<MUX::Controls::NavigationViewItem>().Tag())
        {
            Navigate(contentFrame(), unbox_value<hstring>(tag));
        }
>>>>>>> feature/settings-ui
    }

    void MainPage::SettingsNav_ItemInvoked(MUX::Controls::NavigationView const&, MUX::Controls::NavigationViewItemInvokedEventArgs const& args)
    {
        auto clickedItemContainer = args.InvokedItemContainer();

        if (clickedItemContainer != NULL)
        {
            auto tagPropertyValue = clickedItemContainer.Tag().as<winrt::Windows::Foundation::IPropertyValue>();
            if (tagPropertyValue.Type() == winrt::Windows::Foundation::PropertyType::String)
            {
                Navigate(contentFrame(), unbox_value<hstring>(clickedItemContainer.Tag()));
            }
            else if (tagPropertyValue.Type() == winrt::Windows::Foundation::PropertyType::Guid)
            {
                auto profileGuid = unbox_value<guid>(clickedItemContainer.Tag());
                auto prof = _settingsSource.FindProfile(profileGuid);
                // Navigate to a page with the given profile guid
                contentFrame().Navigate(xaml_typename<Editor::Profiles>(), prof);
            }
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

    void MainPage::Navigate(Controls::Frame contentFrame, hstring clickedItemTag)
    {
        const hstring launchSubpage = L"Launch_Nav";
        const hstring interactionSubpage = L"Interaction_Nav";
        const hstring renderingSubpage = L"Rendering_Nav";

        const hstring globalProfileSubpage = L"GlobalProfile_Nav";

        const hstring colorSchemesPage = L"ColorSchemes_Nav";
        const hstring globalAppearancePage = L"GlobalAppearance_Nav";

        if (clickedItemTag == launchSubpage)
        {
            contentFrame.Navigate(xaml_typename<Editor::Launch>());
        }
        else if (clickedItemTag == interactionSubpage)
        {
            contentFrame.Navigate(xaml_typename<Editor::Interaction>());
        }
        else if (clickedItemTag == renderingSubpage)
        {
            contentFrame.Navigate(xaml_typename<Editor::Rendering>());
        }
        else if (clickedItemTag == globalProfileSubpage)
        {
            contentFrame.Navigate(xaml_typename<Editor::Profiles>());
        }
        else if (clickedItemTag == colorSchemesPage)
        {
            contentFrame.Navigate(xaml_typename<Editor::ColorSchemes>());
        }
        else if (clickedItemTag == globalAppearancePage)
        {
            contentFrame.Navigate(xaml_typename<Editor::GlobalAppearance>());
        }
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
        for (auto profile : _settingsSource.AllProfiles())
        {
            MUX::Controls::NavigationViewItem profileNavItem;
            profileNavItem.Content(box_value(profile.Name()));
            profileNavItem.Tag(box_value<guid>(profile.Guid()));

            const auto iconSource{ IconPathConverter::IconSourceWUX(profile.Icon()) };
            WUX::Controls::IconSourceElement icon;
            icon.IconSource(iconSource);
            profileNavItem.Icon(icon);

            SettingsNav().MenuItems().Append(profileNavItem);
            _profileToNavItemMap.Insert(profile.Guid(), profileNavItem);
        }

        // Top off (the end of the nav view) with the Add Profile item
        MUX::Controls::NavigationViewItem addProfileItem;
        addProfileItem.Content(box_value(RS_(L"Nav_AddNewProfile/Content")));
        addProfileItem.Tag(box_value(L"AddNewProfile_Nav"));

        FontIcon icon;
        icon.Glyph(L"\xE710");
        addProfileItem.Icon(icon);

        SettingsNav().MenuItems().Append(addProfileItem);
    }
}
