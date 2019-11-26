// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "ScreenInfoUiaProviderBase.h"
#include "WindowUiaProviderBase.hpp"

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Types::ScreenInfoUiaProviderTracing;

// A helper function to create a SafeArray Version of an int array of a specified length
SAFEARRAY* BuildIntSafeArray(std::basic_string_view<int> data)
{
    SAFEARRAY* psa = SafeArrayCreateVector(VT_I4, 0, gsl::narrow<ULONG>(data.size()));
    if (psa != nullptr)
    {
        for (size_t i = 0; i < data.size(); i++)
        {
            LONG lIndex = 0;
            if (FAILED(SizeTToLong(i, &lIndex) ||
                       FAILED(SafeArrayPutElement(psa, &lIndex, (void*)&(data.at(i))))))
            {
                SafeArrayDestroy(psa);
                psa = nullptr;
                break;
            }
        }
    }

    return psa;
}

#pragma warning(suppress : 26434) // WRL RuntimeClassInitialize base is a no-op and we need this for MakeAndInitialize
HRESULT ScreenInfoUiaProviderBase::RuntimeClassInitialize(_In_ IUiaData* pData) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pData);
    _pData = pData;
    return S_OK;
}

[[nodiscard]] HRESULT ScreenInfoUiaProviderBase::Signal(_In_ EVENTID id)
{
    HRESULT hr = S_OK;
    // check to see if we're already firing this particular event
    if (_signalFiringMapping.find(id) != _signalFiringMapping.end() &&
        _signalFiringMapping[id] == true)
    {
        return hr;
    }

    try
    {
        _signalFiringMapping[id] = true;
    }
    CATCH_RETURN();

    IRawElementProviderSimple* pProvider = this;
    hr = UiaRaiseAutomationEvent(pProvider, id);
    _signalFiringMapping[id] = false;

    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    // tracing
    /*ApiMsgSignal apiMsg;
    apiMsg.Signal = id;
    Tracing::s_TraceUia(this, ApiCall::Signal, &apiMsg);*/
    return hr;
}

#pragma region IRawElementProviderSimple

// Implementation of IRawElementProviderSimple::get_ProviderOptions.
// Gets UI Automation provider options.
IFACEMETHODIMP ScreenInfoUiaProviderBase::get_ProviderOptions(_Out_ ProviderOptions* pOptions) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pOptions);

    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(this, ApiCall::GetProviderOptions, nullptr);
    *pOptions = ProviderOptions_ServerSideProvider;
    return S_OK;
}

// Implementation of IRawElementProviderSimple::get_PatternProvider.
// Gets the object that supports ISelectionPattern.
IFACEMETHODIMP ScreenInfoUiaProviderBase::GetPatternProvider(_In_ PATTERNID patternId,
                                                             _COM_Outptr_result_maybenull_ IUnknown** ppInterface)
{
    RETURN_HR_IF(E_INVALIDARG, ppInterface == nullptr);
    *ppInterface = nullptr;

    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(this, ApiCall::GetPatternProvider, nullptr);

    *ppInterface = nullptr;
    HRESULT hr = S_OK;

    if (patternId == UIA_TextPatternId)
    {
        hr = QueryInterface(IID_PPV_ARGS(ppInterface));
        if (FAILED(hr))
        {
            *ppInterface = nullptr;
        }
    }
    return hr;
}

// Implementation of IRawElementProviderSimple::get_PropertyValue.
// Gets custom properties.
IFACEMETHODIMP ScreenInfoUiaProviderBase::GetPropertyValue(_In_ PROPERTYID propertyId,
                                                           _Out_ VARIANT* pVariant) noexcept
{
    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(this, ApiCall::GetPropertyValue, nullptr);

    pVariant->vt = VT_EMPTY;

    // Returning the default will leave the property as the default
    // so we only really need to touch it for the properties we want to implement
    if (propertyId == UIA_ControlTypePropertyId)
    {
        // This control is the Document control type, implying that it is
        // a complex document that supports text pattern
        pVariant->vt = VT_I4;
        pVariant->lVal = UIA_DocumentControlTypeId;
    }
    else if (propertyId == UIA_NamePropertyId)
    {
        // TODO: MSFT: 7960168 - These strings should be localized text in the final UIA work
        pVariant->bstrVal = SysAllocString(L"Text Area");
        if (pVariant->bstrVal != nullptr)
        {
            pVariant->vt = VT_BSTR;
        }
    }
    else if (propertyId == UIA_AutomationIdPropertyId)
    {
        pVariant->bstrVal = SysAllocString(L"Text Area");
        if (pVariant->bstrVal != nullptr)
        {
            pVariant->vt = VT_BSTR;
        }
    }
    else if (propertyId == UIA_IsControlElementPropertyId)
    {
        pVariant->vt = VT_BOOL;
        pVariant->boolVal = VARIANT_TRUE;
    }
    else if (propertyId == UIA_IsContentElementPropertyId)
    {
        pVariant->vt = VT_BOOL;
        pVariant->boolVal = VARIANT_TRUE;
    }
    else if (propertyId == UIA_IsKeyboardFocusablePropertyId)
    {
        pVariant->vt = VT_BOOL;
        pVariant->boolVal = VARIANT_TRUE;
    }
    else if (propertyId == UIA_HasKeyboardFocusPropertyId)
    {
        pVariant->vt = VT_BOOL;
        pVariant->boolVal = VARIANT_TRUE;
    }
    else if (propertyId == UIA_ProviderDescriptionPropertyId)
    {
        pVariant->bstrVal = SysAllocString(L"Microsoft Console Host: Screen Information Text Area");
        if (pVariant->bstrVal != nullptr)
        {
            pVariant->vt = VT_BSTR;
        }
    }
    else if (propertyId == UIA_IsEnabledPropertyId)
    {
        pVariant->vt = VT_BOOL;
        pVariant->boolVal = VARIANT_TRUE;
    }

    return S_OK;
}

IFACEMETHODIMP ScreenInfoUiaProviderBase::get_HostRawElementProvider(_COM_Outptr_result_maybenull_ IRawElementProviderSimple** ppProvider) noexcept
{
    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(this, ApiCall::GetHostRawElementProvider, nullptr);
    RETURN_HR_IF(E_INVALIDARG, ppProvider == nullptr);
    *ppProvider = nullptr;

    return S_OK;
}
#pragma endregion

#pragma region IRawElementProviderFragment

IFACEMETHODIMP ScreenInfoUiaProviderBase::GetRuntimeId(_Outptr_result_maybenull_ SAFEARRAY** ppRuntimeId)
{
    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(this, ApiCall::GetRuntimeId, nullptr);

    // Root defers this to host, others must implement it...
    RETURN_HR_IF(E_INVALIDARG, ppRuntimeId == nullptr);
    *ppRuntimeId = nullptr;

    // AppendRuntimeId is a magic Number that tells UIAutomation to Append its own Runtime ID(From the HWND)
    const std::array<int, 2> rId{ UiaAppendRuntimeId, -1 };

    const std::basic_string_view<int> span{ rId.data(), rId.size() };
    // BuildIntSafeArray is a custom function to hide the SafeArray creation
    *ppRuntimeId = BuildIntSafeArray(span);
    RETURN_IF_NULL_ALLOC(*ppRuntimeId);

    return S_OK;
}

IFACEMETHODIMP ScreenInfoUiaProviderBase::GetEmbeddedFragmentRoots(_Outptr_result_maybenull_ SAFEARRAY** ppRoots) noexcept
{
    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(this, ApiCall::GetEmbeddedFragmentRoots, nullptr);

    RETURN_HR_IF(E_INVALIDARG, ppRoots == nullptr);
    *ppRoots = nullptr;
    return S_OK;
}

IFACEMETHODIMP ScreenInfoUiaProviderBase::SetFocus()
{
    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(this, ApiCall::SetFocus, nullptr);

    return Signal(UIA_AutomationFocusChangedEventId);
}

#pragma endregion

#pragma region ITextProvider

IFACEMETHODIMP ScreenInfoUiaProviderBase::GetSelection(_Outptr_result_maybenull_ SAFEARRAY** ppRetVal)
{
    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //ApiMsgGetSelection apiMsg;

    _LockConsole();
    auto Unlock = wil::scope_exit([&]() noexcept {
        _UnlockConsole();
    });

    RETURN_HR_IF_NULL(E_INVALIDARG, ppRetVal);
    *ppRetVal = nullptr;
    HRESULT hr = S_OK;

    if (!_pData->IsSelectionActive())
    {
        // TODO GitHub #1914: Re-attach Tracing to UIA Tree
        //apiMsg.AreaSelected = false;
        //apiMsg.SelectionRowCount = 1;

        // return a degenerate range at the cursor position
        const Cursor& cursor = _getTextBuffer().GetCursor();

        // make a safe array
        *ppRetVal = SafeArrayCreateVector(VT_UNKNOWN, 0, 1);
        if (*ppRetVal == nullptr)
        {
            return E_OUTOFMEMORY;
        }

        if (FAILED(hr))
        {
            SafeArrayDestroy(*ppRetVal);
            *ppRetVal = nullptr;
            return hr;
        }

        WRL::ComPtr<UiaTextRangeBase> range;
        hr = CreateTextRange(this, cursor, &range);
        if (FAILED(hr))
        {
            SafeArrayDestroy(*ppRetVal);
            *ppRetVal = nullptr;
            return hr;
        }

        LONG currentIndex = 0;
        hr = SafeArrayPutElement(*ppRetVal, &currentIndex, range.Detach());
        if (FAILED(hr))
        {
            SafeArrayDestroy(*ppRetVal);
            *ppRetVal = nullptr;
            return hr;
        }
    }
    else
    {
        // get the selection ranges
        std::deque<WRL::ComPtr<UiaTextRangeBase>> ranges;
        RETURN_IF_FAILED(GetSelectionRanges(this, ranges));

        // TODO GitHub #1914: Re-attach Tracing to UIA Tree
        //apiMsg.AreaSelected = true;
        //apiMsg.SelectionRowCount = static_cast<unsigned int>(ranges.size());

        // make a safe array
        *ppRetVal = SafeArrayCreateVector(VT_UNKNOWN, 0, gsl::narrow<ULONG>(ranges.size()));
        if (*ppRetVal == nullptr)
        {
            return E_OUTOFMEMORY;
        }

        // fill the safe array
        for (LONG i = 0; i < gsl::narrow<LONG>(ranges.size()); ++i)
        {
            hr = SafeArrayPutElement(*ppRetVal, &i, ranges.at(i).Detach());
            if (FAILED(hr))
            {
                SafeArrayDestroy(*ppRetVal);
                *ppRetVal = nullptr;
                return hr;
            }
        }
    }

    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(this, ApiCall::GetSelection, &apiMsg);
    return S_OK;
}

IFACEMETHODIMP ScreenInfoUiaProviderBase::GetVisibleRanges(_Outptr_result_maybenull_ SAFEARRAY** ppRetVal)
{
    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(this, ApiCall::GetVisibleRanges, nullptr);

    _LockConsole();
    auto Unlock = wil::scope_exit([&]() noexcept {
        _UnlockConsole();
    });

    RETURN_HR_IF_NULL(E_INVALIDARG, ppRetVal);
    *ppRetVal = nullptr;

    const auto viewport = _getViewport();
    const COORD screenBufferCoords = _getScreenBufferCoords();
    const int totalLines = screenBufferCoords.Y;

    // make a safe array
    const size_t rowCount = viewport.Height();
    *ppRetVal = SafeArrayCreateVector(VT_UNKNOWN, 0, gsl::narrow<ULONG>(rowCount));
    if (*ppRetVal == nullptr)
    {
        return E_OUTOFMEMORY;
    }

    // stuff each visible line in the safearray
    for (size_t i = 0; i < rowCount; ++i)
    {
        const int lineNumber = (viewport.Top() + i) % totalLines;
        const int start = lineNumber * screenBufferCoords.X;
        // - 1 to get the last column in the row
        const int end = start + screenBufferCoords.X - 1;

        HRESULT hr = S_OK;
        WRL::ComPtr<UiaTextRangeBase> range;
        hr = CreateTextRange(this,
                             start,
                             end,
                             false,
                             &range);
        if (FAILED(hr))
        {
            SafeArrayDestroy(*ppRetVal);
            *ppRetVal = nullptr;
            return hr;
        }

        LONG currentIndex = gsl::narrow<LONG>(i);
        hr = SafeArrayPutElement(*ppRetVal, &currentIndex, range.Detach());
        if (FAILED(hr))
        {
            SafeArrayDestroy(*ppRetVal);
            *ppRetVal = nullptr;
            return hr;
        }
    }
    return S_OK;
}

IFACEMETHODIMP ScreenInfoUiaProviderBase::RangeFromChild(_In_ IRawElementProviderSimple* /*childElement*/,
                                                         _COM_Outptr_result_maybenull_ ITextRangeProvider** ppRetVal)
{
    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(this, ApiCall::RangeFromChild, nullptr);

    RETURN_HR_IF_NULL(E_INVALIDARG, ppRetVal);
    *ppRetVal = nullptr;

    WRL::ComPtr<UiaTextRangeBase> utr;
    RETURN_IF_FAILED(CreateTextRange(this, &utr));
    RETURN_IF_FAILED(utr.CopyTo(ppRetVal));
    return S_OK;
}

IFACEMETHODIMP ScreenInfoUiaProviderBase::RangeFromPoint(_In_ UiaPoint point,
                                                         _COM_Outptr_result_maybenull_ ITextRangeProvider** ppRetVal)
{
    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(this, ApiCall::RangeFromPoint, nullptr);

    RETURN_HR_IF_NULL(E_INVALIDARG, ppRetVal);
    *ppRetVal = nullptr;

    WRL::ComPtr<UiaTextRangeBase> utr;
    RETURN_IF_FAILED(CreateTextRange(this,
                                     point,
                                     &utr));
    RETURN_IF_FAILED(utr.CopyTo(ppRetVal));
    return S_OK;
}

IFACEMETHODIMP ScreenInfoUiaProviderBase::get_DocumentRange(_COM_Outptr_result_maybenull_ ITextRangeProvider** ppRetVal)
{
    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(this, ApiCall::GetDocumentRange, nullptr);

    RETURN_HR_IF_NULL(E_INVALIDARG, ppRetVal);
    *ppRetVal = nullptr;

    WRL::ComPtr<UiaTextRangeBase> utr;
    RETURN_IF_FAILED(CreateTextRange(this, &utr));
    RETURN_IF_FAILED(utr->ExpandToEnclosingUnit(TextUnit::TextUnit_Document));
    RETURN_IF_FAILED(utr.CopyTo(ppRetVal));
    return S_OK;
}

IFACEMETHODIMP ScreenInfoUiaProviderBase::get_SupportedTextSelection(_Out_ SupportedTextSelection* pRetVal) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pRetVal);

    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(this, ApiCall::GetSupportedTextSelection, nullptr);

    *pRetVal = SupportedTextSelection::SupportedTextSelection_Single;
    return S_OK;
}

#pragma endregion

const COORD ScreenInfoUiaProviderBase::_getScreenBufferCoords() const
{
    return _getTextBuffer().GetSize().Dimensions();
}

const TextBuffer& ScreenInfoUiaProviderBase::_getTextBuffer() const noexcept
{
    return _pData->GetTextBuffer();
}

const Viewport ScreenInfoUiaProviderBase::_getViewport() const noexcept
{
    return _pData->GetViewport();
}

void ScreenInfoUiaProviderBase::_LockConsole() noexcept
{
    // TODO GitHub #2141: Lock and Unlock in conhost should decouple Ctrl+C dispatch and use smarter handling
    _pData->LockConsole();
}

void ScreenInfoUiaProviderBase::_UnlockConsole() noexcept
{
    // TODO GitHub #2141: Lock and Unlock in conhost should decouple Ctrl+C dispatch and use smarter handling
    _pData->UnlockConsole();
}
