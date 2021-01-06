// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Peasant.h"
#include "CommandlineArgs.h"
#include "Peasant.g.cpp"
#include "../../types/inc/utils.hpp"

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

    // This is a private constructor to be used in unit tests, where we don't
    // want each Peasant to necessarily use the current PID.
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

    void Peasant::ActivateWindow()
    {
        // TODO: projects/5 - somehow, pass an identifier for the current
        // desktop into this method. The Peasant shouldn't need to be able to
        // figure it out, but it will need to report it to the monarch.

        // Raise our WindowActivated event, to let the monarch know we've been
        // activated.
        try
        {
            // Try/catch this, because the other side of this event is handled
            // by the monarch. The monarch might have died. If they have, this
            // will throw and exception. Just eat it, the election thread will
            // handle hooking up the new one.
            _WindowActivatedHandlers(*this, nullptr);
        }
        catch (...)
        {
            // TODO:MG Tracelogging
            LOG_CAUGHT_EXCEPTION();
        }

        // TODO:MG Open three windows, close the first (the monarch). The focus
        // should automatically move to the third, from the windows shell. In
        // that window, `wt -w 0` does not work right.
    }

}
