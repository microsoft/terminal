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

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct SettingContainer : SettingContainerT<SettingContainer>
    {
    public:
        SettingContainer();

        void OnApplyTemplate();

        void SetExpanded(bool expanded);

        til::typed_event<Editor::SettingContainer, Windows::Foundation::IInspectable> ClearSettingValue;

        DEPENDENCY_PROPERTY(Windows::Foundation::IInspectable, Header);
        DEPENDENCY_PROPERTY(hstring, HelpText);
        DEPENDENCY_PROPERTY(hstring, FontIconGlyph);
        DEPENDENCY_PROPERTY(Windows::Foundation::IInspectable, CurrentValue);
        DEPENDENCY_PROPERTY(Windows::UI::Xaml::DataTemplate, CurrentValueTemplate);
        DEPENDENCY_PROPERTY(hstring, CurrentValueAccessibleName);
        DEPENDENCY_PROPERTY(bool, HasSettingValue);
        DEPENDENCY_PROPERTY(bool, StartExpanded);
        DEPENDENCY_PROPERTY(IInspectable, SettingOverrideSource);

    private:
        static void _InitializeProperties();
        static void _OnCurrentValueChanged(const Windows::UI::Xaml::DependencyObject& d, const Windows::UI::Xaml::DependencyPropertyChangedEventArgs& e);
        static void _OnHasSettingValueChanged(const Windows::UI::Xaml::DependencyObject& d, const Windows::UI::Xaml::DependencyPropertyChangedEventArgs& e);
        static void _OnHelpTextChanged(const Windows::UI::Xaml::DependencyObject& d, const Windows::UI::Xaml::DependencyPropertyChangedEventArgs& e);
        static hstring _GenerateOverrideMessage(const IInspectable& settingOrigin);
        hstring _GenerateAccessibleName();
        void _UpdateOverrideSystem();
        void _UpdateHelpText();
        void _UpdateCurrentValueAutoProp();
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(SettingContainer);
}
