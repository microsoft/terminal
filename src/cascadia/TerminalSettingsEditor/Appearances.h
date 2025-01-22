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
#include "SettingContainer.h"
#include <LibraryResources.h>

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
        bool UseDesktopBGImage();
        void UseDesktopBGImage(const bool useDesktop);
        bool BackgroundImageSettingsVisible();
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

        // These settings are not defined in AppearanceConfig, so we grab them
        // from the source profile itself. The reason we still want them in the
        // AppearanceViewModel is so we can continue to have the 'Text' grouping
        // we currently have in xaml, since that grouping has some settings that
        // are defined in AppearanceConfig and some that are not.
        OBSERVABLE_PROJECTED_SETTING(_appearance.SourceProfile().FontInfo(), FontSize);
        OBSERVABLE_PROJECTED_SETTING(_appearance.SourceProfile().FontInfo(), FontWeight);
        OBSERVABLE_PROJECTED_SETTING(_appearance.SourceProfile().FontInfo(), EnableBuiltinGlyphs);
        OBSERVABLE_PROJECTED_SETTING(_appearance.SourceProfile().FontInfo(), EnableColorGlyphs);

        OBSERVABLE_PROJECTED_SETTING(_appearance, RetroTerminalEffect);
        OBSERVABLE_PROJECTED_SETTING(_appearance, CursorShape);
        OBSERVABLE_PROJECTED_SETTING(_appearance, CursorHeight);
        OBSERVABLE_PROJECTED_SETTING(_appearance, DarkColorSchemeName);
        OBSERVABLE_PROJECTED_SETTING(_appearance, LightColorSchemeName);
        OBSERVABLE_PROJECTED_SETTING(_appearance, BackgroundImagePath);
        OBSERVABLE_PROJECTED_SETTING(_appearance, BackgroundImageOpacity);
        OBSERVABLE_PROJECTED_SETTING(_appearance, BackgroundImageStretchMode);
        OBSERVABLE_PROJECTED_SETTING(_appearance, BackgroundImageAlignment);
        OBSERVABLE_PROJECTED_SETTING(_appearance, IntenseTextStyle);
        OBSERVABLE_PROJECTED_SETTING(_appearance, AdjustIndistinguishableColors);
        OBSERVABLE_PROJECTED_SETTING(_appearance, Foreground);
        OBSERVABLE_PROJECTED_SETTING(_appearance, Background);
        OBSERVABLE_PROJECTED_SETTING(_appearance, SelectionBackground);
        OBSERVABLE_PROJECTED_SETTING(_appearance, CursorColor);
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

        // CursorShape visibility logic
        bool IsVintageCursor() const;

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

        WINRT_PROPERTY(Windows::Foundation::Collections::IObservableVector<Microsoft::Terminal::Settings::Editor::EnumEntry>, FontWeightList);

        GETSET_BINDABLE_ENUM_SETTING(CursorShape, Microsoft::Terminal::Core::CursorStyle, Appearance().CursorShape);
        GETSET_BINDABLE_ENUM_SETTING(AdjustIndistinguishableColors, Microsoft::Terminal::Core::AdjustTextMode, Appearance().AdjustIndistinguishableColors);

        DEPENDENCY_PROPERTY(Editor::AppearanceViewModel, Appearance);
        WINRT_PROPERTY(Editor::ProfileViewModel, SourceProfile, nullptr);
        WINRT_PROPERTY(IHostedInWindow, WindowRoot, nullptr);
        GETSET_BINDABLE_ENUM_SETTING(BackgroundImageStretchMode, Windows::UI::Xaml::Media::Stretch, Appearance().BackgroundImageStretchMode);

        GETSET_BINDABLE_ENUM_SETTING(IntenseTextStyle, Microsoft::Terminal::Settings::Model::IntenseStyle, Appearance().IntenseTextStyle);

    private:
        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _ViewModelChangedRevoker;
        std::array<Windows::UI::Xaml::Controls::Primitives::ToggleButton, 9> _BIAlignmentButtons;
        Windows::Foundation::Collections::IMap<uint16_t, Microsoft::Terminal::Settings::Editor::EnumEntry> _FontWeightMap;
        Editor::EnumEntry _CustomFontWeight{ nullptr };
        Windows::Foundation::Collections::IObservableVector<Editor::Font> _filteredFonts;
        Windows::Foundation::Collections::IObservableVector<winrt::hstring> _FontAxesNames;
        Windows::Foundation::Collections::IObservableVector<winrt::hstring> _FontFeaturesNames;
        std::wstring _fontNameFilter;
        bool _ShowAllFonts = false;
        bool _suppressFontFaceBoxList = false;

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
