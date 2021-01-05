// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Profiles.h"
#include "Profiles.g.cpp"
#include "EnumEntry.h"

#include <LibraryResources.h>

using namespace winrt::Windows::UI::Text;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::AccessCache;
using namespace winrt::Windows::Storage::Pickers;
using namespace winrt::Microsoft::Terminal::Settings::Model;

static const std::array<winrt::guid, 2> InBoxProfileGuids{
    winrt::guid{ 0x61c54bbd, 0xc2c6, 0x5271, { 0x96, 0xe7, 0x00, 0x9a, 0x87, 0xff, 0x44, 0xbf } }, // Windows Powershell
    winrt::guid{ 0x0caa0dad, 0x35be, 0x5f56, { 0xa8, 0xff, 0xaf, 0xce, 0xee, 0xaa, 0x61, 0x01 } } // Command Prompt
};

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    bool ProfileViewModel::CanDeleteProfile() const
    {
        const auto guid{ Guid() };
        if (IsBaseLayer())
        {
            return false;
        }
        else if (std::find(std::begin(InBoxProfileGuids), std::end(InBoxProfileGuids), guid) != std::end(InBoxProfileGuids))
        {
            // in-box profile
            return false;
        }
        else if (!Source().empty())
        {
            // dynamic profile
            return false;
        }
        else
        {
            return true;
        }
    }

    void ProfilePageNavigationState::DeleteProfile()
    {
        auto deleteProfileArgs{ winrt::make_self<DeleteProfileEventArgs>(_Profile.Guid()) };
        _DeleteProfileHandlers(*this, *deleteProfileArgs);
    }

    Profiles::Profiles() :
        _ColorSchemeList{ single_threaded_observable_vector<ColorScheme>() }
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

        // manually keep track of all the Background Image Alignment buttons
        _BIAlignmentButtons.at(0) = BIAlign_TopLeft();
        _BIAlignmentButtons.at(1) = BIAlign_Top();
        _BIAlignmentButtons.at(2) = BIAlign_TopRight();
        _BIAlignmentButtons.at(3) = BIAlign_Left();
        _BIAlignmentButtons.at(4) = BIAlign_Center();
        _BIAlignmentButtons.at(5) = BIAlign_Right();
        _BIAlignmentButtons.at(6) = BIAlign_BottomLeft();
        _BIAlignmentButtons.at(7) = BIAlign_Bottom();
        _BIAlignmentButtons.at(8) = BIAlign_BottomRight();
    }

    void Profiles::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _State = e.Parameter().as<Editor::ProfilePageNavigationState>();

        const auto& colorSchemeMap{ _State.Schemes() };
        for (const auto& pair : colorSchemeMap)
        {
            _ColorSchemeList.Append(pair.Value());
        }

        const auto& biAlignmentVal{ static_cast<int32_t>(_State.Profile().BackgroundImageAlignment()) };
        for (const auto& biButton : _BIAlignmentButtons)
        {
            biButton.IsChecked(biButton.Tag().as<int32_t>() == biAlignmentVal);
        }

        // Set the text disclaimer for the text box
        hstring disclaimer{};
        const auto guid{ _State.Profile().Guid() };
        if (std::find(std::begin(InBoxProfileGuids), std::end(InBoxProfileGuids), guid) != std::end(InBoxProfileGuids))
        {
            // load disclaimer for in-box profiles
            disclaimer = RS_(L"Profile_DeleteButtonDisclaimerInBox");
        }
        else if (!_State.Profile().Source().empty())
        {
            // load disclaimer for dynamic profiles
            disclaimer = RS_(L"Profile_DeleteButtonDisclaimerDynamic");
        }
        DeleteButtonDisclaimer().Text(disclaimer);
    }

    ColorScheme Profiles::CurrentColorScheme()
    {
        const auto schemeName{ _State.Profile().ColorSchemeName() };
        if (const auto scheme{ _State.Schemes().TryLookup(schemeName) })
        {
            return scheme;
        }
        else
        {
            // This Profile points to a color scheme that was renamed or deleted.
            // Fallback to Campbell.
            return _State.Schemes().TryLookup(L"Campbell");
        }
    }

    void Profiles::CurrentColorScheme(const ColorScheme& val)
    {
        _State.Profile().ColorSchemeName(val.Name());
    }

    void Profiles::DeleteConfirmation_Click(IInspectable const& /*sender*/, RoutedEventArgs const& /*e*/)
    {
        auto state{ winrt::get_self<ProfilePageNavigationState>(_State) };
        state->DeleteProfile();
    }

    fire_and_forget Profiles::BackgroundImage_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        FileOpenPicker picker;

        _State.WindowRoot().TryPropagateHostingWindow(picker); // if we don't do this, there's no HWND for it to attach to
        picker.ViewMode(PickerViewMode::Thumbnail);
        picker.SuggestedStartLocation(PickerLocationId::PicturesLibrary);

        // Converted into a BitmapImage. This list of supported image file formats is from BitmapImage documentation
        // https://docs.microsoft.com/en-us/uwp/api/Windows.UI.Xaml.Media.Imaging.BitmapImage?view=winrt-19041#remarks
        picker.FileTypeFilter().ReplaceAll({ L".jpg", L".jpeg", L".png", L".bmp", L".gif", L".tiff", L".ico" });

        StorageFile file = co_await picker.PickSingleFileAsync();
        if (file != nullptr)
        {
            _State.Profile().BackgroundImagePath(file.Path());
        }
    }

    fire_and_forget Profiles::Icon_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        FileOpenPicker picker;

        _State.WindowRoot().TryPropagateHostingWindow(picker); // if we don't do this, there's no HWND for it to attach to
        picker.ViewMode(PickerViewMode::Thumbnail);
        picker.SuggestedStartLocation(PickerLocationId::PicturesLibrary);

        // Converted into a BitmapIconSource. This list of supported image file formats is from BitmapImage documentation
        // https://docs.microsoft.com/en-us/uwp/api/Windows.UI.Xaml.Media.Imaging.BitmapImage?view=winrt-19041#remarks
        picker.FileTypeFilter().ReplaceAll({ L".jpg", L".jpeg", L".png", L".bmp", L".gif", L".tiff", L".ico" });

        StorageFile file = co_await picker.PickSingleFileAsync();
        if (file != nullptr)
        {
            _State.Profile().Icon(file.Path());
        }
    }

    fire_and_forget Profiles::Commandline_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        FileOpenPicker picker;

        _State.WindowRoot().TryPropagateHostingWindow(picker); // if we don't do this, there's no HWND for it to attach to
        picker.ViewMode(PickerViewMode::Thumbnail);
        picker.SuggestedStartLocation(PickerLocationId::ComputerFolder);
        picker.FileTypeFilter().ReplaceAll({ L".bat", L".exe", L".cmd" });

        StorageFile file = co_await picker.PickSingleFileAsync();
        if (file != nullptr)
        {
            _State.Profile().Commandline(file.Path());
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
            _State.Profile().StartingDirectory(folder.Path());
        }
    }

    IInspectable Profiles::CurrentFontWeight() const
    {
        // if no value was found, we have a custom value
        const auto maybeEnumEntry{ _FontWeightMap.TryLookup(_State.Profile().FontWeight().Weight) };
        return maybeEnumEntry ? maybeEnumEntry : _CustomFontWeight;
    }

    void Profiles::CurrentFontWeight(const IInspectable& enumEntry)
    {
        if (auto ee = enumEntry.try_as<Editor::EnumEntry>())
        {
            if (ee != _CustomFontWeight)
            {
                const auto weight{ winrt::unbox_value<uint16_t>(ee.EnumValue()) };
                const Windows::UI::Text::FontWeight setting{ weight };
                _State.Profile().FontWeight(setting);

                // Profile does not have observable properties
                // So the TwoWay binding doesn't update on the State --> Slider direction
                FontWeightSlider().Value(weight);
            }
            _PropertyChangedHandlers(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"IsCustomFontWeight" });
        }
    }

    bool Profiles::IsCustomFontWeight()
    {
        // Use SelectedItem instead of CurrentFontWeight.
        // CurrentFontWeight converts the Profile's value to the appropriate enum entry,
        // whereas SelectedItem identifies which one was selected by the user.
        return FontWeightComboBox().SelectedItem() == _CustomFontWeight;
    }

    void Profiles::BIAlignment_Click(IInspectable const& sender, RoutedEventArgs const& /*e*/)
    {
        if (const auto& button{ sender.try_as<Windows::UI::Xaml::Controls::Primitives::ToggleButton>() })
        {
            if (const auto& tag{ button.Tag().try_as<int32_t>() })
            {
                // Update the Profile's value
                _State.Profile().BackgroundImageAlignment(static_cast<ConvergedAlignment>(*tag));

                // reset all of the buttons to unchecked, except for the one that was clicked
                for (const auto& biButton : _BIAlignmentButtons)
                {
                    biButton.IsChecked(biButton == button);
                }
            }
        }
    }

    void Profiles::CursorShape_Changed(IInspectable const& /*sender*/, RoutedEventArgs const& /*e*/)
    {
        _PropertyChangedHandlers(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"IsVintageCursor" });
    }

    bool Profiles::IsVintageCursor() const
    {
        return _State.Profile().CursorShape() == TerminalControl::CursorStyle::Vintage;
    }
}
