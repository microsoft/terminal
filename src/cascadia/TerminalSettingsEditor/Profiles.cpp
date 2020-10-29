// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Utils.h"
#include "Profiles.h"
#include "Profiles.g.cpp"
#include "EnumEntry.h"

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Media;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::AccessCache;
using namespace winrt::Windows::Storage::Pickers;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Profiles::Profiles() :
        _ScrollStates{ winrt::single_threaded_observable_vector<Editor::EnumEntry>() },
        _CursorShapes{ winrt::single_threaded_observable_vector<Editor::EnumEntry>() },
        _BackgroundImageStretchModes{ winrt::single_threaded_observable_vector<Editor::EnumEntry>() },
        _AntialiasingModes{ winrt::single_threaded_observable_vector<Editor::EnumEntry>() }

    {
        InitializeComponent();

        auto scrollStatesMap = EnumMappings::ScrollState();
        for (auto [key, value] : scrollStatesMap)
        {
            auto enumName = LocalizedNameForEnumName(L"Profile_ScrollVisibility", key, L"Content");
            auto entry = winrt::make<EnumEntry>(enumName, winrt::box_value<ScrollbarState>(value));
            _ScrollStates.Append(entry);

            // Initialize the selected item to be our current setting
            if (value == Profile().ScrollState())
            {
                ScrollbarVisibilityButtons().SelectedItem(entry);
            }
        }

        auto cursorShapeMap = EnumMappings::CursorShape();
        for (auto [key, value] : cursorShapeMap)
        {
            auto enumName = LocalizedNameForEnumName(L"Profile_CursorShape", key, L"Content");
            auto entry = winrt::make<EnumEntry>(enumName, winrt::box_value<CursorStyle>(value));
            _ScrollStates.Append(entry);

            // Initialize the selected item to be our current setting
            if (value == Profile().CursorShape())
            {
                CursorShapeButtons().SelectedItem(entry);
            }
        }

        auto backgroundImageStretchModesMap = EnumMappings::BackgroundImageStretchMode();
        for (auto [key, value] : backgroundImageStretchModesMap)
        {
            auto enumName = LocalizedNameForEnumName(L"Profile_BackgroundImageStretchMode", key, L"Content");
            auto entry = winrt::make<EnumEntry>(enumName, winrt::box_value<Stretch>(value));
            _ScrollStates.Append(entry);

            // Initialize the selected item to be our current setting
            if (value == Profile().BackgroundImageStretchMode())
            {
                BackgroundImageStretchModeButtons().SelectedItem(entry);
            }
        }

        auto antialiasingModesMap = EnumMappings::AntialiasingMode();
        for (auto [key, value] : antialiasingModesMap)
        {
            auto enumName = LocalizedNameForEnumName(L"Profile_AntialiasingMode", key, L"Content");
            auto entry = winrt::make<EnumEntry>(enumName, winrt::box_value<TextAntialiasingMode>(value));
            _ScrollStates.Append(entry);

            // Initialize the selected item to be our current setting
            if (value == Profile().AntialiasingMode())
            {
                AntialiasingModeButtons().SelectedItem(entry);
            }
        }
    }

    Profiles::Profiles(Settings::Model::Profile profile)
    {
        InitializeComponent();
        Profile(profile);
    }

    fire_and_forget Profiles::BackgroundImage_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        FileOpenPicker picker;

        picker.ViewMode(PickerViewMode::Thumbnail);
        picker.SuggestedStartLocation(PickerLocationId::PicturesLibrary);
        picker.FileTypeFilter().ReplaceAll({ L".jpg", L".jpeg", L".png", L".gif" });

        StorageFile file = co_await picker.PickSingleFileAsync();
        if (file != nullptr)
        {
            BackgroundImage().Text(file.Path());
        }
    }

    fire_and_forget Profiles::Commandline_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        FileOpenPicker picker;

        //TODO: SETTINGS UI Commandline handling should be robust and intelligent
        picker.ViewMode(PickerViewMode::Thumbnail);
        picker.SuggestedStartLocation(PickerLocationId::ComputerFolder);
        picker.FileTypeFilter().ReplaceAll({ L".bat", L".exe" });

        StorageFile file = co_await picker.PickSingleFileAsync();
        if (file != nullptr)
        {
            Commandline().Text(file.Path());
        }
    }

    // TODO GH#1564: Settings UI
    // This crashes on click, for some reason
    /*
    fire_and_forget Profiles::StartingDirectory_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();
        FolderPicker picker;
        picker.SuggestedStartLocation(PickerLocationId::DocumentsLibrary);
        StorageFolder folder = co_await picker.PickSingleFolderAsync();
        if (folder != nullptr)
        {
            StorageApplicationPermissions::FutureAccessList().AddOrReplace(L"PickedFolderToken", folder);
            StartingDirectory().Text(folder.Path());
        }
    }
    */

    IObservableVector<Editor::EnumEntry> Profiles::ScrollStates()
    {
        return _ScrollStates;
    }

    void Profiles::ScrollStateSelected(IInspectable const& /*sender*/,
                                       SelectionChangedEventArgs const& args)
    {
        if (args.AddedItems().Size() > 0)
        {
            if (auto item = args.AddedItems().GetAt(0).try_as<EnumEntry>())
            {
                Profile().ScrollState(winrt::unbox_value<ScrollbarState>(item->EnumValue()));
            }
        }
    }

    IObservableVector<Editor::EnumEntry> Profiles::CursorShapes()
    {
        return _CursorShapes;
    }

    void Profiles::CursorShapeSelected(IInspectable const& /*sender*/,
                                       SelectionChangedEventArgs const& args)
    {
        if (args.AddedItems().Size() > 0)
        {
            if (auto item = args.AddedItems().GetAt(0).try_as<EnumEntry>())
            {
                Profile().CursorShape(winrt::unbox_value<CursorStyle>(item->EnumValue()));
            }
        }
    }

    IObservableVector<Editor::EnumEntry> Profiles::BackgroundImageStretchModes()
    {
        return _BackgroundImageStretchModes;
    }

    void Profiles::BackgroundImageStretchModeSelected(IInspectable const& /*sender*/,
                                                      SelectionChangedEventArgs const& args)
    {
        if (args.AddedItems().Size() > 0)
        {
            if (auto item = args.AddedItems().GetAt(0).try_as<EnumEntry>())
            {
                Profile().BackgroundImageStretchMode(winrt::unbox_value<Stretch>(item->EnumValue()));
            }
        }
    }

    IObservableVector<Editor::EnumEntry> Profiles::AntialiasingModes()
    {
        return _AntialiasingModes;
    }

    void Profiles::AntialiasingModeSelected(IInspectable const& /*sender*/,
                                                      SelectionChangedEventArgs const& args)
    {
        if (args.AddedItems().Size() > 0)
        {
            if (auto item = args.AddedItems().GetAt(0).try_as<EnumEntry>())
            {
                Profile().AntialiasingMode(winrt::unbox_value<TextAntialiasingMode>(item->EnumValue()));
            }
        }
    }
}
