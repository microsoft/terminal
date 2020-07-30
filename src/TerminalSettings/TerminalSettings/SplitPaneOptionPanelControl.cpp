//
// MyUserControl.cpp
// Implementation of the MyUserControl class
//

#include "pch.h"
#include "SplitPaneOptionPanelControl.h"
#if __has_include("SplitPaneOptionPanelControl.g.cpp")
#include "SplitPaneOptionPanelControl.g.cpp"
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

namespace winrt::SettingsControl::implementation
{
    SplitPaneOptionPanelControl::SplitPaneOptionPanelControl()
    {
        InitializeComponent();

        argumentComboBox = FindName(c_argumentComboBoxName).as<Controls::ComboBox>();
        argumentInputTextBox = FindName(c_textBoxName).as<Controls::TextBox>();
        splitModeComboBox = FindName(c_comboBoxName).as<Controls::ComboBox>();
    }

    int32_t SplitPaneOptionPanelControl::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void SplitPaneOptionPanelControl::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }

    void SplitPaneOptionPanelControl::ComboBox_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::Controls::SelectionChangedEventArgs const& e)
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
