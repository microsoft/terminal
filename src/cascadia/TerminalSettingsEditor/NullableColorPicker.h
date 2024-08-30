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
        void MoreColors_Clicked(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& args);

        DEPENDENCY_PROPERTY(Editor::ColorSchemeViewModel, ColorSchemeVM);
        DEPENDENCY_PROPERTY(Windows::Foundation::IReference<Microsoft::Terminal::Core::Color>, CurrentColor);

    private:
        static void _InitializeProperties();
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(NullableColorPicker);
}
