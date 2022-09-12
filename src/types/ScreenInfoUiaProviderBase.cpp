// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "ScreenInfoUiaProviderBase.h"
#include "UiaTracing.h"

using namespace Microsoft::Console::Types;

// A helper function to create a SafeArray Version of an int array of a specified length
SAFEARRAY* BuildIntSafeArray(gsl::span<const int> data)
{
    auto psa = SafeArrayCreateVector(VT_I4, 0, gsl::narrow<ULONG>(data.size()));
    if (psa != nullptr)
    {
        LONG lIndex{ 0 };
        for (auto val : data)
        {
            if (FAILED(SafeArrayPutElement(psa, &lIndex, (void*)&val)))
            {
                SafeArrayDestroy(psa);
                psa = nullptr;
                break;
            }
            ++lIndex;
        }
    }

    return psa;
}

#pragma warning(suppress : 26434) // WRL RuntimeClassInitialize base is a no-op and we need this for MakeAndInitialize
HRESULT ScreenInfoUiaProviderBase::RuntimeClassInitialize(_In_ IUiaData* pData, _In_ std::wstring_view wordDelimiters) noexcept
try
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pData);
    _pData = pData;
    _wordDelimiters = wordDelimiters;

    UiaTracing::TextProvider::Constructor(*this);
    return S_OK;
}
CATCH_RETURN();

[[nodiscard]] HRESULT ScreenInfoUiaProviderBase::Signal(_In_ EVENTID eventId)
{
    auto hr = S_OK;
    // check to see if we're already firing this particular event
    if (_signalFiringMapping.find(eventId) != _signalFiringMapping.end() &&
        _signalFiringMapping[eventId] == true)
    {
        return hr;
    }

    try
    {
        _signalFiringMapping[eventId] = true;
    }
    CATCH_RETURN();

    IRawElementProviderSimple* pProvider = this;
    hr = UiaRaiseAutomationEvent(pProvider, eventId);
    _signalFiringMapping[eventId] = false;

    return hr;
}

#pragma region IRawElementProviderSimple

// Implementation of IRawElementProviderSimple::get_ProviderOptions.
// Gets UI Automation provider options.
IFACEMETHODIMP ScreenInfoUiaProviderBase::get_ProviderOptions(_Out_ ProviderOptions* pOptions) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pOptions);

    *pOptions = ProviderOptions_ServerSideProvider;
    UiaTracing::TextProvider::get_ProviderOptions(*this, *pOptions);
    return S_OK;
}

// Implementation of IRawElementProviderSimple::get_PatternProvider.
// Gets the object that supports ISelectionPattern.
IFACEMETHODIMP ScreenInfoUiaProviderBase::GetPatternProvider(_In_ PATTERNID patternId,
                                                             _COM_Outptr_result_maybenull_ IUnknown** ppInterface)
{
    RETURN_HR_IF(E_INVALIDARG, ppInterface == nullptr);
    *ppInterface = nullptr;

    auto hr = S_OK;
    if (patternId == UIA_TextPatternId)
    {
        hr = QueryInterface(IID_PPV_ARGS(ppInterface));
        if (FAILED(hr))
        {
            *ppInterface = nullptr;
        }
    }
    UiaTracing::TextProvider::GetPatternProvider(*this, patternId);
    return hr;
}

// Implementation of IRawElementProviderSimple::get_PropertyValue.
// Gets custom properties.
IFACEMETHODIMP ScreenInfoUiaProviderBase::GetPropertyValue(_In_ PROPERTYID propertyId,
                                                           _Out_ VARIANT* pVariant) noexcept
{
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

    UiaTracing::TextProvider::GetPropertyValue(*this, propertyId);
    return S_OK;
}

IFACEMETHODIMP ScreenInfoUiaProviderBase::get_HostRawElementProvider(_COM_Outptr_result_maybenull_ IRawElementProviderSimple** ppProvider) noexcept
{
    RETURN_HR_IF(E_INVALIDARG, ppProvider == nullptr);
    *ppProvider = nullptr;
    UiaTracing::TextProvider::get_HostRawElementProvider(*this);
    return S_OK;
}
#pragma endregion

#pragma region IRawElementProviderFragment

IFACEMETHODIMP ScreenInfoUiaProviderBase::GetRuntimeId(_Outptr_result_maybenull_ SAFEARRAY** ppRuntimeId)
{
    // Root defers this to host, others must implement it...
    RETURN_HR_IF(E_INVALIDARG, ppRuntimeId == nullptr);
    *ppRuntimeId = nullptr;

    // AppendRuntimeId is a magic Number that tells UIAutomation to Append its own Runtime ID(From the HWND)
    const std::array<int, 2> rId{ UiaAppendRuntimeId, -1 };

    const gsl::span<const int> span{ rId.data(), rId.size() };
    // BuildIntSafeArray is a custom function to hide the SafeArray creation
    *ppRuntimeId = BuildIntSafeArray(span);
    RETURN_IF_NULL_ALLOC(*ppRuntimeId);

    UiaTracing::TextProvider::GetRuntimeId(*this);
    return S_OK;
}

IFACEMETHODIMP ScreenInfoUiaProviderBase::GetEmbeddedFragmentRoots(_Outptr_result_maybenull_ SAFEARRAY** ppRoots) noexcept
{
    RETURN_HR_IF(E_INVALIDARG, ppRoots == nullptr);
    *ppRoots = nullptr;
    UiaTracing::TextProvider::GetEmbeddedFragmentRoots(*this);
    return S_OK;
}

IFACEMETHODIMP ScreenInfoUiaProviderBase::SetFocus()
{
    UiaTracing::TextProvider::SetFocus(*this);
    return Signal(UIA_AutomationFocusChangedEventId);
}

#pragma endregion

#pragma region ITextProvider

IFACEMETHODIMP ScreenInfoUiaProviderBase::GetSelection(_Outptr_result_maybenull_ SAFEARRAY** ppRetVal)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, ppRetVal);
    *ppRetVal = nullptr;

    _LockConsole();
    auto Unlock = wil::scope_exit([&]() noexcept {
        _UnlockConsole();
    });
    RETURN_HR_IF(E_FAIL, !_pData->IsUiaDataInitialized());

    // make a safe array
    auto hr = S_OK;
    *ppRetVal = SafeArrayCreateVector(VT_UNKNOWN, 0, 1);
    RETURN_HR_IF_NULL(E_OUTOFMEMORY, *ppRetVal);

    WRL::ComPtr<UiaTextRangeBase> range;
    if (!_pData->IsSelectionActive())
    {
        // return a degenerate range at the cursor position
        const auto& cursor = _getTextBuffer().GetCursor();
        hr = CreateTextRange(this, cursor, _wordDelimiters, &range);
    }
    else
    {
        // get the selection range
        hr = GetSelectionRange(this, _wordDelimiters, &range);
    }

    if (FAILED(hr))
    {
        SafeArrayDestroy(*ppRetVal);
        *ppRetVal = nullptr;
        return hr;
    }

    UiaTracing::TextProvider::GetSelection(*this, *range.Get());

    LONG currentIndex = 0;
    hr = SafeArrayPutElement(*ppRetVal, &currentIndex, range.Detach());
    if (FAILED(hr))
    {
        SafeArrayDestroy(*ppRetVal);
        *ppRetVal = nullptr;
        return hr;
    }

    return S_OK;
}

IFACEMETHODIMP ScreenInfoUiaProviderBase::GetVisibleRanges(_Outptr_result_maybenull_ SAFEARRAY** ppRetVal)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, ppRetVal);
    *ppRetVal = nullptr;

    _LockConsole();
    auto Unlock = wil::scope_exit([&]() noexcept {
        _UnlockConsole();
    });
    RETURN_HR_IF(E_FAIL, !_pData->IsUiaDataInitialized());

    // make a safe array
    *ppRetVal = SafeArrayCreateVector(VT_UNKNOWN, 0, 1);
    RETURN_HR_IF_NULL(E_OUTOFMEMORY, *ppRetVal);

    WRL::ComPtr<UiaTextRangeBase> range;
    const auto bufferSize = _pData->GetTextBuffer().GetSize();
    const auto viewport = bufferSize.ConvertToOrigin(_getViewport());

    const til::point start{ viewport.Left(), viewport.Top() };
    const til::point end{ viewport.Left(), viewport.BottomExclusive() };

    auto hr = CreateTextRange(this, start, end, _wordDelimiters, &range);
    if (FAILED(hr))
    {
        SafeArrayDestroy(*ppRetVal);
        *ppRetVal = nullptr;
        return hr;
    }

    UiaTracing::TextProvider::GetVisibleRanges(*this, *range.Get());

    LONG currentIndex = 0;
    hr = SafeArrayPutElement(*ppRetVal, &currentIndex, range.Detach());
    if (FAILED(hr))
    {
        SafeArrayDestroy(*ppRetVal);
        *ppRetVal = nullptr;
        return hr;
    }

    return S_OK;
}

IFACEMETHODIMP ScreenInfoUiaProviderBase::RangeFromChild(_In_ IRawElementProviderSimple* /*childElement*/,
                                                         _COM_Outptr_result_maybenull_ ITextRangeProvider** ppRetVal)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, ppRetVal);
    *ppRetVal = nullptr;

    WRL::ComPtr<UiaTextRangeBase> utr;
    RETURN_IF_FAILED(CreateTextRange(this, _wordDelimiters, &utr));
    RETURN_IF_FAILED(utr.CopyTo(ppRetVal));
    UiaTracing::TextProvider::RangeFromChild(*this, *utr.Get());
    return S_OK;
}

IFACEMETHODIMP ScreenInfoUiaProviderBase::RangeFromPoint(_In_ UiaPoint point,
                                                         _COM_Outptr_result_maybenull_ ITextRangeProvider** ppRetVal)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, ppRetVal);
    *ppRetVal = nullptr;

    WRL::ComPtr<UiaTextRangeBase> utr;
    RETURN_IF_FAILED(CreateTextRange(this,
                                     point,
                                     _wordDelimiters,
                                     &utr));
    RETURN_IF_FAILED(utr.CopyTo(ppRetVal));
    UiaTracing::TextProvider::RangeFromPoint(*this, point, *utr.Get());
    return S_OK;
}

IFACEMETHODIMP ScreenInfoUiaProviderBase::get_DocumentRange(_COM_Outptr_result_maybenull_ ITextRangeProvider** ppRetVal)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, ppRetVal);
    *ppRetVal = nullptr;

    WRL::ComPtr<UiaTextRangeBase> utr;
    RETURN_IF_FAILED(CreateTextRange(this, _wordDelimiters, &utr));
    RETURN_IF_FAILED(utr->ExpandToEnclosingUnit(TextUnit::TextUnit_Document));
    RETURN_IF_FAILED(utr.CopyTo(ppRetVal));
    UiaTracing::TextProvider::get_DocumentRange(*this, *utr.Get());
    return S_OK;
}

IFACEMETHODIMP ScreenInfoUiaProviderBase::get_SupportedTextSelection(_Out_ SupportedTextSelection* pRetVal) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pRetVal);

    *pRetVal = SupportedTextSelection::SupportedTextSelection_Single;
    UiaTracing::TextProvider::get_SupportedTextSelection(*this, *pRetVal);
    return S_OK;
}

#pragma endregion

til::size ScreenInfoUiaProviderBase::_getScreenBufferCoords() const noexcept
{
    return _getTextBuffer().GetSize().Dimensions();
}

const TextBuffer& ScreenInfoUiaProviderBase::_getTextBuffer() const noexcept
{
    return _pData->GetTextBuffer();
}

Viewport ScreenInfoUiaProviderBase::_getViewport() const noexcept
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
