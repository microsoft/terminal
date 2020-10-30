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
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    GlobalAppearance::GlobalAppearance() :
        _ElementThemes{ winrt::single_threaded_observable_vector<Editor::EnumEntry>() },
        _ElementThemeMap{ winrt::single_threaded_map<ElementTheme, Editor::EnumEntry>() }
    {
        InitializeComponent();

        auto elementThemeMap = EnumMappings::ElementTheme();
        for (auto [key, value] : elementThemeMap)
        {
            auto enumName = LocalizedNameForEnumName(L"Globals_Theme", key, L"Content");
            auto entry = winrt::make<EnumEntry>(enumName, winrt::box_value<ElementTheme>(value));
            _ElementThemes.Append(entry);
            _ElementThemeMap.Insert(value, entry);
        }
    }

    IObservableVector<Editor::EnumEntry> GlobalAppearance::ElementThemes()
    {
        return _ElementThemes;
    }

    IInspectable GlobalAppearance::CurrentTheme()
    {
        return winrt::box_value<Editor::EnumEntry>(_ElementThemeMap.Lookup(GlobalSettings().Theme()));
    }

    void GlobalAppearance::CurrentTheme(const IInspectable& enumEntry)
    {
        if (auto ee = enumEntry.try_as<Editor::EnumEntry>())
        {
            auto theme = winrt::unbox_value<ElementTheme>(ee.EnumValue());
            GlobalSettings().Theme(theme);
        }
    }

    GlobalAppSettings GlobalAppearance::GlobalSettings()
    {
        return MainPage::Settings().GlobalSettings();
    }
}
