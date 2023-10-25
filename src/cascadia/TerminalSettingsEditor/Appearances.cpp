// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Appearances.h"
#include "Appearances.g.cpp"
#include "EnumEntry.h"

#include <LibraryResources.h>
#include "..\WinRTUtils\inc\Utils.h"

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
    bool Font::HasPowerlineCharacters()
    {
        if (!_hasPowerlineCharacters.has_value())
        {
            try
            {
                winrt::com_ptr<IDWriteFont> font;
                THROW_IF_FAILED(_family->GetFont(0, font.put()));
                BOOL exists{};
                // We're actually checking for the "Extended" PowerLine glyph set.
                // They're more fun.
                THROW_IF_FAILED(font->HasCharacter(0xE0B6, &exists));
                _hasPowerlineCharacters = (exists == TRUE);
            }
            catch (...)
            {
                _hasPowerlineCharacters = false;
            }
        }
        return _hasPowerlineCharacters.value_or(false);
    }

    AppearanceViewModel::AppearanceViewModel(const Model::AppearanceConfig& appearance) :
        _appearance{ appearance }
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
        });

        // Cache the original BG image path. If the user clicks "Use desktop
        // wallpaper", then un-checks it, this is the string we'll restore to
        // them.
        if (BackgroundImagePath() != L"desktopWallpaper")
        {
            _lastBgImagePath = BackgroundImagePath();
        }
    }

    double AppearanceViewModel::LineHeight() const noexcept
    {
        const auto fontInfo = _appearance.SourceProfile().FontInfo();
        const auto cellHeight = fontInfo.CellHeight();
        const auto str = cellHeight.c_str();

        auto& errnoRef = errno; // Nonzero cost, pay it once.
        errnoRef = 0;

        wchar_t* end;
        const auto value = std::wcstod(str, &end);

        return str == end || errnoRef == ERANGE ? NAN : value;
    }

    void AppearanceViewModel::LineHeight(const double value)
    {
        std::wstring str;

        if (value >= 0.1 && value <= 10.0)
        {
            str = fmt::format(FMT_STRING(L"{:.6g}"), value);
        }

        const auto fontInfo = _appearance.SourceProfile().FontInfo();

        if (fontInfo.CellHeight() != str)
        {
            if (str.empty())
            {
                fontInfo.ClearCellHeight();
            }
            else
            {
                fontInfo.CellHeight(str);
            }
            _NotifyChanges(L"HasLineHeight", L"LineHeight");
        }
    }

    bool AppearanceViewModel::HasLineHeight() const
    {
        const auto fontInfo = _appearance.SourceProfile().FontInfo();
        return fontInfo.HasCellHeight();
    }

    void AppearanceViewModel::ClearLineHeight()
    {
        LineHeight(NAN);
    }

    Model::FontConfig AppearanceViewModel::LineHeightOverrideSource() const
    {
        const auto fontInfo = _appearance.SourceProfile().FontInfo();
        return fontInfo.CellHeightOverrideSource();
    }

    void AppearanceViewModel::SetFontWeightFromDouble(double fontWeight)
    {
        FontWeight(Converters::DoubleToFontWeight(fontWeight));
    }

    void AppearanceViewModel::SetBackgroundImageOpacityFromPercentageValue(double percentageValue)
    {
        BackgroundImageOpacity(Converters::PercentageValueToPercentage(percentageValue));
    }

    void AppearanceViewModel::SetBackgroundImagePath(winrt::hstring path)
    {
        BackgroundImagePath(path);
    }

    bool AppearanceViewModel::UseDesktopBGImage()
    {
        return BackgroundImagePath() == L"desktopWallpaper";
    }

    void AppearanceViewModel::UseDesktopBGImage(const bool useDesktop)
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

    bool AppearanceViewModel::BackgroundImageSettingsVisible()
    {
        return BackgroundImagePath() != L"";
    }

    void AppearanceViewModel::ClearColorScheme()
    {
        ClearDarkColorSchemeName();
        _NotifyChanges(L"CurrentColorScheme");
    }

    Editor::ColorSchemeViewModel AppearanceViewModel::CurrentColorScheme()
    {
        const auto schemeName{ DarkColorSchemeName() };
        const auto allSchemes{ SchemesList() };
        for (const auto& scheme : allSchemes)
        {
            if (scheme.Name() == schemeName)
            {
                return scheme;
            }
        }
        // This Appearance points to a color scheme that was renamed or deleted.
        // Fallback to the first one in the list.
        return allSchemes.GetAt(0);
    }

    void AppearanceViewModel::CurrentColorScheme(const ColorSchemeViewModel& val)
    {
        DarkColorSchemeName(val.Name());
        LightColorSchemeName(val.Name());
    }

    DependencyProperty Appearances::_AppearanceProperty{ nullptr };

    Appearances::Appearances() :
        _ShowAllFonts{ false },
        _ShowProportionalFontWarning{ false }
    {
        InitializeComponent();

        {
            using namespace winrt::Windows::Globalization::NumberFormatting;
            // > .NET rounds to 12 significant digits when displaying doubles, so we will [...]
            // ...obviously not do that, because this is an UI element for humans. This prevents
            // issues when displaying 32-bit floats, because WinUI is unaware about their existence.
            IncrementNumberRounder rounder;
            rounder.Increment(1e-6);

            for (const auto& box : { _fontSizeBox(), _lineHeightBox() })
            {
                // BODGY: Depends on WinUI internals.
                box.NumberFormatter().as<DecimalFormatter>().NumberRounder(rounder);
            }
        }

        INITIALIZE_BINDABLE_ENUM_SETTING(CursorShape, CursorStyle, winrt::Microsoft::Terminal::Core::CursorStyle, L"Profile_CursorShape", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(AdjustIndistinguishableColors, AdjustIndistinguishableColors, winrt::Microsoft::Terminal::Core::AdjustTextMode, L"Profile_AdjustIndistinguishableColors", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING_REVERSE_ORDER(BackgroundImageStretchMode, BackgroundImageStretchMode, winrt::Windows::UI::Xaml::Media::Stretch, L"Profile_BackgroundImageStretchMode", L"Content");

        // manually add Custom FontWeight option. Don't add it to the Map
        INITIALIZE_BINDABLE_ENUM_SETTING(FontWeight, FontWeight, uint16_t, L"Profile_FontWeight", L"Content");
        _CustomFontWeight = winrt::make<EnumEntry>(RS_(L"Profile_FontWeightCustom/Content"), winrt::box_value<uint16_t>(0u));
        _FontWeightList.Append(_CustomFontWeight);

        if (!_AppearanceProperty)
        {
            _AppearanceProperty =
                DependencyProperty::Register(
                    L"Appearance",
                    xaml_typename<Editor::AppearanceViewModel>(),
                    xaml_typename<Editor::Appearances>(),
                    PropertyMetadata{ nullptr, PropertyChangedCallback{ &Appearances::_ViewModelChanged } });
        }

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

        const auto showAllFontsCheckboxTooltip{ ToolTipService::GetToolTip(ShowAllFontsCheckbox()) };
        Automation::AutomationProperties::SetFullDescription(ShowAllFontsCheckbox(), unbox_value<hstring>(showAllFontsCheckboxTooltip));

        const auto backgroundImgCheckboxTooltip{ ToolTipService::GetToolTip(UseDesktopImageCheckBox()) };
        Automation::AutomationProperties::SetFullDescription(UseDesktopImageCheckBox(), unbox_value<hstring>(backgroundImgCheckboxTooltip));

        INITIALIZE_BINDABLE_ENUM_SETTING(IntenseTextStyle, IntenseTextStyle, winrt::Microsoft::Terminal::Settings::Model::IntenseStyle, L"Appearance_IntenseTextStyle", L"Content");
    }

    // Method Description:
    // - Searches through our list of monospace fonts to determine if the settings model's current font face is a monospace font
    bool Appearances::UsingMonospaceFont() const noexcept
    {
        auto result{ false };
        const auto currentFont{ Appearance().FontFace() };
        for (const auto& font : ProfileViewModel::MonospaceFontList())
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
    bool Appearances::ShowAllFonts() const noexcept
    {
        // - _ShowAllFonts is directly bound to the checkbox. So this is the user set value.
        // - If we are not using a monospace font, show all of the fonts so that the ComboBox is still properly bound
        return _ShowAllFonts || !UsingMonospaceFont();
    }

    void Appearances::ShowAllFonts(const bool& value)
    {
        if (_ShowAllFonts != value)
        {
            _ShowAllFonts = value;
            _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"ShowAllFonts" });
        }
    }

    IInspectable Appearances::CurrentFontFace() const
    {
        const auto& appearanceVM{ Appearance() };
        const auto appearanceFontFace{ appearanceVM.FontFace() };
        return box_value(ProfileViewModel::FindFontWithLocalizedName(appearanceFontFace));
    }

    void Appearances::FontFace_SelectionChanged(const IInspectable& /*sender*/, const SelectionChangedEventArgs& e)
    {
        // NOTE: We need to hook up a selection changed event handler here instead of directly binding to the appearance view model.
        //       A two way binding to the view model causes an infinite loop because both combo boxes keep fighting over which one's right.
        const auto selectedItem{ e.AddedItems().GetAt(0) };
        const auto newFontFace{ unbox_value<Editor::Font>(selectedItem) };
        Appearance().FontFace(newFontFace.LocalizedName());
        if (!UsingMonospaceFont())
        {
            ShowProportionalFontWarning(true);
        }
        else
        {
            ShowProportionalFontWarning(false);
        }
    }

    void Appearances::_ViewModelChanged(const DependencyObject& d, const DependencyPropertyChangedEventArgs& /*args*/)
    {
        const auto& obj{ d.as<Editor::Appearances>() };
        get_self<Appearances>(obj)->_UpdateWithNewViewModel();
    }

    void Appearances::_UpdateWithNewViewModel()
    {
        if (Appearance())
        {
            const auto& biAlignmentVal{ static_cast<int32_t>(Appearance().BackgroundImageAlignment()) };
            for (const auto& biButton : _BIAlignmentButtons)
            {
                biButton.IsChecked(biButton.Tag().as<int32_t>() == biAlignmentVal);
            }

            _ViewModelChangedRevoker = Appearance().PropertyChanged(winrt::auto_revoke, [=](auto&&, const PropertyChangedEventArgs& args) {
                const auto settingName{ args.PropertyName() };
                if (settingName == L"CursorShape")
                {
                    _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentCursorShape" });
                    _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"IsVintageCursor" });
                }
                else if (settingName == L"DarkColorSchemeName" || settingName == L"LightColorSchemeName")
                {
                    _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentColorScheme" });
                }
                else if (settingName == L"BackgroundImageStretchMode")
                {
                    _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentBackgroundImageStretchMode" });
                }
                else if (settingName == L"BackgroundImageAlignment")
                {
                    _UpdateBIAlignmentControl(static_cast<int32_t>(Appearance().BackgroundImageAlignment()));
                }
                else if (settingName == L"FontWeight")
                {
                    _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentFontWeight" });
                    _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"IsCustomFontWeight" });
                }
                else if (settingName == L"FontFace" || settingName == L"CurrentFontList")
                {
                    // notify listener that all font face related values might have changed
                    if (!UsingMonospaceFont())
                    {
                        _ShowAllFonts = true;
                    }
                    _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentFontFace" });
                    _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"ShowAllFonts" });
                    _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"UsingMonospaceFont" });
                }
                else if (settingName == L"IntenseTextStyle")
                {
                    _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentIntenseTextStyle" });
                }
                else if (settingName == L"AdjustIndistinguishableColors")
                {
                    _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentAdjustIndistinguishableColors" });
                }
                else if (settingName == L"ShowProportionalFontWarning")
                {
                    _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"ShowProportionalFontWarning" });
                }
                // YOU THERE ADDING A NEW APPEARANCE SETTING
                // Make sure you add a block like
                //
                //   else if (settingName == L"MyNewSetting")
                //   {
                //       _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentMyNewSetting" });
                //   }
                //
                // To make sure that changes to the AppearanceViewModel will
                // propagate back up to the actual UI (in Appearances). The
                // CurrentMyNewSetting properties are the ones that are bound in
                // XAML. If you don't do this right (or only raise a property
                // changed for "MyNewSetting"), then things like the reset
                // button won't work right.
            });

            // make sure to send all the property changed events once here
            // we do this in the case an old appearance was deleted and then a new one is created,
            // the old settings need to be updated in xaml
            _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentCursorShape" });
            _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"IsVintageCursor" });
            _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentColorScheme" });
            _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentBackgroundImageStretchMode" });
            _UpdateBIAlignmentControl(static_cast<int32_t>(Appearance().BackgroundImageAlignment()));
            _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentFontWeight" });
            _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"IsCustomFontWeight" });
            _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentFontFace" });
            _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"ShowAllFonts" });
            _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"UsingMonospaceFont" });
            _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentIntenseTextStyle" });
            _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentAdjustIndistinguishableColors" });
            _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"ShowProportionalFontWarning" });
        }
    }

    fire_and_forget Appearances::BackgroundImage_Click(const IInspectable&, const RoutedEventArgs&)
    {
        auto lifetime = get_strong();

        const auto parentHwnd{ reinterpret_cast<HWND>(WindowRoot().GetHostingWindow()) };
        auto file = co_await OpenImagePicker(parentHwnd);
        if (!file.empty())
        {
            Appearance().BackgroundImagePath(file);
        }
    }

    void Appearances::BIAlignment_Click(const IInspectable& sender, const RoutedEventArgs& /*e*/)
    {
        if (const auto& button{ sender.try_as<Primitives::ToggleButton>() })
        {
            if (const auto& tag{ button.Tag().try_as<int32_t>() })
            {
                // Update the Appearance's value and the control
                Appearance().BackgroundImageAlignment(static_cast<ConvergedAlignment>(*tag));
                _UpdateBIAlignmentControl(*tag);
            }
        }
    }

    // Method Description:
    // - Resets all of the buttons to unchecked, and checks the one with the provided tag
    // Arguments:
    // - val - the background image alignment (ConvergedAlignment) that we want to represent in the control
    void Appearances::_UpdateBIAlignmentControl(const int32_t val)
    {
        for (const auto& biButton : _BIAlignmentButtons)
        {
            if (const auto& biButtonAlignment{ biButton.Tag().try_as<int32_t>() })
            {
                biButton.IsChecked(biButtonAlignment == val);
            }
        }
    }

    bool Appearances::IsVintageCursor() const
    {
        return Appearance().CursorShape() == Core::CursorStyle::Vintage;
    }

    IInspectable Appearances::CurrentFontWeight() const
    {
        // if no value was found, we have a custom value
        const auto maybeEnumEntry{ _FontWeightMap.TryLookup(Appearance().FontWeight().Weight) };
        return maybeEnumEntry ? maybeEnumEntry : _CustomFontWeight;
    }

    void Appearances::CurrentFontWeight(const IInspectable& enumEntry)
    {
        if (auto ee = enumEntry.try_as<Editor::EnumEntry>())
        {
            if (ee != _CustomFontWeight)
            {
                const auto weight{ winrt::unbox_value<uint16_t>(ee.EnumValue()) };
                const Windows::UI::Text::FontWeight setting{ weight };
                Appearance().FontWeight(setting);

                // Appearance does not have observable properties
                // So the TwoWay binding doesn't update on the State --> Slider direction
                FontWeightSlider().Value(weight);
            }
            _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"IsCustomFontWeight" });
        }
    }

    bool Appearances::IsCustomFontWeight()
    {
        // Use SelectedItem instead of CurrentFontWeight.
        // CurrentFontWeight converts the Appearance's value to the appropriate enum entry,
        // whereas SelectedItem identifies which one was selected by the user.
        return FontWeightComboBox().SelectedItem() == _CustomFontWeight;
    }

}
