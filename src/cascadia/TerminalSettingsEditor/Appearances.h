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
#include "Appearances.g.h"
#include "AppearanceViewModel.g.h"
#include "Utils.h"
#include "ViewModelHelpers.h"
#include "SettingContainer.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct FontComparator
    {
        bool operator()(const Font& lhs, const Font& rhs) const
        {
            return lhs.LocalizedName() < rhs.LocalizedName();
        }
    };

    struct Font : FontT<Font>
    {
    public:
        Font(std::wstring name, std::wstring localizedName, IDWriteFontFamily* family) :
            _Name{ name },
            _LocalizedName{ localizedName }
        {
            _family.copy_from(family);
        }

        hstring ToString() { return _LocalizedName; }
        bool HasPowerlineCharacters();

        WINRT_PROPERTY(hstring, Name);
        WINRT_PROPERTY(hstring, LocalizedName);

    private:
        winrt::com_ptr<IDWriteFontFamily> _family;
        std::optional<bool> _hasPowerlineCharacters;
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

        WINRT_PROPERTY(bool, IsDefault, false);

        // These settings are not defined in AppearanceConfig, so we grab them
        // from the source profile itself. The reason we still want them in the
        // AppearanceViewModel is so we can continue to have the 'Text' grouping
        // we currently have in xaml, since that grouping has some settings that
        // are defined in AppearanceConfig and some that are not.
        OBSERVABLE_PROJECTED_SETTING(_appearance.SourceProfile().FontInfo(), FontFace);
        OBSERVABLE_PROJECTED_SETTING(_appearance.SourceProfile().FontInfo(), FontSize);
        OBSERVABLE_PROJECTED_SETTING(_appearance.SourceProfile().FontInfo(), FontWeight);

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

    private:
        Model::AppearanceConfig _appearance;
        winrt::hstring _lastBgImagePath;
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

        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _ViewModelChangedRevoker;
        static void _ViewModelChanged(const Windows::UI::Xaml::DependencyObject& d, const Windows::UI::Xaml::DependencyPropertyChangedEventArgs& e);
        void _UpdateWithNewViewModel();
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Appearances);
}
