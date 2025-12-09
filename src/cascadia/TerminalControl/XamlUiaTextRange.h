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
#include <windows.ui.xaml.automation.provider.h>

namespace winrt::Microsoft::Terminal::Control::implementation
{
    class XamlUiaTextRange : public ABI::Windows::UI::Xaml::Automation::Provider::ITextRangeProvider
    {
    public:
        static winrt::Windows::UI::Xaml::Automation::Provider::ITextRangeProvider Create(::ITextRangeProvider* uiaProvider, Windows::UI::Xaml::Automation::Provider::IRawElementProviderSimple parentProvider);

        XamlUiaTextRange(::ITextRangeProvider* uiaProvider, Windows::UI::Xaml::Automation::Provider::IRawElementProviderSimple parentProvider);
        ~XamlUiaTextRange();

        // IUnknown
        IFACEMETHODIMP_(ULONG) AddRef() noexcept override;
        IFACEMETHODIMP_(ULONG) Release() noexcept override;
        IFACEMETHODIMP QueryInterface(REFIID riid, void** ppvObject) noexcept override;

        // IInspectable
        IFACEMETHODIMP GetIids(ULONG* iidCount, IID** iids) noexcept override;
        IFACEMETHODIMP GetRuntimeClassName(HSTRING* className) noexcept override;
        IFACEMETHODIMP GetTrustLevel(TrustLevel* trustLevel) noexcept override;

#pragma region ITextRangeProvider
        IFACEMETHODIMP Clone(ABI::Windows::UI::Xaml::Automation::Provider::ITextRangeProvider** result) noexcept override;
        IFACEMETHODIMP Compare(ABI::Windows::UI::Xaml::Automation::Provider::ITextRangeProvider* textRangeProvider, boolean* result) noexcept override;
        IFACEMETHODIMP CompareEndpoints(ABI::Windows::UI::Xaml::Automation::Text::TextPatternRangeEndpoint endpoint,
                                       ABI::Windows::UI::Xaml::Automation::Provider::ITextRangeProvider* textRangeProvider,
                                       ABI::Windows::UI::Xaml::Automation::Text::TextPatternRangeEndpoint targetEndpoint,
                                       INT32* result) noexcept override;
        IFACEMETHODIMP ExpandToEnclosingUnit(ABI::Windows::UI::Xaml::Automation::Text::TextUnit unit) noexcept override;
        IFACEMETHODIMP FindAttribute(INT32 attributeId,
                                    IInspectable* value,
                                    boolean backward,
                                    ABI::Windows::UI::Xaml::Automation::Provider::ITextRangeProvider** result) noexcept override;
        IFACEMETHODIMP FindText(HSTRING text,
                               boolean backward,
                               boolean ignoreCase,
                               ABI::Windows::UI::Xaml::Automation::Provider::ITextRangeProvider** result) noexcept override;
        IFACEMETHODIMP GetAttributeValue(INT32 attributeId, IInspectable** result) noexcept override;
        IFACEMETHODIMP GetBoundingRectangles(UINT32* returnValueLength, DOUBLE** returnValue) noexcept override;
        IFACEMETHODIMP GetEnclosingElement(ABI::Windows::UI::Xaml::Automation::Provider::IIRawElementProviderSimple** result) noexcept override;
        IFACEMETHODIMP GetText(INT32 maxLength, HSTRING* result) noexcept override;
        IFACEMETHODIMP Move(ABI::Windows::UI::Xaml::Automation::Text::TextUnit unit,
                           INT32 count,
                           INT32* result) noexcept override;
        IFACEMETHODIMP MoveEndpointByUnit(ABI::Windows::UI::Xaml::Automation::Text::TextPatternRangeEndpoint endpoint,
                                         ABI::Windows::UI::Xaml::Automation::Text::TextUnit unit,
                                         INT32 count,
                                         INT32* result) noexcept override;
        IFACEMETHODIMP MoveEndpointByRange(ABI::Windows::UI::Xaml::Automation::Text::TextPatternRangeEndpoint endpoint,
                                          ABI::Windows::UI::Xaml::Automation::Provider::ITextRangeProvider* textRangeProvider,
                                          ABI::Windows::UI::Xaml::Automation::Text::TextPatternRangeEndpoint targetEndpoint) noexcept override;
        IFACEMETHODIMP Select() noexcept override;
        IFACEMETHODIMP AddToSelection() noexcept override;
        IFACEMETHODIMP RemoveFromSelection() noexcept override;
        IFACEMETHODIMP ScrollIntoView(boolean alignToTop) noexcept override;
        IFACEMETHODIMP GetChildren(UINT32* resultLength,
                                   ABI::Windows::UI::Xaml::Automation::Provider::IIRawElementProviderSimple*** result) noexcept override;
#pragma endregion ITextRangeProvider

        wil::com_ptr<::ITextRangeProvider> _uiaProvider;
        Windows::UI::Xaml::Automation::Provider::IRawElementProviderSimple _parentProvider;
        std::atomic<ULONG> _refCount{ 1 };
    };
}
