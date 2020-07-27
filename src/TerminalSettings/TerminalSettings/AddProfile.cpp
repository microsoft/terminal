#include "pch.h"
#include "AddProfile.h"
#include "AddProfile.g.cpp"

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::SettingsControl::implementation
{
    AddProfile::AddProfile()
    {
        InitializeComponent();
    }

    AddProfile::AddProfile(winrt::hstring const& name) :
        m_name{ name }
    {
    }

    winrt::hstring AddProfile::Name()
    {
        return m_name;
    }

    void AddProfile::Name(winrt::hstring const& value)
    {
        if (m_name != value)
        {
        }
    }

    int32_t AddProfile::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void AddProfile::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }

    void AddProfile::ClickHandler(IInspectable const&, RoutedEventArgs const&)
    {
    }

    void AddProfile::cursorColorPickerConfirmColor_Click(IInspectable const&, RoutedEventArgs const&)
    {
        //cursorColorPickerButton.Flyout.Hide();
    }

    void AddProfile::cursorColorPickerCancelColor_Click(IInspectable const&, RoutedEventArgs const&)
    {
        //cursorColorPickerButton.Flyout.Hide();
    }

    void AddProfile::foregroundColorPickerConfirmColor_Click(IInspectable const&, RoutedEventArgs const&)
    {
        //foregroundColorPickerButton.Flyout.Hide();
    }

    void AddProfile::foregroundColorPickerCancelColor_Click(IInspectable const&, RoutedEventArgs const&)
    {
        //foregroundColorPickerButton.Flyout.Hide();
    }

    void AddProfile::backgroundColorPickerConfirmColor_Click(IInspectable const&, RoutedEventArgs const&)
    {
        //backgroundColorPickerButton.Flyout.Hide();
    }

    void AddProfile::backgroundColorPickerCancelColor_Click(IInspectable const&, RoutedEventArgs const&)
    {
        //backgroundColorPickerButton.Flyout.Hide();
    }

    void AddProfile::selectionBackgroundColorPickerConfirmColor_Click(IInspectable const&, RoutedEventArgs const&)
    {
        //selectionBackgroundColorPickerButton.Flyout.Hide();
    }

    void AddProfile::selectionBackgroundColorPickerCancelColor_Click(IInspectable const&, RoutedEventArgs const&)
    {
        //selectionBackgroundColorPickerButton.Flyout.Hide();
    }
}
