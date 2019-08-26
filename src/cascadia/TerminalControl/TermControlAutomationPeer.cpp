// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <UIAutomationCore.h>
#include "TermControlAutomationPeer.h"
#include "TermControl.h"
#include "TermControlAutomationPeer.g.cpp"

#include "XamlUiaTextRange.h"

using namespace Microsoft::Console::Types;
using namespace winrt::Windows::UI::Xaml::Automation::Peers;

namespace UIA
{
    using ::ITextRangeProvider;
    using ::SupportedTextSelection;
}

namespace XamlAutomation
{
    using winrt::Windows::UI::Xaml::Automation::SupportedTextSelection;
    using winrt::Windows::UI::Xaml::Automation::Provider::IRawElementProviderSimple;
    using winrt::Windows::UI::Xaml::Automation::Provider::ITextRangeProvider;
}

namespace winrt::Microsoft::Terminal::TerminalControl::implementation
{
    TermControlAutomationPeer::TermControlAutomationPeer(winrt::Microsoft::Terminal::TerminalControl::implementation::TermControl const& owner) :
        TermControlAutomationPeerT<TermControlAutomationPeer>(owner), // pass owner to FrameworkElementAutomationPeer
        _uiaProvider{ owner, std::bind(&TermControlAutomationPeer::GetBoundingRectWrapped, this) } {};

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
    winrt::com_array<XamlAutomation::ITextRangeProvider> TermControlAutomationPeer::GetSelection()
    {
        SAFEARRAY* pReturnVal;
        THROW_IF_FAILED(_uiaProvider.GetSelection(&pReturnVal));
        return WrapArrayOfTextRangeProviders(pReturnVal);
    }

    winrt::com_array<XamlAutomation::ITextRangeProvider> TermControlAutomationPeer::GetVisibleRanges()
    {
        SAFEARRAY* pReturnVal;
        THROW_IF_FAILED(_uiaProvider.GetVisibleRanges(&pReturnVal));
        return WrapArrayOfTextRangeProviders(pReturnVal);
    }

    XamlAutomation::ITextRangeProvider TermControlAutomationPeer::RangeFromChild(XamlAutomation::IRawElementProviderSimple childElement)
    {
        UIA::ITextRangeProvider* returnVal;
        // ScreenInfoUiaProvider doesn't actually use parameter, so just pass in nullptr
        THROW_IF_FAILED(_uiaProvider.RangeFromChild(/* IRawElementProviderSimple */ nullptr,
                                                    &returnVal));

        auto parentProvider = this->ProviderFromPeer(*this);
        auto xutr = winrt::make_self<XamlUiaTextRange>(returnVal, parentProvider);
        return xutr.as<XamlAutomation::ITextRangeProvider>();
    }

    XamlAutomation::ITextRangeProvider TermControlAutomationPeer::RangeFromPoint(Windows::Foundation::Point screenLocation)
    {
        UIA::ITextRangeProvider* returnVal;
        THROW_IF_FAILED(_uiaProvider.RangeFromPoint({ screenLocation.X, screenLocation.Y }, &returnVal));

        auto parentProvider = this->ProviderFromPeer(*this);
        auto xutr = winrt::make_self<XamlUiaTextRange>(returnVal, parentProvider);
        return xutr.as<XamlAutomation::ITextRangeProvider>();
    }

    XamlAutomation::ITextRangeProvider TermControlAutomationPeer::DocumentRange()
    {
        UIA::ITextRangeProvider* returnVal;
        THROW_IF_FAILED(_uiaProvider.get_DocumentRange(&returnVal));

        auto parentProvider = this->ProviderFromPeer(*this);
        auto xutr = winrt::make_self<XamlUiaTextRange>(returnVal, parentProvider);
        return xutr.as<XamlAutomation::ITextRangeProvider>();
    }

    Windows::UI::Xaml::Automation::SupportedTextSelection TermControlAutomationPeer::SupportedTextSelection()
    {
        UIA::SupportedTextSelection returnVal;
        THROW_IF_FAILED(_uiaProvider.get_SupportedTextSelection(&returnVal));
        return static_cast<XamlAutomation::SupportedTextSelection>(returnVal);
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

    // Method Description:
    // - extracts the UiaTextRanges from the SAFEARRAY and converts them to Xaml ITextRangeProviders
    // Arguments:
    // - SAFEARRAY of UIA::UiaTextRange (ITextRangeProviders)
    // Return Value:
    // - com_array of Xaml Wrapped UiaTextRange (ITextRangeProviders)
    winrt::com_array<XamlAutomation::ITextRangeProvider> TermControlAutomationPeer::WrapArrayOfTextRangeProviders(SAFEARRAY* textRanges)
    {
        // transfer ownership of UiaTextRanges to this new vector
        auto providers = SafeArrayToOwningVector<::Microsoft::Terminal::UiaTextRange>(textRanges);
        int count = providers.size();

        std::vector<XamlAutomation::ITextRangeProvider> vec;
        vec.reserve(count);
        auto parentProvider = this->ProviderFromPeer(*this);
        for (int i = 0; i < count; i++)
        {
            auto xutr = winrt::make_self<XamlUiaTextRange>(providers[i].detach(), parentProvider);
            vec.emplace_back(xutr.as<XamlAutomation::ITextRangeProvider>());
        }

        winrt::com_array<XamlAutomation::ITextRangeProvider> result{ vec };

        return result;
    }
}
