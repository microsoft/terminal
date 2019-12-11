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
--*/

#pragma once

#include "TermControl.h"
#include "TermControlAutomationPeer.g.h"
#include <winrt/Microsoft.Terminal.TerminalControl.h>
#include "../../renderer/inc/IRenderData.hpp"
#include "../types/WindowUiaProviderBase.hpp"
#include "TermControlUiaProvider.hpp"

namespace winrt::Microsoft::Terminal::TerminalControl::implementation
{
    struct TermControlAutomationPeer :
        public TermControlAutomationPeerT<TermControlAutomationPeer>
    {
    public:
        TermControlAutomationPeer(winrt::Microsoft::Terminal::TerminalControl::implementation::TermControl* owner);

        winrt::hstring GetClassNameCore() const;
        winrt::Windows::UI::Xaml::Automation::Peers::AutomationControlType GetAutomationControlTypeCore() const;
        winrt::hstring GetLocalizedControlTypeCore() const;
        winrt::Windows::Foundation::IInspectable GetPatternCore(winrt::Windows::UI::Xaml::Automation::Peers::PatternInterface patternInterface) const;

#pragma region ITextProvider Pattern
        Windows::UI::Xaml::Automation::Provider::ITextRangeProvider RangeFromPoint(Windows::Foundation::Point screenLocation);
        Windows::UI::Xaml::Automation::Provider::ITextRangeProvider RangeFromChild(Windows::UI::Xaml::Automation::Provider::IRawElementProviderSimple childElement);
        winrt::com_array<Windows::UI::Xaml::Automation::Provider::ITextRangeProvider> GetVisibleRanges();
        winrt::com_array<Windows::UI::Xaml::Automation::Provider::ITextRangeProvider> GetSelection();
        Windows::UI::Xaml::Automation::SupportedTextSelection SupportedTextSelection();
        Windows::UI::Xaml::Automation::Provider::ITextRangeProvider DocumentRange();
#pragma endregion

        RECT GetBoundingRectWrapped();

    private:
        ::Microsoft::WRL::ComPtr<::Microsoft::Terminal::TermControlUiaProvider> _uiaProvider;

        winrt::com_array<Windows::UI::Xaml::Automation::Provider::ITextRangeProvider> WrapArrayOfTextRangeProviders(SAFEARRAY* textRanges);
    };
}
