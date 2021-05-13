// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <UIAutomationCore.h>
#include <LibraryResources.h>
#include "TermControlAutomationPeer.h"
#include "TermControl.h"
#include "TermControlAutomationPeer.g.cpp"

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
    TermControlAutomationPeer::TermControlAutomationPeer(TermControl* owner,
                                                         Control::InteractivityAutomationPeer impl) :
        TermControlAutomationPeerT<TermControlAutomationPeer>(*owner), // pass owner to FrameworkElementAutomationPeer
        _termControl{ owner },
        _implementation{ impl }
    {
        UpdateControlBounds();

        _implementation.SelectionChanged([this](auto&&, auto&&) { SignalSelectionChanged(); });
        _implementation.TextChanged([this](auto&&, auto&&) { SignalTextChanged(); });
        _implementation.CursorChanged([this](auto&&, auto&&) { SignalCursorChanged(); });
    };

    // Method Description:
    // - Inform the interactivity layer about the bounds of the control.
    //   IControlAccessibilityInfo needs to know this information, but it cannot
    //   ask us directly.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TermControlAutomationPeer::UpdateControlBounds()
    {
        // FrameworkElementAutomationPeer has this great GetBoundingRectangle
        // method that's seemingly impossible to recreate just from the
        // UserControl itself. Weird. But we can use it handily here!
        _implementation.SetControlBounds(GetBoundingRectangle());
    }
    void TermControlAutomationPeer::SetControlPadding(const Core::Padding padding)
    {
        _implementation.SetControlPadding(padding);
    }

    // Method Description:
    // - Signals the ui automation client that the terminal's selection has changed and should be updated
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TermControlAutomationPeer::SignalSelectionChanged()
    {
        UiaTracing::Signal::SelectionChanged();
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
    void TermControlAutomationPeer::SignalTextChanged()
    {
        UiaTracing::Signal::TextChanged();
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
    void TermControlAutomationPeer::SignalCursorChanged()
    {
        UiaTracing::Signal::CursorChanged();
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

    hstring TermControlAutomationPeer::GetClassNameCore() const
    {
        return L"TermControl";
    }

    AutomationControlType TermControlAutomationPeer::GetAutomationControlTypeCore() const
    {
        return AutomationControlType::Text;
    }

    hstring TermControlAutomationPeer::GetLocalizedControlTypeCore() const
    {
        return RS_(L"TerminalControl_ControlType");
    }

    Windows::Foundation::IInspectable TermControlAutomationPeer::GetPatternCore(PatternInterface patternInterface) const
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

    AutomationOrientation TermControlAutomationPeer::GetOrientationCore() const
    {
        return AutomationOrientation::Vertical;
    }

    hstring TermControlAutomationPeer::GetNameCore() const
    {
        // fallback to title if profile name is empty
        auto profileName = _termControl->GetProfileName();
        if (profileName.empty())
        {
            return _termControl->Title();
        }
        return profileName;
    }

    hstring TermControlAutomationPeer::GetHelpTextCore() const
    {
        return _termControl->Title();
    }

    AutomationLiveSetting TermControlAutomationPeer::GetLiveSettingCore() const
    {
        return AutomationLiveSetting::Polite;
    }

#pragma region ITextProvider
    com_array<XamlAutomation::ITextRangeProvider> TermControlAutomationPeer::GetSelection()
    {
        return _implementation.GetSelection();
    }

    com_array<XamlAutomation::ITextRangeProvider> TermControlAutomationPeer::GetVisibleRanges()
    {
        return _implementation.GetVisibleRanges();
    }

    XamlAutomation::ITextRangeProvider TermControlAutomationPeer::RangeFromChild(XamlAutomation::IRawElementProviderSimple childElement)
    {
        return _implementation.RangeFromChild(childElement);
    }

    XamlAutomation::ITextRangeProvider TermControlAutomationPeer::RangeFromPoint(Windows::Foundation::Point screenLocation)
    {
        return _implementation.RangeFromPoint(screenLocation);
    }

    XamlAutomation::ITextRangeProvider TermControlAutomationPeer::DocumentRange()
    {
        return _implementation.DocumentRange();
    }

    XamlAutomation::SupportedTextSelection TermControlAutomationPeer::SupportedTextSelection()
    {
        return _implementation.SupportedTextSelection();
    }

#pragma endregion

    // Method Description:
    // - extracts the UiaTextRanges from the SAFEARRAY and converts them to Xaml ITextRangeProviders
    // Arguments:
    // - SAFEARRAY of UIA::UiaTextRange (ITextRangeProviders)
    // Return Value:
    // - com_array of Xaml Wrapped UiaTextRange (ITextRangeProviders)
    com_array<XamlAutomation::ITextRangeProvider> TermControlAutomationPeer::WrapArrayOfTextRangeProviders(SAFEARRAY* textRanges)
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
