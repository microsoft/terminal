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
        _ElementThemes{ winrt::single_threaded_observable_vector<Editor::EnumEntry>() }
    {
        InitializeComponent();

        auto elementThemeMap = EnumMappings::ElementTheme();
        for (auto [key, value] : elementThemeMap)
        {
            auto enumName = LocalizedNameForEnumName(L"Globals_Theme", value, L"Content");
            auto entry = winrt::make<EnumEntry>(enumName, winrt::box_value<ElementTheme>(key));
            _ElementThemes.Append(entry);
        }
    }

    IObservableVector<Editor::EnumEntry> GlobalAppearance::ElementThemes()
    {
        return _ElementThemes;
    }

    uint8_t GlobalAppearance::CurrentTheme()
    {
        return static_cast<uint8_t>(GlobalSettings().Theme());
    }

    void GlobalAppearance::CurrentTheme(const uint8_t index)
    {
        GlobalSettings().Theme(static_cast<ElementTheme>(index));
    }

    GlobalAppSettings GlobalAppearance::GlobalSettings()
    {
        return MainPage::Settings().GlobalSettings();
    }
}
