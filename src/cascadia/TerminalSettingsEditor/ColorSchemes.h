// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ColorSchemes.g.h"
#include "ObjectModel/ColorSchemeModel.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct ColorSchemes : ColorSchemesT<ColorSchemes>
    {
        ColorSchemes();
        Model::ColorSchemeModel ColorSchemeModel();

        void Background_ColorChanged(Windows::UI::Xaml::Controls::ColorPicker const&, Windows::UI::Xaml::Controls::ColorChangedEventArgs const&);
        void Foreground_ColorChanged(Windows::UI::Xaml::Controls::ColorPicker const&, Windows::UI::Xaml::Controls::ColorChangedEventArgs const&);
        void Black_ColorChanged(Windows::UI::Xaml::Controls::ColorPicker const&, Windows::UI::Xaml::Controls::ColorChangedEventArgs const&);
        void BrightBlack_ColorChanged(Windows::UI::Xaml::Controls::ColorPicker const&, Windows::UI::Xaml::Controls::ColorChangedEventArgs const&);
        void Blue_ColorChanged(Windows::UI::Xaml::Controls::ColorPicker const&, Windows::UI::Xaml::Controls::ColorChangedEventArgs const&);
        void BrightBlue_ColorChanged(Windows::UI::Xaml::Controls::ColorPicker const&, Windows::UI::Xaml::Controls::ColorChangedEventArgs const&);
        void Cyan_ColorChanged(Windows::UI::Xaml::Controls::ColorPicker const&, Windows::UI::Xaml::Controls::ColorChangedEventArgs const&);
        void BrightCyan_ColorChanged(Windows::UI::Xaml::Controls::ColorPicker const&, Windows::UI::Xaml::Controls::ColorChangedEventArgs const&);
        void Green_ColorChanged(Windows::UI::Xaml::Controls::ColorPicker const&, Windows::UI::Xaml::Controls::ColorChangedEventArgs const&);
        void BrightGreen_ColorChanged(Windows::UI::Xaml::Controls::ColorPicker const&, Windows::UI::Xaml::Controls::ColorChangedEventArgs const&);
        void Purple_ColorChanged(Windows::UI::Xaml::Controls::ColorPicker const&, Windows::UI::Xaml::Controls::ColorChangedEventArgs const&);
        void BrightPurple_ColorChanged(Windows::UI::Xaml::Controls::ColorPicker const&, Windows::UI::Xaml::Controls::ColorChangedEventArgs const&);
        void Red_ColorChanged(Windows::UI::Xaml::Controls::ColorPicker const&, Windows::UI::Xaml::Controls::ColorChangedEventArgs const&);
        void BrightRed_ColorChanged(Windows::UI::Xaml::Controls::ColorPicker const&, Windows::UI::Xaml::Controls::ColorChangedEventArgs const&);
        void White_ColorChanged(Windows::UI::Xaml::Controls::ColorPicker const&, Windows::UI::Xaml::Controls::ColorChangedEventArgs const&);
        void BrightWhite_ColorChanged(Windows::UI::Xaml::Controls::ColorPicker const&, Windows::UI::Xaml::Controls::ColorChangedEventArgs const&);
        void Yellow_ColorChanged(Windows::UI::Xaml::Controls::ColorPicker const&, Windows::UI::Xaml::Controls::ColorChangedEventArgs const&);
        void BrightYellow_ColorChanged(Windows::UI::Xaml::Controls::ColorPicker const&, Windows::UI::Xaml::Controls::ColorChangedEventArgs const&);

    private:
        Model::ColorSchemeModel m_colorSchemeModel{ nullptr };
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(ColorSchemes);
}
