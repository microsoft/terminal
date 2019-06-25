// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "screenInfoUiaProvider.hpp"
#include "../../host/screenInfo.hpp"
#include "../inc/ServiceLocator.hpp"

#include "windowUiaProvider.hpp"
#include "window.hpp"
#include "windowdpiapi.hpp"

#include "UiaTextRange.hpp"

using namespace Microsoft::Console::Interactivity::Win32;
using namespace Microsoft::Console::Interactivity::Win32::ScreenInfoUiaProviderTracing;
using namespace Microsoft::Console::Interactivity;
// A helper function to create a SafeArray Version of an int array of a specified length
SAFEARRAY* BuildIntSafeArray(_In_reads_(length) const int* const data, const int length)
{
    SAFEARRAY* psa = SafeArrayCreateVector(VT_I4, 0, length);
    if (psa != nullptr)
    {
        for (long i = 0; i < length; i++)
        {
            if (FAILED(SafeArrayPutElement(psa, &i, (void*)&(data[i]))))
            {
                SafeArrayDestroy(psa);
                psa = nullptr;
                break;
            }
        }
    }

    return psa;
}

ScreenInfoUiaProvider::ScreenInfoUiaProvider(_In_ WindowUiaProvider* const pUiaParent) :
    _pUiaParent(THROW_HR_IF_NULL(E_INVALIDARG, pUiaParent)),
    _signalFiringMapping{},
    _cRefs(1)
{
    Tracing::s_TraceUia(nullptr, ApiCall::Constructor, nullptr);
}

ScreenInfoUiaProvider::~ScreenInfoUiaProvider()
{
}

[[nodiscard]] HRESULT ScreenInfoUiaProvider::Signal(_In_ EVENTID id)
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

    IRawElementProviderSimple* pProvider = static_cast<IRawElementProviderSimple*>(this);
    hr = UiaRaiseAutomationEvent(pProvider, id);
    _signalFiringMapping[id] = false;

    // tracing
    ApiMsgSignal apiMsg;
    apiMsg.Signal = id;
    Tracing::s_TraceUia(this, ApiCall::Signal, &apiMsg);
    return hr;
}

#pragma region IUnknown

IFACEMETHODIMP_(ULONG)
ScreenInfoUiaProvider::AddRef()
{
    Tracing::s_TraceUia(this, ApiCall::AddRef, nullptr);
    return InterlockedIncrement(&_cRefs);
}

IFACEMETHODIMP_(ULONG)
ScreenInfoUiaProvider::Release()
{
    Tracing::s_TraceUia(this, ApiCall::Release, nullptr);
    long val = InterlockedDecrement(&_cRefs);
    if (val == 0)
    {
        delete this;
    }
    return val;
}

IFACEMETHODIMP ScreenInfoUiaProvider::QueryInterface(_In_ REFIID riid,
                                                     _COM_Outptr_result_maybenull_ void** ppInterface)
{
    Tracing::s_TraceUia(this, ApiCall::QueryInterface, nullptr);
    if (riid == __uuidof(IUnknown))
    {
        *ppInterface = static_cast<IRawElementProviderSimple*>(this);
    }
    else if (riid == __uuidof(IRawElementProviderSimple))
    {
        *ppInterface = static_cast<IRawElementProviderSimple*>(this);
    }
    else if (riid == __uuidof(IRawElementProviderFragment))
    {
        *ppInterface = static_cast<IRawElementProviderFragment*>(this);
    }
    else if (riid == __uuidof(ITextProvider))
    {
        *ppInterface = static_cast<ITextProvider*>(this);
    }
    else
    {
        *ppInterface = nullptr;
        return E_NOINTERFACE;
    }

    (static_cast<IUnknown*>(*ppInterface))->AddRef();

    return S_OK;
}

#pragma endregion

#pragma region IRawElementProviderSimple

// Implementation of IRawElementProviderSimple::get_ProviderOptions.
// Gets UI Automation provider options.
IFACEMETHODIMP ScreenInfoUiaProvider::get_ProviderOptions(_Out_ ProviderOptions* pOptions)
{
    Tracing::s_TraceUia(this, ApiCall::GetProviderOptions, nullptr);
    *pOptions = ProviderOptions_ServerSideProvider;
    return S_OK;
}

// Implementation of IRawElementProviderSimple::get_PatternProvider.
// Gets the object that supports ISelectionPattern.
IFACEMETHODIMP ScreenInfoUiaProvider::GetPatternProvider(_In_ PATTERNID patternId,
                                                         _COM_Outptr_result_maybenull_ IUnknown** ppInterface)
{
    Tracing::s_TraceUia(this, ApiCall::GetPatternProvider, nullptr);

    *ppInterface = nullptr;
    HRESULT hr = S_OK;

    if (patternId == UIA_TextPatternId)
    {
        hr = this->QueryInterface(__uuidof(ITextProvider), reinterpret_cast<void**>(ppInterface));
        if (FAILED(hr))
        {
            *ppInterface = nullptr;
        }
    }
    return hr;
}

// Implementation of IRawElementProviderSimple::get_PropertyValue.
// Gets custom properties.
IFACEMETHODIMP ScreenInfoUiaProvider::GetPropertyValue(_In_ PROPERTYID propertyId,
                                                       _Out_ VARIANT* pVariant)
{
    Tracing::s_TraceUia(this, ApiCall::GetPropertyValue, nullptr);

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

IFACEMETHODIMP ScreenInfoUiaProvider::get_HostRawElementProvider(_COM_Outptr_result_maybenull_ IRawElementProviderSimple** ppProvider)
{
    Tracing::s_TraceUia(this, ApiCall::GetHostRawElementProvider, nullptr);
    *ppProvider = nullptr;

    return S_OK;
}
#pragma endregion

#pragma region IRawElementProviderFragment

IFACEMETHODIMP ScreenInfoUiaProvider::Navigate(_In_ NavigateDirection direction,
                                               _COM_Outptr_result_maybenull_ IRawElementProviderFragment** ppProvider)
{
    ApiMsgNavigate apiMsg;
    apiMsg.Direction = direction;
    Tracing::s_TraceUia(this, ApiCall::Navigate, &apiMsg);
    *ppProvider = nullptr;

    if (direction == NavigateDirection_Parent)
    {
        try
        {
            _pUiaParent->QueryInterface(IID_PPV_ARGS(ppProvider));
        }
        catch (...)
        {
            *ppProvider = nullptr;
            return wil::ResultFromCaughtException();
        }
        RETURN_IF_NULL_ALLOC(*ppProvider);
    }

    // For the other directions the default of nullptr is correct
    return S_OK;
}

IFACEMETHODIMP ScreenInfoUiaProvider::GetRuntimeId(_Outptr_result_maybenull_ SAFEARRAY** ppRuntimeId)
{
    Tracing::s_TraceUia(this, ApiCall::GetRuntimeId, nullptr);
    // Root defers this to host, others must implement it...
    *ppRuntimeId = nullptr;

    // AppendRuntimeId is a magic Number that tells UIAutomation to Append its own Runtime ID(From the HWND)
    int rId[] = { UiaAppendRuntimeId, -1 };
    // BuildIntSafeArray is a custom function to hide the SafeArray creation
    *ppRuntimeId = BuildIntSafeArray(rId, 2);
    RETURN_IF_NULL_ALLOC(*ppRuntimeId);

    return S_OK;
}

IFACEMETHODIMP ScreenInfoUiaProvider::get_BoundingRectangle(_Out_ UiaRect* pRect)
{
    Tracing::s_TraceUia(this, ApiCall::GetBoundingRectangle, nullptr);
    const IConsoleWindow* const pIConsoleWindow = _getIConsoleWindow();
    RETURN_HR_IF_NULL((HRESULT)UIA_E_ELEMENTNOTAVAILABLE, pIConsoleWindow);

    RECT rc = pIConsoleWindow->GetWindowRect();

    pRect->left = rc.left;
    pRect->top = rc.top;
    pRect->width = rc.right - rc.left;
    pRect->height = rc.bottom - rc.top;

    return S_OK;
}

IFACEMETHODIMP ScreenInfoUiaProvider::GetEmbeddedFragmentRoots(_Outptr_result_maybenull_ SAFEARRAY** ppRoots)
{
    Tracing::s_TraceUia(this, ApiCall::GetEmbeddedFragmentRoots, nullptr);
    *ppRoots = nullptr;
    return S_OK;
}

IFACEMETHODIMP ScreenInfoUiaProvider::SetFocus()
{
    Tracing::s_TraceUia(this, ApiCall::SetFocus, nullptr);
    return Signal(UIA_AutomationFocusChangedEventId);
}

IFACEMETHODIMP ScreenInfoUiaProvider::get_FragmentRoot(_COM_Outptr_result_maybenull_ IRawElementProviderFragmentRoot** ppProvider)
{
    Tracing::s_TraceUia(this, ApiCall::GetFragmentRoot, nullptr);
    try
    {
        _pUiaParent->QueryInterface(IID_PPV_ARGS(ppProvider));
    }
    catch (...)
    {
        *ppProvider = nullptr;
        return wil::ResultFromCaughtException();
    }
    RETURN_IF_NULL_ALLOC(*ppProvider);
    return S_OK;
}

#pragma endregion

#pragma region ITextProvider

IFACEMETHODIMP ScreenInfoUiaProvider::GetSelection(_Outptr_result_maybenull_ SAFEARRAY** ppRetVal)
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    ApiMsgGetSelection apiMsg;
    gci.LockConsole();
    auto Unlock = wil::scope_exit([&] {
        gci.UnlockConsole();
    });

    *ppRetVal = nullptr;
    HRESULT hr = S_OK;

    if (!Selection::Instance().IsAreaSelected())
    {
        apiMsg.AreaSelected = false;
        apiMsg.SelectionRowCount = 1;
        // return a degenerate range at the cursor position
        SCREEN_INFORMATION& screenInfo = _getScreenInfo();
        const Cursor& cursor = screenInfo.GetTextBuffer().GetCursor();

        // make a safe array
        *ppRetVal = SafeArrayCreateVector(VT_UNKNOWN, 0, 1);
        if (*ppRetVal == nullptr)
        {
            return E_OUTOFMEMORY;
        }

        IRawElementProviderSimple* pProvider;
        hr = this->QueryInterface(IID_PPV_ARGS(&pProvider));
        if (FAILED(hr))
        {
            SafeArrayDestroy(*ppRetVal);
            *ppRetVal = nullptr;
            return hr;
        }

        UiaTextRange* range;
        try
        {
            range = UiaTextRange::Create(pProvider,
                                         cursor);
        }
        catch (...)
        {
            range = nullptr;
            hr = wil::ResultFromCaughtException();
        }
        (static_cast<IUnknown*>(pProvider))->Release();
        if (range == nullptr)
        {
            SafeArrayDestroy(*ppRetVal);
            *ppRetVal = nullptr;
            return hr;
        }

        LONG currentIndex = 0;
        hr = SafeArrayPutElement(*ppRetVal, &currentIndex, reinterpret_cast<void*>(range));
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
        std::deque<UiaTextRange*> ranges;
        IRawElementProviderSimple* pProvider;
        RETURN_IF_FAILED(QueryInterface(IID_PPV_ARGS(&pProvider)));
        try
        {
            ranges = UiaTextRange::GetSelectionRanges(pProvider);
        }
        catch (...)
        {
            hr = wil::ResultFromCaughtException();
        }
        pProvider->Release();
        RETURN_IF_FAILED(hr);

        apiMsg.AreaSelected = true;
        apiMsg.SelectionRowCount = static_cast<unsigned int>(ranges.size());

        // make a safe array
        *ppRetVal = SafeArrayCreateVector(VT_UNKNOWN, 0, static_cast<ULONG>(ranges.size()));
        if (*ppRetVal == nullptr)
        {
            return E_OUTOFMEMORY;
        }

        // fill the safe array
        for (LONG i = 0; i < static_cast<LONG>(ranges.size()); ++i)
        {
            hr = SafeArrayPutElement(*ppRetVal, &i, reinterpret_cast<void*>(ranges[i]));
            if (FAILED(hr))
            {
                SafeArrayDestroy(*ppRetVal);
                *ppRetVal = nullptr;
                while (!ranges.empty())
                {
                    UiaTextRange* pRange = ranges[0];
                    ranges.pop_front();
                    pRange->Release();
                }
                return hr;
            }
        }
    }

    Tracing::s_TraceUia(this, ApiCall::GetSelection, &apiMsg);
    return S_OK;
}

IFACEMETHODIMP ScreenInfoUiaProvider::GetVisibleRanges(_Outptr_result_maybenull_ SAFEARRAY** ppRetVal)
{
    Tracing::s_TraceUia(this, ApiCall::GetVisibleRanges, nullptr);
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    gci.LockConsole();
    auto Unlock = wil::scope_exit([&] {
        gci.UnlockConsole();
    });

    const SCREEN_INFORMATION& screenInfo = _getScreenInfo();
    const auto viewport = screenInfo.GetViewport();
    const COORD screenBufferCoords = _getScreenBufferCoords();
    const int totalLines = screenBufferCoords.Y;

    // make a safe array
    const size_t rowCount = viewport.Height();
    *ppRetVal = SafeArrayCreateVector(VT_UNKNOWN, 0, static_cast<ULONG>(rowCount));
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

        IRawElementProviderSimple* pProvider;
        HRESULT hr = this->QueryInterface(IID_PPV_ARGS(&pProvider));
        if (FAILED(hr))
        {
            SafeArrayDestroy(*ppRetVal);
            *ppRetVal = nullptr;
            return hr;
        }

        UiaTextRange* range;
        try
        {
            range = UiaTextRange::Create(pProvider,
                                         start,
                                         end,
                                         false);
        }
        catch (...)
        {
            range = nullptr;
            hr = wil::ResultFromCaughtException();
        }
        (static_cast<IUnknown*>(pProvider))->Release();

        if (range == nullptr)
        {
            SafeArrayDestroy(*ppRetVal);
            *ppRetVal = nullptr;
            return hr;
        }

        LONG currentIndex = static_cast<LONG>(i);
        hr = SafeArrayPutElement(*ppRetVal, &currentIndex, reinterpret_cast<void*>(range));
        if (FAILED(hr))
        {
            SafeArrayDestroy(*ppRetVal);
            *ppRetVal = nullptr;
            return hr;
        }
    }
    return S_OK;
}

IFACEMETHODIMP ScreenInfoUiaProvider::RangeFromChild(_In_ IRawElementProviderSimple* /*childElement*/,
                                                     _COM_Outptr_result_maybenull_ ITextRangeProvider** ppRetVal)
{
    Tracing::s_TraceUia(this, ApiCall::RangeFromChild, nullptr);

    IRawElementProviderSimple* pProvider;
    RETURN_IF_FAILED(this->QueryInterface(IID_PPV_ARGS(&pProvider)));

    HRESULT hr = S_OK;
    try
    {
        *ppRetVal = UiaTextRange::Create(pProvider);
    }
    catch (...)
    {
        *ppRetVal = nullptr;
        hr = wil::ResultFromCaughtException();
    }
    (static_cast<IUnknown*>(pProvider))->Release();

    return hr;
}

IFACEMETHODIMP ScreenInfoUiaProvider::RangeFromPoint(_In_ UiaPoint point,
                                                     _COM_Outptr_result_maybenull_ ITextRangeProvider** ppRetVal)
{
    Tracing::s_TraceUia(this, ApiCall::RangeFromPoint, nullptr);
    IRawElementProviderSimple* pProvider;
    RETURN_IF_FAILED(this->QueryInterface(IID_PPV_ARGS(&pProvider)));

    HRESULT hr = S_OK;
    try
    {
        *ppRetVal = UiaTextRange::Create(pProvider,
                                         point);
    }
    catch (...)
    {
        *ppRetVal = nullptr;
        hr = wil::ResultFromCaughtException();
    }
    (static_cast<IUnknown*>(pProvider))->Release();

    return hr;
}

IFACEMETHODIMP ScreenInfoUiaProvider::get_DocumentRange(_COM_Outptr_result_maybenull_ ITextRangeProvider** ppRetVal)
{
    Tracing::s_TraceUia(this, ApiCall::GetDocumentRange, nullptr);
    IRawElementProviderSimple* pProvider;
    RETURN_IF_FAILED(this->QueryInterface(IID_PPV_ARGS(&pProvider)));

    HRESULT hr = S_OK;
    try
    {
        *ppRetVal = UiaTextRange::Create(pProvider);
    }
    catch (...)
    {
        *ppRetVal = nullptr;
        hr = wil::ResultFromCaughtException();
    }
    (static_cast<IUnknown*>(pProvider))->Release();

    if (*ppRetVal)
    {
        (*ppRetVal)->ExpandToEnclosingUnit(TextUnit::TextUnit_Document);
    }

    return hr;
}

IFACEMETHODIMP ScreenInfoUiaProvider::get_SupportedTextSelection(_Out_ SupportedTextSelection* pRetVal)
{
    Tracing::s_TraceUia(this, ApiCall::GetSupportedTextSelection, nullptr);
    *pRetVal = SupportedTextSelection::SupportedTextSelection_Single;
    return S_OK;
}

#pragma endregion

const COORD ScreenInfoUiaProvider::_getScreenBufferCoords() const
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return gci.GetScreenBufferSize();
}

SCREEN_INFORMATION& ScreenInfoUiaProvider::_getScreenInfo()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    THROW_HR_IF(E_POINTER, !gci.HasActiveOutputBuffer());
    return gci.GetActiveOutputBuffer();
}

IConsoleWindow* const ScreenInfoUiaProvider::_getIConsoleWindow()
{
    return ServiceLocator::LocateConsoleWindow();
}
