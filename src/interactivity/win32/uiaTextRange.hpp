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

#include "precomp.h"
#include "..\types\UiaTextRangeBase.hpp"

namespace Microsoft::Console::Interactivity::Win32
{
    class UiaTextRange final : public Microsoft::Console::Types::UiaTextRangeBase
    {
    public:
        static HRESULT GetSelectionRanges(_In_ Microsoft::Console::Types::IUiaData* pData,
                                          _In_ IRawElementProviderSimple* pProvider,
                                          _Out_ std::deque<WRL::ComPtr<UiaTextRange>>& ranges);

        UiaTextRange() = default;

        // degenerate range
        HRESULT RuntimeClassInitialize(_In_ Microsoft::Console::Types::IUiaData* pData,
                                       _In_ IRawElementProviderSimple* const pProvider);

        // degenerate range at cursor position
        HRESULT RuntimeClassInitialize(_In_ Microsoft::Console::Types::IUiaData* pData,
                                       _In_ IRawElementProviderSimple* const pProvider,
                                       const Cursor& cursor);

        // specific endpoint range
        HRESULT RuntimeClassInitialize(_In_ Microsoft::Console::Types::IUiaData* pData,
                                       _In_ IRawElementProviderSimple* const pProvider,
                                       const Endpoint start,
                                       const Endpoint end,
                                       const bool degenerate);

        // range from a UiaPoint
        HRESULT RuntimeClassInitialize(_In_ Microsoft::Console::Types::IUiaData* pData,
                                       _In_ IRawElementProviderSimple* const pProvider,
                                       const UiaPoint point);

        HRESULT RuntimeClassInitialize(const UiaTextRange& a);

        IFACEMETHODIMP Clone(_Outptr_result_maybenull_ ITextRangeProvider** ppRetVal) override;
        IFACEMETHODIMP FindText(_In_ BSTR text,
                                _In_ BOOL searchBackward,
                                _In_ BOOL ignoreCase,
                                _Outptr_result_maybenull_ ITextRangeProvider** ppRetVal) override;

    protected:
        void _ChangeViewport(const SMALL_RECT NewWindow) override;
        void _TranslatePointToScreen(LPPOINT clientPoint) const override;
        void _TranslatePointFromScreen(LPPOINT screenPoint) const override;

    private:
        HWND _getWindowHandle() const;

#ifdef UNIT_TESTING
        friend class ::UiaTextRangeTests;
#endif
    };
}
