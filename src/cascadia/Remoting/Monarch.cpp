// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "WindowingBehavior.h"
#include "Monarch.h"
#include "CommandlineArgs.h"
#include "FindTargetWindowArgs.h"
#include "WindowActivatedArgs.h"
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
        try
        {
            _desktopManager = winrt::create_instance<IVirtualDesktopManager>(__uuidof(VirtualDesktopManager));
        }
        CATCH_LOG();
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

    uint64_t Monarch::_lookupPeasantIdForName(const winrt::hstring& name)
    {
        if (name.empty())
        {
            return 0;
        }

        for (const auto& [id, p] : _peasants)
        {
            try
            {
                auto otherName = p.WindowName();
                if (otherName == name)
                {
                    return id;
                }
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
                // Remove the peasant from the list of peasants
                _peasants.erase(id);
            }
        }
        return 0;
    }

    void Monarch::HandleActivatePeasant(const Remoting::WindowActivatedArgs& args)
    {
        // TODO:projects/5 Use a heap/priority queue per-desktop to track which
        // peasant was the most recent per-desktop. When we want to get the most
        // recent of all desktops (WindowingBehavior::UseExisting), then use the
        // most recent of all desktops.
        // const auto oldLastActiveTime = _lastActivatedTime.time_since_epoch().count();
        const auto newLastActiveTime = args.ActivatedTime().time_since_epoch().count();

        // * Check all the current lists to look for this peasant.
        //   remove it from any where it exists.
        for (auto& [g, vec] : _mruPeasants)
        {
            auto result = std::find_if(vec.begin(),
                                       vec.end(),
                                       [args](auto other) {
                                           try
                                           {
                                               return args.PeasantID() == other.PeasantID();
                                           }
                                           CATCH_LOG(); // TODO:MG Oh no, this needs to be fixed in windowingBehavior
                                           return false;
                                       });
            if (result != std::end(vec))
            {
                vec.erase(result);
                std::make_heap(vec.begin(), vec.end(), Remoting::implementation::CompareWindowActivatedArgs());
                continue;
            }
        }
        // * If the current desktop doesn't have a vector, add one.
        if (_mruPeasants.find(args.DesktopID()) == _mruPeasants.end())
        {
            _mruPeasants[args.DesktopID()] = std::vector<Remoting::WindowActivatedArgs>();
        }
        // * Add this args to the queue for the given desktop.
        _mruPeasants[args.DesktopID()].push_back(args);
        std::push_heap(_mruPeasants[args.DesktopID()].begin(),
                       _mruPeasants[args.DesktopID()].end(),
                       Remoting::implementation::CompareWindowActivatedArgs());

        TraceLoggingWrite(g_hRemotingProvider,
                          "Monarch_SetMostRecentPeasant",
                          TraceLoggingUInt64(args.PeasantID(), "peasantID", "the ID of the activated peasant"),
                          // TraceLoggingInt64(oldLastActiveTime, "oldLastActiveTime", "The previous lastActiveTime"),
                          // TODO:MG add the GUID here, better logging
                          TraceLoggingInt64(newLastActiveTime, "newLastActiveTime", "The provided args.ActivatedTime()"),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));
    }

    uint64_t Monarch::_getMostRecentPeasantID(bool limitToCurrentDesktop)
    {
        std::vector<Remoting::WindowActivatedArgs> _mrus;
        for (auto& [g, vec] : _mruPeasants)
        {
            if (vec.size() > 0)
            {
                if (limitToCurrentDesktop && _desktopManager)
                {
                    auto args{ *vec.begin() };
                    BOOL onCurrentDesktop{ false };
                    // SUCCEEDED_LOG will log if it failed, and
                    // return true if it SUCCEEDED
                    if (SUCCEEDED_LOG(_desktopManager->IsWindowOnCurrentVirtualDesktop((HWND)args.Hwnd(), &onCurrentDesktop)) &&
                        onCurrentDesktop)
                    {
                        _mrus.push_back(args);
                        std::push_heap(_mrus.begin(),
                                       _mrus.end(),
                                       Remoting::implementation::CompareWindowActivatedArgs());
                    }
                }
                else
                {
                    _mrus.push_back(*vec.begin());
                    std::push_heap(_mrus.begin(),
                                   _mrus.end(),
                                   Remoting::implementation::CompareWindowActivatedArgs());
                }
            }
        }
        if (!_mrus.empty())
        {
            return _mrus.begin()->PeasantID();
        }
        else if (limitToCurrentDesktop)
        {
            return 0;
        }

        // We haven't yet been told the MRU peasant. Just use the first one.
        // This is just gonna be a random one, but really shouldn't happen
        // in practice. The WindowManager should set the MRU peasant
        // immediately as soon as it creates the monarch/peasant for the
        // first window.
        else if (_peasants.size() > 0)
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
        const auto& targetWindowName = findWindowArgs->ResultTargetWindowName();

        // If there's a valid ID returned, then let's try and find the peasant
        // that goes with it. Alternatively, if we were given a magic windowing
        // constant, we can use that to look up an appropriate peasant.
        if (targetWindow >= 0 ||
            targetWindow == WindowingBehaviorUseName ||
            targetWindow == WindowingBehaviorUseExistingSameDesktop ||
            targetWindow == WindowingBehaviorUseExisting)
        {
            uint64_t windowID = 0;
            switch (targetWindow)
            {
            case WindowingBehaviorUseCurrent:
            case WindowingBehaviorUseExistingSameDesktop:
                // TODO:projects/5 for now, just use the MRU window. Technically,
                // UseExistingSameDesktop and UseCurrent are different.
                // UseCurrent implies that we should try to do the WT_SESSION
                // lookup to find the window that spawned this process (then
                // fall back to sameDesktop if we can't find a match). For now,
                // it's good enough to just try to find a match on this desktop.
                windowID = _getMostRecentPeasantID(true);
                break;
            case WindowingBehaviorUseExisting:
                windowID = _getMostRecentPeasantID(false);
                break;
            case WindowingBehaviorUseName:
                windowID = _lookupPeasantIdForName(targetWindowName);
                break;
            default:
                windowID = ::base::saturated_cast<uint64_t>(targetWindow);
                break;
            }

            // If_getMostRecentPeasantID returns 0 above, then we couldn't find
            // a matching window for that style of windowing. _getPeasant will
            // return nullptr, and we'll fall through to the "create a new
            // window" branch below.

            if (auto targetPeasant{ _getPeasant(windowID) })
            {
                auto result{ winrt::make_self<Remoting::implementation::ProposeCommandlineResult>(false) };
                result->WindowName(targetWindowName);
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
                result->WindowName(targetWindowName);
                return *result;
            }
        }

        // If we get here, we couldn't find an existing window. Make a new one.

        TraceLoggingWrite(g_hRemotingProvider,
                          "Monarch_ProposeCommandline_NewWindow",
                          TraceLoggingInt64(targetWindow, "targetWindow", "The provided ID"),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));

        // In this case, no usable ID was provided. Return { true, nullopt }
        auto result = winrt::make_self<Remoting::implementation::ProposeCommandlineResult>(true);
        result->WindowName(targetWindowName);
        return *result;
    }

}
