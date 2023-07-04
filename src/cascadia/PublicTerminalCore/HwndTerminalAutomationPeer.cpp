// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "HwndTerminalAutomationPeer.hpp"
#include "../../types/UiaTracing.h"
#include <UIAutomationCoreApi.h>
#include <delayimp.h>

#pragma warning(suppress : 4471) // We don't control UIAutomationClient
#include <UIAutomationClient.h>

using namespace Microsoft::Console::Types;

static constexpr wchar_t UNICODE_NEWLINE{ L'\n' };

static int CheckDelayedProcException(int exception) noexcept
{
    if (exception == VcppException(ERROR_SEVERITY_ERROR, ERROR_PROC_NOT_FOUND))
    {
        return EXCEPTION_EXECUTE_HANDLER;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

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

void HwndTerminalAutomationPeer::RecordKeyEvent(const WORD vkey)
{
    if (const auto charCode{ MapVirtualKey(vkey, MAPVK_VK_TO_CHAR) })
    {
        if (const auto keyEventChar{ gsl::narrow_cast<wchar_t>(charCode) }; IsReadable({ &keyEventChar, 1 }))
        {
            _keyEvents.emplace_back(keyEventChar);
        }
    }
}

// Implementation of IRawElementProviderSimple::get_PropertyValue.
// Gets custom properties.
IFACEMETHODIMP HwndTerminalAutomationPeer::GetPropertyValue(_In_ PROPERTYID propertyId,
                                                            _Out_ VARIANT* pVariant) noexcept
{
    pVariant->vt = VT_EMPTY;

    // Returning the default will leave the property as the default
    // so we only really need to touch it for the properties we want to implement
    if (propertyId == UIA_ClassNamePropertyId)
    {
        // IMPORTANT: Do NOT change the name. Screen readers like may be dependent on this being "WpfTermControl".
        pVariant->bstrVal = SysAllocString(L"WPFTermControl");
        if (pVariant->bstrVal != nullptr)
        {
            pVariant->vt = VT_BSTR;
        }
    }
    else
    {
        // fall back to shared implementation
        return TermControlUiaProvider::GetPropertyValue(propertyId, pVariant);
    }
    return S_OK;
}

// Method Description:
// - Signals the ui automation client that the terminal's selection has changed and should be updated
// Arguments:
// - <none>
// Return Value:
// - <none>
void HwndTerminalAutomationPeer::SignalSelectionChanged()
{
    UiaTracing::Signal::SelectionChanged();
    LOG_IF_FAILED(UiaRaiseAutomationEvent(this, UIA_Text_TextSelectionChangedEventId));
}

// Method Description:
// - Signals the ui automation client that the terminal's output has changed and should be updated
// Arguments:
// - <none>
// Return Value:
// - <none>
void HwndTerminalAutomationPeer::SignalTextChanged()
{
    UiaTracing::Signal::TextChanged();
    LOG_IF_FAILED(UiaRaiseAutomationEvent(this, UIA_Text_TextChangedEventId));
}

// Method Description:
// - Signals the ui automation client that the cursor's state has changed and should be updated
// Arguments:
// - <none>
// Return Value:
// - <none>
void HwndTerminalAutomationPeer::SignalCursorChanged()
{
    UiaTracing::Signal::CursorChanged();
    LOG_IF_FAILED(UiaRaiseAutomationEvent(this, UIA_Text_TextSelectionChangedEventId));
}

void HwndTerminalAutomationPeer::NotifyNewOutput(std::wstring_view newOutput)
{
    if (_notificationsUnavailable) [[unlikely]]
    {
        // What if you tried to notify, but God said no
        return;
    }

    // Try to suppress any events (or event data)
    // that is just the keypress the user made
    auto sanitized{ Sanitize(newOutput) };
    while (!_keyEvents.empty() && IsReadable(sanitized))
    {
        if (til::toupper_ascii(sanitized.front()) == _keyEvents.front())
        {
            // the key event's character (i.e. the "A" key) matches
            // the output character (i.e. "a" or "A" text).
            // We can assume that the output character resulted from
            // the pressed key, so we can ignore it.
            sanitized = sanitized.substr(1);
            _keyEvents.pop_front();
        }
        else
        {
            // The output doesn't match,
            // so clear the input stack and
            // move on to fire the event.
            _keyEvents.clear();
            break;
        }
    }

    // Suppress event if the remaining text is not readable
    if (!IsReadable(sanitized))
    {
        return;
    }

    const auto sanitizedBstr = wil::make_bstr_nothrow(sanitized.c_str());
    static auto activityId = wil::make_bstr_nothrow(L"TerminalTextOutput");
    _tryNotify(sanitizedBstr.get(), activityId.get());
}

// This needs to be a separate function because it is using SEH try/except, which
// is incompatible with C++ unwinding
void HwndTerminalAutomationPeer::_tryNotify(BSTR string, BSTR activity)
{
    __try
    {
        LOG_IF_FAILED(UiaRaiseNotificationEvent(this, NotificationKind_ActionCompleted, NotificationProcessing_All, string, activity));
    }
    __except (CheckDelayedProcException(GetExceptionCode()))
    {
        _notificationsUnavailable = true;
    }
}
