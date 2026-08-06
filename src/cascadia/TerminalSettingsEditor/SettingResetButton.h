/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- SettingResetButton

Abstract:
- A small "reset value" button (eraser glyph) shown next to a profile setting's
  control. When the setting has a value set at the current inheritance layer, the
  button is enabled; clicking it clears that value so the setting reverts to its
  inherited value. The button drives all of its state generically through the
  IInheritableViewModel projected on the owning view model, keyed by SettingName.

--*/

#pragma once

#include "SettingResetButton.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct SettingResetButton : SettingResetButtonT<SettingResetButton>
    {
    public:
        SettingResetButton();

        DEPENDENCY_PROPERTY(Editor::IInheritableViewModel, Target);
        DEPENDENCY_PROPERTY(hstring, SettingName);

    private:
        static void _InitializeProperties();
        static void _OnTargetChanged(const Windows::UI::Xaml::DependencyObject& d, const Windows::UI::Xaml::DependencyPropertyChangedEventArgs& e);
        static void _OnSettingNameChanged(const Windows::UI::Xaml::DependencyObject& d, const Windows::UI::Xaml::DependencyPropertyChangedEventArgs& e);

        void _ResubscribeToTarget();
        void _OnTargetPropertyChanged(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Data::PropertyChangedEventArgs& args);
        void _Update();
        void _OnClick(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);
        hstring _GenerateOverrideMessage(const Windows::Foundation::IInspectable& settingOrigin);

        Windows::UI::Xaml::Data::INotifyPropertyChanged _subscribedTarget{ nullptr };
        winrt::event_token _targetPropertyChangedToken{};
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(SettingResetButton);
}
