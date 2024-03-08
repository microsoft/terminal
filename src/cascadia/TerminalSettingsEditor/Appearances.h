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
#include "AxisKeyValuePair.g.h"
#include "FeatureKeyValuePair.g.h"
#include "Appearances.g.h"
#include "AppearanceViewModel.g.h"
#include "Utils.h"
#include "ViewModelHelpers.h"
#include "SettingContainer.h"
#include <LibraryResources.h>

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct Font : FontT<Font>
    {
    public:
        Font(winrt::hstring name, winrt::hstring localizedName, IDWriteFontFamily* family) :
            _Name{ std::move(name) },
            _LocalizedName{ std::move(localizedName) }
        {
            _family.copy_from(family);
        }

        hstring ToString() { return _LocalizedName; }
        bool HasPowerlineCharacters();
        Windows::Foundation::Collections::IMap<winrt::hstring, winrt::hstring> FontAxesTagsAndNames();
        Windows::Foundation::Collections::IMap<winrt::hstring, winrt::hstring> FontFeaturesTagsAndNames();

        WINRT_PROPERTY(hstring, Name);
        WINRT_PROPERTY(hstring, LocalizedName);

    private:
        winrt::com_ptr<IDWriteFontFamily> _family;
        std::optional<bool> _hasPowerlineCharacters;

        winrt::hstring _tagToString(DWRITE_FONT_AXIS_TAG tag);
        winrt::hstring _tagToString(DWRITE_FONT_FEATURE_TAG tag);

        Windows::Foundation::Collections::IMap<winrt::hstring, winrt::hstring> _fontAxesTagsAndNames;
        Windows::Foundation::Collections::IMap<winrt::hstring, winrt::hstring> _fontFeaturesTagsAndNames;
    };

    struct AxisKeyValuePair : AxisKeyValuePairT<AxisKeyValuePair>, ViewModelHelper<AxisKeyValuePair>
    {
        AxisKeyValuePair(winrt::hstring axisKey, float axisValue, const Windows::Foundation::Collections::IMap<winrt::hstring, float>& baseMap, const Windows::Foundation::Collections::IMap<winrt::hstring, winrt::hstring>& tagToNameMap);

        winrt::hstring AxisKey();
        void AxisKey(winrt::hstring axisKey);

        float AxisValue();
        void AxisValue(float axisValue);

        int32_t AxisIndex();
        void AxisIndex(int32_t axisIndex);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);

    private:
        winrt::hstring _AxisKey;
        float _AxisValue;
        int32_t _AxisIndex;
        Windows::Foundation::Collections::IMap<winrt::hstring, float> _baseMap{ nullptr };
        Windows::Foundation::Collections::IMap<winrt::hstring, winrt::hstring> _tagToNameMap{ nullptr };
    };

    struct FeatureKeyValuePair : FeatureKeyValuePairT<FeatureKeyValuePair>, ViewModelHelper<FeatureKeyValuePair>
    {
        FeatureKeyValuePair(winrt::hstring featureKey, uint32_t featureValue, const Windows::Foundation::Collections::IMap<winrt::hstring, uint32_t>& baseMap, const Windows::Foundation::Collections::IMap<winrt::hstring, winrt::hstring>& tagToNameMap);

        winrt::hstring FeatureKey();
        void FeatureKey(winrt::hstring featureKey);

        uint32_t FeatureValue();
        void FeatureValue(uint32_t featureValue);

        int32_t FeatureIndex();
        void FeatureIndex(int32_t featureIndex);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);

    private:
        winrt::hstring _FeatureKey;
        uint32_t _FeatureValue;
        int32_t _FeatureIndex;
        Windows::Foundation::Collections::IMap<winrt::hstring, uint32_t> _baseMap{ nullptr };
        Windows::Foundation::Collections::IMap<winrt::hstring, winrt::hstring> _tagToNameMap{ nullptr };
    };

    struct AppearanceViewModel : AppearanceViewModelT<AppearanceViewModel>, ViewModelHelper<AppearanceViewModel>
    {
    public:
        AppearanceViewModel(const Model::AppearanceConfig& appearance);

        double LineHeight() const noexcept;
        void LineHeight(const double value);
        bool HasLineHeight() const;
        void ClearLineHeight();
        Model::FontConfig LineHeightOverrideSource() const;
        void SetFontWeightFromDouble(double fontWeight);
        void SetBackgroundImageOpacityFromPercentageValue(double percentageValue);
        void SetBackgroundImagePath(winrt::hstring path);

        // background image
        bool UseDesktopBGImage();
        void UseDesktopBGImage(const bool useDesktop);
        bool BackgroundImageSettingsVisible();

        void ClearColorScheme();
        Editor::ColorSchemeViewModel CurrentColorScheme();
        void CurrentColorScheme(const Editor::ColorSchemeViewModel& val);

        void AddNewAxisKeyValuePair();
        void DeleteAxisKeyValuePair(winrt::hstring key);
        void InitializeFontAxesVector();
        bool AreFontAxesAvailable();
        bool CanFontAxesBeAdded();

        void AddNewFeatureKeyValuePair();
        void DeleteFeatureKeyValuePair(winrt::hstring key);
        void InitializeFontFeaturesVector();
        bool AreFontFeaturesAvailable();
        bool CanFontFeaturesBeAdded();

        WINRT_PROPERTY(bool, IsDefault, false);

        // These settings are not defined in AppearanceConfig, so we grab them
        // from the source profile itself. The reason we still want them in the
        // AppearanceViewModel is so we can continue to have the 'Text' grouping
        // we currently have in xaml, since that grouping has some settings that
        // are defined in AppearanceConfig and some that are not.
        OBSERVABLE_PROJECTED_SETTING(_appearance.SourceProfile().FontInfo(), FontFace);
        OBSERVABLE_PROJECTED_SETTING(_appearance.SourceProfile().FontInfo(), FontSize);
        OBSERVABLE_PROJECTED_SETTING(_appearance.SourceProfile().FontInfo(), FontWeight);
        OBSERVABLE_PROJECTED_SETTING(_appearance.SourceProfile().FontInfo(), FontAxes);
        OBSERVABLE_PROJECTED_SETTING(_appearance.SourceProfile().FontInfo(), FontFeatures);
        OBSERVABLE_PROJECTED_SETTING(_appearance.SourceProfile().FontInfo(), EnableBuiltinGlyphs);

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
        WINRT_OBSERVABLE_PROPERTY(Windows::Foundation::Collections::IObservableVector<Editor::ColorSchemeViewModel>, SchemesList, _propertyChangedHandlers, nullptr);
        WINRT_OBSERVABLE_PROPERTY(Windows::Foundation::Collections::IObservableVector<Editor::AxisKeyValuePair>, FontAxesVector, _propertyChangedHandlers, nullptr);
        WINRT_OBSERVABLE_PROPERTY(Windows::Foundation::Collections::IObservableVector<Editor::FeatureKeyValuePair>, FontFeaturesVector, _propertyChangedHandlers, nullptr);

    private:
        Model::AppearanceConfig _appearance;
        winrt::hstring _lastBgImagePath;

        Editor::AxisKeyValuePair _CreateAxisKeyValuePairHelper(winrt::hstring axisKey, float axisValue, const Windows::Foundation::Collections::IMap<winrt::hstring, float>& baseMap, const Windows::Foundation::Collections::IMap<winrt::hstring, winrt::hstring>& tagToNameMap);
        Editor::FeatureKeyValuePair _CreateFeatureKeyValuePairHelper(winrt::hstring axisKey, uint32_t axisValue, const Windows::Foundation::Collections::IMap<winrt::hstring, uint32_t>& baseMap, const Windows::Foundation::Collections::IMap<winrt::hstring, winrt::hstring>& tagToNameMap);

        bool _IsDefaultFeature(winrt::hstring featureTag);
    };

    struct Appearances : AppearancesT<Appearances>
    {
    public:
        Appearances();

        // font face
        Windows::Foundation::IInspectable CurrentFontFace() const;

        // CursorShape visibility logic
        bool IsVintageCursor() const;

        bool UsingMonospaceFont() const noexcept;
        bool ShowAllFonts() const noexcept;
        void ShowAllFonts(const bool& value);

        fire_and_forget BackgroundImage_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& e);
        void BIAlignment_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& e);
        void FontFace_SelectionChanged(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Controls::SelectionChangedEventArgs& e);
        void DeleteAxisKeyValuePair_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& e);
        void AddNewAxisKeyValuePair_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& e);
        void DeleteFeatureKeyValuePair_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& e);
        void AddNewFeatureKeyValuePair_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& e);

        // manually bind FontWeight
        Windows::Foundation::IInspectable CurrentFontWeight() const;
        void CurrentFontWeight(const Windows::Foundation::IInspectable& enumEntry);
        bool IsCustomFontWeight();
        WINRT_PROPERTY(Windows::Foundation::Collections::IObservableVector<Microsoft::Terminal::Settings::Editor::EnumEntry>, FontWeightList);

        GETSET_BINDABLE_ENUM_SETTING(CursorShape, Microsoft::Terminal::Core::CursorStyle, Appearance().CursorShape);
        GETSET_BINDABLE_ENUM_SETTING(AdjustIndistinguishableColors, Microsoft::Terminal::Core::AdjustTextMode, Appearance().AdjustIndistinguishableColors);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        DEPENDENCY_PROPERTY(Editor::AppearanceViewModel, Appearance);
        WINRT_PROPERTY(Editor::ProfileViewModel, SourceProfile, nullptr);
        WINRT_PROPERTY(IHostedInWindow, WindowRoot, nullptr);
        GETSET_BINDABLE_ENUM_SETTING(BackgroundImageStretchMode, Windows::UI::Xaml::Media::Stretch, Appearance().BackgroundImageStretchMode);

        GETSET_BINDABLE_ENUM_SETTING(IntenseTextStyle, Microsoft::Terminal::Settings::Model::IntenseStyle, Appearance().IntenseTextStyle);
        WINRT_OBSERVABLE_PROPERTY(bool, ShowProportionalFontWarning, _PropertyChangedHandlers, nullptr);

    private:
        bool _ShowAllFonts;
        void _UpdateBIAlignmentControl(const int32_t val);
        std::array<Windows::UI::Xaml::Controls::Primitives::ToggleButton, 9> _BIAlignmentButtons;

        Windows::Foundation::Collections::IMap<uint16_t, Microsoft::Terminal::Settings::Editor::EnumEntry> _FontWeightMap;
        Editor::EnumEntry _CustomFontWeight{ nullptr };

        Windows::Foundation::Collections::IObservableVector<winrt::hstring> _FontAxesNames;
        Windows::Foundation::Collections::IObservableVector<winrt::hstring> _FontFeaturesNames;

        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _ViewModelChangedRevoker;
        static void _ViewModelChanged(const Windows::UI::Xaml::DependencyObject& d, const Windows::UI::Xaml::DependencyPropertyChangedEventArgs& e);
        void _UpdateWithNewViewModel();
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Appearances);
    BASIC_FACTORY(AxisKeyValuePair);
    BASIC_FACTORY(FeatureKeyValuePair);
}
