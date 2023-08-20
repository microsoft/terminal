// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "KeyChordListener.h"
#include "KeyChordListener.g.cpp"
#include "LibraryResources.h"

using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Data;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::System;
using namespace winrt::Windows::UI::Xaml::Input;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    DependencyProperty KeyChordListener::_KeysProperty{ nullptr };

    // The ModifierKeys have been sorted by value.
    // Not just binary search, but also your CPU likes sorted data.
    static constexpr std::array ModifierKeys{
        VirtualKey::Shift,
        VirtualKey::Control,
        VirtualKey::Menu,
        VirtualKey::LeftWindows,
        VirtualKey::RightWindows,
        VirtualKey::LeftShift,
        VirtualKey::LeftControl,
        VirtualKey::RightControl,
        VirtualKey::LeftMenu,
        VirtualKey::RightMenu
    };

    static VirtualKeyModifiers _GetModifiers()
    {
        const auto window{ CoreWindow::GetForCurrentThread() };

        auto flags = VirtualKeyModifiers::None;
        for (const auto mod : ModifierKeys)
        {
            const auto state = window.GetKeyState(mod);
            const auto isDown = WI_IsFlagSet(state, CoreVirtualKeyStates::Down);

            if (isDown)
            {
                switch (mod)
                {
                case VirtualKey::Control:
                case VirtualKey::LeftControl:
                case VirtualKey::RightControl:
                    flags |= VirtualKeyModifiers::Control;
                    break;
                case VirtualKey::Menu:
                case VirtualKey::LeftMenu:
                case VirtualKey::RightMenu:
                    flags |= VirtualKeyModifiers::Menu;
                    break;
                case VirtualKey::Shift:
                case VirtualKey::LeftShift:
                    flags |= VirtualKeyModifiers::Shift;
                    break;
                case VirtualKey::LeftWindows:
                case VirtualKey::RightWindows:
                    flags |= VirtualKeyModifiers::Windows;
                    break;
                }
            }
        }
        return flags;
    }

    KeyChordListener::KeyChordListener()
    {
        InitializeComponent();
        _InitializeProperties();
    }

    void KeyChordListener::_InitializeProperties()
    {
        // Initialize any KeyChordListener dependency properties here.
        // This performs a lazy load on these properties, instead of
        // initializing them when the DLL loads.
        if (!_KeysProperty)
        {
            _KeysProperty =
                DependencyProperty::Register(
                    L"Keys",
                    xaml_typename<Control::KeyChord>(),
                    xaml_typename<Editor::KeyChordListener>(),
                    PropertyMetadata{ nullptr, PropertyChangedCallback{ &KeyChordListener::_OnKeysChanged } });
        }
    }

    void KeyChordListener::_OnKeysChanged(const DependencyObject& d, const DependencyPropertyChangedEventArgs& e)
    {
        if (auto control{ d.try_as<Editor::KeyChordListener>() })
        {
            auto controlImpl{ get_self<KeyChordListener>(control) };
            auto tb{ controlImpl->FindName(L"KeyChordTextBox").as<TextBox>() };
            tb.Text(Model::KeyChordSerialization::ToString(unbox_value<Control::KeyChord>(e.NewValue())));
            if (auto automationPeer{ Automation::Peers::FrameworkElementAutomationPeer::FromElement(tb) })
            {
                automationPeer.RaiseNotificationEvent(
                    Automation::Peers::AutomationNotificationKind::ActionCompleted,
                    Automation::Peers::AutomationNotificationProcessing::MostRecent,
                    tb.Text(),
                    L"KeyChordListenerText");
            }
        }
    }

    void KeyChordListener::KeyChordTextBox_KeyDown(const IInspectable& /*sender*/, const KeyRoutedEventArgs& e)
    {
        const auto key{ e.OriginalKey() };
        for (const auto mod : ModifierKeys)
        {
            if (key == mod)
            {
                // Ignore modifier keys
                return;
            }
        }

        const auto modifiers{ _GetModifiers() };
        if (key == VirtualKey::Tab && (modifiers == VirtualKeyModifiers::None || modifiers == VirtualKeyModifiers::Shift))
        {
            // [Shift]+[Tab] && [Tab] are needed for keyboard navigation
            return;
        }

        // Permitted key events are used to update _Keys
        Keys({ modifiers, static_cast<int32_t>(key), 0 });
        e.Handled(true);
    }
}
