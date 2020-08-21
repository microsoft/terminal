//
// MyUserControl.xaml.h
// Declaration of the MyUserControl class
//

#pragma once

#include "SplitPaneOptionPanelControl.g.h"

namespace winrt::SettingsControl::implementation
{
    struct SplitPaneOptionPanelControl : SplitPaneOptionPanelControlT<SplitPaneOptionPanelControl>
    {
        SplitPaneOptionPanelControl();

        int32_t MyProperty();
        void MyProperty(int32_t value);
        void ComboBox_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::Controls::SelectionChangedEventArgs const& e);

        hstring Argument();
        hstring InputValue();

        private:
        const hstring c_argumentComboBoxName = L"argumentComboBox";
        const hstring c_textBoxName = L"splitPaneTextBox";
        const hstring c_comboBoxName = L"splitPaneComboBox";

        winrt::Windows::UI::Xaml::Controls::ComboBox argumentComboBox;
        winrt::Windows::UI::Xaml::Controls::TextBox argumentInputTextBox;
        winrt::Windows::UI::Xaml::Controls::ComboBox splitModeComboBox;
    };
}

namespace winrt::SettingsControl::factory_implementation
{
    struct SplitPaneOptionPanelControl : SplitPaneOptionPanelControlT<SplitPaneOptionPanelControl, implementation::SplitPaneOptionPanelControl>
    {
    };
}
