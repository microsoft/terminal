/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- TermControlUiaProvider.hpp

Abstract:
- This module provides UI Automation access to the screen buffer to
  support both automation tests and accessibility (screen reading)
  applications.
- ConHost and Windows Terminal must use IRenderData to have access to the proper information
- Based on examples, sample code, and guidance from
  https://msdn.microsoft.com/en-us/library/windows/desktop/ee671596(v=vs.85).aspx

Author(s):
- Carlos Zamora   (CaZamor)   2019
--*/

#pragma once

#include "ScreenInfoUiaProviderBase.h"
#include "UiaTextRangeBase.hpp"
#include "IControlAccessibilityInfo.h"
#include "TermControlUiaTextRange.hpp"
namespace Microsoft::Terminal
{
    class TermControlUiaProvider : public Microsoft::Console::Types::ScreenInfoUiaProviderBase
    {
    public:
        TermControlUiaProvider() = default;
        HRESULT RuntimeClassInitialize(_In_ Console::Render::IRenderData* const renderData,
                                       _In_ ::Microsoft::Console::Types::IControlAccessibilityInfo* controlInfo) noexcept;

        // IRawElementProviderSimple methods
        IFACEMETHODIMP GetPropertyValue(_In_ PROPERTYID idProp,
                                        _Out_ VARIANT* pVariant) noexcept override;

        // IRawElementProviderFragment methods
        IFACEMETHODIMP Navigate(_In_ NavigateDirection direction,
                                _COM_Outptr_result_maybenull_ IRawElementProviderFragment** ppProvider) noexcept override;
        IFACEMETHODIMP get_HostRawElementProvider(IRawElementProviderSimple** ppProvider) noexcept override;
        IFACEMETHODIMP get_BoundingRectangle(_Out_ UiaRect* pRect) noexcept override;
        IFACEMETHODIMP get_FragmentRoot(_COM_Outptr_result_maybenull_ IRawElementProviderFragmentRoot** ppProvider) noexcept override;

        til::size GetFontSize() const noexcept;
        til::rect GetPadding() const noexcept;
        double GetScaleFactor() const noexcept;
        void ChangeViewport(const til::inclusive_rect& NewWindow) override;

    protected:
        HRESULT GetSelectionRange(_In_ IRawElementProviderSimple* pProvider, const std::wstring_view wordDelimiters, _COM_Outptr_result_maybenull_ Microsoft::Console::Types::UiaTextRangeBase** ppUtr) override;

        // degenerate range
        HRESULT CreateTextRange(_In_ IRawElementProviderSimple* const pProvider, const std::wstring_view wordDelimiters, _COM_Outptr_result_maybenull_ Microsoft::Console::Types::UiaTextRangeBase** ppUtr) override;

        // degenerate range at cursor position
        HRESULT CreateTextRange(_In_ IRawElementProviderSimple* const pProvider,
                                const Cursor& cursor,
                                const std::wstring_view wordDelimiters,
                                _COM_Outptr_result_maybenull_ Microsoft::Console::Types::UiaTextRangeBase** ppUtr) override;

        // specific endpoint range
        HRESULT CreateTextRange(_In_ IRawElementProviderSimple* const pProvider,
                                const til::point start,
                                const til::point end,
                                const std::wstring_view wordDelimiters,
                                _COM_Outptr_result_maybenull_ Microsoft::Console::Types::UiaTextRangeBase** ppUtr) override;

        // range from a UiaPoint
        HRESULT CreateTextRange(_In_ IRawElementProviderSimple* const pProvider,
                                const UiaPoint point,
                                const std::wstring_view wordDelimiters,
                                _COM_Outptr_result_maybenull_ Microsoft::Console::Types::UiaTextRangeBase** ppUtr) override;

    private:
        ::Microsoft::Console::Types::IControlAccessibilityInfo* _controlInfo{ nullptr };
    };
}
