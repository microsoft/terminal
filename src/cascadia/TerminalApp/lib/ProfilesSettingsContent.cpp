#include "pch.h"
#include "ProfilesSettingsContent.h"
#if __has_include("ProfilesSettingsContent.g.cpp")
#include "ProfilesSettingsContent.g.cpp"
#endif

using namespace winrt;
using namespace Windows::UI::Xaml;

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

    /*void ProfilesSettingsContent::ClickHandler(IInspectable const&, RoutedEventArgs const&)
    {
        Button().Content(box_value(L"Clicked"));
    }*/
    void ProfilesSettingsContent::Control1_Checked(const winrt::Windows::Foundation::IInspectable sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs)
    {
        Controls::CheckBox cb = sender.as<Controls::CheckBox>();

        //if (cb.Name == "cb1")
        //{
        //    /*text1.Text = "2 state CheckBox is checked.";*/
        //}
        //else
        //{
        //    /*text2.Text = "3 state CheckBox is checked.";*/
        //}
    }

    void ProfilesSettingsContent::Control1_Unchecked(const winrt::Windows::Foundation::IInspectable sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs)
    {
        Controls::CheckBox cb = sender.as<Controls::CheckBox>();

        //if (cb.Name == "cb1")
        //{
        //    /*text1.Text = "2 state CheckBox is checked.";*/
        //}
        //else
        //{
        //    /*text2.Text = "3 state CheckBox is checked.";*/
        //}
    }
}
