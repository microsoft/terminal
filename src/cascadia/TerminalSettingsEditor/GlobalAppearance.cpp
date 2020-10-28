// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "GlobalAppearance.h"
#include "GlobalAppearance.g.cpp"
#include "MainPage.h"
#include "EnumEntry.h"

#include <LibraryResources.h>

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    GlobalAppearance::GlobalAppearance()
    {
        // TODO: Maybe find a better way of initializing the selected RadioButton for an Enum setting
        _ElementThemes = winrt::single_threaded_observable_vector<Editor::EnumEntry>({
            winrt::make<EnumEntry>(RS_(L"Globals_ThemeDefault/Content"), winrt::box_value<ElementTheme>(ElementTheme::Default), ElementTheme::Default == GlobalSettings().Theme()),
            winrt::make<EnumEntry>(RS_(L"Globals_ThemeDark/Content"), winrt::box_value<ElementTheme>(ElementTheme::Dark), ElementTheme::Dark == GlobalSettings().Theme()),
            winrt::make<EnumEntry>(RS_(L"Globals_ThemeLight/Content"), winrt::box_value<ElementTheme>(ElementTheme::Light), ElementTheme::Light == GlobalSettings().Theme())
        });

        InitializeComponent();
    }

    IObservableVector<Editor::EnumEntry> GlobalAppearance::ElementThemes()
    {
        return _ElementThemes;
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

    GlobalAppSettings GlobalAppearance::GlobalSettings()
    {
        return MainPage::Settings().GlobalSettings();
    }
}
