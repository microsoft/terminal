// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "GlobalAppearanceViewModel.h"
#include "GlobalAppearanceViewModel.g.cpp"
#include "EnumEntry.h"

#include <LibraryResources.h>

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    constexpr std::wstring_view systemThemeName{ L"system" };
    constexpr std::wstring_view darkThemeName{ L"dark" };
    constexpr std::wstring_view lightThemeName{ L"light" };
    constexpr std::wstring_view legacySystemThemeName{ L"legacySystem" };
    constexpr std::wstring_view legacyDarkThemeName{ L"legacyDark" };
    constexpr std::wstring_view legacyLightThemeName{ L"legacyLight" };

    GlobalAppearanceViewModel::GlobalAppearanceViewModel(Model::GlobalAppSettings globalSettings) :
        _GlobalSettings{ globalSettings },
        _ThemeList{ single_threaded_observable_vector<Model::Theme>() }
    {
        INITIALIZE_BINDABLE_ENUM_SETTING(NewTabPosition, NewTabPosition, NewTabPosition, L"Globals_NewTabPosition", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(TabWidthMode, TabViewWidthMode, winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode, L"Globals_TabWidthMode", L"Content");
        _UpdateThemeList();
    }

    // Function Description:
    // - Updates the list of all themes available to choose from.
    void GlobalAppearanceViewModel::_UpdateThemeList()
    {
        // Surprisingly, though this is called every time we navigate to the page,
        // the list does not keep growing on each navigation.
        const auto& ThemeMap{ _GlobalSettings.Themes() };
        for (const auto& pair : ThemeMap)
        {
            _ThemeList.Append(pair.Value());
        }
    }

    winrt::Windows::Foundation::IInspectable GlobalAppearanceViewModel::CurrentTheme()
    {
        return _GlobalSettings.CurrentTheme();
    }

    // Get the name out of the newly selected item, stash that as the Theme name
    // set for the globals. That controls which theme is actually the current
    // theme.
    void GlobalAppearanceViewModel::CurrentTheme(const winrt::Windows::Foundation::IInspectable& tag)
    {
        if (const auto& theme{ tag.try_as<Model::Theme>() })
        {
            _GlobalSettings.Theme(Model::ThemePair{ theme.Name() });
        }
    }

    // Method Description:
    // - Convert the names of the inbox themes to some more descriptive,
    //   well-known values. If the passed in theme isn't an inbox one, then just
    //   return its set Name.
    //    - "light" becomes "Light"
    //    - "dark" becomes "Dark"
    //    - "system" becomes "Use Windows theme"
    // - These values are all localized based on the app language.
    // Arguments:
    // - theme: the theme to get the display name for.
    // Return Value:
    // - the potentially localized name to use for this Theme.
    winrt::hstring GlobalAppearanceViewModel::ThemeNameConverter(const Model::Theme& theme)
    {
        if (theme.Name() == darkThemeName)
        {
            return RS_(L"Globals_ThemeDark/Content");
        }
        else if (theme.Name() == lightThemeName)
        {
            return RS_(L"Globals_ThemeLight/Content");
        }
        else if (theme.Name() == systemThemeName)
        {
            return RS_(L"Globals_ThemeSystem/Content");
        }
        else if (theme.Name() == legacyDarkThemeName)
        {
            return RS_(L"Globals_ThemeDarkLegacy/Content");
        }
        else if (theme.Name() == legacyLightThemeName)
        {
            return RS_(L"Globals_ThemeLightLegacy/Content");
        }
        else if (theme.Name() == legacySystemThemeName)
        {
            return RS_(L"Globals_ThemeSystemLegacy/Content");
        }
        return theme.Name();
    }

    bool GlobalAppearanceViewModel::InvertedDisableAnimations()
    {
        return !_GlobalSettings.DisableAnimations();
    }

    void GlobalAppearanceViewModel::InvertedDisableAnimations(bool value)
    {
        _GlobalSettings.DisableAnimations(!value);
    }

    void GlobalAppearanceViewModel::ShowTitlebarToggled(const winrt::Windows::Foundation::IInspectable& /* sender */, const RoutedEventArgs& /* args */)
    {
        // Set AlwaysShowTabs to true if ShowTabsInTitlebar was toggled OFF -> ON
        if (!ShowTabsInTitlebar())
        {
            AlwaysShowTabs(true);
        }
    }
}
