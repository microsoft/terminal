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

static constexpr wchar_t UNICODE_NEWLINE{ L'\n' };

// Method Description:
// - creates a copy of the provided text with all of the control characters removed
// Arguments:
// - text: the string we're sanitizing
// Return Value:
// - a copy of "sanitized" with all of the control characters removed
static std::wstring Sanitize(std::wstring_view text)
{
    std::wstring sanitized{ text };
    sanitized.erase(std::remove_if(sanitized.begin(), sanitized.end(), [](wchar_t c) {
                        return (c < UNICODE_SPACE && c != UNICODE_NEWLINE) || c == 0x7F /*DEL*/;
                    }),
                    sanitized.end());
    return sanitized;
}

// Method Description:
// - verifies if a given string has text that would be read by a screen reader.
// - a string of control characters, for example, would not be read.
// Arguments:
// - text: the string we're validating
// Return Value:
// - true, if the text is readable. false, otherwise.
static constexpr bool IsReadable(std::wstring_view text)
{
    for (const auto c : text)
    {
        if (c > UNICODE_SPACE)
        {
            return true;
        }
    }
    return false;
}

namespace winrt::Microsoft::Terminal::Control::implementation
{
    TermControlAutomationPeer::TermControlAutomationPeer(winrt::com_ptr<TermControl> owner,
                                                         const Core::Padding padding,
                                                         Control::InteractivityAutomationPeer impl) :
        TermControlAutomationPeerT<TermControlAutomationPeer>(*owner.get()), // pass owner to FrameworkElementAutomationPeer
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
        _contentAutomationPeer.NewOutput([this](auto&&, hstring newOutput) { NotifyNewOutput(newOutput); });
        _contentAutomationPeer.ParentProvider(*this);
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

    void TermControlAutomationPeer::RecordKeyEvent(const WORD vkey)
    {
        if (const auto charCode{ MapVirtualKey(vkey, MAPVK_VK_TO_CHAR) })
        {
            if (const auto keyEventChar{ gsl::narrow_cast<wchar_t>(charCode) }; IsReadable({ &keyEventChar, 1 }))
            {
                _keyEvents.lock()->emplace_back(keyEventChar);
            }
        }
    }

    void TermControlAutomationPeer::Close()
    {
        // GH#13978: If the TermControl has already been removed from the UI tree, XAML might run into weird bugs.
        // This will prevent the `dispatcher.RunAsync` calls below from raising UIA events on the main thread.
        _termControl = {};
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
                if (auto control{ strongThis->_termControl.get() })
                {
                    // The event that is raised when the text selection is modified.
                    strongThis->RaiseAutomationEvent(AutomationEvents::TextPatternOnTextSelectionChanged);
                }
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
                if (auto control{ strongThis->_termControl.get() })
                {
                    // The event that is raised when textual content is modified.
                    strongThis->RaiseAutomationEvent(AutomationEvents::TextPatternOnTextChanged);
                }
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
                if (auto control{ strongThis->_termControl.get() })
                {
                    // The event that is raised when the text was changed in an edit control.
                    // Do NOT fire a TextEditTextChanged. Generally, an app on the other side
                    //    will expect more information. Though you can dispatch that event
                    //    on its own, it may result in a nullptr exception on the other side
                    //    because no additional information was provided. Crashing the screen
                    //    reader.
                    strongThis->RaiseAutomationEvent(AutomationEvents::TextPatternOnTextSelectionChanged);
                }
            }
        });
    }

    void TermControlAutomationPeer::NotifyNewOutput(std::wstring_view newOutput)
    {
        auto sanitized{ Sanitize(newOutput) };
        // Try to suppress any events (or event data)
        // that are just the keypresses the user made
        {
            auto keyEvents = _keyEvents.lock();
            while (!keyEvents->empty() && IsReadable(sanitized))
            {
                if (til::toupper_ascii(sanitized.front()) == keyEvents->front())
                {
                    // the key event's character (i.e. the "A" key) matches
                    // the output character (i.e. "a" or "A" text).
                    // We can assume that the output character resulted from
                    // the pressed key, so we can ignore it.
                    sanitized = sanitized.substr(1);
                    keyEvents->pop_front();
                }
                else
                {
                    // The output doesn't match,
                    // so clear the input stack and
                    // move on to fire the event.
                    keyEvents->clear();
                    break;
                }
            }
        }

        // Suppress event if the remaining text is not readable
        if (!IsReadable(sanitized))
        {
            return;
        }

        auto dispatcher{ Dispatcher() };
        if (!dispatcher)
        {
            return;
        }

        // IMPORTANT:
        // [1] make sure the scope returns a copy of "sanitized" so that it isn't accidentally deleted
        // [2] AutomationNotificationProcessing::All --> ensures it can be interrupted by keyboard events
        // [3] Do not "RunAsync(...).get()". For whatever reason, this causes NVDA to just not receive "SignalTextChanged()"'s events.
        dispatcher.RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, [weakThis{ get_weak() }, sanitizedCopy{ hstring{ sanitized } }]() {
            if (auto strongThis{ weakThis.get() })
            {
                if (auto control{ strongThis->_termControl.get() })
                {
                    try
                    {
                        strongThis->RaiseNotificationEvent(AutomationNotificationKind::ActionCompleted,
                                                           AutomationNotificationProcessing::All,
                                                           sanitizedCopy,
                                                           L"TerminalTextOutput");
                    }
                    CATCH_LOG();
                }
            }
        });
    }

    hstring TermControlAutomationPeer::GetClassNameCore() const
    {
        // IMPORTANT: Do NOT change the name. Screen readers like JAWS may be dependent on this being "TermControl".
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
        if (auto control{ _termControl.get() })
        {
            const auto profileName = control->GetProfileName();
            if (profileName.empty())
            {
                return control->Title();
            }
            else
            {
                return profileName;
            }
        }

        return L"";
    }

    hstring TermControlAutomationPeer::GetHelpTextCore() const
    {
        if (const auto control{ _termControl.get() })
        {
            return control->Title();
        }
        return L"";
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
}
