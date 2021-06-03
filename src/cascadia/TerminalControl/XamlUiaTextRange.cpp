// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "XamlUiaTextRange.h"
#include "../types/TermControlUiaTextRange.hpp"
#include <UIAutomationClient.h>
#include <UIAutomationCoreApi.h>

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

namespace winrt::Microsoft::Terminal::Control::implementation
{
    XamlAutomation::ITextRangeProvider XamlUiaTextRange::Clone() const
    {
        UIA::ITextRangeProvider* pReturn;
        THROW_IF_FAILED(_uiaProvider->Clone(&pReturn));
        auto xutr = winrt::make_self<XamlUiaTextRange>(pReturn, _parentProvider);
        return xutr.as<XamlAutomation::ITextRangeProvider>();
    }

    bool XamlUiaTextRange::Compare(XamlAutomation::ITextRangeProvider pRange) const
    {
        auto self = winrt::get_self<XamlUiaTextRange>(pRange);

        BOOL returnVal;
        THROW_IF_FAILED(_uiaProvider->Compare(self->_uiaProvider.get(), &returnVal));
        return returnVal;
    }

    int32_t XamlUiaTextRange::CompareEndpoints(XamlAutomation::TextPatternRangeEndpoint endpoint,
                                               XamlAutomation::ITextRangeProvider pTargetRange,
                                               XamlAutomation::TextPatternRangeEndpoint targetEndpoint)
    {
        auto self = winrt::get_self<XamlUiaTextRange>(pTargetRange);

        int32_t returnVal;
        THROW_IF_FAILED(_uiaProvider->CompareEndpoints(static_cast<UIA::TextPatternRangeEndpoint>(endpoint),
                                                       self->_uiaProvider.get(),
                                                       static_cast<UIA::TextPatternRangeEndpoint>(targetEndpoint),
                                                       &returnVal));
        return returnVal;
    }

    void XamlUiaTextRange::ExpandToEnclosingUnit(XamlAutomation::TextUnit unit) const
    {
        THROW_IF_FAILED(_uiaProvider->ExpandToEnclosingUnit(static_cast<UIA::TextUnit>(unit)));
    }

    XamlAutomation::ITextRangeProvider XamlUiaTextRange::FindAttribute(int32_t /*textAttributeId*/,
                                                                       winrt::Windows::Foundation::IInspectable /*val*/,
                                                                       bool /*searchBackward*/)
    {
        // TODO GitHub #2161: potential accessibility improvement
        // we don't support this currently
        throw winrt::hresult_not_implemented();
    }

    XamlAutomation::ITextRangeProvider XamlUiaTextRange::FindText(winrt::hstring text,
                                                                  bool searchBackward,
                                                                  bool ignoreCase)
    {
        UIA::ITextRangeProvider* pReturn;
        const auto queryText = wil::make_bstr(text.c_str());

        THROW_IF_FAILED(_uiaProvider->FindText(queryText.get(), searchBackward, ignoreCase, &pReturn));

        auto xutr = winrt::make_self<XamlUiaTextRange>(pReturn, _parentProvider);
        return *xutr;
    }

    winrt::Windows::Foundation::IInspectable XamlUiaTextRange::GetAttributeValue(int32_t textAttributeId) const
    {
        // Call the function off of the underlying UiaTextRange.
        VARIANT result;
        THROW_IF_FAILED(_uiaProvider->GetAttributeValue(textAttributeId, &result));

        // Convert the resulting VARIANT into a format that is consumable by XAML.
        switch (result.vt)
        {
        case VT_BSTR:
        {
            return box_value(result.bstrVal);
        }
        case VT_I4:
        {
            return box_value(result.iVal);
        }
        case VT_R8:
        {
            return box_value(result.dblVal);
        }
        case VT_BOOL:
        {
            return box_value(result.boolVal);
        }
        case VT_UNKNOWN:
        {
            // This one is particularly special.
            // We might return a special value like UiaGetReservedMixedAttributeValue
            //  or UiaGetReservedNotSupportedValue.
            // Some text attributes may return a real value, however, none of those
            //  are supported at this time.
            // So we need to figure out what was actually intended to be returned.

            IUnknown* notSupportedVal;
            UiaGetReservedNotSupportedValue(&notSupportedVal);
            if (result.punkVal == notSupportedVal)
            {
                // See below for why we need to throw this special value.
                winrt::throw_hresult(XAML_E_NOT_SUPPORTED);
            }

            IUnknown* mixedAttributeVal;
            UiaGetReservedMixedAttributeValue(&mixedAttributeVal);
            if (result.punkVal == mixedAttributeVal)
            {
                return Windows::UI::Xaml::DependencyProperty::UnsetValue();
            }

            __fallthrough;
        }
        default:
        {
            // We _need_ to return XAML_E_NOT_SUPPORTED here.
            // Returning nullptr is an improper implementation of it being unsupported.
            // UIA Clients rely on this HRESULT to signify that the requested attribute is undefined.
            // Anything else will result in the UIA Client refusing to read when navigating by word
            // Magically, this doesn't affect other forms of navigation...
            winrt::throw_hresult(XAML_E_NOT_SUPPORTED);
        }
        }
    }

    void XamlUiaTextRange::GetBoundingRectangles(com_array<double>& returnValue) const
    {
        returnValue = {};
        try
        {
            SAFEARRAY* pReturnVal;
            THROW_IF_FAILED(_uiaProvider->GetBoundingRectangles(&pReturnVal));

            double* pVals;
            THROW_IF_FAILED(SafeArrayAccessData(pReturnVal, (void**)&pVals));

            long lBound, uBound;
            THROW_IF_FAILED(SafeArrayGetLBound(pReturnVal, 1, &lBound));
            THROW_IF_FAILED(SafeArrayGetUBound(pReturnVal, 1, &uBound));

            long count = uBound - lBound + 1;

            std::vector<double> vec;
            vec.reserve(count);
            for (int i = 0; i < count; i++)
            {
                double element = pVals[i];
                vec.push_back(element);
            }

            winrt::com_array<double> result{ vec };
            returnValue = std::move(result);
        }
        catch (...)
        {
        }
    }

    XamlAutomation::IRawElementProviderSimple XamlUiaTextRange::GetEnclosingElement()
    {
        return _parentProvider;
    }

    winrt::hstring XamlUiaTextRange::GetText(int32_t maxLength) const
    {
        BSTR returnVal;
        THROW_IF_FAILED(_uiaProvider->GetText(maxLength, &returnVal));
        return winrt::to_hstring(returnVal);
    }

    int32_t XamlUiaTextRange::Move(XamlAutomation::TextUnit unit,
                                   int32_t count)
    {
        int returnVal;
        THROW_IF_FAILED(_uiaProvider->Move(static_cast<UIA::TextUnit>(unit),
                                           count,
                                           &returnVal));
        return returnVal;
    }

    int32_t XamlUiaTextRange::MoveEndpointByUnit(XamlAutomation::TextPatternRangeEndpoint endpoint,
                                                 XamlAutomation::TextUnit unit,
                                                 int32_t count) const
    {
        int returnVal;
        THROW_IF_FAILED(_uiaProvider->MoveEndpointByUnit(static_cast<UIA::TextPatternRangeEndpoint>(endpoint),
                                                         static_cast<UIA::TextUnit>(unit),
                                                         count,
                                                         &returnVal));
        return returnVal;
    }

    void XamlUiaTextRange::MoveEndpointByRange(XamlAutomation::TextPatternRangeEndpoint endpoint,
                                               XamlAutomation::ITextRangeProvider pTargetRange,
                                               XamlAutomation::TextPatternRangeEndpoint targetEndpoint) const
    {
        auto self = winrt::get_self<XamlUiaTextRange>(pTargetRange);
        THROW_IF_FAILED(_uiaProvider->MoveEndpointByRange(static_cast<UIA::TextPatternRangeEndpoint>(endpoint),
                                                          /*pTargetRange*/ self->_uiaProvider.get(),
                                                          static_cast<UIA::TextPatternRangeEndpoint>(targetEndpoint)));
    }

    void XamlUiaTextRange::Select() const
    {
        THROW_IF_FAILED(_uiaProvider->Select());
    }

    void XamlUiaTextRange::AddToSelection() const
    {
        // we don't support this
        throw winrt::hresult_not_implemented();
    }

    void XamlUiaTextRange::RemoveFromSelection() const
    {
        // we don't support this
        throw winrt::hresult_not_implemented();
    }

    void XamlUiaTextRange::ScrollIntoView(bool alignToTop) const
    {
        THROW_IF_FAILED(_uiaProvider->ScrollIntoView(alignToTop));
    }

    winrt::com_array<XamlAutomation::IRawElementProviderSimple> XamlUiaTextRange::GetChildren() const
    {
        // we don't have any children
        return {};
    }
}
