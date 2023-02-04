/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- XamlUiaTextRange.h

Abstract:
- This module is a wrapper for the UiaTextRange
  (a text range accessibility provider). It allows
  for UiaTextRange to be used in Windows Terminal.
- Wraps the UIAutomationCore ITextRangeProvider
  (https://docs.microsoft.com/en-us/windows/win32/api/uiautomationcore/nn-uiautomationcore-itextrangeprovider)
  with a XAML ITextRangeProvider
  (https://docs.microsoft.com/en-us/uwp/api/windows.ui.xaml.automation.provider.itextrangeprovider)

Author(s):
- Carlos Zamora   (CaZamor)    2019
--*/

#pragma once

#include "TermControlAutomationPeer.h"
#include <UIAutomationCore.h>
#include "../types/TermControlUiaTextRange.hpp"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    class XamlUiaTextRange :
        public winrt::implements<XamlUiaTextRange, WUX::Automation::Provider::ITextRangeProvider>
    {
    public:
        XamlUiaTextRange(::ITextRangeProvider* uiaProvider, WUX::Automation::Provider::IRawElementProviderSimple parentProvider) :
            _parentProvider{ parentProvider }
        {
            _uiaProvider.attach(uiaProvider);
        }

#pragma region ITextRangeProvider
        WUX::Automation::Provider::ITextRangeProvider Clone() const;
        bool Compare(WUX::Automation::Provider::ITextRangeProvider pRange) const;
        int32_t CompareEndpoints(WUX::Automation::Text::TextPatternRangeEndpoint endpoint,
                                 WUX::Automation::Provider::ITextRangeProvider pTargetRange,
                                 WUX::Automation::Text::TextPatternRangeEndpoint targetEndpoint);
        void ExpandToEnclosingUnit(WUX::Automation::Text::TextUnit unit) const;
        WUX::Automation::Provider::ITextRangeProvider FindAttribute(int32_t textAttributeId,
                                                                                  WF::IInspectable val,
                                                                                  bool searchBackward);
        WUX::Automation::Provider::ITextRangeProvider FindText(winrt::hstring text,
                                                                             bool searchBackward,
                                                                             bool ignoreCase);
        WF::IInspectable GetAttributeValue(int32_t textAttributeId) const;
        void GetBoundingRectangles(winrt::com_array<double>& returnValue) const;
        WUX::Automation::Provider::IRawElementProviderSimple GetEnclosingElement();
        winrt::hstring GetText(int32_t maxLength) const;
        int32_t Move(WUX::Automation::Text::TextUnit unit,
                     int32_t count);
        int32_t MoveEndpointByUnit(WUX::Automation::Text::TextPatternRangeEndpoint endpoint,
                                   WUX::Automation::Text::TextUnit unit,
                                   int32_t count) const;
        void MoveEndpointByRange(WUX::Automation::Text::TextPatternRangeEndpoint endpoint,
                                 WUX::Automation::Provider::ITextRangeProvider pTargetRange,
                                 WUX::Automation::Text::TextPatternRangeEndpoint targetEndpoint) const;
        void Select() const;
        void AddToSelection() const;
        void RemoveFromSelection() const;
        void ScrollIntoView(bool alignToTop) const;
        winrt::com_array<WUX::Automation::Provider::IRawElementProviderSimple> GetChildren() const;
#pragma endregion ITextRangeProvider

    private:
        wil::com_ptr<::ITextRangeProvider> _uiaProvider;
        WUX::Automation::Provider::IRawElementProviderSimple _parentProvider;
    };
}
