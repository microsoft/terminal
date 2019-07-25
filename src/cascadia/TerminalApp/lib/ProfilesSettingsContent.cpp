#include "pch.h"
#include "ProfilesSettingsContent.h"
#if __has_include("ProfilesSettingsContent.g.cpp")
#include "ProfilesSettingsContent.g.cpp"
#endif
#include "Profile.h"

using namespace winrt;
using namespace Windows::UI::Xaml;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::TerminalApp;

namespace winrt::TerminalApp::implementation
{
    ProfilesSettingsContent::ProfilesSettingsContent()
    {
        InitializeComponent();
    }

    int32_t ProfilesSettingsContent::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void ProfilesSettingsContent::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }

    void ProfilesSettingsContent::LoadProfiles(const winrt::Windows::Foundation::IInspectable sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs)
    {
        /*Controls::DropDownButton pc = sender.as<Controls::DropDownButton>();
        auto profiles = _settings->GetProfiles();
        auto profileName = profiles[0].GetName();
        winrt::hstring hName{ profileName };*/
    }

    void ProfilesSettingsContent::Control1_Checked(const winrt::Windows::Foundation::IInspectable sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs)
    {
        Controls::CheckBox cb = sender.as<Controls::CheckBox>();

    }

    void ProfilesSettingsContent::Control1_Unchecked(const winrt::Windows::Foundation::IInspectable sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs)
    {
        Controls::CheckBox cb = sender.as<Controls::CheckBox>();
    }
}
