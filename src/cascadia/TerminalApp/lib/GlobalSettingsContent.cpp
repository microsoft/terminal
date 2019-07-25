#include "pch.h"
#include "GlobalSettingsContent.h"
#if __has_include("GlobalSettingsContent.g.cpp")
#include "GlobalSettingsContent.g.cpp"
#endif

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::TerminalApp::implementation
{
    GlobalSettingsContent::GlobalSettingsContent()
    {
        InitializeComponent();
    }

    int32_t GlobalSettingsContent::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void GlobalSettingsContent::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }

    void GlobalSettingsContent::Control1_Checked(const winrt::Windows::Foundation::IInspectable sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs)
    {
        Controls::CheckBox cb = sender.as<Controls::CheckBox>();
    }

    void GlobalSettingsContent::Control1_Unchecked(const winrt::Windows::Foundation::IInspectable sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs)
    {
        Controls::CheckBox cb = sender.as<Controls::CheckBox>();
    }
}
