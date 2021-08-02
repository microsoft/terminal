// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Appearances.h"
#include "Appearances.g.cpp"
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

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
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

    DependencyProperty Appearances::_AppearanceProperty{ nullptr };

    Appearances::Appearances() :
        _ShowAllFonts{ false },
        _ColorSchemeList{ single_threaded_observable_vector<ColorScheme>() }
    {
        InitializeComponent();

        INITIALIZE_BINDABLE_ENUM_SETTING(CursorShape, CursorStyle, winrt::Microsoft::Terminal::Core::CursorStyle, L"Profile_CursorShape", L"Content");
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
    }

    // Method Description:
    // - Searches through our list of monospace fonts to determine if the settings model's current font face is a monospace font
    bool Appearances::UsingMonospaceFont() const noexcept
    {
        bool result{ false };
        const auto currentFont{ Appearance().FontFace() };
        for (const auto& font : SourceProfile().MonospaceFontList())
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
        // look for the current font in our shown list of fonts
        const auto& appearanceVM{ Appearance() };
        const auto appearanceFontFace{ appearanceVM.FontFace() };
        const auto& currentFontList{ ShowAllFonts() ? SourceProfile().CompleteFontList() : SourceProfile().MonospaceFontList() };
        IInspectable fallbackFont;
        for (const auto& font : currentFontList)
        {
            if (font.LocalizedName() == appearanceFontFace)
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

    void Appearances::FontFace_SelectionChanged(IInspectable const& /*sender*/, SelectionChangedEventArgs const& e)
    {
        // NOTE: We need to hook up a selection changed event handler here instead of directly binding to the appearance view model.
        //       A two way binding to the view model causes an infinite loop because both combo boxes keep fighting over which one's right.
        const auto selectedItem{ e.AddedItems().GetAt(0) };
        const auto newFontFace{ unbox_value<Editor::Font>(selectedItem) };
        Appearance().FontFace(newFontFace.LocalizedName());
    }

    void Appearances::_ViewModelChanged(DependencyObject const& d, DependencyPropertyChangedEventArgs const& /*args*/)
    {
        const auto& obj{ d.as<Editor::Appearances>() };
        get_self<Appearances>(obj)->_UpdateWithNewViewModel();
    }

    void Appearances::_UpdateWithNewViewModel()
    {
        if (Appearance())
        {
            const auto& colorSchemeMap{ Appearance().Schemes() };
            for (const auto& pair : colorSchemeMap)
            {
                _ColorSchemeList.Append(pair.Value());
            }

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
                else if (settingName == L"ColorSchemeName")
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
        }
    }

    fire_and_forget Appearances::BackgroundImage_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        const auto parentHwnd{ reinterpret_cast<HWND>(Appearance().WindowRoot().GetHostingWindow()) };
        auto file = co_await OpenImagePicker(parentHwnd);
        if (!file.empty())
        {
            Appearance().BackgroundImagePath(file);
        }
    }

    void Appearances::BIAlignment_Click(IInspectable const& sender, RoutedEventArgs const& /*e*/)
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

    ColorScheme Appearances::CurrentColorScheme()
    {
        const auto schemeName{ Appearance().ColorSchemeName() };
        if (const auto scheme{ Appearance().Schemes().TryLookup(schemeName) })
        {
            return scheme;
        }
        else
        {
            // This Appearance points to a color scheme that was renamed or deleted.
            // Fallback to Campbell.
            return Appearance().Schemes().TryLookup(L"Campbell");
        }
    }

    void Appearances::CurrentColorScheme(const ColorScheme& val)
    {
        Appearance().ColorSchemeName(val.Name());
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
