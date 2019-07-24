// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <UIAutomationCore.h>
#include "TermControlAP.h"
#include "TermControl.h"
#include "TermControlAP.g.cpp"

#include "XamlUiaTextRange.h"

namespace winrt::Microsoft::Terminal::TerminalControl::implementation
{
    TermControlAP::TermControlAP(winrt::Microsoft::Terminal::TerminalControl::implementation::TermControl const& owner) :
        TermControlAPT<TermControlAP>(owner),
        _uiaProvider{ owner.GetRenderData(), nullptr, std::bind(&TermControlAP::GetBoundingRectWrapped, this) } {
        };

    winrt::hstring TermControlAP::GetClassNameCore() const
    {
        return L"TerminalControl";
    }

    AutomationControlType TermControlAP::GetAutomationControlTypeCore() const
    {
        return AutomationControlType::Text;
    }

    winrt::hstring TermControlAP::GetLocalizedControlTypeCore() const
    {
        return L"TerminalControl";
    }

    winrt::Windows::Foundation::IInspectable TermControlAP::GetPatternCore(PatternInterface patternInterface) const
    {
        switch (patternInterface)
        {
        case PatternInterface::Text:
            return *this;
            break;
        default:
            return nullptr;
        }
    }

#pragma region ITextProvider
    winrt::com_array<Windows::UI::Xaml::Automation::Provider::ITextRangeProvider> TermControlAP::GetSelection()
    {
        try
        {
            SAFEARRAY* pReturnVal;
            THROW_IF_FAILED(_uiaProvider.GetSelection(&pReturnVal));

            ::UiaTextRange* pVals;
            THROW_IF_FAILED(SafeArrayAccessData(pReturnVal, (void**)&pVals));

            long lBound, uBound;
            THROW_IF_FAILED(SafeArrayGetLBound(pReturnVal, 1, &lBound));
            THROW_IF_FAILED(SafeArrayGetUBound(pReturnVal, 1, &uBound));

            long count = uBound - lBound + 1;

            std::vector<Windows::UI::Xaml::Automation::Provider::ITextRangeProvider> vec;
            vec.reserve(count);
            auto parentProvider = this->ProviderFromPeer(*this);
            for (int i = 0; i < count; i++)
            {
                ::UiaTextRange* provider = &pVals[i];
                auto xutr = winrt::make_self<XamlUiaTextRange>(provider, parentProvider);
                vec.emplace_back(xutr.as<Windows::UI::Xaml::Automation::Provider::ITextRangeProvider>());
            }

            winrt::com_array<Windows::UI::Xaml::Automation::Provider::ITextRangeProvider> result{ vec };

            return result;
        }
        catch (...)
        {
        }
        return {};
    }

    winrt::com_array<Windows::UI::Xaml::Automation::Provider::ITextRangeProvider> TermControlAP::GetVisibleRanges()
    {
        try
        {
            SAFEARRAY* pReturnVal;
            THROW_IF_FAILED(_uiaProvider.GetVisibleRanges(&pReturnVal));

            ::UiaTextRange* pVals;
            THROW_IF_FAILED(SafeArrayAccessData(pReturnVal, (void**)&pVals));

            long lBound, uBound;
            THROW_IF_FAILED(SafeArrayGetLBound(pReturnVal, 1, &lBound));
            THROW_IF_FAILED(SafeArrayGetUBound(pReturnVal, 1, &uBound));

            long count = uBound - lBound + 1;

            std::vector<Windows::UI::Xaml::Automation::Provider::ITextRangeProvider> vec;
            vec.reserve(count);
            auto parentProvider = this->ProviderFromPeer(*this);
            for (int i = 0; i < count; i++)
            {
                ::UiaTextRange* provider = &pVals[i];
                auto xutr = winrt::make_self<XamlUiaTextRange>(provider, parentProvider);
                vec.emplace_back(xutr.as<Windows::UI::Xaml::Automation::Provider::ITextRangeProvider>());
            }

            winrt::com_array<Windows::UI::Xaml::Automation::Provider::ITextRangeProvider> result{ vec };

            return result;
        }
        catch (...)
        {
        }
        return {};
    }

    Windows::UI::Xaml::Automation::Provider::ITextRangeProvider TermControlAP::RangeFromChild(Windows::UI::Xaml::Automation::Provider::IRawElementProviderSimple childElement)
    {
        ::ITextRangeProvider* returnVal;
        // ScreenInfoUiaProvider doesn't actually use parameter, so just pass in nullptr
        _uiaProvider.RangeFromChild(/* IRawElementProviderSimple */ nullptr,
                                    &returnVal);

        auto parentProvider = this->ProviderFromPeer(*this);
        auto xutr = winrt::make_self<XamlUiaTextRange>(returnVal, parentProvider);
        return xutr.as<Windows::UI::Xaml::Automation::Provider::ITextRangeProvider>();
    }

    Windows::UI::Xaml::Automation::Provider::ITextRangeProvider TermControlAP::RangeFromPoint(Windows::Foundation::Point screenLocation)
    {
        ::ITextRangeProvider* returnVal;
        _uiaProvider.RangeFromPoint({ screenLocation.X, screenLocation.Y }, &returnVal);

        auto parentProvider = this->ProviderFromPeer(*this);
        auto xutr = winrt::make_self<XamlUiaTextRange>(returnVal, parentProvider);
        return xutr.as<Windows::UI::Xaml::Automation::Provider::ITextRangeProvider>();
    }

    Windows::UI::Xaml::Automation::Provider::ITextRangeProvider TermControlAP::DocumentRange()
    {
        ::ITextRangeProvider* returnVal;
        _uiaProvider.get_DocumentRange(&returnVal);

        auto parentProvider = this->ProviderFromPeer(*this);
        auto xutr = winrt::make_self<XamlUiaTextRange>(returnVal, parentProvider);
        return xutr.as<Windows::UI::Xaml::Automation::Provider::ITextRangeProvider>();
    }

    Windows::UI::Xaml::Automation::SupportedTextSelection TermControlAP::SupportedTextSelection()
    {
        ::SupportedTextSelection returnVal;
        _uiaProvider.get_SupportedTextSelection(&returnVal);
        return static_cast<Windows::UI::Xaml::Automation::SupportedTextSelection>(returnVal);
    }

#pragma endregion

    RECT TermControlAP::GetBoundingRectWrapped()
    {
        auto rect = GetBoundingRectangle();
        return {
            gsl::narrow<LONG>(rect.X),
            gsl::narrow<LONG>(rect.Y),
            gsl::narrow<LONG>(rect.X + rect.Width),
            gsl::narrow<LONG>(rect.Y + rect.Y)
        };
    }
}
