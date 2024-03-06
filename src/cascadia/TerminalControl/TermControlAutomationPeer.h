/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- TermControlAutomationPeer.h

Abstract:
- This module provides UI Automation access to the TermControl
  to support both automation tests and accessibility (screen
  reading) applications. This mainly interacts with ScreenInfoUiaProvider
  to allow for shared code between ConHost and Windows Terminal
  accessibility providers.
- Based on the Custom Automation Peers guide on msdn
  (https://docs.microsoft.com/en-us/windows/uwp/design/accessibility/custom-automation-peers)
- Wraps the UIAutomationCore ITextProvider
  (https://docs.microsoft.com/en-us/windows/win32/api/uiautomationcore/nn-uiautomationcore-itextprovider)
  with a XAML ITextProvider
  (https://docs.microsoft.com/en-us/uwp/api/windows.ui.xaml.automation.provider.itextprovider)

Author(s):
- Carlos Zamora   (CaZamor)    2019

Modifications:
- May 2021: Pulled the core logic of ITextProvider implementation into the
  InteractivityAutomationPeer, to support tab tear out.
--*/

#pragma once

#include "ControlInteractivity.h"
#include "TermControlAutomationPeer.g.h"
#include "../types/TermControlUiaProvider.hpp"
#include "../types/IUiaEventDispatcher.h"
#include "../types/IControlAccessibilityInfo.h"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct TermControl;

    struct TermControlAutomationPeer :
        public TermControlAutomationPeerT<TermControlAutomationPeer>,
        ::Microsoft::Console::Types::IUiaEventDispatcher
    {
    public:
        TermControlAutomationPeer(winrt::com_ptr<Microsoft::Terminal::Control::implementation::TermControl> owner,
                                  const Core::Padding padding,
                                  Control::InteractivityAutomationPeer implementation);

        void UpdateControlBounds();
        void SetControlPadding(const Core::Padding padding);
        void RecordKeyEvent(const WORD vkey);
        void Close();

#pragma region FrameworkElementAutomationPeer
        hstring GetClassNameCore() const;
        Windows::UI::Xaml::Automation::Peers::AutomationControlType GetAutomationControlTypeCore() const;
        hstring GetLocalizedControlTypeCore() const;
        Windows::Foundation::IInspectable GetPatternCore(Windows::UI::Xaml::Automation::Peers::PatternInterface patternInterface) const;
        Windows::UI::Xaml::Automation::Peers::AutomationOrientation GetOrientationCore() const;
        hstring GetNameCore() const;
        hstring GetHelpTextCore() const;
        Windows::UI::Xaml::Automation::Peers::AutomationLiveSetting GetLiveSettingCore() const;
#pragma endregion

#pragma region IUiaEventDispatcher
        void SignalSelectionChanged() override;
        void SignalTextChanged() override;
        void SignalCursorChanged() override;
        void NotifyNewOutput(std::wstring_view newOutput) override;
#pragma endregion

#pragma region ITextProvider Pattern
        Windows::UI::Xaml::Automation::Provider::ITextRangeProvider RangeFromPoint(Windows::Foundation::Point screenLocation);
        Windows::UI::Xaml::Automation::Provider::ITextRangeProvider RangeFromChild(Windows::UI::Xaml::Automation::Provider::IRawElementProviderSimple childElement);
        com_array<Windows::UI::Xaml::Automation::Provider::ITextRangeProvider> GetVisibleRanges();
        com_array<Windows::UI::Xaml::Automation::Provider::ITextRangeProvider> GetSelection();
        Windows::UI::Xaml::Automation::SupportedTextSelection SupportedTextSelection();
        Windows::UI::Xaml::Automation::Provider::ITextRangeProvider DocumentRange();
#pragma endregion

    private:
        winrt::weak_ref<Microsoft::Terminal::Control::implementation::TermControl> _termControl;
        Control::InteractivityAutomationPeer _contentAutomationPeer;
        til::shared_mutex<std::deque<wchar_t>> _keyEvents;
    };
}
