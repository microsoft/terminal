// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Monarch.h"
#include "CommandlineArgs.h"
#include "FindTargetWindowArgs.h"

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
        _peasants[newPeasantsId] = peasant;

        // Add an event listener to the peasant's WindowActivated event.
        peasant.WindowActivated({ this, &Monarch::_peasantWindowActivated });

        return newPeasantsId;
    }

    // Method Description:
    // - Event handler for the Peasant::WindowActivated event. Used as an
    //   opportunity for us to update our internal stack of the "most recent
    //   window".
    // Arguments:
    // - sender: the Peasant that raised this event. This might be out-of-proc!
    // Return Value:
    // - <none>
    void Monarch::_peasantWindowActivated(const winrt::Windows::Foundation::IInspectable& sender,
                                          const winrt::Windows::Foundation::IInspectable& /*args*/)
    {
        // TODO:projects/5 Pass the desktop and timestamp of when the window was
        // activated in `args`.

        if (auto peasant{ sender.try_as<Remoting::Peasant>() })
        {
            auto theirID = peasant.GetID();
            _setMostRecentPeasant(theirID);
        }
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
            auto peasantSearch = _peasants.find(peasantID);
            auto maybeThePeasant = peasantSearch == _peasants.end() ? nullptr : peasantSearch->second;
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

    // TODO:MG This probably shouldn't be a public function. I'm making it
    // public so the WindowManager can use it to manually tell the monarch it's
    // own ID to use as the MRU, when the monarch is first instantiated. THat's
    // dumb, but it's a hack to get something working.
    //
    // That was stupid. I knew it would be but yea it didn't work.
    // THe Window manager doesn't have a peasant yet when it first creates the monarch.
    void Monarch::_setMostRecentPeasant(const uint64_t peasantID)
    {
        // TODO:projects/5 Use a heap/priority queue per-desktop to track which
        // peasant was the most recent per-desktop. When we want to get the most
        // recent of all desktops (WindowingBehavior::UseExisting), then use the
        // most recent of all desktops.
        _mostRecentPeasant = peasantID;
    }

    uint64_t Monarch::_getMostRecentPeasantID()
    {
        if (_mostRecentPeasant == 0)
        {
            // We haven't yet been told the MRU peasant. Just use the first one.
            // TODO: GOD this is just gonna be a random one. Hacks on hacks on hacks
            if (_peasants.size() > 0)
            {
                return _peasants.begin()->second.GetID();
            }
            return 0;
        }
        else
        {
            return _mostRecentPeasant;
        }
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
    bool Monarch::ProposeCommandline(const Remoting::CommandlineArgs& args)
    {
        // TODO:projects/5
        // The branch dev/migrie/f/remote-commandlines has a more complete
        // version of this function, with a naive implementation. For now, we
        // always want to create a new window, so we'll just return true. This
        // will tell the caller that we didn't handle the commandline, and they
        // should open a new window to deal with it themselves.
        auto findWindowArgs = winrt::make_self<Remoting::implementation::FindTargetWindowArgs>();
        findWindowArgs->Args(args);
        _FindTargetWindowRequestedHandlers(*this, *findWindowArgs);
        const auto targetWindow = findWindowArgs->ResultTargetWindow();

        // TODO:projects/5 targetWindow==0 -> use the currently active window
        if (targetWindow >= 0)
        {
            uint64_t windowID = ::base::saturated_cast<uint64_t>(targetWindow);

            if (windowID == 0)
            {
                windowID = _getMostRecentPeasantID();
            }

            if (auto targetPeasant{ _getPeasant(windowID) })
            {
                targetPeasant.ExecuteCommandline(args);
                return false;
            }
        }
        // TEMPORARY: if the target window is -1, then we want a new window. All
        // other cases, just do it in this window (for now).
        // return targetWindow == -1;
        return true;
    }

}
