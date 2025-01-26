// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ProfileViewModel.h"
#include "ProfileViewModel.g.cpp"
#include "EnumEntry.h"
#include "Appearances.h"

#include <LibraryResources.h>
#include "../WinRTUtils/inc/Utils.h"
#include "../../renderer/base/FontCache.h"
#include "SegoeFluentIconList.h"

using namespace winrt::Windows::UI::Text;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Data;
using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    static Editor::Font fontObjectForDWriteFont(IDWriteFontFamily* family, const wchar_t* locale);

    Windows::Foundation::Collections::IObservableVector<Editor::Font> ProfileViewModel::_MonospaceFontList{ nullptr };
    Windows::Foundation::Collections::IObservableVector<Editor::Font> ProfileViewModel::_FontList{ nullptr };
    Windows::Foundation::Collections::IVector<IInspectable> ProfileViewModel::_BuiltInIcons{ nullptr };

    static constexpr std::wstring_view HideIconValue{ L"none" };

    ProfileViewModel::ProfileViewModel(const Model::Profile& profile, const Model::CascadiaSettings& appSettings, const Windows::UI::Core::CoreDispatcher& dispatcher) :
        _profile{ profile },
        _defaultAppearanceViewModel{ winrt::make<implementation::AppearanceViewModel>(profile.DefaultAppearance().try_as<AppearanceConfig>()) },
        _originalProfileGuid{ profile.Guid() },
        _appSettings{ appSettings },
        _unfocusedAppearanceViewModel{ nullptr },
        _dispatcher{ dispatcher }
    {
        INITIALIZE_BINDABLE_ENUM_SETTING(AntiAliasingMode, TextAntialiasingMode, winrt::Microsoft::Terminal::Control::TextAntialiasingMode, L"Profile_AntialiasingMode", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING_REVERSE_ORDER(CloseOnExitMode, CloseOnExitMode, winrt::Microsoft::Terminal::Settings::Model::CloseOnExitMode, L"Profile_CloseOnExit", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(ScrollState, ScrollbarState, winrt::Microsoft::Terminal::Control::ScrollbarState, L"Profile_ScrollbarVisibility", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(PathTranslationStyle, PathTranslationStyle, winrt::Microsoft::Terminal::Control::PathTranslationStyle, L"Profile_PathTranslationStyle", L"Content");

        _InitializeCurrentBellSounds();

        // set up IconTypes
        std::vector<IInspectable> iconTypes;
        iconTypes.reserve(4);
        iconTypes.emplace_back(make<EnumEntry>(RS_(L"Profile_IconTypeNone"), box_value(IconType::None)));
        iconTypes.emplace_back(make<EnumEntry>(RS_(L"Profile_IconTypeFontIcon"), box_value(IconType::FontIcon)));
        iconTypes.emplace_back(make<EnumEntry>(RS_(L"Profile_IconTypeEmoji"), box_value(IconType::Emoji)));
        iconTypes.emplace_back(make<EnumEntry>(RS_(L"Profile_IconTypeImage"), box_value(IconType::Image)));
        _IconTypes = winrt::single_threaded_vector<IInspectable>(std::move(iconTypes));
        _DeduceCurrentIconType();
        _DeduceCurrentBuiltInIcon();

        // Add a property changed handler to our own property changed event.
        // This propagates changes from the settings model to anybody listening to our
        //  unique view model members.
        PropertyChanged([this](auto&&, const PropertyChangedEventArgs& args) {
            const auto viewModelProperty{ args.PropertyName() };
            if (viewModelProperty == L"IsBaseLayer")
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
            else if (viewModelProperty == L"AntialiasingMode")
            {
                _NotifyChanges(L"CurrentAntiAliasingMode");
            }
            else if (viewModelProperty == L"CloseOnExit")
            {
                _NotifyChanges(L"CurrentCloseOnExitMode");
            }
            else if (viewModelProperty == L"BellStyle")
            {
                _NotifyChanges(L"IsBellStyleFlagSet");
            }
            else if (viewModelProperty == L"ScrollState")
            {
                _NotifyChanges(L"CurrentScrollState");
            }
            else if (viewModelProperty == L"Icon")
            {
                // _DeduceCurrentIconType() ends with a "CurrentIconType" notification
                //  so we don't need to call _UpdateIconPreview() here
                _DeduceCurrentIconType();
            }
            else if (viewModelProperty == L"CurrentIconType")
            {
                // "Using*" handles the visibility of the IconType-related UI.
                // The others propagate the rendered icon into a preview (i.e. nav view, container item)
                _NotifyChanges(L"UsingNoIcon",
                               L"UsingBuiltInIcon",
                               L"UsingEmojiIcon",
                               L"UsingImageIcon",
                               L"LocalizedIcon",
                               L"IconPreview",
                               L"EvaluatedIcon");
            }
            else if (viewModelProperty == L"CurrentBuiltInIcon")
            {
                Icon(unbox_value<hstring>(_CurrentBuiltInIcon.as<Editor::EnumEntry>().EnumValue()));
            }
            else if (viewModelProperty == L"CurrentEmojiIcon")
            {
                Icon(CurrentEmojiIcon());
            }
            else if (viewModelProperty == L"CurrentBellSounds")
            {
                // we already have infrastructure in place to
                // propagate changes from the CurrentBellSounds
                // to the model. Refer to...
                // - _InitializeCurrentBellSounds() --> _CurrentBellSounds.VectorChanged()
                // - RequestAddBellSound()
                // - RequestDeleteBellSound()
                _MarkDuplicateBellSoundDirectories();
                _NotifyChanges(L"BellSoundPreview", L"HasBellSound");
            }
            else if (viewModelProperty == L"BellSound")
            {
                _InitializeCurrentBellSounds();
            }
            else if (viewModelProperty == L"PathTranslationStyle")
            {
                _NotifyChanges(L"CurrentPathTranslationStyle");
            }
            else if (viewModelProperty == L"Padding")
            {
                _parsedPadding = StringToXamlThickness(_profile.Padding());
                _NotifyChanges(L"LeftPadding", L"TopPadding", L"RightPadding", L"BottomPadding");
            }
        });

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

        if (profile.HasUnfocusedAppearance())
        {
            _unfocusedAppearanceViewModel = winrt::make<implementation::AppearanceViewModel>(profile.UnfocusedAppearance().try_as<AppearanceConfig>());
        }

        _parsedPadding = StringToXamlThickness(_profile.Padding());
        _defaultAppearanceViewModel.IsDefault(true);
    }

    void ProfileViewModel::_UpdateBuiltInIcons()
    {
        std::vector<IInspectable> builtInIcons;
        for (auto& [val, name] : s_SegoeFluentIcons)
        {
            builtInIcons.emplace_back(make<EnumEntry>(hstring{ name }, box_value(val)));
        }
        _BuiltInIcons = single_threaded_vector<IInspectable>(std::move(builtInIcons));
    }

    void ProfileViewModel::_DeduceCurrentIconType()
    {
        const auto& profileIcon = _profile.Icon();
        if (profileIcon == HideIconValue)
        {
            _currentIconType = _IconTypes.GetAt(0);
        }
        else if (L"\uE700" <= profileIcon && profileIcon <= L"\uF8B3")
        {
            _currentIconType = _IconTypes.GetAt(1);
            _DeduceCurrentBuiltInIcon();
        }
        else if (profileIcon.size() <= 2)
        {
            // We already did a range check for MDL2 Assets in the previous one,
            // so if we're out of that range but still short, assume we're an emoji
            _currentIconType = _IconTypes.GetAt(2);
        }
        else
        {
            _currentIconType = _IconTypes.GetAt(3);
        }
        _NotifyChanges(L"CurrentIconType");
    }

    void ProfileViewModel::_DeduceCurrentBuiltInIcon()
    {
        if (!_BuiltInIcons)
        {
            _UpdateBuiltInIcons();
        }
        const auto& profileIcon = Icon();
        for (uint32_t i = 0; i < _BuiltInIcons.Size(); i++)
        {
            const auto& builtIn = _BuiltInIcons.GetAt(i);
            if (profileIcon == unbox_value<hstring>(builtIn.as<Editor::EnumEntry>().EnumValue()))
            {
                _CurrentBuiltInIcon = builtIn;
                return;
            }
        }
        _CurrentBuiltInIcon = _BuiltInIcons.GetAt(0);
        _NotifyChanges(L"CurrentBuiltInIcon");
    }

    void ProfileViewModel::LeftPadding(double value) noexcept
    {
        if (std::abs(_parsedPadding.Left - value) >= .0001)
        {
            _parsedPadding.Left = value;
            Padding(XamlThicknessToOptimalString(_parsedPadding));
        }
    }

    double ProfileViewModel::LeftPadding() const noexcept
    {
        return _parsedPadding.Left;
    }

    void ProfileViewModel::TopPadding(double value) noexcept
    {
        if (std::abs(_parsedPadding.Top - value) >= .0001)
        {
            _parsedPadding.Top = value;
            Padding(XamlThicknessToOptimalString(_parsedPadding));
        }
    }

    double ProfileViewModel::TopPadding() const noexcept
    {
        return _parsedPadding.Top;
    }

    void ProfileViewModel::RightPadding(double value) noexcept
    {
        if (std::abs(_parsedPadding.Right - value) >= .0001)
        {
            _parsedPadding.Right = value;
            Padding(XamlThicknessToOptimalString(_parsedPadding));
        }
    }

    double ProfileViewModel::RightPadding() const noexcept
    {
        return _parsedPadding.Right;
    }

    void ProfileViewModel::BottomPadding(double value) noexcept
    {
        if (std::abs(_parsedPadding.Bottom - value) >= .0001)
        {
            _parsedPadding.Bottom = value;
            Padding(XamlThicknessToOptimalString(_parsedPadding));
        }
    }

    double ProfileViewModel::BottomPadding() const noexcept
    {
        return _parsedPadding.Bottom;
    }
    Model::TerminalSettings ProfileViewModel::TermSettings() const
    {
        return Model::TerminalSettings::CreateForPreview(_appSettings, _profile);
    }

    // Method Description:
    // - Updates the lists of fonts and sorts them alphabetically
    void ProfileViewModel::UpdateFontList() noexcept
    try
    {
        // initialize font list
        std::vector<Editor::Font> fontList;
        std::vector<Editor::Font> monospaceFontList;

        // get the font collection; subscribe to updates
        wil::com_ptr<IDWriteFactory> factory;
        THROW_IF_FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(factory), reinterpret_cast<::IUnknown**>(factory.addressof())));

        wil::com_ptr<IDWriteFontCollection> fontCollection;
        THROW_IF_FAILED(factory->GetSystemFontCollection(fontCollection.addressof(), TRUE));

        wchar_t localeName[LOCALE_NAME_MAX_LENGTH];
        if (!GetUserDefaultLocaleName(&localeName[0], LOCALE_NAME_MAX_LENGTH))
        {
            memcpy(&localeName[0], L"en-US", 12);
        }

        for (UINT32 i = 0; i < fontCollection->GetFontFamilyCount(); ++i)
        {
            try
            {
                // get the font family
                com_ptr<IDWriteFontFamily> fontFamily;
                THROW_IF_FAILED(fontCollection->GetFontFamily(i, fontFamily.put()));

                // construct a font entry for tracking
                if (const auto fontEntry{ fontObjectForDWriteFont(fontFamily.get(), &localeName[0]) })
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

        const auto comparator = [&](const Editor::Font& lhs, const Editor::Font& rhs) {
            const auto a = lhs.LocalizedName();
            const auto b = rhs.LocalizedName();
            return til::compare_linguistic_insensitive(a, b) < 0;
        };

        // sort and save the lists
        std::sort(begin(fontList), end(fontList), comparator);
        _FontList = single_threaded_observable_vector<Editor::Font>(std::move(fontList));

        std::sort(begin(monospaceFontList), end(monospaceFontList), comparator);
        _MonospaceFontList = single_threaded_observable_vector<Editor::Font>(std::move(monospaceFontList));
    }
    CATCH_LOG();

    static winrt::hstring getLocalizedStringByIndex(IDWriteLocalizedStrings* strings, UINT32 index)
    {
        UINT32 length = 0;
        THROW_IF_FAILED(strings->GetStringLength(index, &length));

        winrt::impl::hstring_builder builder{ length };
        THROW_IF_FAILED(strings->GetString(index, builder.data(), length + 1));

        return builder.to_hstring();
    }

    static UINT32 getLocalizedStringIndex(IDWriteLocalizedStrings* strings, const wchar_t* locale, UINT32 fallback)
    {
        UINT32 index;
        BOOL exists;
        if (FAILED(strings->FindLocaleName(locale, &index, &exists)) || !exists)
        {
            index = fallback;
        }
        return index;
    }

    static Editor::Font fontObjectForDWriteFont(IDWriteFontFamily* family, const wchar_t* locale)
    {
        wil::com_ptr<IDWriteLocalizedStrings> familyNames;
        THROW_IF_FAILED(family->GetFamilyNames(familyNames.addressof()));

        // If en-us is missing we fall back to whatever is at index 0.
        const auto ci = getLocalizedStringIndex(familyNames.get(), L"en-US", 0);
        // If our locale is missing we fall back to en-us.
        const auto li = getLocalizedStringIndex(familyNames.get(), locale, ci);

        auto canonical = getLocalizedStringByIndex(familyNames.get(), ci);
        // If the canonical/localized indices are the same, there's no need to get the other string.
        auto localized = ci == li ? canonical : getLocalizedStringByIndex(familyNames.get(), li);

        return make<Font>(std::move(canonical), std::move(localized));
    }

    winrt::guid ProfileViewModel::OriginalProfileGuid() const noexcept
    {
        return _originalProfileGuid;
    }

    bool ProfileViewModel::CanDeleteProfile() const
    {
        return !IsBaseLayer();
    }

    bool ProfileViewModel::Orphaned() const
    {
        return _profile.Orphaned();
    }

    hstring ProfileViewModel::TabTitlePreview() const
    {
        if (const auto tabTitle{ TabTitle() }; !tabTitle.empty())
        {
            return tabTitle;
        }
        return RS_(L"Profile_TabTitleNone");
    }

    Editor::AppearanceViewModel ProfileViewModel::DefaultAppearance()
    {
        return _defaultAppearanceViewModel;
    }

    bool ProfileViewModel::HasUnfocusedAppearance()
    {
        return _profile.HasUnfocusedAppearance();
    }

    bool ProfileViewModel::EditableUnfocusedAppearance() const noexcept
    {
        return Feature_EditableUnfocusedAppearance::IsEnabled();
    }

    bool ProfileViewModel::ShowUnfocusedAppearance()
    {
        return EditableUnfocusedAppearance() && HasUnfocusedAppearance();
    }

    void ProfileViewModel::CreateUnfocusedAppearance()
    {
        _profile.CreateUnfocusedAppearance();

        _unfocusedAppearanceViewModel = winrt::make<implementation::AppearanceViewModel>(_profile.UnfocusedAppearance().try_as<AppearanceConfig>());
        _unfocusedAppearanceViewModel.SchemesList(DefaultAppearance().SchemesList());

        _NotifyChanges(L"UnfocusedAppearance", L"HasUnfocusedAppearance", L"ShowUnfocusedAppearance");
    }

    void ProfileViewModel::DeleteUnfocusedAppearance()
    {
        _profile.DeleteUnfocusedAppearance();

        _unfocusedAppearanceViewModel = nullptr;

        _NotifyChanges(L"UnfocusedAppearance", L"HasUnfocusedAppearance", L"ShowUnfocusedAppearance");
    }

    Editor::AppearanceViewModel ProfileViewModel::UnfocusedAppearance()
    {
        return _unfocusedAppearanceViewModel;
    }

    bool ProfileViewModel::ShowMarksAvailable() const noexcept
    {
        return Feature_ScrollbarMarks::IsEnabled();
    }
    bool ProfileViewModel::AutoMarkPromptsAvailable() const noexcept
    {
        return Feature_ScrollbarMarks::IsEnabled();
    }
    bool ProfileViewModel::RepositionCursorWithMouseAvailable() const noexcept
    {
        return Feature_ScrollbarMarks::IsEnabled();
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

    winrt::hstring ProfileViewModel::LocalizedIcon() const
    {
        if (_currentIconType && unbox_value<IconType>(_currentIconType.as<Editor::EnumEntry>().EnumValue()) == IconType::None)
        {
            return RS_(L"Profile_IconTypeNone");
        }
        return Icon();
    }

    Windows::UI::Xaml::Controls::IconElement ProfileViewModel::IconPreview() const
    {
        // IconWUX sets the icon width/height to 32 by default
        auto icon = Microsoft::Terminal::UI::IconPathConverter::IconWUX(EvaluatedIcon());
        icon.Width(16);
        icon.Height(16);
        return icon;
    }

    void ProfileViewModel::CurrentIconType(const Windows::Foundation::IInspectable& value)
    {
        if (_currentIconType != value)
        {
            // Switching from...
            if (_currentIconType && unbox_value<IconType>(_currentIconType.as<Editor::EnumEntry>().EnumValue()) == IconType::Image)
            {
                // Stash the current value of Icon. If the user
                // switches out of then back to IconType::Image, we want
                // the path that we display in the text box to remain unchanged.
                _lastIconPath = Icon();
            }

            // Set the member here instead of after setting Icon() below!
            // We have an Icon property changed handler defined for when we discard changes.
            // Inadvertently, that means that we call this setter again.
            // Setting the member here means that we early exit at the beginning of the function
            //  because _currentIconType == value.
            _currentIconType = value;

            // Switched to...
            switch (unbox_value<IconType>(value.as<Editor::EnumEntry>().EnumValue()))
            {
            case IconType::None:
            {
                _profile.Icon(HideIconValue);
                break;
            }
            case IconType::Image:
            {
                if (!_lastIconPath.empty())
                {
                    // Conversely, if we switch to Image,
                    // retrieve that saved value and apply it
                    _profile.Icon(_lastIconPath);
                }
                break;
            }
            case IconType::FontIcon:
            {
                if (_CurrentBuiltInIcon)
                {
                    _profile.Icon(unbox_value<hstring>(_CurrentBuiltInIcon.as<Editor::EnumEntry>().EnumValue()));
                }
                break;
            }
            case IconType::Emoji:
            {
                // Don't set Icon here!
                // Clear out the text box so we direct the user to use the emoji picker.
                CurrentEmojiIcon({});
            }
            }
            // We're not using the VM's Icon() setter above,
            // so notify HasIcon changed manually
            _NotifyChanges(L"CurrentIconType", L"HasIcon");
        }
    }

    bool ProfileViewModel::UsingNoIcon() const
    {
        return _currentIconType == _IconTypes.GetAt(0);
    }

    bool ProfileViewModel::UsingBuiltInIcon() const
    {
        return _currentIconType == _IconTypes.GetAt(1);
    }

    bool ProfileViewModel::UsingEmojiIcon() const
    {
        return _currentIconType == _IconTypes.GetAt(2);
    }

    bool ProfileViewModel::UsingImageIcon() const
    {
        return _currentIconType == _IconTypes.GetAt(3);
    }

    bool ProfileViewModel::IsBellStyleFlagSet(const uint32_t flag)
    {
        return (WI_EnumValue(BellStyle()) & flag) == flag;
    }

    void ProfileViewModel::SetBellStyleAudible(winrt::Windows::Foundation::IReference<bool> on)
    {
        auto currentStyle = BellStyle();
        WI_UpdateFlag(currentStyle, Model::BellStyle::Audible, winrt::unbox_value<bool>(on));
        BellStyle(currentStyle);
    }

    void ProfileViewModel::SetBellStyleWindow(winrt::Windows::Foundation::IReference<bool> on)
    {
        auto currentStyle = BellStyle();
        WI_UpdateFlag(currentStyle, Model::BellStyle::Window, winrt::unbox_value<bool>(on));
        BellStyle(currentStyle);
    }

    void ProfileViewModel::SetBellStyleTaskbar(winrt::Windows::Foundation::IReference<bool> on)
    {
        auto currentStyle = BellStyle();
        WI_UpdateFlag(currentStyle, Model::BellStyle::Taskbar, winrt::unbox_value<bool>(on));
        BellStyle(currentStyle);
    }

    // Method Description:
    // - Construct _CurrentBellSounds by importing the _inherited_ value from the model
    // - Adds a PropertyChanged handler to each BellSoundViewModel to propagate changes to the model
    void ProfileViewModel::_InitializeCurrentBellSounds()
    {
        _CurrentBellSounds = winrt::single_threaded_observable_vector<Editor::BellSoundViewModel>();
        if (const auto soundList = _profile.BellSound())
        {
            for (const auto&& bellSound : soundList)
            {
                _CurrentBellSounds.Append(winrt::make<BellSoundViewModel>(bellSound));
            }
        }
        _MarkDuplicateBellSoundDirectories();
        _CheckBellSoundsExistence();
        _NotifyChanges(L"CurrentBellSounds");
    }

    // Method Description:
    // - If the current layer is inheriting the bell sound from its parent,
    //   we need to copy the _inherited_ bell sound list to the current layer
    //   so that we can then apply modifications to it
    void ProfileViewModel::_PrepareModelForBellSoundModification()
    {
        if (!_profile.HasBellSound())
        {
            std::vector<hstring> newSounds;
            if (const auto inheritedSounds = _profile.BellSound())
            {
                // copy inherited bell sounds to the current layer
                newSounds.reserve(inheritedSounds.Size());
                for (const auto sound : inheritedSounds)
                {
                    newSounds.push_back(sound);
                }
            }
            // if we didn't inherit any bell sounds,
            // we should still set the bell sound to an empty list (instead of null)
            _profile.BellSound(winrt::single_threaded_vector<hstring>(std::move(newSounds)));
        }
    }

    // Method Description:
    // - Check if any bell sounds share the same name.
    //   If they do, mark them so that they show the directory path in the UI
    void ProfileViewModel::_MarkDuplicateBellSoundDirectories()
    {
        for (uint32_t i = 0; i < _CurrentBellSounds.Size(); i++)
        {
            auto soundA = _CurrentBellSounds.GetAt(i);
            for (uint32_t j = i + 1; j < _CurrentBellSounds.Size(); j++)
            {
                auto soundB = _CurrentBellSounds.GetAt(j);
                if (soundA.DisplayPath() == soundB.DisplayPath())
                {
                    get_self<BellSoundViewModel>(soundA)->ShowDirectory(true);
                    get_self<BellSoundViewModel>(soundB)->ShowDirectory(true);
                }
            }
        }
    }

    // Method Description:
    // - Check if the bell sounds exist on disk. Mark any that don't exist
    //   so that they show the appropriate UI
    safe_void_coroutine ProfileViewModel::_CheckBellSoundsExistence()
    {
        co_await winrt::resume_background();
        std::vector<Editor::BellSoundViewModel> markedSounds;
        for (auto&& sound : _CurrentBellSounds)
        {
            if (!std::filesystem::exists(std::wstring_view{ sound.Path() }))
            {
                markedSounds.push_back(sound);
            }
        }

        co_await winrt::resume_foreground(_dispatcher);
        for (auto&& sound : markedSounds)
        {
            get_self<BellSoundViewModel>(sound)->FileExists(false);
        }
    }

    BellSoundViewModel::BellSoundViewModel(hstring path) :
        _Path{ path }
    {
        PropertyChanged([this](auto&&, const PropertyChangedEventArgs& args) {
            if (args.PropertyName() == L"FileExists")
            {
                _NotifyChanges(L"DisplayPath", L"SubText");
            }
        });
    }

    hstring BellSoundViewModel::DisplayPath() const
    {
        if (_FileExists)
        {
            // filename
            const std::filesystem::path filePath{ std::wstring_view{ _Path } };
            return hstring{ filePath.filename().wstring() };
        }
        return _Path;
    }

    hstring BellSoundViewModel::SubText() const
    {
        if (_FileExists)
        {
            // Directory
            const std::filesystem::path filePath{ std::wstring_view{ _Path } };
            return hstring{ filePath.parent_path().wstring() };
        }
        return RS_(L"Profile_BellSoundNotFound");
    }

    hstring ProfileViewModel::BellSoundPreview()
    {
        const auto& currentSound = BellSound();
        if (!currentSound || currentSound.Size() == 0)
        {
            return RS_(L"Profile_BellSoundPreviewDefault");
        }
        else if (currentSound.Size() == 1)
        {
            std::filesystem::path filePath{ std::wstring_view{ currentSound.GetAt(0) } };
            return hstring{ filePath.filename().wstring() };
        }
        return RS_(L"Profile_BellSoundPreviewMultiple");
    }

    void ProfileViewModel::RequestAddBellSound(hstring path)
    {
        // If we were inheriting our bell sound,
        // copy it over to the current layer and apply modifications
        _PrepareModelForBellSoundModification();

        // No need to check if the file exists. We came from the FilePicker. That's good enough.
        _CurrentBellSounds.Append(winrt::make<BellSoundViewModel>(path));
        _profile.BellSound().Append(path);
        _NotifyChanges(L"CurrentBellSounds");
    }

    void ProfileViewModel::RequestDeleteBellSound(const Editor::BellSoundViewModel& vm)
    {
        uint32_t index;
        if (_CurrentBellSounds.IndexOf(vm, index))
        {
            // If we were inheriting our bell sound,
            // copy it over to the current layer and apply modifications
            _PrepareModelForBellSoundModification();

            _CurrentBellSounds.RemoveAt(index);
            _profile.BellSound().RemoveAt(index);
            _NotifyChanges(L"CurrentBellSounds");
        }
    }

    void ProfileViewModel::DeleteProfile()
    {
        const auto deleteProfileArgs{ winrt::make_self<DeleteProfileEventArgs>(Guid()) };
        DeleteProfileRequested.raise(*this, *deleteProfileArgs);
    }

    void ProfileViewModel::SetupAppearances(Windows::Foundation::Collections::IObservableVector<Editor::ColorSchemeViewModel> schemesList)
    {
        DefaultAppearance().SchemesList(schemesList);
        if (UnfocusedAppearance())
        {
            UnfocusedAppearance().SchemesList(schemesList);
        }
    }
}
