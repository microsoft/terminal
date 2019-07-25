// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "XamlUiaTextRange.h"
#include "../types/UiaTextRange.hpp"

namespace winrt::Microsoft::Terminal::TerminalControl::implementation
{
    Windows::UI::Xaml::Automation::Provider::ITextRangeProvider XamlUiaTextRange::Clone() const
    {
        ::ITextRangeProvider* pReturn;
        THROW_IF_FAILED(_uiaProvider->Clone(&pReturn));
        auto xutr = winrt::make_self<XamlUiaTextRange>(pReturn, _parentProvider);
        return xutr.as<Windows::UI::Xaml::Automation::Provider::ITextRangeProvider>();
    }

    bool XamlUiaTextRange::Compare(Windows::UI::Xaml::Automation::Provider::ITextRangeProvider pRange) const
    {
        auto self = winrt::get_self<XamlUiaTextRange>(pRange);

        BOOL returnVal;
        THROW_IF_FAILED(_uiaProvider->Compare(self->_uiaProvider.get(), &returnVal));
        return returnVal;
    }

    int32_t XamlUiaTextRange::CompareEndpoints(Windows::UI::Xaml::Automation::Text::TextPatternRangeEndpoint endpoint,
                                               Windows::UI::Xaml::Automation::Provider::ITextRangeProvider pTargetRange,
                                               Windows::UI::Xaml::Automation::Text::TextPatternRangeEndpoint targetEndpoint)
    {
        auto self = winrt::get_self<XamlUiaTextRange>(pTargetRange);

        int32_t returnVal;
        THROW_IF_FAILED(_uiaProvider->Compare(self->_uiaProvider.get(), &returnVal));
        return returnVal;
    }

    void XamlUiaTextRange::ExpandToEnclosingUnit(Windows::UI::Xaml::Automation::Text::TextUnit unit) const
    {
        THROW_IF_FAILED(_uiaProvider->ExpandToEnclosingUnit(static_cast<::TextUnit>(unit)));
    }

    Windows::UI::Xaml::Automation::Provider::ITextRangeProvider XamlUiaTextRange::FindAttribute(int32_t textAttributeId,
                                                                                                winrt::Windows::Foundation::IInspectable val,
                                                                                                bool searchBackward)
    {
        // we don't support this currently
        return nullptr;
    }

    Windows::UI::Xaml::Automation::Provider::ITextRangeProvider XamlUiaTextRange::FindText(winrt::hstring text,
                                                                                           bool searchBackward,
                                                                                           bool ignoreCase)
    {
        // TODO GitHub #605: Search functionality
        // we need to wrap this around the UiaTextRange FindText() function
        // but right now it returns E_NOTIMPL, so let's just return nullptr for now.
        return nullptr;
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

    Windows::UI::Xaml::Automation::Provider::IRawElementProviderSimple XamlUiaTextRange::GetEnclosingElement()
    {
        return _parentProvider;
    }

    winrt::hstring XamlUiaTextRange::GetText(int32_t maxLength) const
    {
        BSTR returnVal;
        _uiaProvider->GetText(maxLength, &returnVal);
        return winrt::to_hstring(returnVal);
    }

    int32_t XamlUiaTextRange::Move(Windows::UI::Xaml::Automation::Text::TextUnit unit,
                                   int32_t count)
    {
        int returnVal;
        _uiaProvider->Move(static_cast<::TextUnit>(unit),
                           count,
                           &returnVal);
        return returnVal;
    }

    int32_t XamlUiaTextRange::MoveEndpointByUnit(Windows::UI::Xaml::Automation::Text::TextPatternRangeEndpoint endpoint,
                                                 Windows::UI::Xaml::Automation::Text::TextUnit unit,
                                                 int32_t count) const
    {
        int returnVal;
        _uiaProvider->MoveEndpointByUnit(static_cast<::TextPatternRangeEndpoint>(endpoint),
                                         static_cast<::TextUnit>(unit),
                                         count,
                                         &returnVal);
        return returnVal;
    }

    void XamlUiaTextRange::MoveEndpointByRange(Windows::UI::Xaml::Automation::Text::TextPatternRangeEndpoint endpoint,
                                               Windows::UI::Xaml::Automation::Provider::ITextRangeProvider pTargetRange,
                                               Windows::UI::Xaml::Automation::Text::TextPatternRangeEndpoint targetEndpoint) const
    {
        auto self = winrt::get_self<XamlUiaTextRange>(pTargetRange);
        _uiaProvider->MoveEndpointByRange(static_cast<::TextPatternRangeEndpoint>(endpoint),
                                          /*pTargetRange*/ self->_uiaProvider.get(),
                                          static_cast<::TextPatternRangeEndpoint>(targetEndpoint));
    }

    void XamlUiaTextRange::Select() const
    {
        THROW_IF_FAILED(_uiaProvider->Select());
    }

    void XamlUiaTextRange::AddToSelection() const
    {
        // we don't support this
    }

    void XamlUiaTextRange::RemoveFromSelection() const
    {
        // we don't support this
    }

    void XamlUiaTextRange::ScrollIntoView(bool alignToTop) const
    {
        THROW_IF_FAILED(_uiaProvider->ScrollIntoView(alignToTop));
    }

    winrt::com_array<Windows::UI::Xaml::Automation::Provider::IRawElementProviderSimple> XamlUiaTextRange::GetChildren() const
    {
        // we don't have any children
        return {};
    }
}
