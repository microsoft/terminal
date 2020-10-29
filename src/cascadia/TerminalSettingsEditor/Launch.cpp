// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Utils.h"
#include "Launch.h"
#include "Launch.g.cpp"
#include "MainPage.h"
#include "EnumEntry.h"

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Launch::Launch() :
        _LaunchSizes{ winrt::single_threaded_observable_vector<Editor::EnumEntry>() }
    {
        InitializeComponent();

        auto launchSizeMap = EnumMappings::LaunchMode();
        for (auto [key, value] : launchSizeMap)
        {
            auto enumName = LocalizedNameForEnumName(L"Globals_LaunchSize", key, L"Content");
            auto entry = winrt::make<EnumEntry>(enumName, winrt::box_value<LaunchMode>(value));
            _LaunchSizes.Append(entry);

            // Initialize the selected item to be our current setting
            if (value == GlobalSettings().LaunchMode())
            {
                LaunchSizeButtons().SelectedItem(entry);
            }
        }
    }

    GlobalAppSettings Launch::GlobalSettings()
    {
        return MainPage::Settings().GlobalSettings();
    }

    IObservableVector<Editor::EnumEntry> Launch::LaunchSizes()
    {
        return _LaunchSizes;
    }

    void Launch::LaunchSizeSelected(IInspectable const& /*sender*/,
                                    SelectionChangedEventArgs const& args)
    {
        if (args.AddedItems().Size() > 0)
        {
            if (auto item = args.AddedItems().GetAt(0).try_as<EnumEntry>())
            {
                GlobalSettings().LaunchMode(winrt::unbox_value<LaunchMode>(item->EnumValue()));
            }
        }
    }
}
