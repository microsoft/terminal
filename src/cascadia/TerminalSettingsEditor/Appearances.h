// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

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
        Font(std::wstring name, std::wstring localizedName) :
            _Name{ name },
            _LocalizedName{ localizedName } {};

        hstring ToString() { return _LocalizedName; }

        WINRT_PROPERTY(hstring, Name);
        WINRT_PROPERTY(hstring, LocalizedName);
    };

    struct AppearanceViewModel : AppearanceViewModelT<AppearanceViewModel>, ViewModelHelper<AppearanceViewModel>
    {
    public:
        AppearanceViewModel(const Model::AppearanceConfig& appearance);

        // background image
        bool UseDesktopBGImage();
        void UseDesktopBGImage(const bool useDesktop);
        bool BackgroundImageSettingsVisible();

        // font face
        static void UpdateFontList() noexcept;
        Windows::Foundation::Collections::IObservableVector<Editor::Font> CompleteFontList() const noexcept;
        Windows::Foundation::Collections::IObservableVector<Editor::Font> MonospaceFontList() const noexcept;
        bool UsingMonospaceFont() const noexcept;
        bool ShowAllFonts() const noexcept;
        void ShowAllFonts(const bool& value);

        Windows::Foundation::Collections::IMapView<hstring, Model::ColorScheme> Schemes() { return _Schemes; }
        void Schemes(const Windows::Foundation::Collections::IMapView<hstring, Model::ColorScheme>& val) { _Schemes = val; }

        WINRT_PROPERTY(IHostedInWindow, WindowRoot, nullptr);

        OBSERVABLE_PROJECTED_SETTING(_appearance.SourceProfile(), FontFace);
        OBSERVABLE_PROJECTED_SETTING(_appearance.SourceProfile(), FontSize);
        OBSERVABLE_PROJECTED_SETTING(_appearance.SourceProfile(), FontWeight);

        OBSERVABLE_PROJECTED_SETTING(_appearance, RetroTerminalEffect);
        OBSERVABLE_PROJECTED_SETTING(_appearance, CursorShape);
        OBSERVABLE_PROJECTED_SETTING(_appearance, CursorHeight);
        OBSERVABLE_PROJECTED_SETTING(_appearance, ColorSchemeName);
        OBSERVABLE_PROJECTED_SETTING(_appearance, BackgroundImagePath);
        OBSERVABLE_PROJECTED_SETTING(_appearance, BackgroundImageOpacity);
        OBSERVABLE_PROJECTED_SETTING(_appearance, BackgroundImageStretchMode);
        OBSERVABLE_PROJECTED_SETTING(_appearance, BackgroundImageAlignment);

    private:
        Model::AppearanceConfig _appearance;
        winrt::hstring _lastBgImagePath;
        Windows::Foundation::Collections::IMapView<hstring, Model::ColorScheme> _Schemes;
        bool _ShowAllFonts;

        static Windows::Foundation::Collections::IObservableVector<Editor::Font> _MonospaceFontList;
        static Windows::Foundation::Collections::IObservableVector<Editor::Font> _FontList;

        static Editor::Font _GetFont(com_ptr<IDWriteLocalizedStrings> localizedFamilyNames);
    };

    struct Appearances : AppearancesT<Appearances>
    {
    public:
        Appearances();

        // font face
        Windows::Foundation::IInspectable CurrentFontFace() const;

        // CursorShape visibility logic
        bool IsVintageCursor() const;

        Model::ColorScheme CurrentColorScheme();
        void CurrentColorScheme(const Model::ColorScheme& val);

        fire_and_forget BackgroundImage_Click(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e);
        void BIAlignment_Click(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e);
        void FontFace_SelectionChanged(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs const& e);

        // manually bind FontWeight
        Windows::Foundation::IInspectable CurrentFontWeight() const;
        void CurrentFontWeight(const Windows::Foundation::IInspectable& enumEntry);
        bool IsCustomFontWeight();
        WINRT_PROPERTY(Windows::Foundation::Collections::IObservableVector<Microsoft::Terminal::Settings::Editor::EnumEntry>, FontWeightList);

        GETSET_BINDABLE_ENUM_SETTING(CursorShape, Microsoft::Terminal::Core::CursorStyle, Appearance, CursorShape);
        WINRT_PROPERTY(Windows::Foundation::Collections::IObservableVector<Model::ColorScheme>, ColorSchemeList, nullptr);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        DEPENDENCY_PROPERTY(Editor::AppearanceViewModel, Appearance);

        GETSET_BINDABLE_ENUM_SETTING(BackgroundImageStretchMode, Windows::UI::Xaml::Media::Stretch, Appearance, BackgroundImageStretchMode);

    private:
        void _UpdateBIAlignmentControl(const int32_t val);
        std::array<Windows::UI::Xaml::Controls::Primitives::ToggleButton, 9> _BIAlignmentButtons;

        Windows::Foundation::Collections::IMap<uint16_t, Microsoft::Terminal::Settings::Editor::EnumEntry> _FontWeightMap;
        Editor::EnumEntry _CustomFontWeight{ nullptr };

        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _ViewModelChangedRevoker;
        static void _ViewModelChanged(Windows::UI::Xaml::DependencyObject const& d, Windows::UI::Xaml::DependencyPropertyChangedEventArgs const& e);
        void _UpdateWithNewViewModel();
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Appearances);
}
