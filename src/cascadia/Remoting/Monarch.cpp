// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "../inc/WindowingBehavior.h"
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
            peasant.IdentifyWindowsRequested({ this, &Monarch::_identifyWindows });
            peasant.RenameRequested({ this, &Monarch::_renameRequested });

            _peasants[newPeasantsId] = peasant;

            TraceLoggingWrite(g_hRemotingProvider,
                              "Monarch_AddPeasant",
                              TraceLoggingUInt64(providedID, "providedID", "the provided ID for the peasant"),
                              TraceLoggingUInt64(newPeasantsId, "peasantID", "the ID of the new peasant"),
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                              TraceLoggingKeyword(TIL_KEYWORD_TRACE));
            return newPeasantsId;
        }
        catch (...)
        {
            TraceLoggingWrite(g_hRemotingProvider,
                              "Monarch_AddPeasant_Failed",
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                              TraceLoggingKeyword(TIL_KEYWORD_TRACE));

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
    // - Lookup a peasant by its ID. If the peasant has died, this will also
    //   remove the peasant from our list of peasants.
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

            // Remove the peasant from the list of MRU windows. They're dead.
            // They can't be the MRU anymore.
            _clearOldMruEntries(peasantID);
            return nullptr;
        }
    }

    // Method Description:
    // - Find the ID of the peasant with the given name. If no such peasant
    //   exists, then we'll return 0. If we encounter any peasants who have died
    //   during this process, then we'll remove them from the set of _peasants
    // Arguments:
    // - name: The window name to look for
    // Return Value:
    // - 0 if we didn't find the given peasant, otherwise a positive number for
    //   the window's ID.
    uint64_t Monarch::_lookupPeasantIdForName(std::wstring_view name)
    {
        if (name.empty())
        {
            return 0;
        }

        std::vector<uint64_t> peasantsToErase{};
        uint64_t result = 0;
        for (const auto& [id, p] : _peasants)
        {
            try
            {
                auto otherName = p.WindowName();
                if (otherName == name)
                {
                    result = id;
                    break;
                }
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
                // Normally, we'd just erase the peasant here. However, we can't
                // erase from the map while we're iterating over it like this.
                // Instead, pull a good ole Java and collect this id for removal
                // later.
                peasantsToErase.push_back(id);
            }
        }

        // Remove the dead peasants we came across while iterating.
        for (const auto& id : peasantsToErase)
        {
            // Remove the peasant from the list of peasants
            _peasants.erase(id);
            // Remove the peasant from the list of MRU windows. They're dead.
            // They can't be the MRU anymore.
            _clearOldMruEntries(id);
        }

        return result;
    }

    // Method Description:
    // - Handler for the `Peasant::WindowActivated` event. We'll make a in-proc
    //   copy of the WindowActivatedArgs from the peasant. That way, we won't
    //   need to worry about the origin process dying when working with the
    //   WindowActivatedArgs.
    // - If the peasant process dies while we're making this copy, then we'll
    //   just log it and do nothing. We certainly don't want to track a dead
    //   peasant.
    // - We'll pass that copy of the WindowActivatedArgs to
    //   _doHandleActivatePeasant, which will actually insert the
    //   WindowActivatedArgs into the list we're using to track the most recent
    //   peasants.
    // Arguments:
    // - args: the WindowActivatedArgs describing when and where the peasant was activated.
    // Return Value:
    // - <none>
    void Monarch::HandleActivatePeasant(const Remoting::WindowActivatedArgs& args)
    {
        // Start by making a local copy of these args. It's easier for us if our
        // tracking of these args is all in-proc. That way, the only thing that
        // could fail due to the peasant dying is _this first copy_.
        winrt::com_ptr<implementation::WindowActivatedArgs> localArgs{ nullptr };
        try
        {
            localArgs = winrt::make_self<implementation::WindowActivatedArgs>(args);
            // This method will actually do the hard work
            _doHandleActivatePeasant(localArgs);
        }
        catch (...)
        {
            TraceLoggingWrite(g_hRemotingProvider,
                              "Monarch_HandleActivatePeasant_Failed",
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                              TraceLoggingKeyword(TIL_KEYWORD_TRACE));
        }
    }

    // Method Description:
    // - Helper for removing a peasant from the list of MRU peasants. We want to
    //   do this both when the peasant dies, and also when the peasant is newly
    //   activated (so that we don't leave an old entry for it in the list).
    // Arguments:
    // - peasantID: The ID of the peasant to remove from the MRU list
    // Return Value:
    // - <none>
    void Monarch::_clearOldMruEntries(const uint64_t peasantID)
    {
        auto result = std::find_if(_mruPeasants.begin(),
                                   _mruPeasants.end(),
                                   [peasantID](auto&& other) {
                                       return peasantID == other.PeasantID();
                                   });

        if (result != std::end(_mruPeasants))
        {
            TraceLoggingWrite(g_hRemotingProvider,
                              "Monarch_RemovedPeasantFromDesktop",
                              TraceLoggingUInt64(peasantID, "peasantID", "The ID of the peasant"),
                              TraceLoggingGuid(result->DesktopID(), "desktopGuid", "The GUID of the previous desktop the window was on"),
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                              TraceLoggingKeyword(TIL_KEYWORD_TRACE));

            _mruPeasants.erase(result);
        }
    }

    // Method Description:
    // - Actually handle inserting the WindowActivatedArgs into our list of MRU windows.
    // Arguments:
    // - localArgs: an in-proc WindowActivatedArgs that we should add to our list of MRU windows.
    // Return Value:
    // - <none>
    void Monarch::_doHandleActivatePeasant(const winrt::com_ptr<implementation::WindowActivatedArgs>& localArgs)
    {
        const auto newLastActiveTime = localArgs->ActivatedTime().time_since_epoch().count();

        // * Check all the current lists to look for this peasant.
        //   remove it from any where it exists.
        _clearOldMruEntries(localArgs->PeasantID());

        // * If the current desktop doesn't have a vector, add one.
        const auto desktopGuid{ localArgs->DesktopID() };

        // * Add this args list. By using lower_bound with insert, we can get it
        //   into exactly the right spot, without having to re-sort the whole
        //   array.
        _mruPeasants.insert(std::lower_bound(_mruPeasants.begin(),
                                             _mruPeasants.end(),
                                             *localArgs,
                                             [](const auto& first, const auto& second) { return first.ActivatedTime() > second.ActivatedTime(); }),
                            *localArgs);

        TraceLoggingWrite(g_hRemotingProvider,
                          "Monarch_SetMostRecentPeasant",
                          TraceLoggingUInt64(localArgs->PeasantID(), "peasantID", "the ID of the activated peasant"),
                          TraceLoggingGuid(desktopGuid, "desktopGuid", "The GUID of the desktop the window is on"),
                          TraceLoggingInt64(newLastActiveTime, "newLastActiveTime", "The provided localArgs->ActivatedTime()"),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }

    // Method Description:
    // - Retrieves the ID of the MRU peasant window. If requested, will limit
    //   the search to windows that are on the current desktop.
    // Arguments:
    // - limitToCurrentDesktop: if true, only return the MRU peasant that's
    //   actually on the current desktop.
    // - ignoreQuakeWindow: if true, then don't return the _quake window when we
    //   find it. This allows us to change our behavior for glomming vs
    //   summoning. When summoning the window, this parameter should be true.
    //   When glomming, this should be false, as to prevent glomming to the
    //   _quake window.
    // Return Value:
    // - the ID of the most recent peasant, otherwise 0 if we could not find one.
    uint64_t Monarch::_getMostRecentPeasantID(const bool limitToCurrentDesktop, const bool ignoreQuakeWindow)
    {
        if (_mruPeasants.empty())
        {
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

            TraceLoggingWrite(g_hRemotingProvider,
                              "Monarch_getMostRecentPeasantID_NoPeasants",
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                              TraceLoggingKeyword(TIL_KEYWORD_TRACE));
            return 0;
        }

        // Here, there's at least one MRU peasant.
        //
        // We're going to iterate over these peasants until we find one that both:
        // 1. Is alive
        // 2. Meets our selection criteria (do we care if it is on this desktop?)
        //
        // If the peasant is dead, then we'll remove it, and try the next one.
        // Once we find one that's alive, we'll either:
        // * check if we only want a peasant on the current desktop, and if so,
        //   check if this peasant is on the current desktop.
        //   - If it isn't on the current desktop, we'll loop again, on the
        //     following peasant.
        // * If we don't care, then we'll just return that one.
        //
        // We're not just using an iterator because the contents of the list
        // might change while we're iterating here (if the peasant is dead we'll
        // remove it from the list).
        int positionInList = 0;
        while (_mruPeasants.cbegin() + positionInList < _mruPeasants.cend())
        {
            const auto mruWindowArgs{ *(_mruPeasants.begin() + positionInList) };
            const auto peasant{ _getPeasant(mruWindowArgs.PeasantID()) };
            if (!peasant)
            {
                TraceLoggingWrite(g_hRemotingProvider,
                                  "Monarch_Collect_WasDead",
                                  TraceLoggingUInt64(mruWindowArgs.PeasantID(),
                                                     "peasantID",
                                                     "We thought this peasant was the MRU one, but it was actually already dead."),
                                  TraceLoggingGuid(mruWindowArgs.DesktopID(), "desktopGuid", "The GUID of the desktop the window is on"),
                                  TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                  TraceLoggingKeyword(TIL_KEYWORD_TRACE));
                // We'll go through the loop again. We removed the current one
                // at positionInList, so the next one in positionInList will be
                // a new, different peasant.
                continue;
            }

            if (ignoreQuakeWindow && peasant.WindowName() == QuakeWindowName)
            {
                // The _quake window should never be treated as the MRU window.
                // Skip it if we see it. Users can still target it with `wt -w
                // _quake`, which will hit `_lookupPeasantIdForName` instead.
            }
            else if (limitToCurrentDesktop && _desktopManager)
            {
                // Check if this peasant is actually on this desktop. We can't
                // simply get the GUID of the current desktop. We have to ask if
                // the HWND is on the current desktop.
                BOOL onCurrentDesktop{ false };

                // SUCCEEDED_LOG will log if it failed, and return true if it SUCCEEDED
                if (SUCCEEDED_LOG(_desktopManager->IsWindowOnCurrentVirtualDesktop(reinterpret_cast<HWND>(mruWindowArgs.Hwnd()),
                                                                                   &onCurrentDesktop)) &&
                    onCurrentDesktop)
                {
                    TraceLoggingWrite(g_hRemotingProvider,
                                      "Monarch_Collect",
                                      TraceLoggingUInt64(mruWindowArgs.PeasantID(),
                                                         "peasantID",
                                                         "the ID of the MRU peasant for a desktop"),
                                      TraceLoggingGuid(mruWindowArgs.DesktopID(),
                                                       "desktopGuid",
                                                       "The GUID of the desktop the window is on"),
                                      TraceLoggingBoolean(limitToCurrentDesktop,
                                                          "limitToCurrentDesktop",
                                                          "True if we should only search for a window on the current desktop"),
                                      TraceLoggingBool(onCurrentDesktop,
                                                       "onCurrentDesktop",
                                                       "true if this window was in fact on the current desktop"),
                                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                      TraceLoggingKeyword(TIL_KEYWORD_TRACE));
                    return mruWindowArgs.PeasantID();
                }
                // If this window wasn't on the current desktop, another one
                // might be. We'll increment positionInList below, and try
                // again.
            }
            else
            {
                TraceLoggingWrite(g_hRemotingProvider,
                                  "Monarch_getMostRecentPeasantID_Found",
                                  TraceLoggingUInt64(mruWindowArgs.PeasantID(), "peasantID", "The ID of the MRU peasant"),
                                  TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                  TraceLoggingKeyword(TIL_KEYWORD_TRACE));

                return mruWindowArgs.PeasantID();
            }
            positionInList++;
        }

        // Here, we've checked all the windows, and none of them was both alive
        // and the most recent (on this desktop). Just return 0 - the caller
        // will use this to create a new window.
        TraceLoggingWrite(g_hRemotingProvider,
                          "Monarch_getMostRecentPeasantID_NotFound",
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));

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
        const auto targetWindowName = findWindowArgs->ResultTargetWindowName();

        TraceLoggingWrite(g_hRemotingProvider,
                          "Monarch_ProposeCommandline",
                          TraceLoggingInt64(targetWindow, "targetWindow", "The window ID the args specified"),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));

        // If there's a valid ID returned, then let's try and find the peasant
        // that goes with it. Alternatively, if we were given a magic windowing
        // constant, we can use that to look up an appropriate peasant.
        if (targetWindow >= 0 ||
            targetWindow == WindowingBehaviorUseName ||
            targetWindow == WindowingBehaviorUseExisting ||
            targetWindow == WindowingBehaviorUseAnyExisting)
        {
            uint64_t windowID = 0;
            switch (targetWindow)
            {
            case WindowingBehaviorUseCurrent:
            case WindowingBehaviorUseExisting:
                // TODO:projects/5 for now, just use the MRU window. Technically,
                // UseExisting and UseCurrent are different.
                // UseCurrent implies that we should try to do the WT_SESSION
                // lookup to find the window that spawned this process (then
                // fall back to sameDesktop if we can't find a match). For now,
                // it's good enough to just try to find a match on this desktop.
                //
                // GH#projects/5#card-60325142 - Don't try to glom to the quake window.
                windowID = _getMostRecentPeasantID(true, true);
                break;
            case WindowingBehaviorUseAnyExisting:
                windowID = _getMostRecentPeasantID(false, true);
                break;
            case WindowingBehaviorUseName:
                windowID = _lookupPeasantIdForName(targetWindowName);
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
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                              TraceLoggingKeyword(TIL_KEYWORD_TRACE));

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
                    result->WindowName(targetWindowName);
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
                                  TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                  TraceLoggingKeyword(TIL_KEYWORD_TRACE));
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
                                  TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                  TraceLoggingKeyword(TIL_KEYWORD_TRACE));

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
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));

        // In this case, no usable ID was provided. Return { true, nullopt }
        auto result = winrt::make_self<Remoting::implementation::ProposeCommandlineResult>(true);
        result->WindowName(targetWindowName);
        return *result;
    }

    // Method Description:
    // - Helper for doing something on each and every peasant, with no regard
    //   for if the peasant is living or dead.
    // - We'll try calling callback on every peasant.
    // - If any single peasant is dead, then we'll call errorCallback, and move on.
    // - We're taking an errorCallback here, because the thing we usually want
    //   to do is TraceLog a message, but TraceLoggingWrite is actually a macro
    //   that _requires_ the second arg to be a string literal. It can't just be
    //   a variable.
    // Arguments:
    // - callback: The function to call on each peasant
    // - errorCallback: The function to call if a peasant is dead.
    // Return Value:
    // - <none>
    void Monarch::_forAllPeasantsIgnoringTheDead(std::function<void(const Remoting::IPeasant&, const uint64_t)> callback,
                                                 std::function<void(const uint64_t)> errorCallback)
    {
        for (const auto& [id, p] : _peasants)
        {
            try
            {
                callback(p, id);
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
                // If this fails, we don't _really_ care. Just move on to the
                // next one. Someone else will clean up the dead peasant.
                errorCallback(id);
            }
        }
    }

    // Method Description:
    // - This is an event handler for the IdentifyWindowsRequested event. A
    //   Peasant may raise that event if they want _all_ windows to identify
    //   themselves.
    // - This will tell each and every peasant to identify themselves. This will
    //   eventually propagate down to TerminalPage::IdentifyWindow.
    // Arguments:
    // - <unused>
    // Return Value:
    // - <none>
    void Monarch::_identifyWindows(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                   const winrt::Windows::Foundation::IInspectable& /*args*/)
    {
        // Notify all the peasants to display their ID.
        auto callback = [](auto&& p, auto&& /*id*/) {
            p.DisplayWindowId();
        };
        auto onError = [](auto&& id) {
            TraceLoggingWrite(g_hRemotingProvider,
                              "Monarch_identifyWindows_Failed",
                              TraceLoggingInt64(id, "peasantID", "The ID of the peasant which we could not identify"),
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                              TraceLoggingKeyword(TIL_KEYWORD_TRACE));
        };
        _forAllPeasantsIgnoringTheDead(callback, onError);
    }

    // Method Description:
    // - This is an event handler for the RenameRequested event. A
    //   Peasant may raise that event when they want to be renamed to something else.
    // - We will check if there are any other windows with this name. If there
    //   are, then we'll reject the rename by setting args.Succeeded=false.
    // - If there aren't any other windows with this name, then we'll set
    //   args.Succeeded=true, allowing the window to keep this name.
    // Arguments:
    // - args: Contains the requested window name and a boolean (Succeeded)
    //   indicating if the request was successful.
    // Return Value:
    // - <none>
    void Monarch::_renameRequested(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                   const winrt::Microsoft::Terminal::Remoting::RenameRequestArgs& args)
    {
        bool successfullyRenamed = false;

        try
        {
            args.Succeeded(false);
            const auto name{ args.NewName() };
            // Try to find a peasant that currently has this name
            const auto id = _lookupPeasantIdForName(name);
            if (_getPeasant(id) == nullptr)
            {
                // If there is one, then oh no! The requestor is not allowed to
                // be renamed.
                args.Succeeded(true);
                successfullyRenamed = true;
            }

            TraceLoggingWrite(g_hRemotingProvider,
                              "Monarch_renameRequested",
                              TraceLoggingWideString(name.c_str(), "name", "The newly proposed name"),
                              TraceLoggingInt64(successfullyRenamed, "successfullyRenamed", "true if the peasant is allowed to rename themselves to that name."),
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                              TraceLoggingKeyword(TIL_KEYWORD_TRACE));
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            // If this fails, we don't _really_ care. The peasant died, but
            // they're the only one who cares about the result.
            TraceLoggingWrite(g_hRemotingProvider,
                              "Monarch_renameRequested_Failed",
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                              TraceLoggingKeyword(TIL_KEYWORD_TRACE));
        }
    }

    // Method Description:
    // - Attempt to summon a window. `args` contains information about which
    //   window we should try to summon:
    //   * if a WindowName is provided, we'll try to find a window with exactly
    //     that name, and fail if there isn't one.
    // - Calls Peasant::Summon on the matching peasant (which might be an RPC call)
    // - This should only ever be called by the WindowManager in the monarch
    //   process itself. The monarch is the one registering for global hotkeys,
    //   so it's the one calling this method.
    // Arguments:
    // - args: contains information about the window that should be summoned.
    // Return Value:
    // - <none>
    // - Sets args.FoundMatch when a window matching args is found successfully.
    void Monarch::SummonWindow(const Remoting::SummonWindowSelectionArgs& args)
    {
        const auto searchedForName{ args.WindowName() };
        try
        {
            args.FoundMatch(false);
            uint64_t windowId = 0;
            // If no name was provided, then just summon the MRU window.
            if (searchedForName.empty())
            {
                // Use the value of the `desktop` arg to determine if we should
                // limit to the current desktop (desktop:onCurrent) or not
                // (desktop:any or desktop:toCurrent)
                windowId = _getMostRecentPeasantID(args.OnCurrentDesktop(), false);
            }
            else
            {
                // Try to find a peasant that currently has this name
                windowId = _lookupPeasantIdForName(searchedForName);
            }
            if (auto targetPeasant{ _getPeasant(windowId) })
            {
                targetPeasant.Summon(args.SummonBehavior());
                args.FoundMatch(true);
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            TraceLoggingWrite(g_hRemotingProvider,
                              "Monarch_SummonWindow_Failed",
                              TraceLoggingWideString(searchedForName.c_str(), "searchedForName", "The name of the window we tried to summon"),
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                              TraceLoggingKeyword(TIL_KEYWORD_TRACE));
        }
    }
}
