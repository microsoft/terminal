// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Utils.h"
#include "GlobalAppearance.h"
#include "GlobalAppearance.g.cpp"
#include "MainPage.h"
#include "EnumEntry.h"

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    GlobalAppearance::GlobalAppearance() :
        _ElementThemes{ winrt::single_threaded_observable_vector<Editor::EnumEntry>() },
        _TabWidthModes{ winrt::single_threaded_observable_vector<Editor::EnumEntry>() }
    {
        InitializeComponent();

        auto elementThemeMap = EnumMappings::ElementTheme();
        for (auto [key, value] : elementThemeMap)
        {
            auto enumName = LocalizedNameForEnumName(L"Globals_Theme", key, L"Content");
            auto entry = winrt::make<EnumEntry>(enumName, winrt::box_value<ElementTheme>(value));
            _ElementThemes.Append(entry);

            // Initialize the selected item to be our current setting
            if (value == GlobalSettings().Theme())
            {
                ThemeButtons().SelectedItem(entry);
            }
        }

        auto tabWidthMap = EnumMappings::TabWidthMode();
        for (auto [key, value] : tabWidthMap)
        {
            auto enumName = LocalizedNameForEnumName(L"Globals_TabWidthMode", key, L"Content");
            auto entry = winrt::make<EnumEntry>(enumName, winrt::box_value<TabViewWidthMode>(value));
            _TabWidthModes.Append(entry);

            // Initialize the selected item to be our current setting
            if (value == GlobalSettings().TabWidthMode())
            {
                TabWidthButtons().SelectedItem(entry);
            }
        }
    }

    IObservableVector<Editor::EnumEntry> GlobalAppearance::ElementThemes()
    {
        return _ElementThemes;
    }

    IObservableVector<Editor::EnumEntry> GlobalAppearance::TabWidthModes()
    {
        return _TabWidthModes;
    }

    void GlobalAppearance::ElementThemeSelected(IInspectable const& /*sender*/,
                                                SelectionChangedEventArgs const& args)
    {
        if (args.AddedItems().Size() > 0)
        {
            if (auto item = args.AddedItems().GetAt(0).try_as<EnumEntry>())
            {
                GlobalSettings().Theme(winrt::unbox_value<ElementTheme>(item->EnumValue()));
            }
        }
    }

    void GlobalAppearance::TabWidthSelected(IInspectable const& /*sender*/,
                                            SelectionChangedEventArgs const& args)
    {
        if (args.AddedItems().Size() > 0)
        {
            if (auto item = args.AddedItems().GetAt(0).try_as<EnumEntry>())
            {
                GlobalSettings().TabWidthMode(winrt::unbox_value<TabViewWidthMode>(item->EnumValue()));
            }
        }
    }

    GlobalAppSettings GlobalAppearance::GlobalSettings()
    {
        return MainPage::Settings().GlobalSettings();
    }
}
