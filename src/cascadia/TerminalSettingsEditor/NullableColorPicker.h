// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "NullableColorPicker.g.h"
#include "NullableColorTemplateSelector.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct NullableColorPicker : public HasScrollViewer<NullableColorPicker>, NullableColorPickerT<NullableColorPicker>
    {
    public:
        NullableColorPicker();

        static winrt::Windows::UI::Xaml::Media::SolidColorBrush CalculateBorderBrush(const Windows::UI::Color& color);
        static bool IsNull(Windows::Foundation::IReference<Microsoft::Terminal::Core::Color> color);
        static Windows::UI::Xaml::Visibility IsNullToVisibility(Windows::Foundation::IReference<Windows::UI::Color> color);

        void ColorChip_Loaded(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& args);
        void ColorChip_Unloaded(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& args);
        void ColorChip_Clicked(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& args);
        void ColorChip_DataContextChanged(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::DataContextChangedEventArgs& args);

        void NullColorButton_Clicked(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& args);
        safe_void_coroutine MoreColors_Clicked(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& args);

        void ColorPickerDialog_Opened(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::Controls::ContentDialogOpenedEventArgs& args);
        void ColorPickerDialog_PrimaryButtonClick(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::Controls::ContentDialogButtonClickEventArgs& args);

        DEPENDENCY_PROPERTY(Editor::ColorSchemeViewModel, ColorSchemeVM);
        DEPENDENCY_PROPERTY(Windows::Foundation::IReference<Microsoft::Terminal::Core::Color>, CurrentColor);
        DEPENDENCY_PROPERTY(bool, ShowNullColorButton);
        DEPENDENCY_PROPERTY(hstring, NullColorButtonLabel);
        DEPENDENCY_PROPERTY(Windows::Foundation::IReference<Windows::UI::Color>, NullColorPreview);

    private:
        static void _InitializeProperties();
        static void _OnCurrentColorValueChanged(const Windows::UI::Xaml::DependencyObject& d, const Windows::UI::Xaml::DependencyPropertyChangedEventArgs& e);

        void _UpdateColorChips();

        std::vector<Windows::UI::Xaml::Controls::Primitives::ToggleButton> _colorChips;
    };

    struct NullableColorTemplateSelector : NullableColorTemplateSelectorT<NullableColorTemplateSelector>
    {
        NullableColorTemplateSelector() = default;

        Windows::UI::Xaml::DataTemplate SelectTemplateCore(const winrt::Windows::Foundation::IInspectable&, const winrt::Windows::UI::Xaml::DependencyObject&);
        Windows::UI::Xaml::DataTemplate SelectTemplateCore(const winrt::Windows::Foundation::IInspectable&);

        WINRT_PROPERTY(winrt::Windows::UI::Xaml::DataTemplate, ColorTemplate);
        WINRT_PROPERTY(winrt::Windows::UI::Xaml::DataTemplate, NullColorTemplate);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(NullableColorPicker);
    BASIC_FACTORY(NullableColorTemplateSelector);
}
