// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "NullableColorPicker.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct NullableColorPicker : public HasScrollViewer<NullableColorPicker>, NullableColorPickerT<NullableColorPicker>
    {
    public:
        NullableColorPicker();

        void ColorChip_Clicked(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& args);
        void NullColorButton_Clicked(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& args);
        Windows::Foundation::IAsyncAction MoreColors_Clicked(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& args);

        void ColorPickerDialog_Opened(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::Controls::ContentDialogOpenedEventArgs& args);
        void ColorPickerDialog_PrimaryButtonClick(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::Controls::ContentDialogButtonClickEventArgs& args);

        DEPENDENCY_PROPERTY(Editor::ColorSchemeViewModel, ColorSchemeVM);
        DEPENDENCY_PROPERTY(Windows::Foundation::IReference<Microsoft::Terminal::Core::Color>, CurrentColor);
        DEPENDENCY_PROPERTY(bool, ShowNullColorButton);
        DEPENDENCY_PROPERTY(hstring, NullColorButtonLabel);
        DEPENDENCY_PROPERTY(Windows::UI::Color, NullColorPreview);

    private:
        static void _InitializeProperties();
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(NullableColorPicker);
}
