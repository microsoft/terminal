// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "NullableColorPicker.h"
#include "NullableColorPicker.g.cpp"

#include <LibraryResources.h>

using namespace winrt;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Media;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
}

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    static constexpr bool equalsColor(Windows::UI::Color a, Microsoft::Terminal::Core::Color b)
    {
        return a.R == b.R && a.G == b.G && a.B == b.B;
    }

    DependencyProperty NullableColorPicker::_ColorSchemeVMProperty{ nullptr };
    DependencyProperty NullableColorPicker::_CurrentColorProperty{ nullptr };
    DependencyProperty NullableColorPicker::_ShowNullColorButtonProperty{ nullptr };
    DependencyProperty NullableColorPicker::_NullColorButtonLabelProperty{ nullptr };
    DependencyProperty NullableColorPicker::_NullColorPreviewProperty{ nullptr };

    NullableColorPicker::NullableColorPicker()
    {
        _InitializeProperties();
        InitializeComponent();
    }

    void NullableColorPicker::_InitializeProperties()
    {
        // Initialize any dependency properties here.
        // This performs a lazy load on these properties, instead of
        // initializing them when the DLL loads.
        if (!_ColorSchemeVMProperty)
        {
            _ColorSchemeVMProperty =
                DependencyProperty::Register(
                    L"ColorSchemeVM",
                    xaml_typename<Editor::ColorSchemeViewModel>(),
                    xaml_typename<Editor::NullableColorPicker>(),
                    PropertyMetadata{ nullptr });
        }
        if (!_CurrentColorProperty)
        {
            _CurrentColorProperty =
                DependencyProperty::Register(
                    L"CurrentColor",
                    xaml_typename<Windows::Foundation::IReference<Microsoft::Terminal::Core::Color>>(),
                    xaml_typename<Editor::NullableColorPicker>(),
                    PropertyMetadata{ nullptr, PropertyChangedCallback{ &NullableColorPicker::_OnCurrentColorValueChanged } });
        }
        if (!_ShowNullColorButtonProperty)
        {
            _ShowNullColorButtonProperty =
                DependencyProperty::Register(
                    L"ShowNullColorButton",
                    xaml_typename<bool>(),
                    xaml_typename<Editor::NullableColorPicker>(),
                    PropertyMetadata{ box_value(true) });
        }
        if (!_NullColorButtonLabelProperty)
        {
            _NullColorButtonLabelProperty =
                DependencyProperty::Register(
                    L"NullColorButtonLabel",
                    xaml_typename<hstring>(),
                    xaml_typename<Editor::NullableColorPicker>(),
                    PropertyMetadata{ nullptr });
        }
        if (!_NullColorPreviewProperty)
        {
            _NullColorPreviewProperty =
                DependencyProperty::Register(
                    L"NullColorPreview",
                    xaml_typename<Windows::UI::Color>(),
                    xaml_typename<Editor::NullableColorPicker>(),
                    PropertyMetadata{ box_value(Windows::UI::Colors::Transparent()) });
        }
    }

    void NullableColorPicker::_OnCurrentColorValueChanged(const DependencyObject& d, const DependencyPropertyChangedEventArgs& /*e*/)
    {
        const auto& obj{ d.try_as<Editor::NullableColorPicker>() };
        get_self<NullableColorPicker>(obj)->_UpdateColorChips();
    }

    void NullableColorPicker::_UpdateColorChips()
    {
        const auto& currentColor = CurrentColor();
        for (const auto& colorChip : _colorChips)
        {
            const auto& chipColor = colorChip.DataContext().as<Editor::ColorTableEntry>().Color();
            colorChip.IsChecked(currentColor ?
                                    equalsColor(chipColor, currentColor.Value()) :
                                    false);
        }
    }

    SolidColorBrush NullableColorPicker::CalculateBorderBrush(const Windows::UI::Color& color)
    {
        static constexpr auto isColorLight = [](const winrt::Windows::UI::Color& clr) -> bool {
            return (((5 * clr.G) + (2 * clr.R) + clr.B) > (8 * 128));
        };
        if (isColorLight(color))
        {
            return SolidColorBrush(Colors::Black());
        }
        else
        {
            return SolidColorBrush(Colors::White());
        }
    }

    void NullableColorPicker::ColorChip_Clicked(const IInspectable& sender, const RoutedEventArgs& /*args*/)
    {
        const auto& btn = sender.as<Windows::UI::Xaml::Controls::Primitives::ToggleButton>();
        const auto& colorEntryColor = btn.DataContext().as<Editor::ColorTableEntry>().Color();
        const Microsoft::Terminal::Core::Color terminalColor{ colorEntryColor.R, colorEntryColor.G, colorEntryColor.B, colorEntryColor.A };
        CurrentColor(terminalColor);
        btn.IsChecked(true);
    }

    void NullableColorPicker::ColorChip_DataContextChanged(const IInspectable& sender, const DataContextChangedEventArgs& args)
    {
        if (const auto& toggleBtn = sender.try_as<Controls::Primitives::ToggleButton>())
        {
            if (const auto& currentColor = CurrentColor())
            {
                const auto& currentColorVal = currentColor.Value();
                const auto& newChipColor = args.NewValue().as<Editor::ColorTableEntry>().Color();
                toggleBtn.IsChecked(equalsColor(newChipColor, currentColorVal));
            }
        }
    }

    bool NullableColorPicker::IsNull(IReference<Microsoft::Terminal::Core::Color> color)
    {
        return color == nullptr;
    }

    void NullableColorPicker::NullColorButton_Clicked(const IInspectable& /*sender*/, const RoutedEventArgs& /*args*/)
    {
        CurrentColor(nullptr);
    }

    safe_void_coroutine NullableColorPicker::MoreColors_Clicked(const IInspectable& /*sender*/, const RoutedEventArgs& /*args*/)
    {
        co_await ColorPickerDialog().ShowAsync();
    }

    void NullableColorPicker::ColorPickerDialog_Opened(const IInspectable& /*sender*/, const ContentDialogOpenedEventArgs& /*args*/)
    {
        // Initialize color picker with current color
        if (CurrentColor())
        {
            const auto& terminalColor = CurrentColor().Value();
            const Windows::UI::Color winuiColor{
                .A = terminalColor.A,
                .R = terminalColor.R,
                .G = terminalColor.G,
                .B = terminalColor.B
            };
            ColorPickerControl().Color(winuiColor);
        }
        else
        {
            // No current color (null), use the deduced value for null
            ColorPickerControl().Color(NullColorPreview());
        }
    }

    void NullableColorPicker::ColorPickerDialog_PrimaryButtonClick(const IInspectable& /*sender*/, const ContentDialogButtonClickEventArgs& /*args*/)
    {
        const auto& selectedColor = ColorPickerControl().Color();
        const Microsoft::Terminal::Core::Color terminalColor{ selectedColor.R, selectedColor.G, selectedColor.B, selectedColor.A };
        CurrentColor(terminalColor);
    }

    void NullableColorPicker::ColorChip_Loaded(const IInspectable& sender, const RoutedEventArgs& /*args*/)
    {
        if (const auto& toggleBtn = sender.try_as<Controls::Primitives::ToggleButton>())
        {
            if (const auto& currentColor = CurrentColor())
            {
                const auto& currentColorVal = currentColor.Value();
                const auto& chipColor = toggleBtn.DataContext().as<Editor::ColorTableEntry>().Color();
                if (equalsColor(chipColor, currentColorVal))
                {
                    toggleBtn.IsChecked(true);
                }
            }
            _colorChips.push_back(toggleBtn);
        }
    }

    void NullableColorPicker::ColorChip_Unloaded(const IInspectable& sender, const RoutedEventArgs& /*args*/)
    {
        if (const auto& toggleBtn = sender.try_as<Controls::Primitives::ToggleButton>())
        {
            for (auto it = _colorChips.begin(); it != _colorChips.end(); ++it)
            {
                if (*it == toggleBtn)
                {
                    _colorChips.erase(it);
                    break;
                }
            }
        }
    }
}
