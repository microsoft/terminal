// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <UIAutomationCore.h>
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
// - true, if the text is readable; otherwise, false.
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
                                                         const Core::Padding padding) :
        TermControlAutomationPeerT<TermControlAutomationPeer>(*owner.get()), // pass owner to FrameworkElementAutomationPeer
        _termControl{ owner }
    {
        THROW_IF_FAILED(::Microsoft::WRL::MakeAndInitialize<::Microsoft::Terminal::TermControlUiaProvider>(&_uiaProvider, owner->_core->GetRenderData(), this));
        SetControlPadding(padding);
    };

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
        // that are just the keypresses that the user made
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
        // fall back to title if profile name is empty
        if (auto control{ _termControl.get() })
        {
            const auto originalName = control->GetStartingTitle();
            if (originalName.empty())
            {
                return control->Title();
            }
            else
            {
                return originalName;
            }
        }

        return {};
    }

    hstring TermControlAutomationPeer::GetHelpTextCore() const
    {
        if (const auto control{ _termControl.get() })
        {
            return control->Title();
        }
        return {};
    }

    AutomationLiveSetting TermControlAutomationPeer::GetLiveSettingCore() const
    {
        return AutomationLiveSetting::Polite;
    }

#pragma region ITextProvider
    com_array<XamlAutomation::ITextRangeProvider> TermControlAutomationPeer::GetSelection()
    {
        SAFEARRAY* pReturnVal;
        THROW_IF_FAILED(_uiaProvider->GetSelection(&pReturnVal));
        return WrapArrayOfTextRangeProviders(pReturnVal);
    }

    com_array<XamlAutomation::ITextRangeProvider> TermControlAutomationPeer::GetVisibleRanges()
    {
        SAFEARRAY* pReturnVal;
        THROW_IF_FAILED(_uiaProvider->GetVisibleRanges(&pReturnVal));
        return WrapArrayOfTextRangeProviders(pReturnVal);
    }

    XamlAutomation::ITextRangeProvider TermControlAutomationPeer::RangeFromChild(XamlAutomation::IRawElementProviderSimple /*childElement*/)
    {
        UIA::ITextRangeProvider* returnVal;
        // ScreenInfoUiaProvider doesn't actually use parameter, so just pass in nullptr
        THROW_IF_FAILED(_uiaProvider->RangeFromChild(/* IRawElementProviderSimple */ nullptr,
                                                     &returnVal));
        return _CreateXamlUiaTextRange(returnVal);
    }

    XamlAutomation::ITextRangeProvider TermControlAutomationPeer::RangeFromPoint(Windows::Foundation::Point screenLocation)
    {
        UIA::ITextRangeProvider* returnVal;
        THROW_IF_FAILED(_uiaProvider->RangeFromPoint({ screenLocation.X, screenLocation.Y }, &returnVal));
        return _CreateXamlUiaTextRange(returnVal);
    }

    XamlAutomation::ITextRangeProvider TermControlAutomationPeer::DocumentRange()
    {
        UIA::ITextRangeProvider* returnVal;
        THROW_IF_FAILED(_uiaProvider->get_DocumentRange(&returnVal));
        return _CreateXamlUiaTextRange(returnVal);
    }

    XamlAutomation::SupportedTextSelection TermControlAutomationPeer::SupportedTextSelection()
    {
        UIA::SupportedTextSelection returnVal;
        THROW_IF_FAILED(_uiaProvider->get_SupportedTextSelection(&returnVal));
        return static_cast<XamlAutomation::SupportedTextSelection>(returnVal);
    }

#pragma endregion

#pragma region IControlAccessibilityInfo
    til::size TermControlAutomationPeer::GetFontSize() const noexcept
    {
        if (const auto control{ _termControl.get() })
        {
            return { til::math::rounding, control->_core->FontSize() };
        }
        return {};
    }

    til::rect TermControlAutomationPeer::GetBounds() const noexcept
    {
        return { til::math::rounding, GetBoundingRectangle() };
    }

    HRESULT TermControlAutomationPeer::GetHostUiaProvider(IRawElementProviderSimple** provider)
    {
        RETURN_HR_IF(E_INVALIDARG, provider == nullptr);
        *provider = nullptr;

        return S_OK;
    }

    til::rect TermControlAutomationPeer::GetPadding() const noexcept
    {
        if (const auto control{ _termControl.get() })
        {
            const auto padding{ control->GetPadding() };
            return {
                static_cast<float>(padding.Left),
                static_cast<float>(padding.Top),
                static_cast<float>(padding.Right),
                static_cast<float>(padding.Bottom),
            };
        }
        return {};
    }

    void TermControlAutomationPeer::ChangeViewport(const til::inclusive_rect& NewWindow)
    {
        if (const auto control{ _termControl.get() })
        {
            control->_interactivity->UpdateScrollbar(static_cast<float>(NewWindow.top));
        }
    }
#pragma endregion

    XamlAutomation::ITextRangeProvider TermControlAutomationPeer::_CreateXamlUiaTextRange(UIA::ITextRangeProvider* returnVal) const
    {
        const auto xutr = winrt::make_self<XamlUiaTextRange>(returnVal, *this);
        return xutr.as<XamlAutomation::ITextRangeProvider>();
    };

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
        const auto len = gsl::narrow<uint32_t>(providers.size());
        com_array<XamlAutomation::ITextRangeProvider> result{ len };

        for (uint32_t i = 0; i < len; ++i)
        {
            if (auto xutr = _CreateXamlUiaTextRange(providers[i].detach()))
            {
                result[i] = std::move(xutr);
            }
        }

        return result;
    }
}
