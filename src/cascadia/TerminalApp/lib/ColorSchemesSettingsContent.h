#pragma once

#include "ColorSchemesSettingsContent.g.h"

namespace winrt::TerminalApp::implementation
{
    struct ColorSchemesSettingsContent : ColorSchemesSettingsContentT<ColorSchemesSettingsContent>
    {
        ColorSchemesSettingsContent();

        int32_t MyProperty();
        void MyProperty(int32_t value);

        void TextBox_TextChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::Controls::TextChangedEventArgs const& e);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct ColorSchemesSettingsContent : ColorSchemesSettingsContentT<ColorSchemesSettingsContent, implementation::ColorSchemesSettingsContent>
    {
    };
}
