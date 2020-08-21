// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "NewTabOptionPanelControl.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct NewTabOptionPanelControl : NewTabOptionPanelControlT<NewTabOptionPanelControl>
    {
        NewTabOptionPanelControl();

        hstring Argument();
        hstring InputValue();

    private:
        const hstring c_argumentComboBoxName = L"newTabArgumentComboBox";
        const hstring c_textBoxName = L"newTabTextBox";

        winrt::Windows::UI::Xaml::Controls::ComboBox argumentComboBox;
        winrt::Windows::UI::Xaml::Controls::TextBox argumentInputTextBox;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(NewTabOptionPanelControl);
}
