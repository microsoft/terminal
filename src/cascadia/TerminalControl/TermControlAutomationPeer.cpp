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
                                                         const Core::Padding padding,
                                                         Control::InteractivityAutomationPeer impl) :
        TermControlAutomationPeerT<TermControlAutomationPeer>(*owner), // pass owner to FrameworkElementAutomationPeer
        _termControl{ owner },
        _contentAutomationPeer{ impl }
    {
        UpdateControlBounds();
        SetControlPadding(padding);
        // Listen for UIA signalling events from the implementation. We need to
        // be the one to actually raise these automation events, so they go
        // through the UI tree correctly.
        _contentAutomationPeer.SelectionChanged([this](auto&&, auto&&) { SignalSelectionChanged(); });
        _contentAutomationPeer.TextChanged([this](auto&&, auto&&) { SignalTextChanged(); });
        _contentAutomationPeer.CursorChanged([this](auto&&, auto&&) { SignalCursorChanged(); });
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
        _contentAutomationPeer.SetControlBounds(GetBoundingRectangle());
    }
    void TermControlAutomationPeer::SetControlPadding(const Core::Padding padding)
    {
        _contentAutomationPeer.SetControlPadding(padding);
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
        auto dispatcher{ Dispatcher() };
        if (!dispatcher)
        {
            return;
        }
        dispatcher.RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, [weakThis{ get_weak() }]() {
            if (auto strongThis{ weakThis.get() })
            {
                // The event that is raised when the text selection is modified.
                strongThis->RaiseAutomationEvent(AutomationEvents::TextPatternOnTextSelectionChanged);
            }
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
        auto dispatcher{ Dispatcher() };
        if (!dispatcher)
        {
            return;
        }
        dispatcher.RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, [weakThis{ get_weak() }]() {
            if (auto strongThis{ weakThis.get() })
            {
                // The event that is raised when textual content is modified.
                strongThis->RaiseAutomationEvent(AutomationEvents::TextPatternOnTextChanged);
            }
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
        auto dispatcher{ Dispatcher() };
        if (!dispatcher)
        {
            return;
        }
        dispatcher.RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, [weakThis{ get_weak() }]() {
            if (auto strongThis{ weakThis.get() })
            {
                // The event that is raised when the text was changed in an edit control.
                // Do NOT fire a TextEditTextChanged. Generally, an app on the other side
                //    will expect more information. Though you can dispatch that event
                //    on its own, it may result in a nullptr exception on the other side
                //    because no additional information was provided. Crashing the screen
                //    reader.
                strongThis->RaiseAutomationEvent(AutomationEvents::TextPatternOnTextSelectionChanged);
            }
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
        return _contentAutomationPeer.GetSelection();
    }

    com_array<XamlAutomation::ITextRangeProvider> TermControlAutomationPeer::GetVisibleRanges()
    {
        return _contentAutomationPeer.GetVisibleRanges();
    }

    XamlAutomation::ITextRangeProvider TermControlAutomationPeer::RangeFromChild(XamlAutomation::IRawElementProviderSimple childElement)
    {
        return _contentAutomationPeer.RangeFromChild(childElement);
    }

    XamlAutomation::ITextRangeProvider TermControlAutomationPeer::RangeFromPoint(Windows::Foundation::Point screenLocation)
    {
        return _contentAutomationPeer.RangeFromPoint(screenLocation);
    }

    XamlAutomation::ITextRangeProvider TermControlAutomationPeer::DocumentRange()
    {
        return _contentAutomationPeer.DocumentRange();
    }

    XamlAutomation::SupportedTextSelection TermControlAutomationPeer::SupportedTextSelection()
    {
        return _contentAutomationPeer.SupportedTextSelection();
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
