// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// The functions in this class are specific to the handling of out-of-proc
// content processes by the TermControl. Putting them all in one file keeps
// TermControl.cpp a little less cluttered.

#include "pch.h"
#include "TermControl.h"

using namespace ::Microsoft::Console::Types;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::System;

namespace winrt::Microsoft::Terminal::Control::implementation
{
    winrt::guid TermControl::ContentGuid() const
    {
        return _contentIsOutOfProc() ? _contentProc.Guid() : winrt::guid{};
    }

    bool TermControl::_contentIsOutOfProc() const
    {
        return _contentProc != nullptr;
    }

    bool s_waitOnContentProcess(uint64_t contentPid, HANDLE contentWaitInterrupt)
    {
        // This is the array of HANDLEs that we're going to wait on in
        // WaitForMultipleObjects below.
        // * waits[0] will be the handle to the content process. It gets
        //   signalled when the process exits / dies.
        // * waits[1] is the handle to our _contentWaitInterrupt event. Another
        //   thread can use that to manually break this loop. We'll do that when
        //   we're getting torn down.
        HANDLE waits[2];
        waits[1] = contentWaitInterrupt;
        bool displayError = true;

        // At any point in all this, the content process might die. If it does,
        // we want to raise an error message, to inform that this control is now
        // dead.
        try
        {
            // This might fail to even ask the content for it's PID.
            wil::unique_handle hContent{ OpenProcess(SYNCHRONIZE,
                                                     FALSE,
                                                     static_cast<DWORD>(contentPid)) };

            // If we fail to open the content, then they don't exist
            //  anymore! We'll need to immediately raise the notification that the content has died.
            if (hContent.get() == nullptr)
            {
                const auto gle = GetLastError();
                TraceLoggingWrite(g_hTerminalControlProvider,
                                  "TermControl_FailedToOpenContent",
                                  TraceLoggingUInt64(gle, "lastError", "The result of GetLastError"),
                                  TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                  TraceLoggingKeyword(TIL_KEYWORD_TRACE));
                return displayError;
            }

            waits[0] = hContent.get();

            switch (WaitForMultipleObjects(2, waits, FALSE, INFINITE))
            {
            case WAIT_OBJECT_0 + 0: // waits[0] was signaled, the handle to the content process

                TraceLoggingWrite(g_hTerminalControlProvider,
                                  "TermControl_ContentDied",
                                  TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                  TraceLoggingKeyword(TIL_KEYWORD_TRACE));
                break;

            case WAIT_OBJECT_0 + 1: // waits[1] was signaled, our manual interrupt

                TraceLoggingWrite(g_hTerminalControlProvider,
                                  "TermControl_ContentWaitInterrupted",
                                  TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                  TraceLoggingKeyword(TIL_KEYWORD_TRACE));
                displayError = false;
                break;

            case WAIT_TIMEOUT:
                // This should be impossible.
                TraceLoggingWrite(g_hTerminalControlProvider,
                                  "TermControl_ContentWaitTimeout",
                                  TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                  TraceLoggingKeyword(TIL_KEYWORD_TRACE));
                break;

            default:
            {
                // Returning any other value is invalid. Just die.
                const auto gle = GetLastError();
                TraceLoggingWrite(g_hTerminalControlProvider,
                                  "TermControl_WaitFailed",
                                  TraceLoggingUInt64(gle, "lastError", "The result of GetLastError"),
                                  TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                  TraceLoggingKeyword(TIL_KEYWORD_TRACE));
                break;
            }
            }
        }
        catch (...)
        {
            // Theoretically, if window[1] dies when we're trying to get
            // its PID we'll get here. We can probably just exit here.

            TraceLoggingWrite(g_hTerminalControlProvider,
                              "TermControl_ExceptionInWaitThread",
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                              TraceLoggingKeyword(TIL_KEYWORD_TRACE));
        }
        return displayError;
    }

    void TermControl::_createContentWaitThread()
    {
        _contentWaitThread = std::thread([weakThis = get_weak(), contentPid = _contentProc.GetPID(), contentWaitInterrupt = _contentWaitInterrupt.get()] {
            if (s_waitOnContentProcess(contentPid, contentWaitInterrupt))
            {
                // When s_waitOnContentProcess returns, if it returned true, we
                // should display a dialog in our bounds to indicate that we
                // were closed unexpectedly. If we closed in an expected way,
                // then s_waitOnContentProcess will return false.
                if (auto control{ weakThis.get() })
                {
                    control->_raiseContentDied();
                }
            }
        });
    }

    winrt::fire_and_forget TermControl::_raiseContentDied()
    {
        auto weakThis{ get_weak() };
        co_await winrt::resume_foreground(Dispatcher());

        if (auto control{ weakThis.get() }; !control->_IsClosing())
        {
            if (auto loadedUiElement{ FindName(L"ContentDiedNotice") })
            {
                if (auto uiElement{ loadedUiElement.try_as<::winrt::Windows::UI::Xaml::UIElement>() })
                {
                    uiElement.Visibility(Visibility::Visible);
                }
            }
        }
    }
    // Method Description:
    // - Handler for when the "Content Died" dialog's button is clicked.
    void TermControl::_ContentDiedCloseButton_Click(IInspectable const& /*sender*/, IInspectable const& /*args*/)
    {
        // Alert whoever's hosting us that the connection was closed.
        // When they come asking what the new connection state is, we'll reply with Closed
        _ConnectionStateChangedHandlers(*this, nullptr);
    }
}
