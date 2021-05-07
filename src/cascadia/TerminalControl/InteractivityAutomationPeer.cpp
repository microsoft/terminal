// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <UIAutomationCore.h>
#include <LibraryResources.h>
#include "InteractivityAutomationPeer.h"
#include "InteractivityAutomationPeer.g.cpp"

#include "XamlUiaTextRange.h"
#include "../types/UiaTracing.h"

using namespace Microsoft::Console::Types;
using namespace winrt::Windows::UI::Xaml::Automation::Peers;
using namespace winrt::Windows::Graphics::Display;

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

namespace winrt::Microsoft::Terminal::Control::implementation
{
    InteractivityAutomationPeer::InteractivityAutomationPeer(Control::implementation::ControlInteractivity* owner) :
        _interactivity{ owner }
    {
        THROW_IF_FAILED(::Microsoft::WRL::MakeAndInitialize<::Microsoft::Terminal::TermControlUiaProvider>(&_uiaProvider, _interactivity->GetUiaData(), this));
    };

    void InteractivityAutomationPeer::SetControlBounds(const Windows::Foundation::Rect bounds)
    {
        _controlBounds = til::rectangle{ til::math::rounding, bounds };
    }
    void InteractivityAutomationPeer::SetControlPadding(const Core::Padding padding)
    {
        _controlPadding = padding;
    }

    // Method Description:
    // - Signals the ui automation client that the terminal's selection has changed and should be updated
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void InteractivityAutomationPeer::SignalSelectionChanged()
    {
        UiaTracing::Signal::SelectionChanged();

        // TODO:projects/5#card-50760282
        // We seemingly got a Dispatcher() for free when we said we extended
        // Windows.UI.Automation.Peers.AutomationPeer. This is suspect to me.
        // This probably won't work when out-of-proc from the WinUI layer.

        Dispatcher().RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, [&]() {
            // The event that is raised when the text selection is modified.
            RaiseAutomationEvent(AutomationEvents::TextPatternOnTextSelectionChanged);
        });
    }

    // Method Description:
    // - Signals the ui automation client that the terminal's output has changed and should be updated
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void InteractivityAutomationPeer::SignalTextChanged()
    {
        UiaTracing::Signal::TextChanged();

        // TODO:projects/5#card-50760282
        // We seemingly got a Dispatcher() for free when we said we extended
        // Windows.UI.Automation.Peers.AutomationPeer. This is suspect to me.
        // This probably won't work when out-of-proc from the WinUI layer.

        Dispatcher().RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, [&]() {
            // The event that is raised when textual content is modified.
            RaiseAutomationEvent(AutomationEvents::TextPatternOnTextChanged);
        });
    }

    // Method Description:
    // - Signals the ui automation client that the cursor's state has changed and should be updated
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void InteractivityAutomationPeer::SignalCursorChanged()
    {
        UiaTracing::Signal::CursorChanged();

        // TODO:projects/5#card-50760282
        // We seemingly got a Dispatcher() for free when we said we extended
        // Windows.UI.Automation.Peers.AutomationPeer. This is suspect to me.
        // This probably won't work when out-of-proc from the WinUI layer.

        Dispatcher().RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, [&]() {
            // The event that is raised when the text was changed in an edit control.
            // Do NOT fire a TextEditTextChanged. Generally, an app on the other side
            //    will expect more information. Though you can dispatch that event
            //    on its own, it may result in a nullptr exception on the other side
            //    because no additional information was provided. Crashing the screen
            //    reader.
            RaiseAutomationEvent(AutomationEvents::TextPatternOnTextSelectionChanged);
        });
    }

#pragma region ITextProvider
    com_array<XamlAutomation::ITextRangeProvider> InteractivityAutomationPeer::GetSelection()
    {
        SAFEARRAY* pReturnVal;
        THROW_IF_FAILED(_uiaProvider->GetSelection(&pReturnVal));
        return WrapArrayOfTextRangeProviders(pReturnVal);
    }

    com_array<XamlAutomation::ITextRangeProvider> InteractivityAutomationPeer::GetVisibleRanges()
    {
        SAFEARRAY* pReturnVal;
        THROW_IF_FAILED(_uiaProvider->GetVisibleRanges(&pReturnVal));
        return WrapArrayOfTextRangeProviders(pReturnVal);
    }

    XamlAutomation::ITextRangeProvider InteractivityAutomationPeer::RangeFromChild(XamlAutomation::IRawElementProviderSimple childElement)
    {
        UIA::ITextRangeProvider* returnVal;
        // ScreenInfoUiaProvider doesn't actually use parameter, so just pass in nullptr
        THROW_IF_FAILED(_uiaProvider->RangeFromChild(/* IRawElementProviderSimple */ nullptr,
                                                     &returnVal));

        auto parentProvider = this->ProviderFromPeer(*this);
        auto xutr = winrt::make_self<XamlUiaTextRange>(returnVal, parentProvider);
        return xutr.as<XamlAutomation::ITextRangeProvider>();
    }

    XamlAutomation::ITextRangeProvider InteractivityAutomationPeer::RangeFromPoint(Windows::Foundation::Point screenLocation)
    {
        UIA::ITextRangeProvider* returnVal;
        THROW_IF_FAILED(_uiaProvider->RangeFromPoint({ screenLocation.X, screenLocation.Y }, &returnVal));

        auto parentProvider = this->ProviderFromPeer(*this);
        auto xutr = winrt::make_self<XamlUiaTextRange>(returnVal, parentProvider);
        return xutr.as<XamlAutomation::ITextRangeProvider>();
    }

    XamlAutomation::ITextRangeProvider InteractivityAutomationPeer::DocumentRange()
    {
        UIA::ITextRangeProvider* returnVal;
        THROW_IF_FAILED(_uiaProvider->get_DocumentRange(&returnVal));

        auto parentProvider = this->ProviderFromPeer(*this);
        auto xutr = winrt::make_self<XamlUiaTextRange>(returnVal, parentProvider);
        return xutr.as<XamlAutomation::ITextRangeProvider>();
    }

    XamlAutomation::SupportedTextSelection InteractivityAutomationPeer::SupportedTextSelection()
    {
        UIA::SupportedTextSelection returnVal;
        THROW_IF_FAILED(_uiaProvider->get_SupportedTextSelection(&returnVal));
        return static_cast<XamlAutomation::SupportedTextSelection>(returnVal);
    }

#pragma endregion

#pragma region IControlAccessibilityInfo
    COORD InteractivityAutomationPeer::GetFontSize() const
    {
        return til::size{ til::math::rounding, _interactivity->GetCore().FontSize() };
    }

    RECT InteractivityAutomationPeer::GetBounds() const
    {
        return _controlBounds;
    }

    HRESULT InteractivityAutomationPeer::GetHostUiaProvider(IRawElementProviderSimple** provider)
    {
        RETURN_HR_IF(E_INVALIDARG, provider == nullptr);
        *provider = nullptr;

        return S_OK;
    }

    RECT InteractivityAutomationPeer::GetPadding() const
    {
        return _controlPadding;
    }

    double InteractivityAutomationPeer::GetScaleFactor() const
    {
        return DisplayInformation::GetForCurrentView().RawPixelsPerViewPixel();
    }

    void InteractivityAutomationPeer::ChangeViewport(const SMALL_RECT NewWindow)
    {
        _interactivity->UpdateScrollbar(NewWindow.Top);
    }
#pragma endregion

    // Method Description:
    // - extracts the UiaTextRanges from the SAFEARRAY and converts them to Xaml ITextRangeProviders
    // Arguments:
    // - SAFEARRAY of UIA::UiaTextRange (ITextRangeProviders)
    // Return Value:
    // - com_array of Xaml Wrapped UiaTextRange (ITextRangeProviders)
    com_array<XamlAutomation::ITextRangeProvider> InteractivityAutomationPeer::WrapArrayOfTextRangeProviders(SAFEARRAY* textRanges)
    {
        // transfer ownership of UiaTextRanges to this new vector
        auto providers = SafeArrayToOwningVector<::Microsoft::Terminal::TermControlUiaTextRange>(textRanges);
        int count = gsl::narrow<int>(providers.size());

        std::vector<XamlAutomation::ITextRangeProvider> vec;
        vec.reserve(count);
        auto parentProvider = this->ProviderFromPeer(*this);
        for (int i = 0; i < count; i++)
        {
            auto xutr = make_self<XamlUiaTextRange>(providers[i].detach(), parentProvider);
            vec.emplace_back(xutr.as<XamlAutomation::ITextRangeProvider>());
        }

        com_array<XamlAutomation::ITextRangeProvider> result{ vec };

        return result;
    }
}
