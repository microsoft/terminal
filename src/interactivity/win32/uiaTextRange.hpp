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
#include "../types/UiaTextRangeBase.hpp"

namespace Microsoft::Console::Interactivity::Win32
{
    class UiaTextRange final : public Microsoft::Console::Types::UiaTextRangeBase
    {
    public:
        UiaTextRange() = default;

        // degenerate range
        HRESULT RuntimeClassInitialize(_In_ Render::IRenderData* pData,
                                       _In_ IRawElementProviderSimple* const pProvider,
                                       _In_ const std::wstring_view wordDelimiters = DefaultWordDelimiter) noexcept override;

        // degenerate range at cursor position
        HRESULT RuntimeClassInitialize(_In_ Render::IRenderData* pData,
                                       _In_ IRawElementProviderSimple* const pProvider,
                                       const Cursor& cursor,
                                       _In_ const std::wstring_view wordDelimiters = DefaultWordDelimiter) noexcept override;

        // specific endpoint range
        HRESULT RuntimeClassInitialize(_In_ Render::IRenderData* pData,
                                       _In_ IRawElementProviderSimple* const pProvider,
                                       _In_ const til::point start,
                                       _In_ const til::point end,
                                       _In_ bool blockRange = false,
                                       _In_ const std::wstring_view wordDelimiters = DefaultWordDelimiter) noexcept override;

        // range from a UiaPoint
        HRESULT RuntimeClassInitialize(_In_ Render::IRenderData* pData,
                                       _In_ IRawElementProviderSimple* const pProvider,
                                       const UiaPoint point,
                                       _In_ const std::wstring_view wordDelimiters = DefaultWordDelimiter);

        HRESULT RuntimeClassInitialize(const UiaTextRange& a);

        IFACEMETHODIMP Clone(_Outptr_result_maybenull_ ITextRangeProvider** ppRetVal) override;

    protected:
        void _TranslatePointToScreen(til::point* clientPoint) const override;
        void _TranslatePointFromScreen(til::point* screenPoint) const override;

    private:
        HWND _getWindowHandle() const;

#ifdef UNIT_TESTING
        friend class ::UiaTextRangeTests;
#endif
    };
}
