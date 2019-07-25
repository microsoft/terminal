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
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Automation.Peers.h>
#include "../../renderer/inc/IRenderData.hpp"
#include "../types/ScreenInfoUiaProvider.h"
#include "../types/WindowUiaProviderBase.hpp"

using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Types;
using namespace winrt::Windows::UI::Xaml::Automation::Peers;

namespace winrt::Microsoft::Terminal::TerminalControl::implementation
{
    struct TermControlAutomationPeer :
        public TermControlAutomationPeerT<TermControlAutomationPeer>
    {
    public:
        TermControlAutomationPeer(winrt::Microsoft::Terminal::TerminalControl::implementation::TermControl const& owner);

        winrt::hstring GetClassNameCore() const;
        AutomationControlType GetAutomationControlTypeCore() const;
        winrt::hstring GetLocalizedControlTypeCore() const;
        winrt::Windows::Foundation::IInspectable GetPatternCore(PatternInterface patternInterface) const;

        // ITextProvider Pattern
        Windows::UI::Xaml::Automation::Provider::ITextRangeProvider RangeFromPoint(Windows::Foundation::Point screenLocation);
        Windows::UI::Xaml::Automation::Provider::ITextRangeProvider RangeFromChild(Windows::UI::Xaml::Automation::Provider::IRawElementProviderSimple childElement);
        winrt::com_array<Windows::UI::Xaml::Automation::Provider::ITextRangeProvider> GetVisibleRanges();
        winrt::com_array<Windows::UI::Xaml::Automation::Provider::ITextRangeProvider> GetSelection();
        Windows::UI::Xaml::Automation::SupportedTextSelection SupportedTextSelection();
        Windows::UI::Xaml::Automation::Provider::ITextRangeProvider DocumentRange();

        RECT GetBoundingRectWrapped();

        int32_t MyProperty() { return 0; }

    private:
        TermControl* _owner;
        ScreenInfoUiaProvider _uiaProvider;
    };
}
