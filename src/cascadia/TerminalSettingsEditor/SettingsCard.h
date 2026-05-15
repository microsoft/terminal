/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- SettingsCard

Abstract:
- A base control for building consistent settings experiences. Based
  on the Windows Community Toolkit's SettingsCard.

Author(s):
- Carlos Zamora - 2026 May

--*/

#pragma once

#include "SettingsCard.g.h"
#include "SettingsCardAutomationPeer.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct SettingsCard : SettingsCardT<SettingsCard>
    {
    public:
        SettingsCard();

        void OnApplyTemplate();

        // Automation peer override.
        Windows::UI::Xaml::Automation::Peers::AutomationPeer OnCreateAutomationPeer();

        DEPENDENCY_PROPERTY(Windows::Foundation::IInspectable, Header);
        DEPENDENCY_PROPERTY(Windows::Foundation::IInspectable, Description);
        DEPENDENCY_PROPERTY(Windows::UI::Xaml::Controls::IconElement, HeaderIcon);
        DEPENDENCY_PROPERTY(Windows::UI::Xaml::Controls::IconElement, ActionIcon);
        DEPENDENCY_PROPERTY(hstring, ActionIconToolTip);
        DEPENDENCY_PROPERTY(bool, IsClickEnabled);
        DEPENDENCY_PROPERTY(bool, IsActionIconVisible);
        DEPENDENCY_PROPERTY(Editor::SettingsCardContentAlignment, ContentAlignment);

    private:
        static void _InitializeProperties();
        static void _OnHeaderChanged(const Windows::UI::Xaml::DependencyObject& d, const Windows::UI::Xaml::DependencyPropertyChangedEventArgs& e);
        static void _OnDescriptionChanged(const Windows::UI::Xaml::DependencyObject& d, const Windows::UI::Xaml::DependencyPropertyChangedEventArgs& e);
        static void _OnHeaderIconChanged(const Windows::UI::Xaml::DependencyObject& d, const Windows::UI::Xaml::DependencyPropertyChangedEventArgs& e);
        static void _OnIsClickEnabledChanged(const Windows::UI::Xaml::DependencyObject& d, const Windows::UI::Xaml::DependencyPropertyChangedEventArgs& e);
        static void _OnIsActionIconVisibleChanged(const Windows::UI::Xaml::DependencyObject& d, const Windows::UI::Xaml::DependencyPropertyChangedEventArgs& e);
        static void _OnContentAlignmentChanged(const Windows::UI::Xaml::DependencyObject& d, const Windows::UI::Xaml::DependencyPropertyChangedEventArgs& e);

        void _EnableButtonInteraction();
        void _DisableButtonInteraction();
        void _GoToCommonState(const std::wstring_view& state, bool useTransitions);
        void _UpdateActionIconVisibility();
        void _UpdateHeaderVisibility();
        void _UpdateDescriptionVisibility();
        void _UpdateHeaderIconVisibility();
        void _UpdateContentAlignmentState();
        void _CheckInitialVisualState();
        void _SetAccessibleContentName();
        Windows::UI::Xaml::FrameworkElement _GetFocusedElement();

        bool _interactionEnabled{ false };
        Windows::UI::Xaml::Controls::Control::IsEnabledChanged_revoker _isEnabledChangedRevoker;
        Windows::UI::Xaml::UIElement::PointerEntered_revoker _pointerEnteredRevoker;
        Windows::UI::Xaml::UIElement::PointerExited_revoker _pointerExitedRevoker;
        Windows::UI::Xaml::UIElement::PointerCaptureLost_revoker _pointerCaptureLostRevoker;
        Windows::UI::Xaml::UIElement::PointerCanceled_revoker _pointerCanceledRevoker;
        Windows::UI::Xaml::UIElement::PreviewKeyDown_revoker _previewKeyDownRevoker;
        Windows::UI::Xaml::UIElement::PreviewKeyUp_revoker _previewKeyUpRevoker;
        int64_t _contentChangedToken{ 0 };
    };

    // AutomationPeer for SettingsCard. Mirrors the Community Toolkit's
    // SettingsCardAutomationPeer: only exposes Invoke + Button control type when
    // the card has IsClickEnabled=true; otherwise reports as a Group.
    struct SettingsCardAutomationPeer : SettingsCardAutomationPeerT<SettingsCardAutomationPeer>
    {
    public:
        SettingsCardAutomationPeer(const Editor::SettingsCard& owner);

        Windows::UI::Xaml::Automation::Peers::AutomationControlType GetAutomationControlTypeCore() const;
        hstring GetClassNameCore() const;
        hstring GetNameCore() const;
        winrt::Windows::Foundation::IInspectable GetPatternCore(Windows::UI::Xaml::Automation::Peers::PatternInterface patternInterface) const;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(SettingsCard);
    BASIC_FACTORY(SettingsCardAutomationPeer);
}
