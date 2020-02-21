// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "UiaTextRange.hpp"
#include "TermControlUiaProvider.hpp"

using namespace Microsoft::Terminal;
using namespace Microsoft::Console::Types;
using namespace Microsoft::WRL;
using namespace winrt::Windows::Graphics::Display;

// degenerate range constructor.
HRESULT UiaTextRange::RuntimeClassInitialize(_In_ IUiaData* pData, _In_ IRawElementProviderSimple* const pProvider, _In_ const std::wstring_view wordDelimiters)
{
    return UiaTextRangeBase::RuntimeClassInitialize(pData, pProvider, wordDelimiters);
}

HRESULT UiaTextRange::RuntimeClassInitialize(_In_ IUiaData* pData,
                                             _In_ IRawElementProviderSimple* const pProvider,
                                             const Cursor& cursor,
                                             const std::wstring_view wordDelimiters)
{
    return UiaTextRangeBase::RuntimeClassInitialize(pData, pProvider, cursor, wordDelimiters);
}

HRESULT UiaTextRange::RuntimeClassInitialize(_In_ IUiaData* pData,
                                             _In_ IRawElementProviderSimple* const pProvider,
                                             const COORD start,
                                             const COORD end,
                                             const std::wstring_view wordDelimiters)
{
    return UiaTextRangeBase::RuntimeClassInitialize(pData, pProvider, start, end, wordDelimiters);
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
    auto hr = MakeAndInitialize<UiaTextRange>(ppRetVal, *this);

    if (hr != S_OK)
    {
        *ppRetVal = nullptr;
        return hr;
    }

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

void UiaTextRange::_ChangeViewport(const SMALL_RECT NewWindow)
{
    auto provider = static_cast<TermControlUiaProvider*>(_pProvider);
    provider->ChangeViewport(NewWindow);
}

// Method Description:
// - Transform coordinates relative to the client to relative to the screen
// Arguments:
// - clientPoint: coordinates relative to the client where
//                (0,0) is the top-left of the app window
// Return Value:
// - <none>
void UiaTextRange::_TranslatePointToScreen(LPPOINT clientPoint) const
{
    auto provider = static_cast<TermControlUiaProvider*>(_pProvider);

    auto includeOffsets = [](long clientPos, double termControlPos, double padding, double scaleFactor) {
        auto result = base::ClampedNumeric<double>(clientPos);
        result += padding;
        result *= scaleFactor;
        result += termControlPos;
        return result;
    };

    // update based on TermControl location (important for Panes)
    UiaRect boundingRect;
    THROW_IF_FAILED(provider->get_BoundingRectangle(&boundingRect));

    // update based on TermControl padding
    const auto padding = provider->GetPadding();

    // Get scale factor for display
    const auto scaleFactor = DisplayInformation::GetForCurrentView().RawPixelsPerViewPixel();

    clientPoint->x = includeOffsets(clientPoint->x, boundingRect.left, padding.Left, scaleFactor);
    clientPoint->y = includeOffsets(clientPoint->y, boundingRect.top, padding.Top, scaleFactor);
}

// Method Description:
// - Transform coordinates relative to the screen to relative to the client
// Arguments:
// - screenPoint: coordinates relative to the screen where
//                (0,0) is the top-left of the screen
// Return Value:
// - <none>
void UiaTextRange::_TranslatePointFromScreen(LPPOINT screenPoint) const
{
    auto provider = static_cast<TermControlUiaProvider*>(_pProvider);

    auto includeOffsets = [](long screenPos, double termControlPos, double padding, double scaleFactor) {
        auto result = base::ClampedNumeric<double>(screenPos);
        result -= termControlPos;
        result /= scaleFactor;
        result -= padding;
        return result;
    };

    // update based on TermControl location (important for Panes)
    UiaRect boundingRect;
    THROW_IF_FAILED(provider->get_BoundingRectangle(&boundingRect));

    // update based on TermControl padding
    const auto padding = provider->GetPadding();

    // Get scale factor for display
    const auto scaleFactor = DisplayInformation::GetForCurrentView().RawPixelsPerViewPixel();

    screenPoint->x = includeOffsets(screenPoint->x, boundingRect.left, padding.Left, scaleFactor);
    screenPoint->y = includeOffsets(screenPoint->y, boundingRect.top, padding.Top, scaleFactor);
}

const COORD UiaTextRange::_getScreenFontSize() const
{
    // Do NOT get the font info from IRenderData. It is a dummy font info.
    // Instead, the font info is saved in the TermControl. So we have to
    // ask our parent to get it for us.
    auto provider = static_cast<TermControlUiaProvider*>(_pProvider);
    return provider->GetFontSize();
}
