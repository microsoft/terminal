// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "SplitPaneOptionPanelControl.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct SplitPaneOptionPanelControl : SplitPaneOptionPanelControlT<SplitPaneOptionPanelControl>
    {
        SplitPaneOptionPanelControl();

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

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(SplitPaneOptionPanelControl);
}
