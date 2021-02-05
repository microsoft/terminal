/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- SettingContainer

Abstract:
- This is a XAML container that wraps settings in the Settings UI.
  It interacts with the inheritance logic from the TerminalSettingsModel
  and represents it in the Settings UI.

Author(s):
- Carlos Zamora - January 2021

--*/

#pragma once

#include "SettingContainer.g.h"
#include "Utils.h"

// This macro defines a dependency property for a WinRT class.
// Use this in your class' header file after declaring it in the idl.
// Remember to register your dependency property in the respective cpp file.
#define DEPENDENCY_PROPERTY(type, name)                                  \
public:                                                                  \
    static winrt::Windows::UI::Xaml::DependencyProperty name##Property() \
    {                                                                    \
        return _##name##Property;                                        \
    }                                                                    \
    type name() const                                                    \
    {                                                                    \
        return winrt::unbox_value<type>(GetValue(_##name##Property));    \
    }                                                                    \
    void name(type const& value)                                         \
    {                                                                    \
        SetValue(_##name##Property, winrt::box_value(value));            \
    }                                                                    \
                                                                         \
private:                                                                 \
    static winrt::Windows::UI::Xaml::DependencyProperty _##name##Property;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct SettingContainer : SettingContainerT<SettingContainer>
    {
    public:
        SettingContainer();

        void OnApplyTemplate();

        DEPENDENCY_PROPERTY(Windows::Foundation::IInspectable, Header);
        DEPENDENCY_PROPERTY(hstring, HelpText);
        DEPENDENCY_PROPERTY(bool, HasSettingValue);
        TYPED_EVENT(ClearSettingValue, Editor::SettingContainer, Windows::Foundation::IInspectable);

    private:
        static void _InitializeProperties();
        static void _OnHasSettingValueChanged(Windows::UI::Xaml::DependencyObject const& d, Windows::UI::Xaml::DependencyPropertyChangedEventArgs const& e);
        hstring _GenerateOverrideMessageText();
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(SettingContainer);
}
