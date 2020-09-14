// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "SplitPaneOptionPanelControl.h"
#include "SplitPaneOptionPanelControl.g.cpp"

#include "Utils.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Controls::Primitives;
using namespace winrt::Windows::UI::Xaml::Data;
using namespace winrt::Windows::UI::Xaml::Input;
using namespace winrt::Windows::UI::Xaml::Media;
using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Microsoft::Terminal::Settings;

// The User Control item template is documented at https://go.microsoft.com/fwlink/?LinkId=234236

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    SplitPaneOptionPanelControl::SplitPaneOptionPanelControl()
    {
        InitializeComponent();

        argumentComboBox = FindName(c_argumentComboBoxName).as<Controls::ComboBox>();
        argumentInputTextBox = FindName(c_textBoxName).as<Controls::TextBox>();
        splitModeComboBox = FindName(c_comboBoxName).as<Controls::ComboBox>();
    }

    void SplitPaneOptionPanelControl::ComboBox_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::Controls::SelectionChangedEventArgs const& /*e*/)
    {
        bool isSplitMode = (GetSelectedItemTag(sender) == L"splitMode");
        splitModeComboBox.Visibility(isSplitMode ? Visibility::Visible : Visibility::Collapsed);
        argumentInputTextBox.Visibility(isSplitMode ? Visibility::Collapsed : Visibility::Visible);
    }

    hstring SplitPaneOptionPanelControl::Argument()
    {
        return GetSelectedItemTag(argumentComboBox);
    }

    hstring SplitPaneOptionPanelControl::InputValue()
    {
        if (Argument() == L"splitMode")
        {
            return GetSelectedItemTag(splitModeComboBox);
        }
        else
        {
            return argumentInputTextBox.Text();
        }
    }
}
