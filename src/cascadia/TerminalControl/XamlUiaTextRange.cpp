// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "XamlUiaTextRange.h"
#include "UiaTextRange.hpp"

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

namespace winrt::Microsoft::Terminal::TerminalControl::implementation
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

    XamlAutomation::ITextRangeProvider XamlUiaTextRange::FindText(winrt::hstring /*text*/,
                                                                  bool /*searchBackward*/,
                                                                  bool /*ignoreCase*/)
    {
        // TODO GitHub #605: Search functionality
        // we need to wrap this around the UiaTextRange FindText() function
        // but right now it returns E_NOTIMPL, so let's just return nullptr for now.
        throw winrt::hresult_not_implemented();
    }

    winrt::Windows::Foundation::IInspectable XamlUiaTextRange::GetAttributeValue(int32_t textAttributeId) const
    {
        // Copied functionality from Types::UiaTextRange.cpp
        if (textAttributeId == UIA_IsReadOnlyAttributeId)
        {
            return winrt::box_value(false);
        }
        else
        {
            return nullptr;
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
