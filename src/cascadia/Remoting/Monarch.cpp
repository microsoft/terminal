// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "WindowingBehavior.h"
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
    // - Add the given peasant to the list of peasants we're tracking. This
    //   Peasant may have already been assigned an ID. If it hasn't, then give
    //   it an ID.
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

            // Remove the peasant from the MRU windows mapping. They're dead.
            // They can't be the MRU anymore.
            _clearOldMruEntries(peasantID);
            return nullptr;
        }
    }

    void Monarch::HandleActivatePeasant(const Remoting::WindowActivatedArgs& args)
    {
        // Start by making a local copy of these args. It's easier for us if our
        // tracking of these args is all in-proc. That way, the only thing that
        // could fail due to the peasant dying is _this first copy_.
        winrt::com_ptr<implementation::WindowActivatedArgs> localArgs{ nullptr };
        try
        {
            localArgs = winrt::make_self<implementation::WindowActivatedArgs>(args.PeasantID(),
                                                                              args.Hwnd(),
                                                                              args.DesktopID(),
                                                                              args.ActivatedTime());
            _doHandleActivatePeasant(localArgs);
        }
        catch (...)
        {
            TraceLoggingWrite(g_hRemotingProvider,
                              "Monarch_HandleActivatePeasant_Failed",
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));
        }
    }

    void Monarch::_clearOldMruEntries(const uint64_t peasantID)
    {
        // * Check all the current lists to look for this peasant.
        //   remove it from any where it exists.
        for (auto& [g, vec] : _mruPeasants)
        {
            auto result = std::find_if(vec.begin(),
                                       vec.end(),
                                       [peasantID](auto other) {
                                           return peasantID == other.PeasantID();
                                       });
            if (result != std::end(vec))
            {
                vec.erase(result);

                TraceLoggingWrite(g_hRemotingProvider,
                                  "Monarch_RemovedPeasantFromDesktop",
                                  TraceLoggingUInt64(peasantID, "peasantID", "The ID of the peasant"),
                                  TraceLoggingGuid(g, "desktopGuid", "The GUID of the previous desktop the window was on"),
                                  TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));

                std::make_heap(vec.begin(),
                               vec.end(),
                               Remoting::implementation::CompareWindowActivatedArgs());
                continue;
            }
        }
    }

    void Monarch::_doHandleActivatePeasant(const winrt::com_ptr<implementation::WindowActivatedArgs>& localArgs)
    {
        const auto newLastActiveTime = localArgs->ActivatedTime().time_since_epoch().count();

        // * Check all the current lists to look for this peasant.
        //   remove it from any where it exists.
        _clearOldMruEntries(localArgs->PeasantID());

        // * If the current desktop doesn't have a vector, add one.
        const auto desktopGuid{ localArgs->DesktopID() };
        if (_mruPeasants.find(desktopGuid) == _mruPeasants.end())
        {
            _mruPeasants[desktopGuid] = std::vector<Remoting::WindowActivatedArgs>();
        }
        // * Add this args to the queue for the given desktop.
        _mruPeasants[desktopGuid].push_back(*localArgs);
        std::push_heap(_mruPeasants[desktopGuid].begin(),
                       _mruPeasants[desktopGuid].end(),
                       Remoting::implementation::CompareWindowActivatedArgs());

        TraceLoggingWrite(g_hRemotingProvider,
                          "Monarch_SetMostRecentPeasant",
                          TraceLoggingUInt64(localArgs->PeasantID(), "peasantID", "the ID of the activated peasant"),
                          TraceLoggingGuid(desktopGuid, "desktopGuid", "The GUID of the desktop the window is on"),
                          TraceLoggingInt64(newLastActiveTime, "newLastActiveTime", "The provided localArgs->ActivatedTime()"),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));
    }

    void Monarch::_collectMRUPeasant(const bool limitToCurrentDesktop,
                                     std::vector<Remoting::WindowActivatedArgs>& mruWindows,
                                     std::vector<Remoting::WindowActivatedArgs>& windowsForDesktop)
    {
        while (!windowsForDesktop.empty())
        {
            const auto mruWindowArgs{ *windowsForDesktop.begin() };
            const auto desktopGuid = mruWindowArgs.DesktopID();
            // Trying to get the peasant will verify the peasant is still alive.
            // If they aren't, the peasant will be removed from the ID->Peasant
            // map, and from the the desktop->[Peasant] map as well.
            // * If the peasant is dead, just try this loop again.
            // * Otherwise, try and collect it.
            auto peasant{ _getPeasant(mruWindowArgs.PeasantID()) };
            if (!peasant)
            {
                TraceLoggingWrite(g_hRemotingProvider,
                                  "Monarch_Collect_WasDead",
                                  TraceLoggingUInt64(mruWindowArgs.PeasantID(),
                                                     "peasantID",
                                                     "We thought this peasant was the MRU one, but it was actually already dead."),
                                  TraceLoggingGuid(desktopGuid, "desktopGuid", "The GUID of the desktop the window is on"),
                                  TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));
                continue;
            }

            // Here, the peasant mruWindowArgs belongs to is still alive.
            if (limitToCurrentDesktop && _desktopManager)
            {
                // Check if this peasant is actually on this desktop. We can't
                // simply get the GUID of the current desktop. We have to ask if
                // the HWND is on the current desktop.
                BOOL onCurrentDesktop{ false };

                // SUCCEEDED_LOG will log if it failed, and return true if it
                // SUCCEEDED
                if (SUCCEEDED_LOG(_desktopManager->IsWindowOnCurrentVirtualDesktop((HWND)mruWindowArgs.Hwnd(),
                                                                                   &onCurrentDesktop)) &&
                    onCurrentDesktop)
                {
                    mruWindows.push_back(mruWindowArgs);

                    TraceLoggingWrite(g_hRemotingProvider,
                                      "Monarch_Collect",
                                      TraceLoggingUInt64(mruWindowArgs.PeasantID(),
                                                         "peasantID",
                                                         "the ID of the MRU peasant for a desktop"),
                                      TraceLoggingGuid(desktopGuid,
                                                       "desktopGuid",
                                                       "The GUID of the desktop the window is on"),
                                      TraceLoggingBoolean(limitToCurrentDesktop,
                                                          "limitToCurrentDesktop",
                                                          "True if we should only search for a window on the current desktop"),
                                      TraceLoggingBool(onCurrentDesktop,
                                                       "onCurrentDesktop",
                                                       "true if this window was in fact on the current desktop"),
                                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));

                    // Update the mruWindows heap, so the actually most recent
                    // args is at the front.
                    std::push_heap(mruWindows.begin(),
                                   mruWindows.end(),
                                   Remoting::implementation::CompareWindowActivatedArgs());
                }

                // KNOWN BUG with useExistingSameDesktop
                // 1. Open two windows on desktop A.
                // 2. Drag the second to desktop B. DON'T let either window get focused.
                // 3. Switch to desktop A. Again, DON'T activate window 1
                // 4. Open a new wt.
                //    - EXPECTED: Opens a new tab in window 1. It's on desktop A.
                //    - Actual: A new window is created. We see that the MRU
                //      window on desktop A is window 2. When we ask if window 2
                //      is on desktop A, _it isn't_, and we assume the rest
                //      aren't either.
                //
                // The simplest bugfix for this would be if we could get some
                // window message when we change desktops. Otherwise, we have to
                // check all the windows when we determine one _isn't_ on this
                // monitor.
            }
            else
            {
                // Here, we don't care about which desktop the window is on.
                // We'll add this window to the list of MRU windows.
                mruWindows.push_back(mruWindowArgs);

                TraceLoggingWrite(g_hRemotingProvider,
                                  "Monarch_Collect",
                                  TraceLoggingUInt64(mruWindowArgs.PeasantID(),
                                                     "peasantID",
                                                     "the ID of the MRU peasant for a desktop"),
                                  TraceLoggingGuid(desktopGuid, "desktopGuid", "The GUID of the desktop the window is on"),
                                  TraceLoggingBoolean(limitToCurrentDesktop,
                                                      "limitToCurrentDesktop",
                                                      "True if we should only search for a window on the current desktop"),
                                  TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));

                // Update the mruWindows heap, so the actually most recent args is at
                // the front.
                std::push_heap(mruWindows.begin(),
                               mruWindows.end(),
                               Remoting::implementation::CompareWindowActivatedArgs());
            }

            // Exit the loop. Either we took the most recent window from the
            // windows on the given desktop, or we decided not to. Either way,
            // we don't need to keep looping.
            break;
        }
    }

    uint64_t Monarch::_getMostRecentPeasantID(const bool limitToCurrentDesktop)
    {
        std::vector<Remoting::WindowActivatedArgs> mruWindows;
        for (auto& [g, vec] : _mruPeasants)
        {
            if (vec.empty())
            {
                continue;
            }
            _collectMRUPeasant(limitToCurrentDesktop, mruWindows, vec);
        }

        if (!mruWindows.empty())
        {
            TraceLoggingWrite(g_hRemotingProvider,
                              "Monarch_getMostRecentPeasantID_Found",
                              TraceLoggingUInt64(mruWindows.begin()->PeasantID(), "peasantID", "The ID of the MRU peasant"),
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));

            return mruWindows.begin()->PeasantID();
        }
        else if (limitToCurrentDesktop)
        {
            TraceLoggingWrite(g_hRemotingProvider,
                              "Monarch_getMostRecentPeasantID_NotFound_ThisDesktop",
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));
            return 0;
        }

        TraceLoggingWrite(g_hRemotingProvider,
                          "Monarch_getMostRecentPeasantID_NotFound",
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));

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

        TraceLoggingWrite(g_hRemotingProvider,
                          "Monarch_ProposeCommandline",
                          TraceLoggingInt64(targetWindow, "targetWindow", "The window ID the args specified"),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));

        // If there's a valid ID returned, then let's try and find the peasant
        // that goes with it. Alternatively, if we were given a magic windowing
        // constant, we can use that to look up an appropriate peasant.
        if (targetWindow >= 0 ||
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
            default:
                windowID = ::base::saturated_cast<uint64_t>(targetWindow);
                break;
            }

            TraceLoggingWrite(g_hRemotingProvider,
                              "Monarch_ProposeCommandline",
                              TraceLoggingInt64(windowID,
                                                "windowID",
                                                "The actual peasant ID we evaluated the window ID as"),
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));

            // If_getMostRecentPeasantID returns 0 above, then we couldn't find
            // a matching window for that style of windowing. _getPeasant will
            // return nullptr, and we'll fall through to the "create a new
            // window" branch below.

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
                                  TraceLoggingUInt64(windowID,
                                                     "peasantID",
                                                     "the ID of the peasant the commandline waws intended for"),
                                  TraceLoggingBoolean(true, "foundMatch", "true if we found a peasant with that ID"),
                                  TraceLoggingBoolean(!result->ShouldCreateWindow(),
                                                      "succeeded",
                                                      "true if we successfully dispatched the commandline to the peasant"),
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
                                  TraceLoggingUInt64(windowID,
                                                     "peasantID",
                                                     "the ID of the peasant the commandline waws intended for"),
                                  TraceLoggingBoolean(false, "foundMatch", "true if we found a peasant with that ID"),
                                  TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));

                auto result{ winrt::make_self<Remoting::implementation::ProposeCommandlineResult>(true) };
                result->Id(windowID);
                return *result;
            }
        }

        // If we get here, we couldn't find an existing window. Make a new one.
        TraceLoggingWrite(g_hRemotingProvider,
                          "Monarch_ProposeCommandline_NewWindow",
                          TraceLoggingInt64(targetWindow, "targetWindow", "The provided ID"),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));

        // In this case, no usable ID was provided. Return { true, nullopt }
        return winrt::make<Remoting::implementation::ProposeCommandlineResult>(true);
    }
}
