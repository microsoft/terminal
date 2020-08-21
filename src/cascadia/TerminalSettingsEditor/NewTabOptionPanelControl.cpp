// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "NewTabOptionPanelControl.h"
#if __has_include("NewTabOptionPanelControl.g.cpp")
#include "NewTabOptionPanelControl.g.cpp"
#endif

#include <winrt/Windows.Foundation.h>
#include "Utils.h"

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;

// The User Control item template is documented at https://go.microsoft.com/fwlink/?LinkId=234236

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    NewTabOptionPanelControl::NewTabOptionPanelControl()
    {
        InitializeComponent();

        argumentComboBox = FindName(c_argumentComboBoxName).as<Controls::ComboBox>();
        argumentInputTextBox = FindName(c_textBoxName).as<Controls::TextBox>();
    }

    hstring NewTabOptionPanelControl::Argument()
    {
        return GetSelectedItemTag(argumentComboBox);
    }

    hstring NewTabOptionPanelControl::InputValue()
    {
        return argumentInputTextBox.Text();
    }
}
