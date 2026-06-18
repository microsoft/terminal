/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- Appearances

Abstract:
- The classes defined in this module are responsible for encapsulating the appearance settings
  of profiles and presenting them in the settings UI

Author(s):
- Pankaj Bhojwani - May 2021

--*/

#pragma once

#include "Font.g.h"
#include "FontKeyValuePair.g.h"
#include "Appearances.g.h"
#include "AppearanceViewModel.g.h"
#include "Utils.h"
#include "ViewModelHelpers.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct AppearanceViewModel;

    struct Font : FontT<Font>
    {
        Font(winrt::hstring name, winrt::hstring localizedName);

        WINRT_PROPERTY(hstring, Name);
        WINRT_PROPERTY(hstring, LocalizedName);
    };

    struct FontKeyValuePair : FontKeyValuePairT<FontKeyValuePair>
    {
        static bool SortAscending(const Editor::FontKeyValuePair& lhs, const Editor::FontKeyValuePair& rhs);

        FontKeyValuePair(winrt::weak_ref<AppearanceViewModel> vm, winrt::hstring keyDisplayString, uint32_t key, float value, bool isFontFeature);

        winrt::hstring KeyDisplayString();
        const winrt::hstring& KeyDisplayStringRef();
        uint32_t Key() const noexcept;
        float Value() const noexcept;
        void Value(float v);
        winrt::hstring AutomationName();

        void SetValueDirect(float v);
        bool IsFontFeature() const noexcept;

    private:
        winrt::weak_ref<AppearanceViewModel> _vm;
        winrt::hstring _keyDisplayString;
        uint32_t _key;
        float _value;
        bool _isFontFeature;
    };

    struct AppearanceViewModel : AppearanceViewModelT<AppearanceViewModel>, ViewModelHelper<AppearanceViewModel>
    {
        enum FontSettingIndex
        {
            FontAxesIndex,
            FontFeaturesIndex,
        };

        struct FontFaceDependentsData
        {
            winrt::hstring missingFontFaces;
            winrt::hstring proportionalFontFaces;
            bool hasPowerlineCharacters = false;

            std::array<Windows::Foundation::Collections::IObservableVector<Editor::FontKeyValuePair>, 2> fontSettingsUsed;
            std::array<std::vector<Windows::UI::Xaml::Controls::MenuFlyoutItemBase>, 2> fontSettingsUnused;
        };

        AppearanceViewModel(const Model::AppearanceConfig& appearance);

        // IInheritableViewModel
        bool HasSetting(const hstring& name);
        void ClearSetting(const hstring& name);
        Windows::Foundation::IInspectable SettingOverrideSource(const hstring& name);

        winrt::hstring FontFace() const;
        void FontFace(const winrt::hstring& value);
        bool HasFontFace() const;
        void ClearFontFace();
        Model::FontConfig FontFaceOverrideSource() const;

        double LineHeight() const;
        void LineHeight(const double value);
        bool HasLineHeight() const;
        void ClearLineHeight();
        Model::FontConfig LineHeightOverrideSource() const;

        double CellWidth() const;
        void CellWidth(const double value);
        bool HasCellWidth() const;
        void ClearCellWidth();
        Model::FontConfig CellWidthOverrideSource() const;

        void SetFontWeightFromDouble(double fontWeight);

        const FontFaceDependentsData& FontFaceDependents();
        winrt::hstring MissingFontFaces();
        winrt::hstring ProportionalFontFaces();
        bool HasPowerlineCharacters();

        Windows::Foundation::Collections::IObservableVector<Editor::FontKeyValuePair> FontAxes();
        bool HasFontAxes() const;
        void ClearFontAxes();
        Model::FontConfig FontAxesOverrideSource() const;

        Windows::Foundation::Collections::IObservableVector<Editor::FontKeyValuePair> FontFeatures();
        bool HasFontFeatures() const;
        void ClearFontFeatures();
        Model::FontConfig FontFeaturesOverrideSource() const;

        void AddFontKeyValuePair(const IInspectable& sender, const Editor::FontKeyValuePair& kv);
        void DeleteFontKeyValuePair(const Editor::FontKeyValuePair& kv);
        void UpdateFontSetting(const FontKeyValuePair* kv);

        // background image
        hstring CurrentBackgroundImagePath() const;
        bool UseDesktopBGImage() const;
        void UseDesktopBGImage(const bool useDesktop);
        bool BackgroundImageSettingsEnabled() const;
        void SetBackgroundImageOpacityFromPercentageValue(double percentageValue);
        void SetBackgroundImagePath(winrt::hstring path);
        hstring BackgroundImageAlignmentCurrentValue() const;

        void ClearColorScheme();
        Editor::ColorSchemeViewModel CurrentColorScheme() const;
        void CurrentColorScheme(const Editor::ColorSchemeViewModel& val);

        Windows::UI::Color ForegroundPreview() const;
        Windows::UI::Color BackgroundPreview() const;
        Windows::UI::Color SelectionBackgroundPreview() const;
        Windows::UI::Color CursorColorPreview() const;

        WINRT_PROPERTY(bool, IsDefault, false);

// Inheritable appearance settings that expose a reset button.
// Each entry is wired up automatically in two places:
//   * the projected property accessors generated immediately below, and
//   * the HasSetting/ClearSetting/SettingOverrideSource dispatch in Appearances.cpp.
// To add a new inheritable appearance setting, add exactly ONE line here:
//   * PROJECTED(target, Name) - a standard setting backed by OBSERVABLE_PROJECTED_SETTING.
//   * CUSTOM(Name)            - a setting whose Name()/Has##Name()/Clear##Name()/
//                               Name##OverrideSource() accessors are hand-written above
//                               (e.g. FontFace, FontAxes). It still participates in
//                               dispatch, but no property is generated for it here.
// "ColorScheme" is special-cased in the dispatch (it is backed by the Dark/Light
// scheme names) and so is intentionally not listed here.
#define APPEARANCE_INHERITABLE_SETTINGS(PROJECTED, CUSTOM)          \
    CUSTOM(FontFace)                                                \
    PROJECTED(_appearance.SourceProfile().FontInfo(), FontSize)     \
    CUSTOM(LineHeight)                                              \
    CUSTOM(CellWidth)                                               \
    PROJECTED(_appearance.SourceProfile().FontInfo(), FontWeight)   \
    PROJECTED(_appearance.SourceProfile().FontInfo(), EnableBuiltinGlyphs) \
    PROJECTED(_appearance.SourceProfile().FontInfo(), EnableColorGlyphs)   \
    CUSTOM(FontAxes)                                                \
    CUSTOM(FontFeatures)                                            \
    PROJECTED(_appearance, RetroTerminalEffect)                    \
    PROJECTED(_appearance, CursorShape)                            \
    PROJECTED(_appearance, CursorHeight)                           \
    PROJECTED(_appearance, DarkColorSchemeName)                    \
    PROJECTED(_appearance, LightColorSchemeName)                   \
    PROJECTED(_appearance, BackgroundImagePath)                    \
    PROJECTED(_appearance, BackgroundImageOpacity)                 \
    PROJECTED(_appearance, BackgroundImageStretchMode)             \
    PROJECTED(_appearance, BackgroundImageAlignment)               \
    PROJECTED(_appearance, IntenseTextStyle)                       \
    PROJECTED(_appearance, AdjustIndistinguishableColors)          \
    PROJECTED(_appearance, Foreground)                             \
    PROJECTED(_appearance, Background)                             \
    PROJECTED(_appearance, SelectionBackground)                    \
    PROJECTED(_appearance, CursorColor)

#define APPEARANCE_GEN_PROJECTED(target, name) OBSERVABLE_PROJECTED_SETTING(target, name);
#define APPEARANCE_GEN_CUSTOM(name)
        APPEARANCE_INHERITABLE_SETTINGS(APPEARANCE_GEN_PROJECTED, APPEARANCE_GEN_CUSTOM)
#undef APPEARANCE_GEN_PROJECTED
#undef APPEARANCE_GEN_CUSTOM

        WINRT_OBSERVABLE_PROPERTY(Windows::Foundation::Collections::IObservableVector<Editor::ColorSchemeViewModel>, SchemesList, _propertyChangedHandlers, nullptr);

    private:
        void _invalidateFontFaceDependents() { _fontFaceDependents.reset(); }
        void _refreshFontFaceDependents();
        static std::pair<std::vector<Editor::FontKeyValuePair>::const_iterator, bool> _fontSettingSortedByKeyInsertPosition(const std::vector<Editor::FontKeyValuePair>& vec, uint32_t key);
        void _generateFontAxes(IDWriteFontFace* fontFace, const wchar_t* localeName, std::vector<Editor::FontKeyValuePair>& list);
        void _generateFontFeatures(IDWriteFontFace* fontFace, std::vector<Editor::FontKeyValuePair>& list);
        Windows::UI::Xaml::Controls::MenuFlyoutItemBase _createFontSettingMenuItem(const Editor::FontKeyValuePair& kv);
        void _notifyChangesForFontSettings();
        void _notifyChangesForFontSettingsReactive(FontSettingIndex fontSettingsIndex);
        void _deleteAllFontKeyValuePairs(FontSettingIndex index);
        void _addMenuFlyoutItemToUnused(FontSettingIndex index, Windows::UI::Xaml::Controls::MenuFlyoutItemBase item);

        double _parseCellSizeValue(const hstring& val) const;

        Model::AppearanceConfig _appearance;
        winrt::hstring _lastBgImagePath;
        std::optional<FontFaceDependentsData> _fontFaceDependents;
    };

    struct Appearances : AppearancesT<Appearances>
    {
        Appearances();
        void BringIntoViewWhenLoaded(hstring elementToFocus);

        // CursorShape visibility logic
        bool IsVintageCursor() const;

        Editor::IHostedInWindow WindowRoot() const noexcept { return _WeakWindowRoot.get(); }
        void WindowRoot(const Editor::IHostedInWindow& value) noexcept { _WeakWindowRoot = value; }

        Windows::Foundation::Collections::IObservableVector<Editor::Font> FilteredFontList();
        bool ShowAllFonts() const noexcept;
        void ShowAllFonts(bool value);

        void FontFaceBox_GotFocus(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& e);
        void FontFaceBox_LostFocus(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& e);
        void FontFaceBox_QuerySubmitted(const winrt::Windows::UI::Xaml::Controls::AutoSuggestBox&, const winrt::Windows::UI::Xaml::Controls::AutoSuggestBoxQuerySubmittedEventArgs&);
        void FontFaceBox_TextChanged(const winrt::Windows::UI::Xaml::Controls::AutoSuggestBox&, const winrt::Windows::UI::Xaml::Controls::AutoSuggestBoxTextChangedEventArgs&);
        void DeleteFontKeyValuePair_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& e);
        safe_void_coroutine BackgroundImage_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& e);
        void BIAlignment_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& e);

        // manually bind FontWeight
        Windows::Foundation::IInspectable CurrentFontWeight() const;
        void CurrentFontWeight(const Windows::Foundation::IInspectable& enumEntry);
        bool IsCustomFontWeight();

        til::property_changed_event PropertyChanged;
        winrt::Windows::UI::Xaml::FrameworkElement::Loaded_revoker _loadedRevoker;

        WINRT_PROPERTY(Windows::Foundation::Collections::IObservableVector<Microsoft::Terminal::Settings::Editor::EnumEntry>, FontWeightList);

        GETSET_BINDABLE_ENUM_SETTING(CursorShape, Microsoft::Terminal::Core::CursorStyle, Appearance().CursorShape);
        GETSET_BINDABLE_ENUM_SETTING(AdjustIndistinguishableColors, Microsoft::Terminal::Core::AdjustTextMode, Appearance().AdjustIndistinguishableColors);

        DEPENDENCY_PROPERTY(Editor::AppearanceViewModel, Appearance);
        WINRT_PROPERTY(Editor::ProfileViewModel, SourceProfile, nullptr);
        GETSET_BINDABLE_ENUM_SETTING(BackgroundImageStretchMode, Windows::UI::Xaml::Media::Stretch, Appearance().BackgroundImageStretchMode);

        GETSET_BINDABLE_ENUM_SETTING(IntenseTextStyle, Microsoft::Terminal::Settings::Model::IntenseStyle, Appearance().IntenseTextStyle);

    private:
        winrt::weak_ref<Editor::IHostedInWindow> _WeakWindowRoot{ nullptr };
        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _ViewModelChangedRevoker;
        std::array<Windows::UI::Xaml::Controls::Primitives::ToggleButton, 9> _BIAlignmentButtons;
        Windows::Foundation::Collections::IMap<uint16_t, Microsoft::Terminal::Settings::Editor::EnumEntry> _FontWeightMap;
        Editor::EnumEntry _CustomFontWeight{ nullptr };
        Windows::Foundation::Collections::IObservableVector<Editor::Font> _filteredFonts;
        Windows::Foundation::Collections::IObservableVector<winrt::hstring> _FontAxesNames;
        Windows::Foundation::Collections::IObservableVector<winrt::hstring> _FontFeaturesNames;
        std::wstring _fontNameFilter;
        bool _ShowAllFonts = false;

        static void _ViewModelChanged(const Windows::UI::Xaml::DependencyObject& d, const Windows::UI::Xaml::DependencyPropertyChangedEventArgs& e);

        void _updateFontName(winrt::hstring fontSpec);
        void _updateFontNameFilter(std::wstring_view filter);
        void _updateFilteredFontList();
        void _UpdateBIAlignmentControl(const int32_t val);
        void _UpdateWithNewViewModel();
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Appearances);
}
