/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- UiaTextRange.hpp

Abstract:
- This module provides UI Automation access to the text of the console
  window to support both automation tests and accessibility (screen
  reading) applications.

Author(s):
- Carlos Zamora   (CaZamor)    2019
--*/

#pragma once

#include "..\types\UiaTextRangeBase.hpp"

namespace Microsoft::Terminal
{
    class UiaTextRange final : public Microsoft::Console::Types::UiaTextRangeBase
    {
    public:
        static HRESULT GetSelectionRanges(_In_ Microsoft::Console::Types::IUiaData* pData,
                                          _In_ IRawElementProviderSimple* pProvider,
                                          _In_ const std::wstring_view wordDelimiters,
                                          _Out_ std::deque<WRL::ComPtr<UiaTextRange>>& ranges);

        UiaTextRange() = default;

        // degenerate range
        HRESULT RuntimeClassInitialize(_In_ Microsoft::Console::Types::IUiaData* pData,
                                       _In_ IRawElementProviderSimple* const pProvider,
                                       _In_ const std::wstring_view wordDelimiters = DefaultWordDelimiter);

        // degenerate range at cursor position
        HRESULT RuntimeClassInitialize(_In_ Microsoft::Console::Types::IUiaData* pData,
                                       _In_ IRawElementProviderSimple* const pProvider,
                                       const Cursor& cursor,
                                       const std::wstring_view wordDelimiters = DefaultWordDelimiter);

        // specific endpoint range
        HRESULT RuntimeClassInitialize(_In_ Microsoft::Console::Types::IUiaData* pData,
                                       _In_ IRawElementProviderSimple* const pProvider,
                                       const COORD start,
                                       const COORD end,
                                       const std::wstring_view wordDelimiters = DefaultWordDelimiter);

        // range from a UiaPoint
        HRESULT RuntimeClassInitialize(_In_ Microsoft::Console::Types::IUiaData* pData,
                                       _In_ IRawElementProviderSimple* const pProvider,
                                       const UiaPoint point,
                                       const std::wstring_view wordDelimiters = DefaultWordDelimiter);

        HRESULT RuntimeClassInitialize(const UiaTextRange& a);

        IFACEMETHODIMP Clone(_Outptr_result_maybenull_ ITextRangeProvider** ppRetVal) override;

    protected:
        void _ChangeViewport(const SMALL_RECT NewWindow) override;
        void _TranslatePointToScreen(LPPOINT clientPoint) const override;
        void _TranslatePointFromScreen(LPPOINT screenPoint) const override;
        const COORD _getScreenFontSize() const override;
    };
}
