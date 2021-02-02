// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Monarch.h"
#include "CommandlineArgs.h"
#include "FindTargetWindowArgs.h"
#include "ProposeCommandlineResult.h"

#include "Monarch.g.cpp"
#include "../../types/inc/utils.hpp"

using namespace winrt;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Windows::Foundation;
using namespace ::Microsoft::Console;

namespace winrt::Microsoft::Terminal::Remoting::implementation
{
    Monarch::Monarch() :
        _ourPID{ GetCurrentProcessId() }
    {
    }

    // This is a private constructor to be used in unit tests, where we don't
    // want each Monarch to necessarily use the current PID.
    Monarch::Monarch(const uint64_t testPID) :
        _ourPID{ testPID }
    {
    }

    Monarch::~Monarch()
    {
    }

    uint64_t Monarch::GetPID()
    {
        return _ourPID;
    }

    // Method Description:
    // - Add the given peasant to the list of peasants we're tracking. This Peasant may have already been assigned an ID. If it hasn't, then give it an ID.
    // Arguments:
    // - peasant: the new Peasant to track.
    // Return Value:
    // - the ID assigned to the peasant.
    uint64_t Monarch::AddPeasant(Remoting::IPeasant peasant)
    {
        try
        {
            // TODO:projects/5 This is terrible. There's gotta be a better way
            // of finding the first opening in a non-consecutive map of int->object
            const auto providedID = peasant.GetID();

            if (providedID == 0)
            {
                // Peasant doesn't currently have an ID. Assign it a new one.
                peasant.AssignID(_nextPeasantID++);
            }
            else
            {
                // Peasant already had an ID (from an older monarch). Leave that one
                // be. Make sure that the next peasant's ID is higher than it.
                _nextPeasantID = providedID >= _nextPeasantID ? providedID + 1 : _nextPeasantID;
            }

            auto newPeasantsId = peasant.GetID();
            // Add an event listener to the peasant's WindowActivated event.
            peasant.WindowActivated({ this, &Monarch::_peasantWindowActivated });

            _peasants[newPeasantsId] = peasant;

            TraceLoggingWrite(g_hRemotingProvider,
                              "Monarch_AddPeasant",
                              TraceLoggingUInt64(providedID, "providedID", "the provided ID for the peasant"),
                              TraceLoggingUInt64(newPeasantsId, "peasantID", "the ID of the new peasant"),
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));
            return newPeasantsId;
        }
        catch (...)
        {
            TraceLoggingWrite(g_hRemotingProvider,
                              "Monarch_AddPeasant_Failed",
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));

            // We can only get into this try/catch if the peasant died on us. So
            // the return value doesn't _really_ matter. They're not about to
            // get it.
            return -1;
        }
    }

    // Method Description:
    // - Event handler for the Peasant::WindowActivated event. Used as an
    //   opportunity for us to update our internal stack of the "most recent
    //   window".
    // Arguments:
    // - sender: the Peasant that raised this event. This might be out-of-proc!
    // - args: a bundle of the peasant ID, timestamp, and desktop ID, for the activated peasant
    // Return Value:
    // - <none>
    void Monarch::_peasantWindowActivated(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                          const Remoting::WindowActivatedArgs& args)
    {
        HandleActivatePeasant(args);
    }

    // Method Description:
    // - Lookup a peasant by its ID.
    // Arguments:
    // - peasantID: The ID Of the peasant to find
    // Return Value:
    // - the peasant if it exists in our map, otherwise null
    Remoting::IPeasant Monarch::_getPeasant(uint64_t peasantID)
    {
        try
        {
            const auto peasantSearch = _peasants.find(peasantID);
            auto maybeThePeasant = peasantSearch == _peasants.end() ? nullptr : peasantSearch->second;
            // Ask the peasant for their PID. This will validate that they're
            // actually still alive.
            if (maybeThePeasant)
            {
                maybeThePeasant.GetPID();
            }
            return maybeThePeasant;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            // Remove the peasant from the list of peasants
            _peasants.erase(peasantID);
            return nullptr;
        }
    }

    void Monarch::HandleActivatePeasant(const Remoting::WindowActivatedArgs& args)
    {
        // TODO:projects/5 Use a heap/priority queue per-desktop to track which
        // peasant was the most recent per-desktop. When we want to get the most
        // recent of all desktops (WindowingBehavior::UseExisting), then use the
        // most recent of all desktops.
        const auto oldLastActiveTime = _lastActivatedTime.time_since_epoch().count();
        const auto newLastActiveTime = args.ActivatedTime().time_since_epoch().count();

        // For now, we'll just pay attention to whoever the most recent peasant
        // was. We're not too worried about the mru peasant dying. Worst case -
        // when the user executes a `wt -w 0`, we won't be able to find that
        // peasant, and it'll open in a new window instead of the current one.
        if (args.ActivatedTime() > _lastActivatedTime)
        {
            _mostRecentPeasant = args.PeasantID();
            _lastActivatedTime = args.ActivatedTime();
        }

        TraceLoggingWrite(g_hRemotingProvider,
                          "Monarch_SetMostRecentPeasant",
                          TraceLoggingUInt64(args.PeasantID(), "peasantID", "the ID of the activated peasant"),
                          TraceLoggingInt64(oldLastActiveTime, "oldLastActiveTime", "The previous lastActiveTime"),
                          TraceLoggingInt64(newLastActiveTime, "newLastActiveTime", "The provided args.ActivatedTime()"),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));
    }

    uint64_t Monarch::_getMostRecentPeasantID()
    {
        if (_mostRecentPeasant != 0)
        {
            return _mostRecentPeasant;
        }

        // We haven't yet been told the MRU peasant. Just use the first one.
        // This is just gonna be a random one, but really shouldn't happen
        // in practice. The WindowManager should set the MRU peasant
        // immediately as soon as it creates the monarch/peasant for the
        // first window.
        if (_peasants.size() > 0)
        {
            try
            {
                return _peasants.begin()->second.GetID();
            }
            catch (...)
            {
                // This shouldn't really happen. If we're the monarch, then the
                // first peasant should also _be us_. So we should be able to
                // get our own ID.
                return 0;
            }
        }
        return 0;
    }

    // Method Description:
    // - Try to handle a commandline from a new WT invocation. We might need to
    //   hand the commandline to an existing window, or we might need to tell
    //   the caller that they need to become a new window to handle it themselves.
    // Arguments:
    // - <none>
    // Return Value:
    // - true if the caller should create a new window for this commandline.
    //   False otherwise - the monarch should have dispatched this commandline
    //   to another window in this case.
    Remoting::ProposeCommandlineResult Monarch::ProposeCommandline(const Remoting::CommandlineArgs& args)
    {
        // Raise an event, to ask how to handle this commandline. We can't ask
        // the app ourselves - we exist isolated from that knowledge (and
        // dependency hell). The WindowManager will raise this up to the app
        // host, which will then ask the AppLogic, who will then parse the
        // commandline and determine the provided ID of the window.
        auto findWindowArgs{ winrt::make_self<Remoting::implementation::FindTargetWindowArgs>(args) };

        // This is handled by some handler in-proc
        _FindTargetWindowRequestedHandlers(*this, *findWindowArgs);

        // After the event was handled, ResultTargetWindow() will be filled with
        // the parsed result.
        const auto targetWindow = findWindowArgs->ResultTargetWindow();

        // If there's a valid ID returned, then let's try and find the peasant that goes with it.
        if (targetWindow >= 0)
        {
            uint64_t windowID = ::base::saturated_cast<uint64_t>(targetWindow);

            if (windowID == 0)
            {
                windowID = _getMostRecentPeasantID();
            }

            if (auto targetPeasant{ _getPeasant(windowID) })
            {
                auto result{ winrt::make_self<Remoting::implementation::ProposeCommandlineResult>(false) };

                try
                {
                    // This will raise the peasant's ExecuteCommandlineRequested
                    // event, which will then ask the AppHost to handle the
                    // commandline, which will then pass it to AppLogic for
                    // handling.
                    targetPeasant.ExecuteCommandline(args);
                }
                catch (...)
                {
                    // If we fail to propose the commandline to the peasant (it
                    // died?) then just tell this process to become a new window
                    // instead.
                    result->ShouldCreateWindow(true);

                    // If this fails, it'll be logged in the following
                    // TraceLoggingWrite statement, with succeeded=false
                }

                TraceLoggingWrite(g_hRemotingProvider,
                                  "Monarch_ProposeCommandline_Existing",
                                  TraceLoggingUInt64(windowID, "peasantID", "the ID of the peasant the commandline waws intended for"),
                                  TraceLoggingBoolean(true, "foundMatch", "true if we found a peasant with that ID"),
                                  TraceLoggingBoolean(!result->ShouldCreateWindow(), "succeeded", "true if we successfully dispatched the commandline to the peasant"),
                                  TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));
                return *result;
            }
            else if (windowID > 0)
            {
                // In this case, an ID was provided, but there's no
                // peasant with that ID. Instead, we should tell the caller that
                // they should make a new window, but _with that ID_.

                TraceLoggingWrite(g_hRemotingProvider,
                                  "Monarch_ProposeCommandline_Existing",
                                  TraceLoggingUInt64(windowID, "peasantID", "the ID of the peasant the commandline waws intended for"),
                                  TraceLoggingBoolean(false, "foundMatch", "true if we found a peasant with that ID"),
                                  TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));

                auto result{ winrt::make_self<Remoting::implementation::ProposeCommandlineResult>(true) };
                result->Id(windowID);
                return *result;
            }
        }

        TraceLoggingWrite(g_hRemotingProvider,
                          "Monarch_ProposeCommandline_NewWindow",
                          TraceLoggingInt64(targetWindow, "targetWindow", "The provided ID"),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));

        // In this case, no usable ID was provided. Return { true, nullopt }
        return winrt::make<Remoting::implementation::ProposeCommandlineResult>(true);
    }

}
