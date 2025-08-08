// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "TermControlUiaTextRange.hpp"
#include "TermControlUiaProvider.hpp"

using namespace Microsoft::Terminal;
using namespace Microsoft::Console::Types;
using namespace Microsoft::WRL;

// degenerate range constructor.
HRESULT TermControlUiaTextRange::RuntimeClassInitialize(_In_ Console::Render::IRenderData* pData, _In_ IRawElementProviderSimple* const pProvider, _In_ const std::wstring_view wordDelimiters) noexcept
{
    return UiaTextRangeBase::RuntimeClassInitialize(pData, pProvider, wordDelimiters);
}

HRESULT TermControlUiaTextRange::RuntimeClassInitialize(_In_ Console::Render::IRenderData* pData,
                                                        _In_ IRawElementProviderSimple* const pProvider,
                                                        const Cursor& cursor,
                                                        const std::wstring_view wordDelimiters) noexcept
{
    return UiaTextRangeBase::RuntimeClassInitialize(pData, pProvider, cursor, wordDelimiters);
}

HRESULT TermControlUiaTextRange::RuntimeClassInitialize(_In_ Console::Render::IRenderData* pData,
                                                        _In_ IRawElementProviderSimple* const pProvider,
                                                        const til::point start,
                                                        const til::point end,
                                                        bool blockRange,
                                                        const std::wstring_view wordDelimiters) noexcept
{
    return UiaTextRangeBase::RuntimeClassInitialize(pData, pProvider, start, end, blockRange, wordDelimiters);
}

// returns a degenerate text range of the start of the row closest to the y value of point
#pragma warning(suppress : 26434) // WRL RuntimeClassInitialize base is a no-op and we need this for MakeAndInitialize
HRESULT TermControlUiaTextRange::RuntimeClassInitialize(_In_ Console::Render::IRenderData* pData,
                                                        _In_ IRawElementProviderSimple* const pProvider,
                                                        const UiaPoint point,
                                                        const std::wstring_view wordDelimiters)
{
    RETURN_IF_FAILED(UiaTextRangeBase::RuntimeClassInitialize(pData, pProvider, wordDelimiters));
    Initialize(point);
    return S_OK;
}

#pragma warning(suppress : 26434) // WRL RuntimeClassInitialize base is a no-op and we need this for MakeAndInitialize
HRESULT TermControlUiaTextRange::RuntimeClassInitialize(const TermControlUiaTextRange& a) noexcept
{
    return UiaTextRangeBase::RuntimeClassInitialize(a);
}

IFACEMETHODIMP TermControlUiaTextRange::Clone(_Outptr_result_maybenull_ ITextRangeProvider** ppRetVal)
{
    RETURN_HR_IF(E_INVALIDARG, ppRetVal == nullptr);
    *ppRetVal = nullptr;
    const auto hr = MakeAndInitialize<TermControlUiaTextRange>(ppRetVal, *this);

    if (hr != S_OK)
    {
        *ppRetVal = nullptr;
        return hr;
    }

    return S_OK;
}

// Method Description:
// - Transform coordinates relative to the client to relative to the screen
// Arguments:
// - clientPoint: coordinates relative to the client where
//                (0,0) is the top-left of the app window
// Return Value:
// - <none>
void TermControlUiaTextRange::_TranslatePointToScreen(til::point& clientPoint) const
{
    const gsl::not_null<TermControlUiaProvider*> provider = static_cast<TermControlUiaProvider*>(_pProvider);
    const auto point = provider->GetContentOrigin();
    clientPoint.x += point.x;
    clientPoint.y += point.y;
}

// Method Description:
// - Transform coordinates relative to the screen to relative to the client
// Arguments:
// - screenPoint: coordinates relative to the screen where
//                (0,0) is the top-left of the screen
// Return Value:
// - <none>
void TermControlUiaTextRange::_TranslatePointFromScreen(til::point& screenPoint) const
{
    const gsl::not_null<TermControlUiaProvider*> provider = static_cast<TermControlUiaProvider*>(_pProvider);
    const auto point = provider->GetContentOrigin();
    screenPoint.x -= point.x;
    screenPoint.y -= point.y;
}

til::size TermControlUiaTextRange::_getScreenFontSize() const noexcept
{
    // Do NOT get the font info from IRenderData. It is a dummy font info.
    // Instead, the font info is saved in the TermControl. So we have to
    // ask our parent to get it for us.
    const gsl::not_null<const TermControlUiaProvider*> provider = static_cast<TermControlUiaProvider*>(_pProvider);
    return provider->GetFontSize();
}
