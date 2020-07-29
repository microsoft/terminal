#pragma once

#include "Keybindings.g.h"
#include <set>

namespace winrt::SettingsControl::implementation
{
    struct Keybindings : KeybindingsT<Keybindings>
    {
        Keybindings();

        int32_t MyProperty();
        void MyProperty(int32_t value);

        void ClickHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
        void Button_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void Button_Click_1(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void HyperlinkButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void KeyDown(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);

        private:
        hstring KeyToString(winrt::Windows::System::VirtualKey key);
        hstring GetKeyListString();
        void ShowOptionsButtonIfRequired(hstring tag);
        hstring GetSelectedItemTag(winrt::Windows::Foundation::IInspectable const& comboBoxAsInspectable);

        const hstring c_openSettingsTag = L"openSettings";
        const hstring c_newTabTag = L"newTab";
        const hstring c_switchToTabTag = L"switchToTab";
        const hstring c_renameTabTag = L"renameTab";
        const hstring c_setTabColorTag = L"setTabColor";
        const hstring c_moveFocusTag = L"moveFocus";
        const hstring c_resizePaneTag = L"resizePane";
        const hstring c_splitPaneTag = L"splitPane";
        const hstring c_copyTag = L"copy";

        std::set<winrt::Windows::System::VirtualKey> m_keysInBind;

    public:
        void asdf_TextChanging(winrt::Windows::UI::Xaml::Controls::TextBox const& sender, winrt::Windows::UI::Xaml::Controls::TextBoxTextChangingEventArgs const& args);
        void CommandComboBox_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::Controls::SelectionChangedEventArgs const& e);
    };
}

namespace winrt::SettingsControl::factory_implementation
{
    struct Keybindings : KeybindingsT<Keybindings, implementation::Keybindings>
    {
    };
}
