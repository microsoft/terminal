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
#include "UiaTextRange.hpp"

namespace winrt::Microsoft::Terminal::TerminalControl::implementation
{
    class XamlUiaTextRange :
        public winrt::implements<XamlUiaTextRange, Windows::UI::Xaml::Automation::Provider::ITextRangeProvider>
    {
    public:
        XamlUiaTextRange(::ITextRangeProvider* uiaProvider, Windows::UI::Xaml::Automation::Provider::IRawElementProviderSimple parentProvider) :
            _parentProvider{ parentProvider }
        {
            _uiaProvider.attach(uiaProvider);
        }

#pragma region ITextRangeProvider
        Windows::UI::Xaml::Automation::Provider::ITextRangeProvider Clone() const;
        bool Compare(Windows::UI::Xaml::Automation::Provider::ITextRangeProvider pRange) const;
        int32_t CompareEndpoints(Windows::UI::Xaml::Automation::Text::TextPatternRangeEndpoint endpoint,
                                 Windows::UI::Xaml::Automation::Provider::ITextRangeProvider pTargetRange,
                                 Windows::UI::Xaml::Automation::Text::TextPatternRangeEndpoint targetEndpoint);
        void ExpandToEnclosingUnit(Windows::UI::Xaml::Automation::Text::TextUnit unit) const;
        Windows::UI::Xaml::Automation::Provider::ITextRangeProvider FindAttribute(int32_t textAttributeId,
                                                                                  winrt::Windows::Foundation::IInspectable val,
                                                                                  bool searchBackward);
        Windows::UI::Xaml::Automation::Provider::ITextRangeProvider FindText(winrt::hstring text,
                                                                             bool searchBackward,
                                                                             bool ignoreCase);
        winrt::Windows::Foundation::IInspectable GetAttributeValue(int32_t textAttributeId) const;
        void GetBoundingRectangles(winrt::com_array<double>& returnValue) const;
        Windows::UI::Xaml::Automation::Provider::IRawElementProviderSimple GetEnclosingElement();
        winrt::hstring GetText(int32_t maxLength) const;
        int32_t Move(Windows::UI::Xaml::Automation::Text::TextUnit unit,
                     int32_t count);
        int32_t MoveEndpointByUnit(Windows::UI::Xaml::Automation::Text::TextPatternRangeEndpoint endpoint,
                                   Windows::UI::Xaml::Automation::Text::TextUnit unit,
                                   int32_t count) const;
        void MoveEndpointByRange(Windows::UI::Xaml::Automation::Text::TextPatternRangeEndpoint endpoint,
                                 Windows::UI::Xaml::Automation::Provider::ITextRangeProvider pTargetRange,
                                 Windows::UI::Xaml::Automation::Text::TextPatternRangeEndpoint targetEndpoint) const;
        void Select() const;
        void AddToSelection() const;
        void RemoveFromSelection() const;
        void ScrollIntoView(bool alignToTop) const;
        winrt::com_array<Windows::UI::Xaml::Automation::Provider::IRawElementProviderSimple> GetChildren() const;
#pragma endregion ITextRangeProvider

    private:
        wil::com_ptr<::ITextRangeProvider> _uiaProvider;
        Windows::UI::Xaml::Automation::Provider::IRawElementProviderSimple _parentProvider;
    };
}
