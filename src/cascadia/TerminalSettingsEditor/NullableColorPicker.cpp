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
                    PropertyMetadata{ nullptr });
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
                    PropertyMetadata{ box_value(RS_(L"NullableColorPicker_NullColorButtonDefaultText")) });
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

    void NullableColorPicker::ColorChip_Clicked(const IInspectable& sender, const RoutedEventArgs& /*args*/)
    {
        const auto& btn = sender.as<Windows::UI::Xaml::Controls::Primitives::ToggleButton>();
        const auto& colorEntry = btn.DataContext().as<Editor::ColorTableEntry>();
        const auto& colorEntryColor = colorEntry.Color();
        const Microsoft::Terminal::Core::Color terminalColor{ colorEntryColor.R, colorEntryColor.G, colorEntryColor.B, colorEntryColor.A };
        CurrentColor(terminalColor);
        btn.IsChecked(true);
    }

    void NullableColorPicker::NullColorButton_Clicked(const IInspectable& /*sender*/, const RoutedEventArgs& /*args*/)
    {
        CurrentColor(nullptr);
    }

    IAsyncAction NullableColorPicker::MoreColors_Clicked(const IInspectable& /*sender*/, const RoutedEventArgs& /*args*/)
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
            // TODO CARLOS: evaluate the value of CurrentColor and set the color picker to that value
        }
    }

    void NullableColorPicker::ColorPickerDialog_PrimaryButtonClick(const IInspectable& /*sender*/, const ContentDialogButtonClickEventArgs& /*args*/)
    {
        const auto& selectedColor = ColorPickerControl().Color();
        const Microsoft::Terminal::Core::Color terminalColor{ selectedColor.R, selectedColor.G, selectedColor.B, selectedColor.A };
        CurrentColor(terminalColor);
    }
}
