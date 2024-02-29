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

    static constexpr std::wstring_view HideIconValue{ L"none" };

    ProfileViewModel::ProfileViewModel(const Model::Profile& profile, const Model::CascadiaSettings& appSettings) :
        _profile{ profile },
        _defaultAppearanceViewModel{ winrt::make<implementation::AppearanceViewModel>(profile.DefaultAppearance().try_as<AppearanceConfig>()) },
        _originalProfileGuid{ profile.Guid() },
        _appSettings{ appSettings },
        _unfocusedAppearanceViewModel{ nullptr }
    {
        INITIALIZE_BINDABLE_ENUM_SETTING(AntiAliasingMode, TextAntialiasingMode, winrt::Microsoft::Terminal::Control::TextAntialiasingMode, L"Profile_AntialiasingMode", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING_REVERSE_ORDER(CloseOnExitMode, CloseOnExitMode, winrt::Microsoft::Terminal::Settings::Model::CloseOnExitMode, L"Profile_CloseOnExit", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(ScrollState, ScrollbarState, winrt::Microsoft::Terminal::Control::ScrollbarState, L"Profile_ScrollbarVisibility", L"Content");

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
                _NotifyChanges(L"HideIcon");
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

        _defaultAppearanceViewModel.IsDefault(true);
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

    Editor::Font ProfileViewModel::FindFontWithLocalizedName(const winrt::hstring& name) noexcept
    {
        // look for the current font in our shown list of fonts
        Editor::Font fallbackFont{ nullptr };
        try
        {
            if (!CompleteFontList())
            {
                UpdateFontList();
            }
            const auto& currentFontList{ CompleteFontList() };
            for (const auto& font : currentFontList)
            {
                if (font.LocalizedName() == name)
                {
                    return font;
                }
                else if (font.LocalizedName() == L"Cascadia Mono")
                {
                    fallbackFont = font;
                }
            }
        }
        CATCH_LOG();

        // we couldn't find the desired font, set to "Cascadia Mono" if we found that since it ships by default
        return fallbackFont;
    }

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
        const auto ci = getLocalizedStringIndex(familyNames.get(), L"en-us", 0);
        // If our locale is missing we fall back to en-us.
        const auto li = getLocalizedStringIndex(familyNames.get(), locale, ci);

        auto canonical = getLocalizedStringByIndex(familyNames.get(), ci);
        // If the canonical/localized indices are the same, there's no need to get the other string.
        auto localized = ci == li ? canonical : getLocalizedStringByIndex(familyNames.get(), li);

        return make<Font>(std::move(canonical), std::move(localized), family);
    }

    winrt::guid ProfileViewModel::OriginalProfileGuid() const noexcept
    {
        return _originalProfileGuid;
    }

    bool ProfileViewModel::CanDeleteProfile() const
    {
        return !IsBaseLayer();
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

    bool ProfileViewModel::VtPassthroughAvailable() const noexcept
    {
        return Feature_VtPassthroughMode::IsEnabled() && Feature_VtPassthroughModeSettingInUI::IsEnabled();
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

    bool ProfileViewModel::HideIcon()
    {
        return Icon() == HideIconValue;
    }
    void ProfileViewModel::HideIcon(const bool hide)
    {
        if (hide)
        {
            // Stash the current value of Icon. If the user
            // checks and un-checks the "Hide Icon" checkbox, we want
            // the path that we display in the text box to remain unchanged.
            _lastIcon = Icon();
            Icon(HideIconValue);
        }
        else
        {
            Icon(_lastIcon);
        }
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

    void ProfileViewModel::DeleteProfile()
    {
        auto deleteProfileArgs{ winrt::make_self<DeleteProfileEventArgs>(Guid()) };
        _DeleteProfileHandlers(*this, *deleteProfileArgs);
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
