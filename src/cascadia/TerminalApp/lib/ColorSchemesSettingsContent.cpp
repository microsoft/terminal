#include "pch.h"
#include "ColorSchemesSettingsContent.h"
#if __has_include("ColorSchemesSettingsContent.g.cpp")
#include "ColorSchemesSettingsContent.g.cpp"
#endif

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::TerminalApp::implementation
{
    ColorSchemesSettingsContent::ColorSchemesSettingsContent()
    {
        InitializeComponent();
    }

    int32_t ColorSchemesSettingsContent::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void ColorSchemesSettingsContent::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }
}

void winrt::TerminalApp::implementation::ColorSchemesSettingsContent::TextBox_TextChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::Controls::TextChangedEventArgs const& e)
{
}
