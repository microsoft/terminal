// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Profiles.h"
#include "PreviewConnection.h"
#include "Profiles.g.cpp"
#include "EnumEntry.h"

#include <LibraryResources.h>

using namespace winrt::Windows::UI::Text;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Data;
using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::Terminal::Settings::Model;

static const std::array<winrt::guid, 2> InBoxProfileGuids{
    winrt::guid{ 0x61c54bbd, 0xc2c6, 0x5271, { 0x96, 0xe7, 0x00, 0x9a, 0x87, 0xff, 0x44, 0xbf } }, // Windows Powershell
    winrt::guid{ 0x0caa0dad, 0x35be, 0x5f56, { 0xa8, 0xff, 0xaf, 0xce, 0xee, 0xaa, 0x61, 0x01 } } // Command Prompt
};

// Function Description:
// - This function presents a File Open "common dialog" and returns its selected file asynchronously.
// Parameters:
// - customize: A lambda that receives an IFileDialog* to customize.
// Return value:
// (async) path to the selected item.
template<typename TLambda>
static winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> OpenFilePicker(HWND parentHwnd, TLambda&& customize)
{
    auto fileDialog{ winrt::create_instance<IFileDialog>(CLSID_FileOpenDialog) };
    DWORD flags{};
    THROW_IF_FAILED(fileDialog->GetOptions(&flags));
    THROW_IF_FAILED(fileDialog->SetOptions(flags | FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR | FOS_DONTADDTORECENT)); // filesystem objects only; no recent places
    customize(fileDialog.get());

    auto hr{ fileDialog->Show(parentHwnd) };
    if (!SUCCEEDED(hr))
    {
        if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
        {
            co_return winrt::hstring{};
        }
        THROW_HR(hr);
    }

    winrt::com_ptr<IShellItem> result;
    THROW_IF_FAILED(fileDialog->GetResult(result.put()));

    wil::unique_cotaskmem_string filePath;
    THROW_IF_FAILED(result->GetDisplayName(SIGDN_FILESYSPATH, &filePath));

    co_return winrt::hstring{ filePath.get() };
}

// Function Description:
// - Helper that opens a file picker pre-seeded with image file types.
static winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> OpenImagePicker(HWND parentHwnd)
{
    static constexpr COMDLG_FILTERSPEC supportedImageFileTypes[] = {
        { L"All Supported Bitmap Types (*.jpg, *.jpeg, *.png, *.bmp, *.gif, *.tiff, *.ico)", L"*.jpg;*.jpeg;*.png;*.bmp;*.gif;*.tiff;*.ico" },
        { L"All Files (*.*)", L"*.*" }
    };

    static constexpr winrt::guid clientGuidImagePicker{ 0x55675F54, 0x74A1, 0x4552, { 0xA3, 0x9D, 0x94, 0xAE, 0x85, 0xD8, 0xF2, 0x7A } };
    return OpenFilePicker(parentHwnd, [](auto&& dialog) {
        THROW_IF_FAILED(dialog->SetClientGuid(clientGuidImagePicker));
        try
        {
            auto pictureFolderShellItem{ winrt::capture<IShellItem>(&SHGetKnownFolderItem, FOLDERID_PicturesLibrary, KF_FLAG_DEFAULT, nullptr) };
            dialog->SetDefaultFolder(pictureFolderShellItem.get());
        }
        CATCH_LOG(); // non-fatal
        THROW_IF_FAILED(dialog->SetFileTypes(ARRAYSIZE(supportedImageFileTypes), supportedImageFileTypes));
        THROW_IF_FAILED(dialog->SetFileTypeIndex(1)); // the array is 1-indexed
        THROW_IF_FAILED(dialog->SetDefaultExtension(L"jpg;jpeg;png;bmp;gif;tiff;ico"));
    });
}

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Windows::Foundation::Collections::IObservableVector<Editor::Font> ProfileViewModel::_MonospaceFontList{ nullptr };
    Windows::Foundation::Collections::IObservableVector<Editor::Font> ProfileViewModel::_FontList{ nullptr };

    ProfileViewModel::ProfileViewModel(const Model::Profile& profile, const Model::CascadiaSettings& appSettings) :
        _profile{ profile },
        _originalProfileGuid{ profile.Guid() },
        _ShowAllFonts{ false },
        _appSettings{ appSettings }
    {
        // Add a property changed handler to our own property changed event.
        // This propagates changes from the settings model to anybody listening to our
        //  unique view model members.
        PropertyChanged([this](auto&&, const PropertyChangedEventArgs& args) {
            const auto viewModelProperty{ args.PropertyName() };
            if (viewModelProperty == L"BackgroundImagePath")
            {
                // notify listener that all background image related values might have changed
                //
                // We need to do this so if someone manually types "desktopWallpaper"
                // into the path TextBox, we properly update the checkbox and stored
                // _lastBgImagePath. Without this, then we'll permanently hide the text
                // box, prevent it from ever being changed again.
                _NotifyChanges(L"UseDesktopBGImage", L"BackgroundImageSettingsVisible");
            }
            else if (viewModelProperty == L"IsBaseLayer")
            {
                // we _always_ want to show the background image settings in base layer
                _NotifyChanges(L"BackgroundImageSettingsVisible");
            }
            else if (viewModelProperty == L"StartingDirectory")
            {
                // notify listener that all starting directory related values might have changed
                // NOTE: this is similar to what is done with BackgroundImagePath above
                _NotifyChanges(L"UseParentProcessDirectory", L"UseCustomStartingDirectory");
            }
            else if (viewModelProperty == L"FontFace")
            {
                // notify listener that all font face related values might have changed
                if (!UsingMonospaceFont())
                {
                    _ShowAllFonts = true;
                }
                _NotifyChanges(L"ShowAllFonts", L"UsingMonospaceFont");
            }
        });

        // Cache the original BG image path. If the user clicks "Use desktop
        // wallpaper", then un-checks it, this is the string we'll restore to
        // them.
        if (BackgroundImagePath() != L"desktopWallpaper")
        {
            _lastBgImagePath = BackgroundImagePath();
        }

        // Do the same for the starting directory
        if (!StartingDirectory().empty())
        {
            _lastStartingDirectoryPath = StartingDirectory();
        }

        // generate the font list, if we don't have one
        if (!_FontList || !_MonospaceFontList)
        {
            UpdateFontList();
        }
    }

    Model::TerminalSettings ProfileViewModel::TermSettings() const
    {
        return Model::TerminalSettings::CreateWithProfileByID(_appSettings, _profile.Guid(), nullptr).DefaultSettings();
    }

    // Method Description:
    // - Updates the lists of fonts and sorts them alphabetically
    void ProfileViewModel::UpdateFontList() noexcept
    try
    {
        // initialize font list
        std::vector<Editor::Font> fontList;
        std::vector<Editor::Font> monospaceFontList;

        // get a DWriteFactory
        com_ptr<IDWriteFactory> factory;
        THROW_IF_FAILED(DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<::IUnknown**>(factory.put())));

        // get the font collection; subscribe to updates
        com_ptr<IDWriteFontCollection> fontCollection;
        THROW_IF_FAILED(factory->GetSystemFontCollection(fontCollection.put(), TRUE));

        for (UINT32 i = 0; i < fontCollection->GetFontFamilyCount(); ++i)
        {
            try
            {
                // get the font family
                com_ptr<IDWriteFontFamily> fontFamily;
                THROW_IF_FAILED(fontCollection->GetFontFamily(i, fontFamily.put()));

                // get the font's localized names
                com_ptr<IDWriteLocalizedStrings> localizedFamilyNames;
                THROW_IF_FAILED(fontFamily->GetFamilyNames(localizedFamilyNames.put()));

                // construct a font entry for tracking
                if (const auto fontEntry{ _GetFont(localizedFamilyNames) })
                {
                    // check if the font is monospaced
                    try
                    {
                        com_ptr<IDWriteFont> font;
                        THROW_IF_FAILED(fontFamily->GetFirstMatchingFont(DWRITE_FONT_WEIGHT::DWRITE_FONT_WEIGHT_NORMAL,
                                                                         DWRITE_FONT_STRETCH::DWRITE_FONT_STRETCH_NORMAL,
                                                                         DWRITE_FONT_STYLE::DWRITE_FONT_STYLE_NORMAL,
                                                                         font.put()));

                        // add the font name to our list of monospace fonts
                        const auto castedFont{ font.try_as<IDWriteFont1>() };
                        if (castedFont && castedFont->IsMonospacedFont())
                        {
                            monospaceFontList.emplace_back(fontEntry);
                        }
                    }
                    CATCH_LOG();

                    // add the font name to our list of all fonts
                    fontList.emplace_back(std::move(fontEntry));
                }
            }
            CATCH_LOG();
        }

        // sort and save the lists
        std::sort(begin(fontList), end(fontList), FontComparator());
        _FontList = single_threaded_observable_vector<Editor::Font>(std::move(fontList));

        std::sort(begin(monospaceFontList), end(monospaceFontList), FontComparator());
        _MonospaceFontList = single_threaded_observable_vector<Editor::Font>(std::move(monospaceFontList));
    }
    CATCH_LOG();

    Editor::Font ProfileViewModel::_GetFont(com_ptr<IDWriteLocalizedStrings> localizedFamilyNames)
    {
        // used for the font's name as an identifier (i.e. text block's font family property)
        std::wstring nameID;
        UINT32 nameIDIndex;

        // used for the font's localized name
        std::wstring localizedName;
        UINT32 localizedNameIndex;

        // use our current locale to find the localized name
        BOOL exists{ FALSE };
        HRESULT hr;
        wchar_t localeName[LOCALE_NAME_MAX_LENGTH];
        if (GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH))
        {
            hr = localizedFamilyNames->FindLocaleName(localeName, &localizedNameIndex, &exists);
        }
        if (SUCCEEDED(hr) && !exists)
        {
            // if we can't find the font for our locale, fallback to the en-us one
            // Source: https://docs.microsoft.com/en-us/windows/win32/api/dwrite/nf-dwrite-idwritelocalizedstrings-findlocalename
            hr = localizedFamilyNames->FindLocaleName(L"en-us", &localizedNameIndex, &exists);
        }
        if (!exists)
        {
            // failed to find the correct locale, using the first one
            localizedNameIndex = 0;
        }

        // get the localized name
        UINT32 nameLength;
        THROW_IF_FAILED(localizedFamilyNames->GetStringLength(localizedNameIndex, &nameLength));

        localizedName.resize(nameLength);
        THROW_IF_FAILED(localizedFamilyNames->GetString(localizedNameIndex, localizedName.data(), nameLength + 1));

        // now get the nameID
        hr = localizedFamilyNames->FindLocaleName(L"en-us", &nameIDIndex, &exists);
        if (FAILED(hr) || !exists)
        {
            // failed to find it, using the first one
            nameIDIndex = 0;
        }

        // get the nameID
        THROW_IF_FAILED(localizedFamilyNames->GetStringLength(nameIDIndex, &nameLength));
        nameID.resize(nameLength);
        THROW_IF_FAILED(localizedFamilyNames->GetString(nameIDIndex, nameID.data(), nameLength + 1));

        if (!nameID.empty() && !localizedName.empty())
        {
            return make<Font>(nameID, localizedName);
        }
        return nullptr;
    }

    IObservableVector<Editor::Font> ProfileViewModel::CompleteFontList() const noexcept
    {
        return _FontList;
    }

    IObservableVector<Editor::Font> ProfileViewModel::MonospaceFontList() const noexcept
    {
        return _MonospaceFontList;
    }

    // Method Description:
    // - Searches through our list of monospace fonts to determine if the settings model's current font face is a monospace font
    // - NOTE: This is information stored from DWrite in _UpdateFontList()
    bool ProfileViewModel::UsingMonospaceFont() const noexcept
    {
        bool result{ false };
        const auto currentFont{ FontFace() };
        for (const auto& font : _MonospaceFontList)
        {
            if (font.LocalizedName() == currentFont)
            {
                result = true;
            }
        }
        return result;
    }

    // Method Description:
    // - Determines whether we should show the list of all the fonts, or we should just show monospace fonts
    bool ProfileViewModel::ShowAllFonts() const noexcept
    {
        // - _ShowAllFonts is directly bound to the checkbox. So this is the user set value.
        // - If we are not using a monospace font, show all of the fonts so that the ComboBox is still properly bound
        return _ShowAllFonts || !UsingMonospaceFont();
    }

    void ProfileViewModel::ShowAllFonts(const bool& value)
    {
        if (_ShowAllFonts != value)
        {
            _ShowAllFonts = value;
            _NotifyChanges(L"ShowAllFonts");
        }
    }

    winrt::guid ProfileViewModel::OriginalProfileGuid() const noexcept
    {
        return _originalProfileGuid;
    }

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

    bool ProfileViewModel::UseDesktopBGImage()
    {
        return BackgroundImagePath() == L"desktopWallpaper";
    }

    void ProfileViewModel::UseDesktopBGImage(const bool useDesktop)
    {
        if (useDesktop)
        {
            // Stash the current value of BackgroundImagePath. If the user
            // checks and un-checks the "Use desktop wallpaper" button, we want
            // the path that we display in the text box to remain unchanged.
            //
            // Only stash this value if it's not the special "desktopWallpaper"
            // value.
            if (BackgroundImagePath() != L"desktopWallpaper")
            {
                _lastBgImagePath = BackgroundImagePath();
            }
            BackgroundImagePath(L"desktopWallpaper");
        }
        else
        {
            // Restore the path we had previously cached. This might be the
            // empty string.
            BackgroundImagePath(_lastBgImagePath);
        }
    }

    bool ProfileViewModel::UseParentProcessDirectory()
    {
        return StartingDirectory().empty();
    }

    // This function simply returns the opposite of UseParentProcessDirectory.
    // We bind the 'IsEnabled' parameters of the textbox and browse button
    // to this because it needs to be the reverse of UseParentProcessDirectory
    // but we don't want to create a whole new converter for inverting a boolean
    bool ProfileViewModel::UseCustomStartingDirectory()
    {
        return !UseParentProcessDirectory();
    }

    void ProfileViewModel::UseParentProcessDirectory(const bool useParent)
    {
        if (useParent)
        {
            // Stash the current value of StartingDirectory. If the user
            // checks and un-checks the "Use parent process directory" button, we want
            // the path that we display in the text box to remain unchanged.
            //
            // Only stash this value if it's not empty
            if (!StartingDirectory().empty())
            {
                _lastStartingDirectoryPath = StartingDirectory();
            }
            StartingDirectory(L"");
        }
        else
        {
            // Restore the path we had previously cached as long as it wasn't empty
            // If it was empty, set the starting directory to %USERPROFILE%
            // (we need to set it to something non-empty otherwise we will automatically
            // disable the text box)
            if (_lastStartingDirectoryPath.empty())
            {
                StartingDirectory(L"%USERPROFILE%");
            }
            else
            {
                StartingDirectory(_lastStartingDirectoryPath);
            }
        }
    }

    bool ProfileViewModel::BackgroundImageSettingsVisible()
    {
        return IsBaseLayer() || BackgroundImagePath() != L"";
    }

    void ProfilePageNavigationState::DeleteProfile()
    {
        auto deleteProfileArgs{ winrt::make_self<DeleteProfileEventArgs>(_Profile.Guid()) };
        _DeleteProfileHandlers(*this, *deleteProfileArgs);
    }

    Profiles::Profiles() :
        _ColorSchemeList{ single_threaded_observable_vector<ColorScheme>() },
        _previewControl{ Control::TermControl(Model::TerminalSettings{}, make<PreviewConnection>()) }
    {
        InitializeComponent();

        INITIALIZE_BINDABLE_ENUM_SETTING(CursorShape, CursorStyle, winrt::Microsoft::Terminal::Core::CursorStyle, L"Profile_CursorShape", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING_REVERSE_ORDER(BackgroundImageStretchMode, BackgroundImageStretchMode, winrt::Windows::UI::Xaml::Media::Stretch, L"Profile_BackgroundImageStretchMode", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(AntiAliasingMode, TextAntialiasingMode, winrt::Microsoft::Terminal::Control::TextAntialiasingMode, L"Profile_AntialiasingMode", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING_REVERSE_ORDER(CloseOnExitMode, CloseOnExitMode, winrt::Microsoft::Terminal::Settings::Model::CloseOnExitMode, L"Profile_CloseOnExit", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(ScrollState, ScrollbarState, winrt::Microsoft::Terminal::Control::ScrollbarState, L"Profile_ScrollbarVisibility", L"Content");

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

        // apply automation properties to more complex setting controls
        for (const auto& biButton : _BIAlignmentButtons)
        {
            const auto tooltip{ ToolTipService::GetToolTip(biButton) };
            Automation::AutomationProperties::SetName(biButton, unbox_value<hstring>(tooltip));
        }

        const auto startingDirCheckboxTooltip{ ToolTipService::GetToolTip(StartingDirectoryUseParentCheckbox()) };
        Automation::AutomationProperties::SetFullDescription(StartingDirectoryUseParentCheckbox(), unbox_value<hstring>(startingDirCheckboxTooltip));

        const auto backgroundImgCheckboxTooltip{ ToolTipService::GetToolTip(UseDesktopImageCheckBox()) };
        Automation::AutomationProperties::SetFullDescription(UseDesktopImageCheckBox(), unbox_value<hstring>(backgroundImgCheckboxTooltip));

        const auto showAllFontsCheckboxTooltip{ ToolTipService::GetToolTip(ShowAllFontsCheckbox()) };
        Automation::AutomationProperties::SetFullDescription(ShowAllFontsCheckbox(), unbox_value<hstring>(showAllFontsCheckboxTooltip));

        Automation::AutomationProperties::SetName(DeleteButton(), RS_(L"Profile_DeleteButton/Text"));

        _previewControl.IsEnabled(false);
        _previewControl.AllowFocusWhenDisabled(false);
        ControlPreview().Child(_previewControl);
    }

    IInspectable Profiles::CurrentFontFace() const
    {
        // look for the current font in our shown list of fonts
        const auto& profileVM{ State().Profile() };
        const auto profileFontFace{ profileVM.FontFace() };
        const auto& currentFontList{ profileVM.ShowAllFonts() ? profileVM.CompleteFontList() : profileVM.MonospaceFontList() };
        IInspectable fallbackFont;
        for (const auto& font : currentFontList)
        {
            if (font.LocalizedName() == profileFontFace)
            {
                return box_value(font);
            }
            else if (font.LocalizedName() == L"Cascadia Mono")
            {
                fallbackFont = box_value(font);
            }
        }

        // we couldn't find the desired font, set to "Cascadia Mono" since that ships by default
        return fallbackFont;
    }

    void Profiles::FontFace_SelectionChanged(IInspectable const& /*sender*/, SelectionChangedEventArgs const& e)
    {
        // NOTE: We need to hook up a selection changed event handler here instead of directly binding to the profile view model.
        //       A two way binding to the view model causes an infinite loop because both combo boxes keep fighting over which one's right.
        const auto selectedItem{ e.AddedItems().GetAt(0) };
        const auto newFontFace{ unbox_value<Editor::Font>(selectedItem) };
        State().Profile().FontFace(newFontFace.LocalizedName());
    }

    void Profiles::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _State = e.Parameter().as<Editor::ProfilePageNavigationState>();

        // generate the font list, if we don't have one
        if (!_State.Profile().CompleteFontList() || !_State.Profile().MonospaceFontList())
        {
            ProfileViewModel::UpdateFontList();
        }

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

        // Check the use parent directory box if the starting directory is empty
        if (_State.Profile().StartingDirectory().empty())
        {
            StartingDirectoryUseParentCheckbox().IsChecked(true);
        }

        // Subscribe to some changes in the view model
        // These changes should force us to update our own set of "Current<Setting>" members,
        // and propagate those changes to the UI
        _ViewModelChangedRevoker = _State.Profile().PropertyChanged(winrt::auto_revoke, [=](auto&&, const PropertyChangedEventArgs& args) {
            const auto settingName{ args.PropertyName() };
            if (settingName == L"CursorShape")
            {
                _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentCursorShape" });
                _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"IsVintageCursor" });
            }
            else if (settingName == L"BackgroundImageStretchMode")
            {
                _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentBackgroundImageStretchMode" });
            }
            else if (settingName == L"AntialiasingMode")
            {
                _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentAntiAliasingMode" });
            }
            else if (settingName == L"CloseOnExit")
            {
                _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentCloseOnExitMode" });
            }
            else if (settingName == L"BellStyle")
            {
                _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"IsBellStyleFlagSet" });
            }
            else if (settingName == L"ScrollState")
            {
                _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentScrollState" });
            }
            else if (settingName == L"FontWeight")
            {
                _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentFontWeight" });
                _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"IsCustomFontWeight" });
            }
            else if (settingName == L"ColorSchemeName")
            {
                _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentColorScheme" });
            }
            else if (settingName == L"FontFace" || settingName == L"CurrentFontList")
            {
                _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentFontFace" });
            }
            else if (settingName == L"BackgroundImageAlignment")
            {
                _UpdateBIAlignmentControl(static_cast<int32_t>(_State.Profile().BackgroundImageAlignment()));
            }
            _previewControl.Settings(_State.Profile().TermSettings());
            _previewControl.UpdateSettings();
        });

        // Navigate to the pivot in the provided navigation state
        ProfilesPivot().SelectedIndex(static_cast<int>(_State.LastActivePivot()));

        _previewControl.Settings(_State.Profile().TermSettings());
        // There is a possibility that the control has not fully initialized yet,
        // so wait for it to initialize before updating the settings (so we know
        // that the renderer is set up)
        _previewControl.Initialized([&](auto&& /*s*/, auto&& /*e*/) {
            _previewControl.UpdateSettings();
        });
    }

    void Profiles::OnNavigatedFrom(const NavigationEventArgs& /*e*/)
    {
        _ViewModelChangedRevoker.revoke();
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

    bool Profiles::IsBellStyleFlagSet(const uint32_t flag)
    {
        return (WI_EnumValue(_State.Profile().BellStyle()) & flag) == flag;
    }

    void Profiles::SetBellStyleAudible(winrt::Windows::Foundation::IReference<bool> on)
    {
        auto currentStyle = State().Profile().BellStyle();
        WI_UpdateFlag(currentStyle, Model::BellStyle::Audible, winrt::unbox_value<bool>(on));
        State().Profile().BellStyle(currentStyle);
    }

    void Profiles::SetBellStyleWindow(winrt::Windows::Foundation::IReference<bool> on)
    {
        auto currentStyle = State().Profile().BellStyle();
        WI_UpdateFlag(currentStyle, Model::BellStyle::Window, winrt::unbox_value<bool>(on));
        State().Profile().BellStyle(currentStyle);
    }

    void Profiles::SetBellStyleTaskbar(winrt::Windows::Foundation::IReference<bool> on)
    {
        auto currentStyle = State().Profile().BellStyle();
        WI_UpdateFlag(currentStyle, Model::BellStyle::Taskbar, winrt::unbox_value<bool>(on));
        State().Profile().BellStyle(currentStyle);
    }

    void Profiles::DeleteConfirmation_Click(IInspectable const& /*sender*/, RoutedEventArgs const& /*e*/)
    {
        auto state{ winrt::get_self<ProfilePageNavigationState>(_State) };
        state->DeleteProfile();
    }

    fire_and_forget Profiles::BackgroundImage_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        const auto parentHwnd{ reinterpret_cast<HWND>(_State.WindowRoot().GetHostingWindow()) };
        auto file = co_await OpenImagePicker(parentHwnd);
        if (!file.empty())
        {
            _State.Profile().BackgroundImagePath(file);
        }
    }

    fire_and_forget Profiles::Icon_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        const auto parentHwnd{ reinterpret_cast<HWND>(_State.WindowRoot().GetHostingWindow()) };
        auto file = co_await OpenImagePicker(parentHwnd);
        if (!file.empty())
        {
            _State.Profile().Icon(file);
        }
    }

    fire_and_forget Profiles::Commandline_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        static constexpr COMDLG_FILTERSPEC supportedFileTypes[] = {
            { L"Executable Files (*.exe, *.cmd, *.bat)", L"*.exe;*.cmd;*.bat" },
            { L"All Files (*.*)", L"*.*" }
        };

        static constexpr winrt::guid clientGuidExecutables{ 0x2E7E4331, 0x0800, 0x48E6, { 0xB0, 0x17, 0xA1, 0x4C, 0xD8, 0x73, 0xDD, 0x58 } };
        const auto parentHwnd{ reinterpret_cast<HWND>(_State.WindowRoot().GetHostingWindow()) };
        auto path = co_await OpenFilePicker(parentHwnd, [](auto&& dialog) {
            THROW_IF_FAILED(dialog->SetClientGuid(clientGuidExecutables));
            try
            {
                auto folderShellItem{ winrt::capture<IShellItem>(&SHGetKnownFolderItem, FOLDERID_ComputerFolder, KF_FLAG_DEFAULT, nullptr) };
                dialog->SetDefaultFolder(folderShellItem.get());
            }
            CATCH_LOG(); // non-fatal
            THROW_IF_FAILED(dialog->SetFileTypes(ARRAYSIZE(supportedFileTypes), supportedFileTypes));
            THROW_IF_FAILED(dialog->SetFileTypeIndex(1)); // the array is 1-indexed
            THROW_IF_FAILED(dialog->SetDefaultExtension(L"exe;cmd;bat"));
        });

        if (!path.empty())
        {
            _State.Profile().Commandline(path);
        }
    }

    fire_and_forget Profiles::StartingDirectory_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();
        const auto parentHwnd{ reinterpret_cast<HWND>(_State.WindowRoot().GetHostingWindow()) };
        auto folder = co_await OpenFilePicker(parentHwnd, [](auto&& dialog) {
            static constexpr winrt::guid clientGuidFolderPicker{ 0xAADAA433, 0xB04D, 0x4BAE, { 0xB1, 0xEA, 0x1E, 0x6C, 0xD1, 0xCD, 0xA6, 0x8B } };
            THROW_IF_FAILED(dialog->SetClientGuid(clientGuidFolderPicker));
            try
            {
                auto folderShellItem{ winrt::capture<IShellItem>(&SHGetKnownFolderItem, FOLDERID_ComputerFolder, KF_FLAG_DEFAULT, nullptr) };
                dialog->SetDefaultFolder(folderShellItem.get());
            }
            CATCH_LOG(); // non-fatal

            DWORD flags{};
            THROW_IF_FAILED(dialog->GetOptions(&flags));
            THROW_IF_FAILED(dialog->SetOptions(flags | FOS_PICKFOLDERS)); // folders only
        });

        if (!folder.empty())
        {
            _State.Profile().StartingDirectory(folder);
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
            _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"IsCustomFontWeight" });
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
        if (const auto& button{ sender.try_as<Primitives::ToggleButton>() })
        {
            if (const auto& tag{ button.Tag().try_as<int32_t>() })
            {
                // Update the Profile's value and the control
                _State.Profile().BackgroundImageAlignment(static_cast<ConvergedAlignment>(*tag));
                _UpdateBIAlignmentControl(*tag);
            }
        }
    }

    // Method Description:
    // - Resets all of the buttons to unchecked, and checks the one with the provided tag
    // Arguments:
    // - val - the background image alignment (ConvergedAlignment) that we want to represent in the control
    void Profiles::_UpdateBIAlignmentControl(const int32_t val)
    {
        for (const auto& biButton : _BIAlignmentButtons)
        {
            if (const auto& biButtonAlignment{ biButton.Tag().try_as<int32_t>() })
            {
                biButton.IsChecked(biButtonAlignment == val);
            }
        }
    }

    bool Profiles::IsVintageCursor() const
    {
        return _State.Profile().CursorShape() == Core::CursorStyle::Vintage;
    }

    void Profiles::Pivot_SelectionChanged(Windows::Foundation::IInspectable const& /*sender*/,
                                          RoutedEventArgs const& /*e*/)
    {
        _State.LastActivePivot(static_cast<Editor::ProfilesPivots>(ProfilesPivot().SelectedIndex()));
    }

}
