// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Keybindings.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct Keybindings : KeybindingsT<Keybindings>
    {
        Keybindings();

        void Button_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void AddNewButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void KeyDown(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);
        void KeyBindTextBox_TextChanging(winrt::Windows::UI::Xaml::Controls::TextBox const& sender, winrt::Windows::UI::Xaml::Controls::TextBoxTextChangingEventArgs const& args);
        void CommandComboBox_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::Controls::SelectionChangedEventArgs const& e);
        void SaveButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);

    private:
        hstring GetKeyListString();
        void ShowOptionsButtonIfRequired(hstring tag);
        hstring CollectInputData();
        hstring TraversePanel(const winrt::Windows::UI::Xaml::Controls::Panel& panel);

        const hstring c_openSettingsTag = L"openSettings";
        const hstring c_newTabTag = L"newTab";
        const hstring c_switchToTabTag = L"switchToTab";
        const hstring c_renameTabTag = L"renameTab";
        const hstring c_setTabColorTag = L"setTabColor";
        const hstring c_moveFocusTag = L"moveFocus";
        const hstring c_resizePaneTag = L"resizePane";
        const hstring c_splitPaneTag = L"splitPane";
        const hstring c_copyTag = L"copy";

        winrt::Windows::UI::Xaml::Controls::StackPanel m_lastOpenedArgsPanel{};
        winrt::Windows::UI::Xaml::Controls::StackPanel m_optionalSettingsPanel{};
        winrt::Windows::UI::Xaml::Controls::HyperlinkButton m_addNewButton{};

        std::set<winrt::Windows::System::VirtualKey> m_keysInBind;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Keybindings);
}
