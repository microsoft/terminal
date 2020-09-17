// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Keybindings.h"
#include "Keybindings.g.cpp"

#include "Utils.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Input;
using namespace winrt::Windows::UI::Xaml::Controls::Primitives;
using namespace winrt::Windows::System;
using namespace winrt::Microsoft::Terminal::Settings;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Keybindings::Keybindings()
    {
        InitializeComponent();

        m_optionalSettingsPanel = FindName(L"OptionalSettingsPanel").as<Controls::StackPanel>();
        m_addNewButton = FindName(L"AddNewLink").as<Controls::HyperlinkButton>();

        Controls::TextBox tb = FindName(L"KeyBindTextBox").as<Controls::TextBox>();
        tb.KeyDown({ this, &Keybindings::KeyDown });
    }

    void Keybindings::Button_Click(IInspectable const& /*sender*/, RoutedEventArgs const& /*e*/)
    {
        Popup popup = FindName(L"StandardPopup").as<Popup>();

        if (!popup.IsOpen())
        {
            popup.IsOpen(true);
        }
    }

    void Keybindings::AddNewButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
        hstring setting = GetSelectedItemTag(FindName(L"CommandComboBox"));

        Controls::StackPanel panel{};

        if (setting == c_moveFocusTag || setting == c_resizePaneTag)
        {
            panel = FindName(L"moveResizeFocusOptionPanel").as<Controls::StackPanel>();
            panel.Visibility(Visibility::Visible);
        }
        else
        {
            panel = FindName(setting + L"OptionPanel").as<Controls::StackPanel>();
            bool panelWasVisible = (panel.Visibility() == Visibility::Visible);
            panel.Visibility(Visibility::Visible);
            m_lastOpenedArgsPanel = panel;

            Controls::HyperlinkButton button = sender.as<Controls::HyperlinkButton>();
            if (setting == c_splitPaneTag)
            {
                if (panelWasVisible)
                {
                    panel.Children().Append(SplitPaneOptionPanelControl());
                }
                button.Visibility(Visibility::Visible);
            }
            else if (setting == c_newTabTag)
            {
                panel.Children().Append(NewTabOptionPanelControl());
                button.Visibility(Visibility::Visible);
            }
            else
            {
                button.Visibility(Visibility::Collapsed);
            }
        }

        m_lastOpenedArgsPanel = panel;
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

    void Keybindings::KeyBindTextBox_TextChanging(winrt::Windows::UI::Xaml::Controls::TextBox const& sender, winrt::Windows::UI::Xaml::Controls::TextBoxTextChangingEventArgs const& /*args*/)
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

        Windows::UI::Xaml::Visibility expectedVisibility = Visibility::Collapsed;
        if (settingsWithOptions.find(tag) != settingsWithOptions.end())
        {
            expectedVisibility = Visibility::Visible;
        }
        m_optionalSettingsPanel.Visibility(expectedVisibility);
        m_addNewButton.Visibility(expectedVisibility);
        if (m_lastOpenedArgsPanel != nullptr)
        {
            m_lastOpenedArgsPanel.Visibility(Visibility::Collapsed);
        }
    }

    void Keybindings::CommandComboBox_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::Controls::SelectionChangedEventArgs const& /*e*/)
    {
        hstring selectedItemTag = GetSelectedItemTag(sender);
        ShowOptionsButtonIfRequired(selectedItemTag);
    }

    void Keybindings::SaveButton_Click(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Windows::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
        // Collect the information here
        winrt::Windows::UI::Popups::MessageDialog msg{ CollectInputData() };
        msg.ShowAsync();
    }

    hstring Keybindings::TraversePanel(const Controls::Panel& panel)
    {
        hstring fullInfo;

        for (const auto panelChild : panel.Children())
        {
            if (Controls::ComboBox childComboBox = panelChild.try_as<Controls::ComboBox>())
            {
                fullInfo = fullInfo + childComboBox.Name() + L":" + GetSelectedItemTag(childComboBox);
            }
            else if (Controls::TextBox childTextBox = panelChild.try_as<Controls::TextBox>())
            {
                fullInfo = fullInfo + childTextBox.Name() + L":" + childTextBox.Text();
            }
            else if (Editor::NewTabOptionPanelControl ntOptionPanel = panelChild.try_as<Editor::NewTabOptionPanelControl>())
            {
                fullInfo = fullInfo + ntOptionPanel.Argument() + L":" + ntOptionPanel.InputValue();
            }
            else if (Editor::SplitPaneOptionPanelControl spOptionPanel = panelChild.try_as<Editor::SplitPaneOptionPanelControl>())
            {
                fullInfo = fullInfo + spOptionPanel.Argument() + L":" + spOptionPanel.InputValue();
            }
            else if (Controls::Grid grid = panelChild.try_as<Controls::Grid>())
            {
                fullInfo = fullInfo + TraversePanel(grid);
            }
            fullInfo = fullInfo + L"\n";
        }

        return fullInfo;
    }

    hstring Keybindings::CollectInputData()
    {
        hstring fullInfo;

        Controls::ComboBox comboBox = FindName(L"CommandComboBox").as<Controls::ComboBox>();
        fullInfo = fullInfo + comboBox.Name() + L":" + GetSelectedItemTag(comboBox) + L"\n";

        Controls::TextBox textBox = FindName(L"KeyBindTextBox").as<Controls::TextBox>();
        fullInfo = fullInfo + textBox.Name() + L":" + textBox.Text() + L"\n";

        fullInfo = fullInfo + TraversePanel(m_lastOpenedArgsPanel);

        return fullInfo;
    }
}
