#include "pch.h"
#include "Keybindings.h"
#if __has_include("Keybindings.g.cpp")
#include "Keybindings.g.cpp"
#endif

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Input.h>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::System;

namespace winrt::TerminalSettings::implementation
{
    Keybindings::Keybindings()
    {
        InitializeComponent();

        Controls::TextBox tb = FindName(L"asdf").as<Controls::TextBox>();
        tb.KeyDown({ this, &Keybindings::KeyDown });

        // tb.BeforeTextChanging({this, &Keybindings::asdf_BeforeTextChanging})
    }

    int32_t Keybindings::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void Keybindings::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }

    void Keybindings::ClickHandler(IInspectable const&, RoutedEventArgs const&)
    {
        //Button().Content(box_value(L"Clicked"));
    }

    void Keybindings::Button_Click(IInspectable const& sender, RoutedEventArgs const& e)
    {
        Popup popup = FindName(L"StandardPopup").as<Popup>();

        if (!popup.IsOpen())
        {
            popup.IsOpen(true);
        }   
    }

    void Keybindings::Button_Click_1(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e)
    {
        Popup popup = FindName(L"StandardPopup").as<Popup>();

        if (popup.IsOpen())
        {
            popup.IsOpen(false);
        }
    }

    void Keybindings::HyperlinkButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e)
    {
        hstring setting = GetSelectedItemTag(FindName(L"CommandComboBox"));

        Controls::StackPanel panel = FindName(setting + L"OptionPanel").as<Controls::StackPanel>();
        panel.Visibility(Visibility::Visible);
    }

    // This can be used to populate a map<VirtualKey, hstring> to perform conversions from key lists to hstring and viceversa
    hstring Keybindings::KeyToString(VirtualKey key)
    {
        hstring generatedString;

        if (key >= VirtualKey::F1 && key <= VirtualKey::F24)
        {
            return L"F" + hstring(std::to_wstring((int)key - (int)VirtualKey::F1 + 1));
        }

        if (key >= VirtualKey::A && key <= VirtualKey::Z)
        {
            return hstring(std::wstring(1, (wchar_t)key));
        }

        if (key >= VirtualKey::Number0 && key <= VirtualKey::Number9)
        {
            return hstring(std::to_wstring((int)key - (int)VirtualKey::Number0));
        }

        if (key >= VirtualKey::NumberPad0 && key <= VirtualKey::NumberPad9)
        {
            return L"numpad_" + hstring(std::to_wstring((int)key - (int)VirtualKey::NumberPad0));
        }

        switch (key)
        {
        case VirtualKey::Control:
            return L"ctrl+";
        case VirtualKey::Shift:
            return L"shift+";
        case VirtualKey::Menu:
            return L"alt+";
        case VirtualKey::Add:
            return L"plus";
        case VirtualKey::Subtract:
            return L"-";
        case VirtualKey::Divide:
            return L"/";
        case VirtualKey::Decimal:
            return L".";
        case VirtualKey::Left:
            return L"left";
        case VirtualKey::Down:
            return L"down";
        case VirtualKey::Right:
            return L"right";
        case VirtualKey::Up:
            return L"up";
        case VirtualKey::PageDown:
            return L"pagedown";
        case VirtualKey::PageUp:
            return L"pageup";
        case VirtualKey::End:
            return L"end";
        case VirtualKey::Home:
            return L"home";
        case VirtualKey::Tab:
            return L"tab";
        case VirtualKey::Enter:
            return L"enter";
        case VirtualKey::Escape:
            return L"esc";
        case VirtualKey::Space:
            return L"space";
        case VirtualKey::Back:
            return L"backspace";
        case VirtualKey::Delete:
            return L"delete";
        case VirtualKey::Insert:
            return L"insert";
        default:
            return L"";
        }

        return L"";
    }

    hstring Keybindings::GetKeyListString()
    {
        hstring generatedString = L"";
        boolean lastKeyWasModifier{};

        if (m_keysInBind.find(VirtualKey::Control) != m_keysInBind.end())
        {
            generatedString = generatedString + KeyToString(VirtualKey::Control);
            lastKeyWasModifier = true;
        }

        if (m_keysInBind.find(VirtualKey::Shift) != m_keysInBind.end())
        {
            generatedString = generatedString + KeyToString(VirtualKey::Shift);
            lastKeyWasModifier = true;
        }

        if (m_keysInBind.find(VirtualKey::Menu) != m_keysInBind.end())
        {
            generatedString = generatedString + KeyToString(VirtualKey::Menu);
            lastKeyWasModifier = true;
        }

        for (const auto& key : m_keysInBind)
        {
            if (key != VirtualKey::Control && key != VirtualKey::Shift && key != VirtualKey::Menu)
            {
                hstring keyString = KeyToString(key);

                if (!keyString.empty())
                {
                    if (!generatedString.empty() && !lastKeyWasModifier)
                    {
                        generatedString = generatedString + L"+";
                    }
                    generatedString = generatedString + KeyToString(key);
                    lastKeyWasModifier = false;
                }
            }
        }

        return generatedString;
    }

    void Keybindings::KeyDown(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e)
    {
        Controls::TextBox textBox = sender.as<Controls::TextBox>();

        if (e.Key() == VirtualKey::Back)
        {
            m_keysInBind.clear();
        }
        else
        {
            m_keysInBind.insert(e.Key());
            if (textBox != nullptr)
            {
                textBox.Text(GetKeyListString());
            }
        }

        e.Handled(true);
    }

    void Keybindings::asdf_TextChanging(winrt::Windows::UI::Xaml::Controls::TextBox const& sender, winrt::Windows::UI::Xaml::Controls::TextBoxTextChangingEventArgs const& args)
    {
        hstring currText = sender.Text();
        hstring newText = hstring(currText.data(), currText.size());

        sender.Text(newText);
    }

    void Keybindings::ShowOptionsButtonIfRequired(hstring tag)
    {
        std::set<hstring> settingsWithOptions;
        settingsWithOptions.insert(c_openSettingsTag);
        settingsWithOptions.insert(c_newTabTag);
        settingsWithOptions.insert(c_switchToTabTag);
        settingsWithOptions.insert(c_renameTabTag);
        settingsWithOptions.insert(c_setTabColorTag);
        settingsWithOptions.insert(c_moveFocusTag);
        settingsWithOptions.insert(c_resizePaneTag);
        settingsWithOptions.insert(c_splitPaneTag);
        settingsWithOptions.insert(c_copyTag);

        Controls::StackPanel optionalSettingsPanel = FindName(L"OptionalSettingsPanel").as<Controls::StackPanel>();
        Controls::HyperlinkButton addNewButton = FindName(L"AddNewLink").as<Controls::HyperlinkButton>();
        Windows::UI::Xaml::Visibility expectedVisibility = Visibility::Collapsed;
        if (settingsWithOptions.find(tag) != settingsWithOptions.end())
        {
            expectedVisibility = Visibility::Visible;
        }
        optionalSettingsPanel.Visibility(expectedVisibility);
        addNewButton.Visibility(expectedVisibility);
    }

    void Keybindings::CommandComboBox_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::Controls::SelectionChangedEventArgs const& e)
    {
        hstring selectedItemTag = GetSelectedItemTag(sender);
        ShowOptionsButtonIfRequired(selectedItemTag);
    }

    hstring Keybindings::GetSelectedItemTag(winrt::Windows::Foundation::IInspectable const& comboBoxAsInspectable)
    {
        Controls::ComboBox comboBox = comboBoxAsInspectable.as<Controls::ComboBox>();
        Controls::ComboBoxItem selectedOption = comboBox.SelectedItem().as<Controls::ComboBoxItem>();

        return unbox_value<hstring>(selectedOption.Tag());
    }

}
