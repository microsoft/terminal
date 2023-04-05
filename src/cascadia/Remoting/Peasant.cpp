// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Peasant.h"
#include "CommandlineArgs.h"
#include "SummonWindowBehavior.h"
#include "GetWindowLayoutArgs.h"
#include "Peasant.g.cpp"
#include "../../types/inc/utils.hpp"
#include "AttachRequest.g.cpp"
#include "RequestReceiveContentArgs.g.cpp"

using namespace winrt;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Windows::Foundation;
using namespace ::Microsoft::Console;

namespace winrt::Microsoft::Terminal::Remoting::implementation
{
    Peasant::Peasant() :
        _ourPID{ GetCurrentProcessId() }
    {
    }

    // This constructor is intended to be used in unit tests,
    // but we need to make it public in order to use make_self
    // in the tests. It's not exposed through the idl though
    // so it's not _truly_ fully public which should be acceptable.
    Peasant::Peasant(const uint64_t testPID) :
        _ourPID{ testPID }
    {
    }

    void Peasant::AssignID(uint64_t id)
    {
        _id = id;
    }

    uint64_t Peasant::GetID()
    {
        return _id;
    }

    uint64_t Peasant::GetPID()
    {
        return _ourPID;
    }

    bool Peasant::ExecuteCommandline(const Remoting::CommandlineArgs& args)
    {
        // If this is the first set of args we were ever told about, stash them
        // away. We'll need to get at them later, when we setup the startup
        // actions for the window.
        if (_initialArgs == nullptr)
        {
            _initialArgs = args;
        }

        TraceLoggingWrite(g_hRemotingProvider,
                          "Peasant_ExecuteCommandline",
                          TraceLoggingUInt64(GetID(), "peasantID", "Our ID"),
                          TraceLoggingWideString(args.CurrentDirectory().c_str(), "directory", "the provided cwd"),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));

        // Raise an event with these args. The AppHost will listen for this
        // event to know when to take these args and dispatch them to a
        // currently-running window.
        _ExecuteCommandlineRequestedHandlers(*this, args);

        return true;
    }

    Remoting::CommandlineArgs Peasant::InitialArgs()
    {
        return _initialArgs;
    }

    void Peasant::ActivateWindow(const Remoting::WindowActivatedArgs& args)
    {
        // TODO: projects/5 - somehow, pass an identifier for the current
        // desktop into this method. The Peasant shouldn't need to be able to
        // figure it out, but it will need to report it to the monarch.

        // Store these new args as our last activated state. If a new monarch
        // comes looking, we can use this info to tell them when we were last
        // activated.
        _lastActivatedArgs = args;

        auto successfullyNotified = false;
        // Raise our WindowActivated event, to let the monarch know we've been
        // activated.
        try
        {
            // Try/catch this, because the other side of this event is handled
            // by the monarch. The monarch might have died. If they have, this
            // will throw an exception. Just eat it, the election thread will
            // handle hooking up the new one.
            _WindowActivatedHandlers(*this, args);
            successfullyNotified = true;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }

        TraceLoggingWrite(g_hRemotingProvider,
                          "Peasant_ActivateWindow",
                          TraceLoggingUInt64(GetID(), "peasantID", "Our ID"),
                          TraceLoggingBoolean(successfullyNotified, "successfullyNotified", "true if we successfully notified the monarch"),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }

    // Method Description:
    // - Retrieve the WindowActivatedArgs describing the last activation of this
    //   peasant. New monarchs can use this state to determine when we were last
    //   activated.
    // Arguments:
    // - <none>
    // Return Value:
    // - a WindowActivatedArgs with info about when and where we were last activated.
    Remoting::WindowActivatedArgs Peasant::GetLastActivatedArgs()
    {
        return _lastActivatedArgs;
    }

    // Method Description:
    // - Summon this peasant to become the active window. Currently, it just
    //   causes the peasant to become the active window wherever the window
    //   already was.
    // - Will raise a SummonRequested event to ask the hosting window to handle for us.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Peasant::Summon(const Remoting::SummonWindowBehavior& summonBehavior)
    {
        auto localCopy = winrt::make<implementation::SummonWindowBehavior>(summonBehavior);

        TraceLoggingWrite(g_hRemotingProvider,
                          "Peasant_Summon",
                          TraceLoggingUInt64(GetID(), "peasantID", "Our ID"),
                          TraceLoggingUInt64(localCopy.MoveToCurrentDesktop(), "MoveToCurrentDesktop", "true if we should move to the current desktop"),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));

        _SummonRequestedHandlers(*this, localCopy);
    }

    // Method Description:
    // - Tell this window to display its window ID. We'll raise a
    //   DisplayWindowIdRequested event, which will get handled in the AppHost,
    //   and used to tell the app to display the ID toast.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Peasant::DisplayWindowId()
    {
        // Not worried about try/catching this. The handler is in AppHost, which
        // is in-proc for us.
        _DisplayWindowIdRequestedHandlers(*this, nullptr);
    }

    // Method Description:
    // - Raises an event to ask that all windows be identified. This will come
    //   back to us when the Monarch handles the event and calls our
    //   DisplayWindowId method.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Peasant::RequestIdentifyWindows()
    {
        auto successfullyNotified = false;

        try
        {
            // Try/catch this, because the other side of this event is handled
            // by the monarch. The monarch might have died. If they have, this
            // will throw an exception. Just eat it, the election thread will
            // handle hooking up the new one.
            _IdentifyWindowsRequestedHandlers(*this, nullptr);
            successfullyNotified = true;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
        TraceLoggingWrite(g_hRemotingProvider,
                          "Peasant_RequestIdentifyWindows",
                          TraceLoggingUInt64(GetID(), "peasantID", "Our ID"),
                          TraceLoggingBoolean(successfullyNotified, "successfullyNotified", "true if we successfully notified the monarch"),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }

    void Peasant::RequestRename(const winrt::Microsoft::Terminal::Remoting::RenameRequestArgs& args)
    {
        auto successfullyNotified = false;
        const auto oldName{ _WindowName };
        try
        {
            // Try/catch this, because the other side of this event is handled
            // by the monarch. The monarch might have died. If they have, this
            // will throw an exception. Just eat it, the election thread will
            // handle hooking up the new one.
            _RenameRequestedHandlers(*this, args);
            if (args.Succeeded())
            {
                _WindowName = args.NewName();
            }
            successfullyNotified = true;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
        TraceLoggingWrite(g_hRemotingProvider,
                          "Peasant_RequestRename",
                          TraceLoggingUInt64(GetID(), "peasantID", "Our ID"),
                          TraceLoggingWideString(oldName.c_str(), "oldName", "Our old name"),
                          TraceLoggingWideString(args.NewName().c_str(), "newName", "The proposed name"),
                          TraceLoggingBoolean(args.Succeeded(), "succeeded", "true if the monarch ok'd this new name for us."),
                          TraceLoggingBoolean(successfullyNotified, "successfullyNotified", "true if we successfully notified the monarch"),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }

    void Peasant::RequestShowNotificationIcon()
    {
        try
        {
            _ShowNotificationIconRequestedHandlers(*this, nullptr);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
        TraceLoggingWrite(g_hRemotingProvider,
                          "Peasant_RequestShowNotificationIcon",
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }

    void Peasant::RequestHideNotificationIcon()
    {
        try
        {
            _HideNotificationIconRequestedHandlers(*this, nullptr);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
        TraceLoggingWrite(g_hRemotingProvider,
                          "Peasant_RequestHideNotificationIcon",
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }

    void Peasant::RequestQuitAll()
    {
        try
        {
            _QuitAllRequestedHandlers(*this, nullptr);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
        TraceLoggingWrite(g_hRemotingProvider,
                          "Peasant_RequestQuit",
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }

    void Peasant::AttachContentToWindow(Remoting::AttachRequest request)
    {
        try
        {
            _AttachRequestedHandlers(*this, request);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
        TraceLoggingWrite(g_hRemotingProvider,
                          "Peasant_AttachContentToWindow",
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }

    void Peasant::Quit()
    {
        try
        {
            _QuitRequestedHandlers(*this, nullptr);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
        TraceLoggingWrite(g_hRemotingProvider,
                          "Peasant_Quit",
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }

    // Method Description:
    // - Request and return the window layout from the current TerminalPage
    // Arguments:
    // - <none>
    // Return Value:
    // - the window layout as a json string
    hstring Peasant::GetWindowLayout()
    {
        auto args = winrt::make_self<implementation::GetWindowLayoutArgs>();
        _GetWindowLayoutRequestedHandlers(nullptr, *args);
        if (const auto op = args->WindowLayoutJsonAsync())
        {
            // This will fail if called on the UI thread, so the monarch should
            // never set WindowLayoutJsonAsync.
            auto str = op.get();
            return str;
        }
        return args->WindowLayoutJson();
    }

    void Peasant::SendContent(const Remoting::RequestReceiveContentArgs& args)
    {
        _SendContentRequestedHandlers(*this, args);
    }
}
