// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "XamlUiaTextRange.h"
#include "../types/TermControlUiaTextRange.hpp"
#include <UIAutomationClient.h>
#include <UIAutomationCoreApi.h>
#include "../types/UiaTracing.h"

// the same as COR_E_NOTSUPPORTED
// we don't want to import the CLR headers to get it
#define XAML_E_NOT_SUPPORTED 0x80131515L

namespace UIA
{
    using ::ITextRangeProvider;
    using ::SupportedTextSelection;
    using ::TextPatternRangeEndpoint;
    using ::TextUnit;
}

namespace XamlAutomation
{
    using winrt::Windows::UI::Xaml::Automation::SupportedTextSelection;
    using winrt::Windows::UI::Xaml::Automation::Provider::IRawElementProviderSimple;
    using winrt::Windows::UI::Xaml::Automation::Provider::ITextRangeProvider;
    using winrt::Windows::UI::Xaml::Automation::Text::TextPatternRangeEndpoint;
    using winrt::Windows::UI::Xaml::Automation::Text::TextUnit;
}

static std::atomic<int64_t> refCount_XamlUiaTextRange;

namespace winrt::Microsoft::Terminal::Control::implementation
{
    XamlAutomation::ITextRangeProvider XamlUiaTextRange::Create(::ITextRangeProvider* uiaProvider, Windows::UI::Xaml::Automation::Provider::IRawElementProviderSimple parentProvider)
    {
        const auto self = new XamlUiaTextRange(uiaProvider, parentProvider);
        XamlAutomation::ITextRangeProvider provider{ nullptr };
        winrt::attach_abi(provider, (ABI::Windows::UI::Xaml::Automation::Provider::ITextRangeProvider*)self);
        return provider;
    }

    XamlUiaTextRange::XamlUiaTextRange(::ITextRangeProvider* uiaProvider, Windows::UI::Xaml::Automation::Provider::IRawElementProviderSimple parentProvider) :
        _parentProvider{ std::move(parentProvider) }
    {
        _uiaProvider.attach(uiaProvider);
        refCount_XamlUiaTextRange.fetch_add(1, std::memory_order_relaxed);
    }

    XamlUiaTextRange::~XamlUiaTextRange()
    {
        _uiaProvider.reset();
        refCount_XamlUiaTextRange.fetch_sub(1, std::memory_order_relaxed);
    }

    IFACEMETHODIMP_(ULONG) XamlUiaTextRange::AddRef() noexcept
    {
        return _refCount.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    IFACEMETHODIMP_(ULONG) XamlUiaTextRange::Release() noexcept
    {
        const auto count = _refCount.fetch_sub(1, std::memory_order_release) - 1;
        if (count == 0)
        {
            std::atomic_thread_fence(std::memory_order_acquire);
            delete this;
        }
        return count;
    }

    IFACEMETHODIMP XamlUiaTextRange::QueryInterface(REFIID riid, void** ppvObject) noexcept
    {
        if (!ppvObject)
        {
            return E_INVALIDARG;
        }

        *ppvObject = nullptr;

        if (riid == __uuidof(ABI::Windows::UI::Xaml::Automation::Provider::ITextRangeProvider) ||
            riid == __uuidof(IInspectable) ||
            riid == __uuidof(IUnknown))
        {
            *ppvObject = static_cast<ABI::Windows::UI::Xaml::Automation::Provider::ITextRangeProvider*>(this);
            AddRef();
            return S_OK;
        }

        return E_NOINTERFACE;
    }

    IFACEMETHODIMP XamlUiaTextRange::GetIids(ULONG* iidCount, IID** iids) noexcept
    {
        if (!iidCount || !iids)
        {
            return E_INVALIDARG;
        }

        *iidCount = 1;
        *iids = static_cast<IID*>(CoTaskMemAlloc(sizeof(IID)));
        if (!*iids)
        {
            return E_OUTOFMEMORY;
        }

        (*iids)[0] = __uuidof(ABI::Windows::UI::Xaml::Automation::Provider::ITextRangeProvider);
        return S_OK;
    }

    IFACEMETHODIMP XamlUiaTextRange::GetRuntimeClassName(HSTRING* className) noexcept
    {
        if (!className)
        {
            return E_INVALIDARG;
        }

        return WindowsCreateString(L"Microsoft.Terminal.Control.XamlUiaTextRange", 47, className);
    }

    IFACEMETHODIMP XamlUiaTextRange::GetTrustLevel(TrustLevel* trustLevel) noexcept
    {
        if (!trustLevel)
        {
            return E_INVALIDARG;
        }

        *trustLevel = TrustLevel::BaseTrust;
        return S_OK;
    }

    IFACEMETHODIMP XamlUiaTextRange::Clone(ABI::Windows::UI::Xaml::Automation::Provider::ITextRangeProvider** result) noexcept
    try
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, result);
        *result = nullptr;

        UIA::ITextRangeProvider* pReturn;
        RETURN_IF_FAILED(_uiaProvider->Clone(&pReturn));

        auto xamlRange = new (std::nothrow) XamlUiaTextRange(pReturn, _parentProvider);
        RETURN_IF_NULL_ALLOC(xamlRange);

        *result = xamlRange;
        return S_OK;
    }
    CATCH_RETURN();

    IFACEMETHODIMP XamlUiaTextRange::Compare(ABI::Windows::UI::Xaml::Automation::Provider::ITextRangeProvider* textRangeProvider, boolean* result) noexcept
    try
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, result);
        *result = false;
        RETURN_HR_IF_NULL(E_INVALIDARG, textRangeProvider);

        auto self = static_cast<XamlUiaTextRange*>(textRangeProvider);

        BOOL returnVal;
        RETURN_IF_FAILED(_uiaProvider->Compare(self->_uiaProvider.get(), &returnVal));
        *result = !!returnVal;
        return S_OK;
    }
    CATCH_RETURN();

    IFACEMETHODIMP XamlUiaTextRange::CompareEndpoints(ABI::Windows::UI::Xaml::Automation::Text::TextPatternRangeEndpoint endpoint,
                                                      ABI::Windows::UI::Xaml::Automation::Provider::ITextRangeProvider* textRangeProvider,
                                                      ABI::Windows::UI::Xaml::Automation::Text::TextPatternRangeEndpoint targetEndpoint,
                                                      INT32* result) noexcept
    try
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, result);
        *result = 0;
        RETURN_HR_IF_NULL(E_INVALIDARG, textRangeProvider);

        auto self = static_cast<XamlUiaTextRange*>(textRangeProvider);

        int32_t returnVal;
        RETURN_IF_FAILED(_uiaProvider->CompareEndpoints(static_cast<UIA::TextPatternRangeEndpoint>(endpoint),
                                                        self->_uiaProvider.get(),
                                                        static_cast<UIA::TextPatternRangeEndpoint>(targetEndpoint),
                                                        &returnVal));
        *result = returnVal;
        return S_OK;
    }
    CATCH_RETURN();

    IFACEMETHODIMP XamlUiaTextRange::ExpandToEnclosingUnit(ABI::Windows::UI::Xaml::Automation::Text::TextUnit unit) noexcept
    try
    {
        RETURN_IF_FAILED(_uiaProvider->ExpandToEnclosingUnit(static_cast<UIA::TextUnit>(unit)));
        return S_OK;
    }
    CATCH_RETURN();

    IFACEMETHODIMP XamlUiaTextRange::FindAttribute(INT32 /*attributeId*/,
                                                   IInspectable* /*value*/,
                                                   boolean /*backward*/,
                                                   ABI::Windows::UI::Xaml::Automation::Provider::ITextRangeProvider** result) noexcept
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, result);
        *result = nullptr;
        // TODO GitHub #2161: potential accessibility improvement
        // we don't support this currently
        return E_NOTIMPL;
    }

    IFACEMETHODIMP XamlUiaTextRange::FindText(HSTRING text,
                                              boolean searchBackward,
                                              boolean ignoreCase,
                                              ABI::Windows::UI::Xaml::Automation::Provider::ITextRangeProvider** result) noexcept
    try
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, result);
        *result = nullptr;

        UIA::ITextRangeProvider* pReturn;
        uint32_t length;
        const auto textPtr = WindowsGetStringRawBuffer(text, &length);
        const auto queryText = wil::unique_bstr{ SysAllocStringLen(textPtr, length) };

        RETURN_IF_FAILED(_uiaProvider->FindText(queryText.get(), searchBackward, ignoreCase, &pReturn));

        if (pReturn)
        {
            auto xamlRange = new (std::nothrow) XamlUiaTextRange(pReturn, _parentProvider);
            RETURN_IF_NULL_ALLOC(xamlRange);
            *result = xamlRange;
        }

        return S_OK;
    }
    CATCH_RETURN();

    IFACEMETHODIMP XamlUiaTextRange::GetAttributeValue(INT32 attributeId, IInspectable** result) noexcept
    try
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, result);
        *result = nullptr;

        // Call the function off of the underlying UiaTextRange.
        wil::unique_variant varResult;
        RETURN_IF_FAILED(_uiaProvider->GetAttributeValue(attributeId, varResult.addressof()));

        // Convert the resulting VARIANT into a format that is consumable by XAML.
        switch (varResult.vt)
        {
        case VT_BSTR:
        {
            auto value = winrt::box_value(varResult.bstrVal);
            *result = static_cast<IInspectable*>(winrt::detach_abi(value));
            return S_OK;
        }
        case VT_I4:
        {
            auto value = winrt::box_value<int32_t>(varResult.lVal);
            *result = static_cast<IInspectable*>(winrt::detach_abi(value));
            return S_OK;
        }
        case VT_R8:
        {
            auto value = winrt::box_value(varResult.dblVal);
            *result = static_cast<IInspectable*>(winrt::detach_abi(value));
            return S_OK;
        }
        case VT_BOOL:
        {
            auto value = winrt::box_value<bool>(varResult.boolVal);
            *result = static_cast<IInspectable*>(winrt::detach_abi(value));
            return S_OK;
        }
        case VT_UNKNOWN:
        {
            com_ptr<IUnknown> mixedAttributeVal;
            UiaGetReservedMixedAttributeValue(mixedAttributeVal.put());

            if (varResult.punkVal == mixedAttributeVal.get())
            {
                auto value = Windows::UI::Xaml::DependencyProperty::UnsetValue();
                *result = static_cast<IInspectable*>(winrt::detach_abi(value));
                return S_OK;
            }

            [[fallthrough]];
        }
        default:
            // We _need_ to return XAML_E_NOT_SUPPORTED here.
            return XAML_E_NOT_SUPPORTED;
        }
    }
    CATCH_RETURN();

    IFACEMETHODIMP XamlUiaTextRange::GetBoundingRectangles(UINT32* returnValueLength, DOUBLE** returnValue) noexcept
    try
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, returnValueLength);
        RETURN_HR_IF_NULL(E_INVALIDARG, returnValue);
        *returnValueLength = 0;
        *returnValue = nullptr;

        SAFEARRAY* pReturnVal;
        RETURN_IF_FAILED(_uiaProvider->GetBoundingRectangles(&pReturnVal));

        double* pVals;
        RETURN_IF_FAILED(SafeArrayAccessData(pReturnVal, (void**)&pVals));

        long lBound, uBound;
        RETURN_IF_FAILED(SafeArrayGetLBound(pReturnVal, 1, &lBound));
        RETURN_IF_FAILED(SafeArrayGetUBound(pReturnVal, 1, &uBound));

        auto count = uBound - lBound + 1;

        auto result = static_cast<DOUBLE*>(CoTaskMemAlloc(sizeof(DOUBLE) * count));
        if (!result)
        {
            SafeArrayUnaccessData(pReturnVal);
            SafeArrayDestroy(pReturnVal);
            return E_OUTOFMEMORY;
        }

        for (auto i = 0; i < count; i++)
        {
            result[i] = pVals[i];
        }

        *returnValueLength = count;
        *returnValue = result;

        RETURN_IF_FAILED(SafeArrayUnaccessData(pReturnVal));
        RETURN_IF_FAILED(SafeArrayDestroy(pReturnVal));
        return S_OK;
    }
    CATCH_RETURN();

    IFACEMETHODIMP XamlUiaTextRange::GetEnclosingElement(ABI::Windows::UI::Xaml::Automation::Provider::IIRawElementProviderSimple** result) noexcept
    try
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, result);
        *result = nullptr;

        ::Microsoft::Console::Types::UiaTracing::TextRange::GetEnclosingElement(*static_cast<::Microsoft::Console::Types::UiaTextRangeBase*>(_uiaProvider.get()));
        winrt::copy_to_abi(_parentProvider, (void*&)*result);
        return S_OK;
    }
    CATCH_RETURN();

    IFACEMETHODIMP XamlUiaTextRange::GetText(INT32 maxLength, HSTRING* result) noexcept
    try
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, result);
        *result = nullptr;

        wil::unique_bstr returnVal;
        RETURN_IF_FAILED(_uiaProvider->GetText(maxLength, returnVal.put()));

        RETURN_IF_FAILED(WindowsCreateString(returnVal.get(), SysStringLen(returnVal.get()), result));
        return S_OK;
    }
    CATCH_RETURN();

    IFACEMETHODIMP XamlUiaTextRange::Move(ABI::Windows::UI::Xaml::Automation::Text::TextUnit unit,
                                          INT32 count,
                                          INT32* result) noexcept
    try
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, result);
        *result = 0;

        int returnVal;
        RETURN_IF_FAILED(_uiaProvider->Move(static_cast<UIA::TextUnit>(unit),
                                            count,
                                            &returnVal));
        *result = returnVal;
        return S_OK;
    }
    CATCH_RETURN();

    IFACEMETHODIMP XamlUiaTextRange::MoveEndpointByUnit(ABI::Windows::UI::Xaml::Automation::Text::TextPatternRangeEndpoint endpoint,
                                                        ABI::Windows::UI::Xaml::Automation::Text::TextUnit unit,
                                                        INT32 count,
                                                        INT32* result) noexcept
    try
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, result);
        *result = 0;

        int returnVal;
        RETURN_IF_FAILED(_uiaProvider->MoveEndpointByUnit(static_cast<UIA::TextPatternRangeEndpoint>(endpoint),
                                                          static_cast<UIA::TextUnit>(unit),
                                                          count,
                                                          &returnVal));
        *result = returnVal;
        return S_OK;
    }
    CATCH_RETURN();

    IFACEMETHODIMP XamlUiaTextRange::MoveEndpointByRange(ABI::Windows::UI::Xaml::Automation::Text::TextPatternRangeEndpoint endpoint,
                                                         ABI::Windows::UI::Xaml::Automation::Provider::ITextRangeProvider* textRangeProvider,
                                                         ABI::Windows::UI::Xaml::Automation::Text::TextPatternRangeEndpoint targetEndpoint) noexcept
    try
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, textRangeProvider);

        auto self = static_cast<XamlUiaTextRange*>(textRangeProvider);
        RETURN_IF_FAILED(_uiaProvider->MoveEndpointByRange(static_cast<UIA::TextPatternRangeEndpoint>(endpoint),
                                                           self->_uiaProvider.get(),
                                                           static_cast<UIA::TextPatternRangeEndpoint>(targetEndpoint)));
        return S_OK;
    }
    CATCH_RETURN();

    IFACEMETHODIMP XamlUiaTextRange::Select() noexcept
    try
    {
        RETURN_IF_FAILED(_uiaProvider->Select());
        return S_OK;
    }
    CATCH_RETURN();

    IFACEMETHODIMP XamlUiaTextRange::AddToSelection() noexcept
    {
        // we don't support this
        return E_NOTIMPL;
    }

    IFACEMETHODIMP XamlUiaTextRange::RemoveFromSelection() noexcept
    {
        // we don't support this
        return E_NOTIMPL;
    }

    IFACEMETHODIMP XamlUiaTextRange::ScrollIntoView(boolean alignToTop) noexcept
    try
    {
        RETURN_IF_FAILED(_uiaProvider->ScrollIntoView(alignToTop));
        return S_OK;
    }
    CATCH_RETURN();

    IFACEMETHODIMP XamlUiaTextRange::GetChildren(UINT32* resultLength,
                                                 ABI::Windows::UI::Xaml::Automation::Provider::IIRawElementProviderSimple*** result) noexcept
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, resultLength);
        RETURN_HR_IF_NULL(E_INVALIDARG, result);

        // we don't have any children
        *resultLength = 0;
        *result = nullptr;
        return S_OK;
    }
}
