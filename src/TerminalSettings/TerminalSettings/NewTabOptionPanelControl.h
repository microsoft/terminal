//
// MyUserControl.xaml.h
// Declaration of the MyUserControl class
//

#pragma once

#include "NewTabOptionPanelControl.g.h"

namespace winrt::SettingsControl::implementation
{
    struct NewTabOptionPanelControl : NewTabOptionPanelControlT<NewTabOptionPanelControl>
    {
        NewTabOptionPanelControl();

        int32_t MyProperty();
        void MyProperty(int32_t value);

        hstring Argument();
        hstring InputValue();

        private:
        const hstring c_argumentComboBoxName = L"newTabArgumentComboBox";
        const hstring c_textBoxName = L"newTabTextBox";

        winrt::Windows::UI::Xaml::Controls::ComboBox argumentComboBox;
        winrt::Windows::UI::Xaml::Controls::TextBox argumentInputTextBox;
    };
}

namespace winrt::SettingsControl::factory_implementation
{
    struct NewTabOptionPanelControl : NewTabOptionPanelControlT<NewTabOptionPanelControl, implementation::NewTabOptionPanelControl>
    {
    };
}
