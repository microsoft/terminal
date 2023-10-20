// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "../inc/WindowingBehavior.h"
#include "Monarch.h"
#include "CommandlineArgs.h"
#include "FindTargetWindowArgs.h"
#include "QuitAllRequestedArgs.h"
#include "ProposeCommandlineResult.h"

#include "Monarch.g.cpp"
#include "WindowRequestedArgs.g.cpp"
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

    // This constructor is intended to be used in unit tests,
    // but we need to make it public in order to use make_self
    // in the tests. It's not exposed through the idl though
    // so it's not _truly_ fully public which should be acceptable.
    Monarch::Monarch(const uint64_t testPID) :
        _ourPID{ testPID }
    {
    }

    Monarch::~Monarch() = default;

    uint64_t Monarch::GetPID()
    {
        return _ourPID;
    }

    // Method Description:
    // - Add the given peasant to the list of peasants we're tracking. This
    //   Peasant may have already been assigned an ID. If it hasn't, then give
    //   it an ID.
    // - NB: this takes a unique_lock on _peasantsMutex.
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
                // If multiple peasants are added concurrently we keep trying to update
                // until we get to set the new id.
                uint64_t current;
                do
                {
                    current = _nextPeasantID.load(std::memory_order_relaxed);
                } while (current <= providedID && !_nextPeasantID.compare_exchange_weak(current, providedID + 1, std::memory_order_relaxed));
            }

            auto newPeasantsId = peasant.GetID();

            // Keep track of which peasant we are
            // SAFETY: this is only true for one peasant, and each peasant
            // is only added to a monarch once, so we do not need synchronization here.
            if (peasant.GetPID() == _ourPID)
            {
                _ourPeasantId = newPeasantsId;
            }
            // Add an event listener to the peasant's WindowActivated event.
            peasant.WindowActivated({ this, &Monarch::_peasantWindowActivated });
            peasant.IdentifyWindowsRequested({ this, &Monarch::_identifyWindows });
            peasant.RenameRequested({ this, &Monarch::_renameRequested });

            peasant.ShowNotificationIconRequested([this](auto&&, auto&&) { _ShowNotificationIconRequestedHandlers(*this, nullptr); });
            peasant.HideNotificationIconRequested([this](auto&&, auto&&) { _HideNotificationIconRequestedHandlers(*this, nullptr); });
            peasant.QuitAllRequested({ this, &Monarch::_handleQuitAll });

            {
                std::unique_lock lock{ _peasantsMutex };
                _peasants[newPeasantsId] = peasant;
            }

            TraceLoggingWrite(g_hRemotingProvider,
                              "Monarch_AddPeasant",
                              TraceLoggingUInt64(providedID, "providedID", "the provided ID for the peasant"),
                              TraceLoggingUInt64(newPeasantsId, "peasantID", "the ID of the new peasant"),
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                              TraceLoggingKeyword(TIL_KEYWORD_TRACE));

            _WindowCreatedHandlers(nullptr, nullptr);
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
    // - Gives the host process an opportunity to run any pre-close logic then
    //   requests all peasants to close.
    // Arguments:
    // - <none> used
    // Return Value:
    // - <none>
    winrt::fire_and_forget Monarch::_handleQuitAll(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                                   const winrt::Windows::Foundation::IInspectable& /*args*/)
    {
        // Let the process hosting the monarch run any needed logic before
        // closing all windows.
        auto args = winrt::make_self<implementation::QuitAllRequestedArgs>();
        _QuitAllRequestedHandlers(*this, *args);

        if (const auto action = args->BeforeQuitAllAction())
        {
            co_await action;
        }

        _quitting.store(true);
        // Tell all peasants to exit.
        const auto callback = [&](const auto& id, const auto& p) {
            // We want to tell our peasant to quit last, so that we don't try
            // to perform a bunch of elections on quit.
            if (id != _ourPeasantId)
            {
                p.Quit();
            }
        };
        const auto onError = [&](const auto& id) {
            TraceLoggingWrite(g_hRemotingProvider,
                              "Monarch_handleQuitAll_Failed",
                              TraceLoggingInt64(id, "peasantID", "The ID of the peasant which we could not close"),
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                              TraceLoggingKeyword(TIL_KEYWORD_TRACE));
        };

        _forEachPeasant(callback, onError);

        {
            std::shared_lock lock{ _peasantsMutex };
            const auto peasantSearch = _peasants.find(_ourPeasantId);
            if (peasantSearch != _peasants.end())
            {
                peasantSearch->second.Quit();
            }
            else
            {
                // Somehow we don't have our own peasant, this should never happen.
                // We are trying to quit anyways so just fail here.
                assert(peasantSearch != _peasants.end());
            }
        }
    }

    // Method Description:
    // - Tells the monarch that a peasant is being closed.
    // - NB: this (separately) takes unique locks on _peasantsMutex and
    //   _mruPeasantsMutex.
    // Arguments:
    // - peasantId: the id of the peasant
    // Return Value:
    // - <none>
    void Monarch::SignalClose(const uint64_t peasantId)
    {
        // If we are quitting we don't care about maintaining our list of
        // peasants anymore, and don't need to notify the host that something
        // changed.
        if (_quitting.load(std::memory_order_acquire))
        {
            return;
        }

        _clearOldMruEntries({ peasantId });
        {
            std::unique_lock lock{ _peasantsMutex };
            _peasants.erase(peasantId);
        }
        _WindowClosedHandlers(nullptr, nullptr);
    }

    // Method Description:
    // - Counts the number of living peasants.
    // Arguments:
    // - <none>
    // Return Value:
    // - the number of active peasants.
    uint64_t Monarch::GetNumberOfPeasants()
    {
        std::shared_lock lock{ _peasantsMutex };
        return _peasants.size();
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
    // - NB: this (separately) takes unique locks on _peasantsMutex and
    //   _mruPeasantsMutex.
    // Arguments:
    // - peasantID: The ID Of the peasant to find
    // - clearMruPeasantOnFailure: When true this function will handle clearing
    //   from _mruPeasants if a peasant was not found, otherwise the caller is
    //   expected to handle that cleanup themselves.
    // Return Value:
    // - the peasant if it exists in our map, otherwise null
    Remoting::IPeasant Monarch::_getPeasant(uint64_t peasantID, bool clearMruPeasantOnFailure)
    {
        try
        {
            IPeasant maybeThePeasant = nullptr;
            {
                std::shared_lock lock{ _peasantsMutex };
                const auto peasantSearch = _peasants.find(peasantID);
                maybeThePeasant = peasantSearch == _peasants.end() ? nullptr : peasantSearch->second;
            }
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
            {
                std::unique_lock lock{ _peasantsMutex };
                _peasants.erase(peasantID);
            }

            if (clearMruPeasantOnFailure)
            {
                // Remove the peasant from the list of MRU windows. They're dead.
                // They can't be the MRU anymore.
                _clearOldMruEntries({ peasantID });
            }
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
        if (name == L"new")
        {
            return 0;
        }

        uint64_t result = 0;

        const auto callback = [&](const auto& id, const auto& p) {
            auto otherName = p.WindowName();
            if (otherName == name)
            {
                result = id;
                return false;
            }
            return true;
        };

        const auto onError = [&](const auto& id) {
            TraceLoggingWrite(g_hRemotingProvider,
                              "Monarch_lookupPeasantIdForName_Failed",
                              TraceLoggingInt64(id, "peasantID", "The ID of the peasant which we could not get the name of"),
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                              TraceLoggingKeyword(TIL_KEYWORD_TRACE));
        };

        _forEachPeasant(callback, onError);

        TraceLoggingWrite(g_hRemotingProvider,
                          "Monarch_lookupPeasantIdForName",
                          TraceLoggingWideString(std::wstring{ name }.c_str(), "name", "the name we're looking for"),
                          TraceLoggingUInt64(result, "peasantID", "the ID of the peasant with that name"),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));
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
        if (args == nullptr)
        {
            // MSFT:35731327, GH #12624. There's a chance that the way the
            // window gets set up for defterm, the ActivatedArgs haven't been
            // created for this window yet. Check here and just ignore them if
            // they're null. They'll come back with real args soon
            return;
        }
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
    // - NB: This takes a unique lock on _mruPeasantsMutex.
    // Arguments:
    // - peasantIds: The list of peasant IDs to remove from the MRU list
    // Return Value:
    // - <none>
    void Monarch::_clearOldMruEntries(const std::unordered_set<uint64_t>& peasantIds)
    {
        if (peasantIds.size() == 0)
        {
            return;
        }

        std::unique_lock lock{ _mruPeasantsMutex };
        auto partition = std::remove_if(_mruPeasants.begin(), _mruPeasants.end(), [&](const auto& p) {
            const auto id = p.PeasantID();
            // remove the element if it was found in the list to erase.
            if (peasantIds.count(id) == 1)
            {
                TraceLoggingWrite(g_hRemotingProvider,
                                  "Monarch_RemovedPeasantFromDesktop",
                                  TraceLoggingUInt64(id, "peasantID", "The ID of the peasant"),
                                  TraceLoggingGuid(p.DesktopID(), "desktopGuid", "The GUID of the previous desktop the window was on"),
                                  TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                  TraceLoggingKeyword(TIL_KEYWORD_TRACE));
                return true;
            }
            return false;
        });

        // Remove everything that was in the list
        _mruPeasants.erase(partition, _mruPeasants.end());
    }

    // Method Description:
    // - Actually handle inserting the WindowActivatedArgs into our list of MRU windows.
    // - NB: this takes a unique_lock on _mruPeasantsMutex.
    // Arguments:
    // - localArgs: an in-proc WindowActivatedArgs that we should add to our list of MRU windows.
    // Return Value:
    // - <none>
    void Monarch::_doHandleActivatePeasant(const winrt::com_ptr<implementation::WindowActivatedArgs>& localArgs)
    {
        // We're sure that localArgs isn't null here, we checked before in our
        // one caller (in Monarch::HandleActivatePeasant)

        const auto newLastActiveTime = localArgs->ActivatedTime().time_since_epoch().count();

        // * Check all the current lists to look for this peasant.
        //   remove it from any where it exists.
        _clearOldMruEntries({ localArgs->PeasantID() });

        // * If the current desktop doesn't have a vector, add one.
        const auto desktopGuid{ localArgs->DesktopID() };

        {
            std::unique_lock lock{ _mruPeasantsMutex };
            // * Add this args list. By using lower_bound with insert, we can get it
            //   into exactly the right spot, without having to re-sort the whole
            //   array.
            _mruPeasants.insert(std::lower_bound(_mruPeasants.begin(),
                                                 _mruPeasants.end(),
                                                 *localArgs,
                                                 [](const auto& first, const auto& second) { return first.ActivatedTime() > second.ActivatedTime(); }),
                                *localArgs);
        }

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
    // - NB: This method will hold a shared lock on _mruPeasantsMutex and
    //   potentially a unique_lock on _peasantsMutex at the same time.
    //   Separately it might hold a unique_lock on _mruPeasantsMutex.
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
        std::shared_lock lock{ _mruPeasantsMutex };
        if (_mruPeasants.empty())
        {
            // unlock the mruPeasants mutex to make sure we can't deadlock here.
            lock.unlock();
            // Only need a shared lock for read
            std::shared_lock peasantsLock{ _peasantsMutex };
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
        uint64_t result = 0;
        std::unordered_set<uint64_t> peasantsToErase{};
        for (const auto& mruWindowArgs : _mruPeasants)
        {
            // Try to get the peasant, but do not have _getPeasant clean up old
            // _mruPeasants because we are iterating here.
            // SAFETY: _getPeasant can take a unique_lock on _peasantsMutex if
            // it detects a peasant is dead. Currently _getMostRecentPeasantId
            // is the only method that holds a lock on both _mruPeasantsMutex and
            // _peasantsMutex at the same time so there cannot be a deadlock here.
            const auto peasant{ _getPeasant(mruWindowArgs.PeasantID(), false) };
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
                peasantsToErase.emplace(mruWindowArgs.PeasantID());
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
                    result = mruWindowArgs.PeasantID();
                    break;
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

                result = mruWindowArgs.PeasantID();
                break;
            }
        }

        lock.unlock();

        if (peasantsToErase.size() > 0)
        {
            _clearOldMruEntries(peasantsToErase);
        }

        if (result == 0)
        {
            // Here, we've checked all the windows, and none of them was both alive
            // and the most recent (on this desktop). Just return 0 - the caller
            // will use this to create a new window.
            TraceLoggingWrite(g_hRemotingProvider,
                              "Monarch_getMostRecentPeasantID_NotFound",
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                              TraceLoggingKeyword(TIL_KEYWORD_TRACE));
        }

        return result;
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

        if (targetWindow == WindowingBehaviorUseNone)
        {
            // In this case, the targetWindow was UseNone, which means that we
            // want to make a message box, but otherwise not make a Terminal
            // window.
            return winrt::make<Remoting::implementation::ProposeCommandlineResult>(false);
        }
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
            case WindowingBehaviorUseNone:
                // This should be impossible. The if statement above should have
                // prevented WindowingBehaviorUseNone from falling in here.
                // Explode, because this is a programming error.
                THROW_HR(E_UNEXPECTED);
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

                    _RequestNewWindowHandlers(*this, *winrt::make_self<WindowRequestedArgs>(*result, args));

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

                _RequestNewWindowHandlers(*this, *winrt::make_self<WindowRequestedArgs>(*result, args));

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

        _RequestNewWindowHandlers(*this, *winrt::make_self<WindowRequestedArgs>(*result, args));

        return *result;
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
        const auto callback = [&](const auto& /*id*/, const auto& p) {
            p.DisplayWindowId();
        };
        const auto onError = [&](const auto& id) {
            TraceLoggingWrite(g_hRemotingProvider,
                              "Monarch_identifyWindows_Failed",
                              TraceLoggingInt64(id, "peasantID", "The ID of the peasant which we could not identify"),
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                              TraceLoggingKeyword(TIL_KEYWORD_TRACE));
        };

        _forEachPeasant(callback, onError);
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
        auto successfullyRenamed = false;

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

            // If a WindowID is provided from the args, use that first.
            uint64_t windowId = 0;
            if (args.WindowID())
            {
                windowId = args.WindowID().Value();
            }
            else
            {
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
            }

            if (auto targetPeasant{ _getPeasant(windowId) })
            {
                targetPeasant.Summon(args.SummonBehavior());
                args.FoundMatch(true);

                TraceLoggingWrite(g_hRemotingProvider,
                                  "Monarch_SummonWindow_Success",
                                  TraceLoggingWideString(searchedForName.c_str(), "searchedForName", "The name of the window we tried to summon"),
                                  TraceLoggingUInt64(windowId, "peasantID", "The id of the window we tried to summon"),
                                  TraceLoggingBoolean(args.OnCurrentDesktop(), "OnCurrentDesktop", "true iff the window needs to be on the current virtual desktop"),
                                  TraceLoggingBoolean(args.SummonBehavior().MoveToCurrentDesktop(), "MoveToCurrentDesktop", "if true, move the window to the current virtual desktop"),
                                  TraceLoggingBoolean(args.SummonBehavior().ToggleVisibility(), "ToggleVisibility", "true if we should toggle the visibility of the window"),
                                  TraceLoggingUInt32(args.SummonBehavior().DropdownDuration(), "DropdownDuration", "the duration to dropdown the window"),
                                  TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                  TraceLoggingKeyword(TIL_KEYWORD_TRACE));
            }
            else
            {
                TraceLoggingWrite(g_hRemotingProvider,
                                  "Monarch_SummonWindow_NoPeasant",
                                  TraceLoggingWideString(searchedForName.c_str(), "searchedForName", "The name of the window we tried to summon"),
                                  TraceLoggingUInt64(windowId, "peasantID", "The id of the window we tried to summon"),
                                  TraceLoggingBoolean(args.OnCurrentDesktop(), "OnCurrentDesktop", "true iff the window needs to be on the current virtual desktop"),
                                  TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                  TraceLoggingKeyword(TIL_KEYWORD_TRACE));
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

    // Method Description:
    // - This method creates a map of peasant IDs to peasant names
    //   while removing dead peasants.
    // Arguments:
    // - <none>
    // Return Value:
    // - A map of peasant IDs to their names.
    Windows::Foundation::Collections::IVectorView<PeasantInfo> Monarch::GetPeasantInfos()
    {
        std::vector<PeasantInfo> names;
        {
            std::shared_lock lock{ _peasantsMutex };
            names.reserve(_peasants.size());
        }

        const auto func = [&](const auto& id, const auto& p) -> void {
            names.push_back({ id, p.WindowName(), p.ActiveTabTitle() });
        };

        const auto onError = [&](const auto& id) {
            TraceLoggingWrite(g_hRemotingProvider,
                              "Monarch_identifyWindows_Failed",
                              TraceLoggingInt64(id, "peasantID", "The ID of the peasant which we could not identify"),
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                              TraceLoggingKeyword(TIL_KEYWORD_TRACE));
        };

        _forEachPeasant(func, onError);

        return winrt::single_threaded_vector<PeasantInfo>(std::move(names)).GetView();
    }

    bool Monarch::DoesQuakeWindowExist()
    {
        auto result = false;
        const auto func = [&](const auto& /*id*/, const auto& p) {
            if (p.WindowName() == QuakeWindowName)
            {
                result = true;
            }
            // continue if we didn't get a positive result
            return !result;
        };

        const auto onError = [&](const auto& id) {
            TraceLoggingWrite(g_hRemotingProvider,
                              "Monarch_DoesQuakeWindowExist_Failed",
                              TraceLoggingInt64(id, "peasantID", "The ID of the peasant which we could not ask for its name"),
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                              TraceLoggingKeyword(TIL_KEYWORD_TRACE));
        };

        _forEachPeasant(func, onError);
        return result;
    }

    void Monarch::SummonAllWindows()
    {
        const auto func = [&](const auto& /*id*/, const auto& p) {
            SummonWindowBehavior args{};
            args.ToggleVisibility(false);
            p.Summon(args);
        };

        const auto onError = [&](const auto& id) {
            TraceLoggingWrite(g_hRemotingProvider,
                              "Monarch_SummonAll_Failed",
                              TraceLoggingInt64(id, "peasantID", "The ID of the peasant which we could not summon"),
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                              TraceLoggingKeyword(TIL_KEYWORD_TRACE));
        };

        _forEachPeasant(func, onError);
    }

    // Method Description:
    // - Ask all peasants to return their window layout as json
    // Arguments:
    // - <none>
    // Return Value:
    // - The collection of window layouts from each peasant.
    Windows::Foundation::Collections::IVector<winrt::hstring> Monarch::GetAllWindowLayouts()
    {
        std::vector<winrt::hstring> vec;
        auto callback = [&](const auto& /*id*/, const auto& p) {
            vec.emplace_back(p.GetWindowLayout());
        };
        auto onError = [](auto&& id) {
            TraceLoggingWrite(g_hRemotingProvider,
                              "Monarch_GetAllWindowLayouts_Failed",
                              TraceLoggingInt64(id, "peasantID", "The ID of the peasant which we could not get a window layout from"),
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                              TraceLoggingKeyword(TIL_KEYWORD_TRACE));
        };
        _forEachPeasant(callback, onError);

        return winrt::single_threaded_vector(std::move(vec));
    }

    void Monarch::RequestMoveContent(winrt::hstring window,
                                     winrt::hstring content,
                                     uint32_t tabIndex,
                                     const Windows::Foundation::IReference<Windows::Foundation::Rect>& windowBounds)
    {
        TraceLoggingWrite(g_hRemotingProvider,
                          "Monarch_MoveContent_Requested",
                          TraceLoggingWideString(window.c_str(), "window", "The name of the window we tried to move to"),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));

        uint64_t windowId = _lookupPeasantIdForName(window);
        if (windowId == 0)
        {
            // Try the name as an integer ID
            uint32_t temp;
            if (!Utils::StringToUint(window.c_str(), temp))
            {
                TraceLoggingWrite(g_hRemotingProvider,
                                  "Monarch_MoveContent_FailedToParseId",
                                  TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                  TraceLoggingKeyword(TIL_KEYWORD_TRACE));
            }
            else
            {
                windowId = temp;
            }
        }

        if (auto targetPeasant{ _getPeasant(windowId) })
        {
            auto request = winrt::make_self<implementation::AttachRequest>(content, tabIndex);
            targetPeasant.AttachContentToWindow(*request);
            TraceLoggingWrite(g_hRemotingProvider,
                              "Monarch_MoveContent_Completed",
                              TraceLoggingInt64(windowId, "windowId", "The ID of the peasant which we sent the content to"),
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                              TraceLoggingKeyword(TIL_KEYWORD_TRACE));
        }
        else
        {
            TraceLoggingWrite(g_hRemotingProvider,
                              "Monarch_MoveContent_NoWindow",
                              TraceLoggingInt64(windowId, "windowId", "We could not find a peasant with this ID"),
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                              TraceLoggingKeyword(TIL_KEYWORD_TRACE));

            // In the case where window couldn't be found, then create a window
            // for that name / ID.
            //
            // Don't let the window literally be named "-1", because that's silly. Same with "new"
            const bool nameIsReserved = window == L"-1" || window == L"new";
            auto request = winrt::make_self<implementation::WindowRequestedArgs>(nameIsReserved ? L"" : window,
                                                                                 content,
                                                                                 windowBounds);
            _RequestNewWindowHandlers(*this, *request);
        }
    }

    // Very similar to the above. Someone came and told us that they were the target of a drag/drop, and they know who started it.
    // We will go tell the person who started it that they should send that target the content which was dragged.
    void Monarch::RequestSendContent(const Remoting::RequestReceiveContentArgs& args)
    {
        TraceLoggingWrite(g_hRemotingProvider,
                          "Monarch_SendContent_Requested",
                          TraceLoggingUInt64(args.SourceWindow(), "source", "The window which started the drag"),
                          TraceLoggingUInt64(args.TargetWindow(), "target", "The window which was the target of the drop"),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));

        if (auto senderPeasant{ _getPeasant(args.SourceWindow()) })
        {
            senderPeasant.SendContent(args);

            TraceLoggingWrite(g_hRemotingProvider,
                              "Monarch_SendContent_Completed",
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                              TraceLoggingKeyword(TIL_KEYWORD_TRACE));
        }
        else
        {
            // We couldn't find the peasant that started the drag. Well that
            // sure is weird, but that would indicate that the sender closed
            // after starting the drag. No matter. We can just do nothing.

            TraceLoggingWrite(g_hRemotingProvider,
                              "Monarch_SendContent_NoWindow",
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                              TraceLoggingKeyword(TIL_KEYWORD_TRACE));
        }
    }
}
