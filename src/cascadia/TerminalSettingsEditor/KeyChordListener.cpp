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
    DependencyProperty KeyChordListener::_CurrentKeysProperty{ nullptr };

    static constexpr std::array<VirtualKey, 10> ModifierKeys{
        VirtualKey::Menu,
        VirtualKey::RightMenu,
        VirtualKey::LeftMenu,
        VirtualKey::Control,
        VirtualKey::RightControl,
        VirtualKey::LeftControl,
        VirtualKey::Shift,
        VirtualKey::LeftShift,
        VirtualKey::RightWindows,
        VirtualKey::LeftWindows
    };

    static VirtualKeyModifiers _GetModifiers()
    {
        const auto window{ CoreWindow::GetForCurrentThread() };

        VirtualKeyModifiers flags = VirtualKeyModifiers::None;
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
                {
                    flags |= VirtualKeyModifiers::Control;
                    break;
                }
                case VirtualKey::Menu:
                case VirtualKey::LeftMenu:
                case VirtualKey::RightMenu:
                {
                    flags |= VirtualKeyModifiers::Menu;
                    break;
                }
                case VirtualKey::Shift:
                case VirtualKey::LeftShift:
                {
                    flags |= VirtualKeyModifiers::Shift;
                    break;
                }
                case VirtualKey::LeftWindows:
                case VirtualKey::RightWindows:
                {
                    flags |= VirtualKeyModifiers::Windows;
                    break;
                }
                }
            }
        }
        return flags;
    }

    KeyChordListener::KeyChordListener()
    {
        InitializeComponent();
        _InitializeProperties();

        PropertyChanged([this](auto&&, const PropertyChangedEventArgs& args) {
            if (args.PropertyName() == L"ProposedKeys")
            {
                KeyChordTextBox().Text(Model::KeyChordSerialization::ToString(_ProposedKeys));
            }
        });
    }

    void KeyChordListener::_InitializeProperties()
    {
        // Initialize any KeyChordListener dependency properties here.
        // This performs a lazy load on these properties, instead of
        // initializing them when the DLL loads.
        if (!_CurrentKeysProperty)
        {
            _CurrentKeysProperty =
                DependencyProperty::Register(
                    L"CurrentKeys",
                    xaml_typename<Control::KeyChord>(),
                    xaml_typename<Editor::KeyChordListener>(),
                    PropertyMetadata{ nullptr, PropertyChangedCallback{ &KeyChordListener::_OnCurrentKeysChanged } });
        }
    }

    void KeyChordListener::_OnCurrentKeysChanged(DependencyObject const& d, DependencyPropertyChangedEventArgs const& e)
    {
        if (auto control{ d.try_as<Editor::KeyChordListener>() })
        {
            auto controlImpl{ get_self<KeyChordListener>(control) };
            controlImpl->ProposedKeys(unbox_value<Control::KeyChord>(e.NewValue()));
        }
    }

    void KeyChordListener::KeyChordTextBox_LosingFocus(IInspectable const& /*sender*/, LosingFocusEventArgs const& /*e*/)
    {
        // export our key chord into the attached view model
        SetValue(_CurrentKeysProperty, _ProposedKeys);
    }

    void KeyChordListener::KeyChordTextBox_KeyDown(IInspectable const& /*sender*/, KeyRoutedEventArgs const& e)
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

        // Permitted key events are used to update _ProposedKeys
        ProposedKeys({ modifiers, static_cast<int32_t>(key) });
        e.Handled(true);
    }
}
