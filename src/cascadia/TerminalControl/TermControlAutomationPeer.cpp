// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <UIAutomationCore.h>
#include "TermControlAutomationPeer.h"
#include "TermControl.h"
#include "TermControlAutomationPeer.g.cpp"

#include "XamlUiaTextRange.h"

namespace winrt::Microsoft::Terminal::TerminalControl::implementation
{
    TermControlAutomationPeer::TermControlAutomationPeer(winrt::Microsoft::Terminal::TerminalControl::implementation::TermControl const& owner) :
        TermControlAutomationPeerT<TermControlAutomationPeer>(owner),
        _uiaProvider{ owner.GetRenderData(), nullptr, std::bind(&TermControlAutomationPeer::GetBoundingRectWrapped, this) } {};

    winrt::hstring TermControlAutomationPeer::GetClassNameCore() const
    {
        return L"TermControl";
    }

    AutomationControlType TermControlAutomationPeer::GetAutomationControlTypeCore() const
    {
        return AutomationControlType::Text;
    }

    winrt::hstring TermControlAutomationPeer::GetLocalizedControlTypeCore() const
    {
        // TODO GitHub #2142: Localize string
        return L"TerminalControl";
    }

    winrt::Windows::Foundation::IInspectable TermControlAutomationPeer::GetPatternCore(PatternInterface patternInterface) const
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
    winrt::com_array<Windows::UI::Xaml::Automation::Provider::ITextRangeProvider> TermControlAutomationPeer::GetSelection()
    {
        SAFEARRAY* pReturnVal;
        THROW_IF_FAILED(_uiaProvider.GetSelection(&pReturnVal));
        return SafeArrayToComArray(pReturnVal);
    }

    winrt::com_array<Windows::UI::Xaml::Automation::Provider::ITextRangeProvider> TermControlAutomationPeer::GetVisibleRanges()
    {
        SAFEARRAY* pReturnVal;
        THROW_IF_FAILED(_uiaProvider.GetVisibleRanges(&pReturnVal));
        return SafeArrayToComArray(pReturnVal);
    }

    Windows::UI::Xaml::Automation::Provider::ITextRangeProvider TermControlAutomationPeer::RangeFromChild(Windows::UI::Xaml::Automation::Provider::IRawElementProviderSimple childElement)
    {
        ::ITextRangeProvider* returnVal;
        // ScreenInfoUiaProvider doesn't actually use parameter, so just pass in nullptr
        _uiaProvider.RangeFromChild(/* IRawElementProviderSimple */ nullptr,
                                    &returnVal);

        auto parentProvider = this->ProviderFromPeer(*this);
        auto xutr = winrt::make_self<XamlUiaTextRange>(returnVal, parentProvider);
        return xutr.as<Windows::UI::Xaml::Automation::Provider::ITextRangeProvider>();
    }

    Windows::UI::Xaml::Automation::Provider::ITextRangeProvider TermControlAutomationPeer::RangeFromPoint(Windows::Foundation::Point screenLocation)
    {
        ::ITextRangeProvider* returnVal;
        _uiaProvider.RangeFromPoint({ screenLocation.X, screenLocation.Y }, &returnVal);

        auto parentProvider = this->ProviderFromPeer(*this);
        auto xutr = winrt::make_self<XamlUiaTextRange>(returnVal, parentProvider);
        return xutr.as<Windows::UI::Xaml::Automation::Provider::ITextRangeProvider>();
    }

    Windows::UI::Xaml::Automation::Provider::ITextRangeProvider TermControlAutomationPeer::DocumentRange()
    {
        ::ITextRangeProvider* returnVal;
        _uiaProvider.get_DocumentRange(&returnVal);

        auto parentProvider = this->ProviderFromPeer(*this);
        auto xutr = winrt::make_self<XamlUiaTextRange>(returnVal, parentProvider);
        return xutr.as<Windows::UI::Xaml::Automation::Provider::ITextRangeProvider>();
    }

    Windows::UI::Xaml::Automation::SupportedTextSelection TermControlAutomationPeer::SupportedTextSelection()
    {
        ::SupportedTextSelection returnVal;
        _uiaProvider.get_SupportedTextSelection(&returnVal);
        return static_cast<Windows::UI::Xaml::Automation::SupportedTextSelection>(returnVal);
    }

#pragma endregion

    RECT TermControlAutomationPeer::GetBoundingRectWrapped()
    {
        auto rect = GetBoundingRectangle();
        return {
            gsl::narrow<LONG>(rect.X),
            gsl::narrow<LONG>(rect.Y),
            gsl::narrow<LONG>(rect.X + rect.Width),
            gsl::narrow<LONG>(rect.Y + rect.Height)
        };
    }

    winrt::com_array<Windows::UI::Xaml::Automation::Provider::ITextRangeProvider> TermControlAutomationPeer::SafeArrayToComArray(SAFEARRAY* textRanges)
    {
        auto owningVector = SafeArrayToOwningVector<::UiaTextRange>(textRanges);
        int count = owningVector.size();

        std::vector<Windows::UI::Xaml::Automation::Provider::ITextRangeProvider> vec;
        vec.reserve(count);
        auto parentProvider = this->ProviderFromPeer(*this);
        for (int i = 0; i < count; i++)
        {
            auto provider = owningVector[i];
            auto xutr = winrt::make_self<XamlUiaTextRange>(provider.get(), parentProvider);
            vec.emplace_back(xutr.as<Windows::UI::Xaml::Automation::Provider::ITextRangeProvider>());
        }

        winrt::com_array<Windows::UI::Xaml::Automation::Provider::ITextRangeProvider> result{ vec };

        return result;
    }
}
