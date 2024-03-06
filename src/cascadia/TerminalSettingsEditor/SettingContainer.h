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

        DEPENDENCY_PROPERTY(Windows::Foundation::IInspectable, Header);
        DEPENDENCY_PROPERTY(hstring, HelpText);
        DEPENDENCY_PROPERTY(hstring, CurrentValue);
        DEPENDENCY_PROPERTY(bool, HasSettingValue);
        DEPENDENCY_PROPERTY(bool, StartExpanded);
        DEPENDENCY_PROPERTY(IInspectable, SettingOverrideSource);
        TYPED_EVENT(ClearSettingValue, Editor::SettingContainer, Windows::Foundation::IInspectable);

    private:
        static void _InitializeProperties();
        static void _OnHasSettingValueChanged(const Windows::UI::Xaml::DependencyObject& d, const Windows::UI::Xaml::DependencyPropertyChangedEventArgs& e);
        static hstring _GenerateOverrideMessage(const IInspectable& settingOrigin);
        void _UpdateOverrideSystem();
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(SettingContainer);
}
