#include "pch.h"
#include "Profiles.h"
#include "Profiles.g.cpp"

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::SettingsControl::implementation
{
    Profiles::Profiles()
    {
        InitializeComponent();
    }

    Profiles::Profiles(winrt::hstring const& name) : m_name{ name }
    {

    }

    winrt::hstring Profiles::Name()
    {
        return m_name;
    }

    void Profiles::Name(winrt::hstring const& value)
    {
        if (m_name != value)
        {

        }
    }

    int32_t Profiles::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void Profiles::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }

    void Profiles::ClickHandler(IInspectable const&, RoutedEventArgs const&)
    {

    }

    void Profiles::cursorColorPickerConfirmColor_Click(IInspectable const&, RoutedEventArgs const&)
    {
        //cursorColorPickerButton.Flyout.Hide();
    }

    void Profiles::cursorColorPickerCancelColor_Click(IInspectable const&, RoutedEventArgs const&)
    {
        //cursorColorPickerButton.Flyout.Hide();
    }

    void Profiles::foregroundColorPickerConfirmColor_Click(IInspectable const&, RoutedEventArgs const&)
    {
        //foregroundColorPickerButton.Flyout.Hide();
    }

    void Profiles::foregroundColorPickerCancelColor_Click(IInspectable const&, RoutedEventArgs const&)
    {
        //foregroundColorPickerButton.Flyout.Hide();
    }

    void Profiles::backgroundColorPickerConfirmColor_Click(IInspectable const&, RoutedEventArgs const&)
    {
        //backgroundColorPickerButton.Flyout.Hide();
    }

    void Profiles::backgroundColorPickerCancelColor_Click(IInspectable const&, RoutedEventArgs const&)
    {
        //backgroundColorPickerButton.Flyout.Hide();
    }

    void Profiles::selectionBackgroundColorPickerConfirmColor_Click(IInspectable const&, RoutedEventArgs const&)
    {
        //selectionBackgroundColorPickerButton.Flyout.Hide();
    }

    void Profiles::selectionBackgroundColorPickerCancelColor_Click(IInspectable const&, RoutedEventArgs const&)
    {
        //selectionBackgroundColorPickerButton.Flyout.Hide();
    }
}
