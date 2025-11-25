// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ProfileViewModel.h"
#include "ProfileViewModel.g.cpp"
#include "Appearances.h"
#include "EnumEntry.h"

#include <LibraryResources.h>
#include "../../renderer/base/FontCache.h"
#include "../TerminalSettingsAppAdapterLib/TerminalSettings.h"

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
                _NotifyChanges(L"UseParentProcessDirectory", L"CurrentStartingDirectoryPreview");
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
                _NotifyChanges(L"IsBellStyleFlagSet", L"BellStylePreview");
            }
            else if (viewModelProperty == L"ScrollState")
            {
                _NotifyChanges(L"CurrentScrollState");
            }
            else if (viewModelProperty == L"Icon")
            {
                // The icon changed; let's re-evaluate it with its new context.
                _appSettings.ResolveMediaResources();

                // Propagate the rendered icon into a preview (i.e. nav view, container item)
                _NotifyChanges(L"LocalizedIcon",
                               L"IconPreview",
                               L"IconPath",
                               L"EvaluatedIcon",
                               L"UsingNoIcon");
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
            else if (viewModelProperty == L"TabTitle")
            {
                _NotifyChanges(L"TabTitlePreview");
            }
            else if (viewModelProperty == L"AnswerbackMessage")
            {
                _NotifyChanges(L"AnswerbackMessagePreview");
            }
            else if (viewModelProperty == L"TabColor")
            {
                _NotifyChanges(L"TabColorPreview");
            }
            else if (viewModelProperty == L"TabThemeColorPreview")
            {
                _NotifyChanges(L"TabColorPreview");
            }
        });

        _defaultAppearanceViewModel.PropertyChanged([this](auto&&, const PropertyChangedEventArgs& args) {
            const auto viewModelProperty{ args.PropertyName() };
            if (viewModelProperty == L"DarkColorSchemeName" || viewModelProperty == L"LightColorSchemeName")
            {
                _NotifyChanges(L"TabThemeColorPreview");
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
    Control::IControlSettings ProfileViewModel::TermSettings() const
    {
        // This may look pricey, but it only resolves resources that have not been visited
        // and the preview update is debounced.
        _appSettings.ResolveMediaResources();
        return *Settings::TerminalSettings::CreateForPreview(_appSettings, _profile);
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

    hstring ProfileViewModel::AnswerbackMessagePreview() const
    {
        if (const auto answerbackMessage{ AnswerbackMessage() }; !answerbackMessage.empty())
        {
            return answerbackMessage;
        }
        return RS_(L"Profile_AnswerbackMessageNone");
    }

    Windows::UI::Color ProfileViewModel::TabColorPreview() const
    {
        if (const auto modelVal = _profile.TabColor())
        {
            const auto color = modelVal.Value();
            // user defined an override value
            return Windows::UI::Color{
                .A = 255,
                .R = color.R,
                .G = color.G,
                .B = color.B
            };
        }
        // set to null --> deduce value from theme
        return TabThemeColorPreview();
    }

    Windows::UI::Color ProfileViewModel::TabThemeColorPreview() const
    {
        const auto currentTheme = _appSettings.GlobalSettings().CurrentTheme();
        if (const auto tabTheme = currentTheme.Tab())
        {
            // theme.tab.background: theme color must be evaluated
            if (const auto tabBackground = tabTheme.Background())
            {
                const auto& tabBrush = tabBackground.Evaluate(Application::Current().Resources(),
                                                              Windows::UI::Xaml::Media::SolidColorBrush{ DefaultAppearance().CurrentColorScheme().BackgroundColor().Color() },
                                                              false);
                if (const auto& tabColorBrush = tabBrush.try_as<Windows::UI::Xaml::Media::SolidColorBrush>())
                {
                    const auto brushColor = tabColorBrush.Color();
                    return brushColor;
                }
            }
        }
        else if (const auto windowTheme = currentTheme.Window())
        {
            // theme.window.applicationTheme: evaluate light/dark to XAML default tab color
            // Can also be "Default", in which case we fall through below
            const auto appTheme = windowTheme.RequestedTheme();
            if (appTheme == ElementTheme::Dark)
            {
                return Windows::UI::Color{
                    .A = 0xFF,
                    .R = 0x28,
                    .G = 0x28,
                    .B = 0x28
                };
            }
            else if (appTheme == ElementTheme::Light)
            {
                return Windows::UI::Color{
                    .A = 0xFF,
                    .R = 0xF9,
                    .G = 0xF9,
                    .B = 0xF9
                };
            }
        }

        // XAML default tab color
        if (Model::Theme::IsSystemInDarkTheme())
        {
            return Windows::UI::Color{
                .A = 0xFF,
                .R = 0x28,
                .G = 0x28,
                .B = 0x28
            };
        }
        return Windows::UI::Color{
            .A = 0xFF,
            .R = 0xF9,
            .G = 0xF9,
            .B = 0xF9
        };
    }

    Editor::AppearanceViewModel ProfileViewModel::DefaultAppearance() const
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

    Editor::AppearanceViewModel ProfileViewModel::UnfocusedAppearance() const
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

    hstring ProfileViewModel::CurrentStartingDirectoryPreview() const
    {
        if (UseParentProcessDirectory())
        {
            return RS_(L"Profile_StartingDirectoryUseParentCheckbox/Content");
        }
        return StartingDirectory();
    }

    bool ProfileViewModel::UseParentProcessDirectory() const
    {
        return StartingDirectory().empty();
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
        const auto iconPath = IconPath();
        if (iconPath.empty())
        {
            return RS_(L"Profile_IconTypeNone");
        }
        return iconPath; // For display as a string
    }

    Windows::UI::Xaml::Controls::IconElement ProfileViewModel::IconPreview() const
    {
        // IconWUX sets the icon width/height to 32 by default
        auto icon = Microsoft::Terminal::UI::IconPathConverter::IconWUX(EvaluatedIcon());
        icon.Width(16);
        icon.Height(16);
        return icon;
    }

    bool ProfileViewModel::UsingNoIcon() const noexcept
    {
        static constexpr std::wstring_view HideIconValue{ L"none" };
        return IconPath() == HideIconValue;
    }

    hstring ProfileViewModel::BellStylePreview() const
    {
        const auto bellStyle = BellStyle();
        if (WI_AreAllFlagsSet(bellStyle, BellStyle::Audible | BellStyle::Window | BellStyle::Taskbar))
        {
            return RS_(L"Profile_BellStyleAll/Content");
        }
        else if (bellStyle == static_cast<Model::BellStyle>(0))
        {
            return RS_(L"Profile_BellStyleNone/Content");
        }

        std::vector<hstring> resultList;
        resultList.reserve(3);
        if (WI_IsFlagSet(bellStyle, BellStyle::Audible))
        {
            resultList.emplace_back(RS_(L"Profile_BellStyleAudible/Content"));
        }
        if (WI_IsFlagSet(bellStyle, BellStyle::Window))
        {
            resultList.emplace_back(RS_(L"Profile_BellStyleWindow/Content"));
        }
        if (WI_IsFlagSet(bellStyle, BellStyle::Taskbar))
        {
            resultList.emplace_back(RS_(L"Profile_BellStyleTaskbar/Content"));
        }

        // add in the commas
        hstring result{};
        for (auto&& entry : resultList)
        {
            if (result.empty())
            {
                result = entry;
            }
            else
            {
                result = result + L", " + entry;
            }
        }
        return result;
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
            std::vector<IMediaResource> newSounds;
            if (const auto inheritedSounds = _profile.BellSound())
            {
                newSounds = wil::to_vector(inheritedSounds);
            }
            // if we didn't inherit any bell sounds,
            // we should still set the bell sound to an empty list (instead of null)
            _profile.BellSound(winrt::single_threaded_vector(std::move(newSounds)));
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

    BellSoundViewModel::BellSoundViewModel(const Model::IMediaResource& resource) :
        _resource{ resource }
    {
        if (_resource.Ok() && _resource.Path() != _resource.Resolved())
        {
            // If the resource was resolved to something other than its path, show the path!
            _ShowDirectory = true;
        }
    }

    hstring BellSoundViewModel::DisplayPath() const
    {
        if (_resource.Ok())
        {
            // filename; start from the resolved path to show where it actually landed
            auto resolvedPath{ _resource.Resolved() };
            const std::filesystem::path filePath{ std::wstring_view{ resolvedPath } };
            return hstring{ filePath.filename().wstring() };
        }
        return _resource.Path();
    }

    hstring BellSoundViewModel::SubText() const
    {
        if (_resource.Ok())
        {
            // Directory
            auto resolvedPath{ _resource.Resolved() };
            const std::filesystem::path filePath{ std::wstring_view{ resolvedPath } };
            return hstring{ filePath.parent_path().wstring() };
        }
        return RS_(L"Profile_BellSoundNotFound");
    }

    hstring ProfileViewModel::BellSoundPreview()
    {
        if (!_CurrentBellSounds || _CurrentBellSounds.Size() == 0)
        {
            return RS_(L"Profile_BellSoundPreviewDefault");
        }
        if (_CurrentBellSounds.Size() > 1)
        {
            return RS_(L"Profile_BellSoundPreviewMultiple");
        }

        const auto currentBellSound = _CurrentBellSounds.GetAt(0);
        if (currentBellSound.FileExists())
        {
            return currentBellSound.DisplayPath();
        }

        return RS_(L"Profile_BellSoundNotFound");
    }

    void ProfileViewModel::RequestAddBellSound(hstring path)
    {
        // If we were inheriting our bell sound,
        // copy it over to the current layer and apply modifications
        _PrepareModelForBellSoundModification();

        auto bellResource{ MediaResourceHelper::FromString(path) };
        bellResource.Resolve(path); // No need to check if the file exists. We came from the FilePicker. That's good enough.
        _CurrentBellSounds.Append(winrt::make<BellSoundViewModel>(bellResource));
        _profile.BellSound().Append(bellResource);
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
