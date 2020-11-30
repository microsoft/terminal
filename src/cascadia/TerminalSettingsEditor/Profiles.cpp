// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Profiles.h"
#include "Profiles.g.cpp"
#include "EnumEntry.h"

#include <LibraryResources.h>

using namespace winrt::Windows::UI::Text;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::AccessCache;
using namespace winrt::Windows::Storage::Pickers;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Profiles::Profiles()
    {
        InitializeComponent();

        INITIALIZE_BINDABLE_ENUM_SETTING(CursorShape, CursorStyle, winrt::Microsoft::Terminal::TerminalControl::CursorStyle, L"Profile_CursorShape", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(BackgroundImageStretchMode, BackgroundImageStretchMode, winrt::Windows::UI::Xaml::Media::Stretch, L"Profile_BackgroundImageStretchMode", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(AntiAliasingMode, TextAntialiasingMode, winrt::Microsoft::Terminal::TerminalControl::TextAntialiasingMode, L"Profile_AntialiasingMode", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(CloseOnExitMode, CloseOnExitMode, winrt::Microsoft::Terminal::Settings::Model::CloseOnExitMode, L"Profile_CloseOnExit", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(BellStyle, BellStyle, winrt::Microsoft::Terminal::Settings::Model::BellStyle, L"Profile_BellStyle", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(ScrollState, ScrollbarState, winrt::Microsoft::Terminal::TerminalControl::ScrollbarState, L"Profile_ScrollbarVisibility", L"Content");

        // manually add Custom FontWeight option. Don't add it to the Map
        INITIALIZE_BINDABLE_ENUM_SETTING(FontWeight, FontWeight, uint16_t, L"Profile_FontWeight", L"Content");
        _CustomFontWeight = winrt::make<EnumEntry>(RS_(L"Profile_FontWeightCustom/Content"), winrt::box_value<uint16_t>(0u));
        _FontWeightList.Append(_CustomFontWeight);
    }

    void Profiles::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _State = e.Parameter().as<Editor::ProfilePageNavigationState>();
    }

    fire_and_forget Profiles::BackgroundImage_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        FileOpenPicker picker;

        _State.WindowRoot().TryPropagateHostingWindow(picker); // if we don't do this, there's no HWND for it to attach to
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
        _State.WindowRoot().TryPropagateHostingWindow(picker); // if we don't do this, there's no HWND for it to attach to
        picker.ViewMode(PickerViewMode::Thumbnail);
        picker.SuggestedStartLocation(PickerLocationId::ComputerFolder);
        picker.FileTypeFilter().ReplaceAll({ L".bat", L".exe" });

        StorageFile file = co_await picker.PickSingleFileAsync();
        if (file != nullptr)
        {
            Commandline().Text(file.Path());
        }
    }

    fire_and_forget Profiles::StartingDirectory_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();
        FolderPicker picker;
        _State.WindowRoot().TryPropagateHostingWindow(picker); // if we don't do this, there's no HWND for it to attach to
        picker.SuggestedStartLocation(PickerLocationId::DocumentsLibrary);
        picker.FileTypeFilter().ReplaceAll({ L"*" });
        StorageFolder folder = co_await picker.PickSingleFolderAsync();
        if (folder != nullptr)
        {
            StorageApplicationPermissions::FutureAccessList().AddOrReplace(L"PickedFolderToken", folder);
            StartingDirectory().Text(folder.Path());
        }
    }

    bool Profiles::_IsCustomFontWeight(const Windows::UI::Text::FontWeight& weight)
    {
        if (weight == FontWeights::Thin() ||
            weight == FontWeights::ExtraLight() ||
            weight == FontWeights::Light() ||
            weight == FontWeights::SemiLight() ||
            weight == FontWeights::Normal() ||
            weight == FontWeights::Medium() ||
            weight == FontWeights::SemiBold() ||
            weight == FontWeights::Bold() ||
            weight == FontWeights::ExtraBold() ||
            weight == FontWeights::Black() ||
            weight == FontWeights::ExtraBlack())
        {
            return false;
        }
        else
        {
            return true;
        }
    }

    IInspectable Profiles::CurrentFontWeight()
    {
        // if no value was found, we have a custom value, and we show the slider.
        const auto maybeEnumEntry{ _FontWeightMap.TryLookup(_State.Profile().FontWeight().Weight) };
        CustomFontWeightControl().Visibility(maybeEnumEntry ? Visibility::Collapsed : Visibility::Visible);
        return maybeEnumEntry ? maybeEnumEntry : _CustomFontWeight;
    }

    void Profiles::CurrentFontWeight(const IInspectable& enumEntry)
    {
        if (auto ee = enumEntry.try_as<Editor::EnumEntry>())
        {
            const auto weight{ winrt::unbox_value<uint16_t>(ee.EnumValue()) };
            const Windows::UI::Text::FontWeight setting{ weight };
            if (_IsCustomFontWeight(setting))
            {
                // Custom FontWeight is selected using the slider
                // Initialize the value to what is currently set
                CustomFontWeightControl().Visibility(Visibility::Visible);
                FontWeightSlider().Value(_State.Profile().FontWeight().Weight);
            }
            else
            {
                // Otherwise, the selected ComboBox item has an associated fontWeight
                CustomFontWeightControl().Visibility(Visibility::Collapsed);
                _State.Profile().FontWeight(setting);
            }
        }
    }
}
