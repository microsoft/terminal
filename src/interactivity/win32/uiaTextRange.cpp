// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "uiaTextRange.hpp"
#include "screenInfoUiaProvider.hpp"
#include "../interactivity/inc/ServiceLocator.hpp"

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Interactivity::Win32;
using namespace Microsoft::WRL;
using Microsoft::Console::Interactivity::ServiceLocator;

// degenerate range constructor.
HRESULT UiaTextRange::RuntimeClassInitialize(_In_ IUiaData* pData, _In_ IRawElementProviderSimple* const pProvider, _In_ const std::wstring_view wordDelimiters) noexcept
{
    return UiaTextRangeBase::RuntimeClassInitialize(pData, pProvider, wordDelimiters);
}

// degenerate range at cursor position
HRESULT UiaTextRange::RuntimeClassInitialize(_In_ IUiaData* pData,
                                             _In_ IRawElementProviderSimple* const pProvider,
                                             const Cursor& cursor,
                                             const std::wstring_view wordDelimiters) noexcept
{
    return UiaTextRangeBase::RuntimeClassInitialize(pData, pProvider, cursor, wordDelimiters);
}

// specific endpoint range
HRESULT UiaTextRange::RuntimeClassInitialize(_In_ IUiaData* pData,
                                             _In_ IRawElementProviderSimple* const pProvider,
                                             const til::point start,
                                             const til::point end,
                                             bool blockRange,
                                             const std::wstring_view wordDelimiters) noexcept
{
    return UiaTextRangeBase::RuntimeClassInitialize(pData, pProvider, start, end, blockRange, wordDelimiters);
}

// returns a degenerate text range of the start of the row closest to the y value of point
HRESULT UiaTextRange::RuntimeClassInitialize(_In_ IUiaData* pData,
                                             _In_ IRawElementProviderSimple* const pProvider,
                                             const UiaPoint point,
                                             const std::wstring_view wordDelimiters)
{
    RETURN_IF_FAILED(UiaTextRangeBase::RuntimeClassInitialize(pData, pProvider, wordDelimiters));
    Initialize(point);
    return S_OK;
}

HRESULT UiaTextRange::RuntimeClassInitialize(const UiaTextRange& a)
{
    return UiaTextRangeBase::RuntimeClassInitialize(a);
}

IFACEMETHODIMP UiaTextRange::Clone(_Outptr_result_maybenull_ ITextRangeProvider** ppRetVal)
{
    RETURN_HR_IF(E_INVALIDARG, ppRetVal == nullptr);
    *ppRetVal = nullptr;
    RETURN_IF_FAILED(MakeAndInitialize<UiaTextRange>(ppRetVal, *this));

#if defined(_DEBUG) && defined(UiaTextRangeBase_DEBUG_MSGS)
    OutputDebugString(L"Clone\n");
    std::wstringstream ss;
    ss << _id << L" cloned to " << (static_cast<UiaTextRangeBase*>(*ppRetVal))->_id;
    std::wstring str = ss.str();
    OutputDebugString(str.c_str());
    OutputDebugString(L"\n");
#endif
    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    // tracing
    /*ApiMsgClone apiMsg;
    apiMsg.CloneId = static_cast<UiaTextRangeBase*>(*ppRetVal)->GetId();
    Tracing::s_TraceUia(this, ApiCall::Clone, &apiMsg);*/

    return S_OK;
}

void UiaTextRange::_TranslatePointToScreen(til::point* clientPoint) const
{
    ClientToScreen(_getWindowHandle(), clientPoint->as_win32_point());
}

void UiaTextRange::_TranslatePointFromScreen(til::point* screenPoint) const
{
    ScreenToClient(_getWindowHandle(), screenPoint->as_win32_point());
}

HWND UiaTextRange::_getWindowHandle() const
{
    const auto provider = static_cast<ScreenInfoUiaProvider*>(_pProvider);
    return provider->GetWindowHandle();
}
