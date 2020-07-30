#include "pch.h"
#include "Profiles.h"
#include "Profiles.g.cpp"

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::SettingsControl::implementation
{
    Profiles::Profiles()
    {
        m_profileModel = winrt::make<ObjectModel::implementation::ProfileModel>();
        InitializeComponent();
    }

    Profiles::Profiles(ObjectModel::ProfileModel profile) :
        m_profileModel{ profile }
    {
        InitializeComponent();
    }

    ObjectModel::ProfileModel Profiles::ProfileModel()
    {
        return m_profileModel;
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
