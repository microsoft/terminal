/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- TermControlUiaTextRange.hpp

Abstract:
- This module provides UI Automation access to the text of the console
  window to support both automation tests and accessibility (screen
  reading) applications.

Author(s):
- Carlos Zamora   (CaZamor)    2019
--*/

#pragma once

#include "../types/UiaTextRangeBase.hpp"

namespace Microsoft::Terminal
{
    class TermControlUiaTextRange final : public Microsoft::Console::Types::UiaTextRangeBase
    {
    public:
        TermControlUiaTextRange() = default;

        // degenerate range
        HRESULT RuntimeClassInitialize(_In_ Microsoft::Console::Types::IUiaData* pData,
                                       _In_ IRawElementProviderSimple* const pProvider,
                                       _In_ const std::wstring_view wordDelimiters = DefaultWordDelimiter) noexcept override;

        // degenerate range at cursor position
        HRESULT RuntimeClassInitialize(_In_ Microsoft::Console::Types::IUiaData* pData,
                                       _In_ IRawElementProviderSimple* const pProvider,
                                       const Cursor& cursor,
                                       const std::wstring_view wordDelimiters = DefaultWordDelimiter) noexcept override;

        // specific endpoint range
        HRESULT RuntimeClassInitialize(_In_ Microsoft::Console::Types::IUiaData* pData,
                                       _In_ IRawElementProviderSimple* const pProvider,
                                       const til::point start,
                                       const til::point end,
                                       bool blockRange = false,
                                       const std::wstring_view wordDelimiters = DefaultWordDelimiter) noexcept override;

        // range from a UiaPoint
        HRESULT RuntimeClassInitialize(_In_ Microsoft::Console::Types::IUiaData* pData,
                                       _In_ IRawElementProviderSimple* const pProvider,
                                       const UiaPoint point,
                                       const std::wstring_view wordDelimiters = DefaultWordDelimiter);

        HRESULT RuntimeClassInitialize(const TermControlUiaTextRange& a) noexcept;

        IFACEMETHODIMP Clone(_Outptr_result_maybenull_ ITextRangeProvider** ppRetVal) override;

    protected:
        void _TranslatePointToScreen(til::point* clientPoint) const override;
        void _TranslatePointFromScreen(til::point* screenPoint) const override;
        til::size _getScreenFontSize() const noexcept override;
    };
}
