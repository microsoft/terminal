// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "WindowManager.h"
#include "MonarchFactory.h"
#include "CommandlineArgs.h"
#include "../inc/WindowingBehavior.h"
#include "FindTargetWindowArgs.h"
#include "ProposeCommandlineResult.h"

#include "WindowManager.g.cpp"
#include "../../types/inc/utils.hpp"

using namespace winrt;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Windows::Foundation;
using namespace ::Microsoft::Console;

namespace winrt::Microsoft::Terminal::Remoting::implementation
{
    WindowManager::WindowManager()
    {
        _monarchWaitInterrupt.create();

        // Register with COM as a server for the Monarch class
        _registerAsMonarch();
        // Instantiate an instance of the Monarch. This may or may not be in-proc!
        auto foundMonarch = false;
        while (!foundMonarch)
        {
            try
            {
                _createMonarchAndCallbacks();
                // _createMonarchAndCallbacks will initialize _isKing
                foundMonarch = true;
            }
            catch (...)
            {
                // If we fail to find the monarch,
                // stay in this jail until we do.
                TraceLoggingWrite(g_hRemotingProvider,
                                  "WindowManager_ExceptionInCtor",
                                  TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                  TraceLoggingKeyword(TIL_KEYWORD_TRACE));
            }
        }
    }

    WindowManager::~WindowManager()
    {
        // IMPORTANT! Tear down the registration as soon as we exit. If we're not a
        // real peasant window (the monarch passed our commandline to someone else),
        // then the monarch dies, we don't want our registration becoming the active
        // monarch!
        CoRevokeClassObject(_registrationHostClass);
        _registrationHostClass = 0;
        SignalClose();
        _monarchWaitInterrupt.SetEvent();

        // A thread is joinable once it's been started. Basically this just
        // makes sure that the thread isn't just default-constructed.
        if (_electionThread.joinable())
        {
            _electionThread.join();
        }
    }

    void WindowManager::SignalClose()
    {
        if (_monarch)
        {
            try
            {
                _monarch.SignalClose(_peasant.GetID());
            }
            CATCH_LOG()
        }
    }

    void WindowManager::_proposeToMonarch(const Remoting::CommandlineArgs& args,
                                          std::optional<uint64_t>& givenID,
                                          winrt::hstring& givenName)
    {
        // these two errors are Win32 errors, convert them to HRESULTS so we can actually compare below.
        static constexpr auto RPC_SERVER_UNAVAILABLE_HR = HRESULT_FROM_WIN32(RPC_S_SERVER_UNAVAILABLE);
        static constexpr auto RPC_CALL_FAILED_HR = HRESULT_FROM_WIN32(RPC_S_CALL_FAILED);

        // The monarch may respond back "you should be a new
        // window, with ID,name of (id, name)". Really the responses are:
        // * You should not create a new window
        // * Create a new window (but without a given ID or name). The
        //   Monarch will assign your ID/name later
        // * Create a new window, and you'll have this ID or name
        //   - This is the case where the user provides `wt -w 1`, and
        //     there's no existing window 1

        // You can emulate the monarch dying by: starting a terminal, sticking a
        // breakpoint in
        // TerminalApp!winrt::TerminalApp::implementation::AppLogic::_doFindTargetWindow,
        // starting a defterm, and when that BP gets hit, kill the original
        // monarch, and see what happens here.

        auto proposedCommandline = false;
        Remoting::ProposeCommandlineResult result{ nullptr };
        auto attempts = 0;
        while (!proposedCommandline)
        {
            try
            {
                // MSFT:38542548 _We believe_ that this is the source of the
                // crash here. After we get the result, stash its values into a
                // local copy, so that we can check them later. If the Monarch
                // dies between now and the inspection of
                // `result.ShouldCreateWindow` below, we don't want to explode
                // (since _proposeToMonarch is not try/caught).
                auto outOfProcResult = _monarch.ProposeCommandline(args);
                result = winrt::make<implementation::ProposeCommandlineResult>(outOfProcResult);

                proposedCommandline = true;
            }
            catch (...)
            {
                // We did not successfully ask the king what to do. This could
                // be for many reasons. Most commonly, the monarch died as we
                // were talking to it. That could be a RPC_SERVER_UNAVAILABLE_HR
                // or RPC_CALL_FAILED_HR (GH#12666). We also saw a
                // RPC_S_CALL_FAILED_DNE in GH#11790. Ultimately, if this is
                // gonna fail, we want to just try again, regardless of the
                // cause. That's why we're no longer checking what the exception
                // was, we're just always gonna try again regardless.
                //
                //  They hopefully just died here. That's okay, let's just go
                // ask the next in the line of succession. At the very worst,
                // we'll find _us_, (likely last in the line).
                TraceLoggingWrite(g_hRemotingProvider,
                                  "WindowManager_proposeToMonarch_unexpectedExceptionFromKing",
                                  TraceLoggingInt32(attempts, "attempts", "How many times we've tried"),
                                  TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                  TraceLoggingKeyword(TIL_KEYWORD_TRACE));
                LOG_CAUGHT_EXCEPTION();
                attempts++;

                if (attempts >= 10)
                {
                    // We've tried 10 times to find the monarch, failing each
                    // time. Since we have no idea why, we're guessing that in
                    // this case, there's just a Monarch registered that's
                    // misbehaving. In this case, just fall back to
                    // "IsolatedMonarchMode" - we can't trust the currently
                    // registered one.
                    TraceLoggingWrite(g_hRemotingProvider,
                                      "WindowManager_TooManyAttempts_NullMonarchIsolateMode",
                                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                      TraceLoggingKeyword(TIL_KEYWORD_TRACE));

                    _monarch = winrt::make<winrt::Microsoft::Terminal::Remoting::implementation::Monarch>();
                    _createCallbacks();
                }
                else
                {
                    // We failed to ask the monarch. It must have died. Try and
                    // find the real monarch. Don't perform an election, that
                    // assumes we have a peasant, which we don't yet.
                    _createMonarchAndCallbacks();
                    // _createMonarchAndCallbacks will initialize _isKing
                }
                if (_isKing)
                {
                    // We became the king. We don't need to ProposeCommandline to ourself, we're just
                    // going to do it.
                    //
                    // Return early, because there's nothing else for us to do here.
                    TraceLoggingWrite(g_hRemotingProvider,
                                      "WindowManager_proposeToMonarch_becameKing",
                                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                      TraceLoggingKeyword(TIL_KEYWORD_TRACE));

                    // In WindowManager::ProposeCommandline, had we been the
                    // king originally, we would have started by setting
                    // this to true. We became the monarch here, so set it
                    // here as well.
                    _shouldCreateWindow = true;
                    return;
                }

                // Here, we created the new monarch, it wasn't us, so we're
                // gonna go through the while loop again and ask the new
                // king.
                TraceLoggingWrite(g_hRemotingProvider,
                                  "WindowManager_proposeToMonarch_tryAgain",
                                  TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                  TraceLoggingKeyword(TIL_KEYWORD_TRACE));
            }
        }

        // Here, the monarch (not us) has replied to the message. Get the
        // valuables out of the response:
        _shouldCreateWindow = result.ShouldCreateWindow();
        if (result.Id())
        {
            givenID = result.Id().Value();
        }
        givenName = result.WindowName();

        // TraceLogging doesn't have a good solution for logging an
        // optional. So we have to repeat the calls here:
        if (givenID)
        {
            TraceLoggingWrite(g_hRemotingProvider,
                              "WindowManager_ProposeCommandline",
                              TraceLoggingBoolean(_shouldCreateWindow, "CreateWindow", "true iff we should create a new window"),
                              TraceLoggingUInt64(givenID.value(), "Id", "The ID we should assign our peasant"),
                              TraceLoggingWideString(givenName.c_str(), "Name", "The name we should assign this window"),
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                              TraceLoggingKeyword(TIL_KEYWORD_TRACE));
        }
        else
        {
            TraceLoggingWrite(g_hRemotingProvider,
                              "WindowManager_ProposeCommandline",
                              TraceLoggingBoolean(_shouldCreateWindow, "CreateWindow", "true iff we should create a new window"),
                              TraceLoggingPointer(nullptr, "Id", "No ID provided"),
                              TraceLoggingWideString(givenName.c_str(), "Name", "The name we should assign this window"),
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                              TraceLoggingKeyword(TIL_KEYWORD_TRACE));
        }
    }
    void WindowManager::ProposeCommandline(const Remoting::CommandlineArgs& args)
    {
        // If we're the king, we _definitely_ want to process the arguments, we were
        // launched with them!
        //
        // Otherwise, the King will tell us if we should make a new window
        _shouldCreateWindow = _isKing;
        std::optional<uint64_t> givenID;
        winrt::hstring givenName{};
        if (!_isKing)
        {
            _proposeToMonarch(args, givenID, givenName);
        }

        // During _proposeToMonarch, it's possible that we found that the king was dead, and we're the new king. Cool! Do this now.
        if (_isKing)
        {
            // We're the monarch, we don't need to propose anything. We're just
            // going to do it.
            //
            // However, we _do_ need to ask what our name should be. It's
            // possible someone started the _first_ wt with something like `wt
            // -w king` as the commandline - we want to make sure we set our
            // name to "king".
            //
            // The FindTargetWindow event is the WindowManager's way of saying
            // "I do not know how to figure out how to turn this list of args
            // into a window ID/name. Whoever's listening to this event does, so
            // I'll ask them". It's a convoluted way of hooking the
            // WindowManager up to AppLogic without actually telling it anything
            // about TerminalApp (or even WindowsTerminal)
            auto findWindowArgs{ winrt::make_self<Remoting::implementation::FindTargetWindowArgs>(args) };
            _raiseFindTargetWindowRequested(nullptr, *findWindowArgs);

            const auto responseId = findWindowArgs->ResultTargetWindow();
            if (responseId > 0)
            {
                givenID = ::base::saturated_cast<uint64_t>(responseId);

                TraceLoggingWrite(g_hRemotingProvider,
                                  "WindowManager_ProposeCommandline_AsMonarch",
                                  TraceLoggingBoolean(_shouldCreateWindow, "CreateWindow", "true iff we should create a new window"),
                                  TraceLoggingUInt64(givenID.value(), "Id", "The ID we should assign our peasant"),
                                  TraceLoggingWideString(givenName.c_str(), "Name", "The name we should assign this window"),
                                  TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                  TraceLoggingKeyword(TIL_KEYWORD_TRACE));
            }
            else if (responseId == WindowingBehaviorUseName)
            {
                givenName = findWindowArgs->ResultTargetWindowName();

                TraceLoggingWrite(g_hRemotingProvider,
                                  "WindowManager_ProposeCommandline_AsMonarch",
                                  TraceLoggingBoolean(_shouldCreateWindow, "CreateWindow", "true iff we should create a new window"),
                                  TraceLoggingUInt64(0, "Id", "The ID we should assign our peasant"),
                                  TraceLoggingWideString(givenName.c_str(), "Name", "The name we should assign this window"),
                                  TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                  TraceLoggingKeyword(TIL_KEYWORD_TRACE));
            }
            else
            {
                TraceLoggingWrite(g_hRemotingProvider,
                                  "WindowManager_ProposeCommandline_AsMonarch",
                                  TraceLoggingBoolean(_shouldCreateWindow, "CreateWindow", "true iff we should create a new window"),
                                  TraceLoggingUInt64(0, "Id", "The ID we should assign our peasant"),
                                  TraceLoggingWideString(L"", "Name", "The name we should assign this window"),
                                  TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                  TraceLoggingKeyword(TIL_KEYWORD_TRACE));
            }
        }

        if (_shouldCreateWindow)
        {
            // If we should create a new window, then instantiate our Peasant
            // instance, and tell that peasant to handle that commandline.
            _createOurPeasant({ givenID }, givenName);

            // Spawn a thread to wait on the monarch, and handle the election
            if (!_isKing)
            {
                _createPeasantThread();
            }

            // This right here will just tell us to stash the args away for the
            // future. The AppHost hasn't yet set up the callbacks, and the rest
            // of the app hasn't started at all. We'll note them and come back
            // later.
            _peasant.ExecuteCommandline(args);
        }
        // Otherwise, we'll do _nothing_.
    }

    bool WindowManager::ShouldCreateWindow()
    {
        return _shouldCreateWindow;
    }

    void WindowManager::_registerAsMonarch()
    {
        winrt::check_hresult(CoRegisterClassObject(Monarch_clsid,
                                                   winrt::make<::MonarchFactory>().get(),
                                                   CLSCTX_LOCAL_SERVER,
                                                   REGCLS_MULTIPLEUSE,
                                                   &_registrationHostClass));
    }

    void WindowManager::_createMonarch()
    {
        // Heads up! This only works because we're using
        // "metadata-based-marshalling" for our WinRT types. That means the OS is
        // using the .winmd file we generate to figure out the proxy/stub
        // definitions for our types automatically. This only works in the following
        // cases:
        //
        // * If we're running unpackaged: the .winmd must be a sibling of the .exe
        // * If we're running packaged: the .winmd must be in the package root
        _monarch = create_instance<Remoting::IMonarch>(Monarch_clsid,
                                                       CLSCTX_LOCAL_SERVER);
    }

    // Tries to instantiate a monarch, tries again, and eventually either throws
    // (so that the caller will try again) or falls back to the isolated
    // monarch.
    void WindowManager::_redundantCreateMonarch()
    {
        _createMonarch();

        if (_monarch == nullptr)
        {
            // See MSFT:38540483, GH#12774 for details.
            TraceLoggingWrite(g_hRemotingProvider,
                              "WindowManager_NullMonarchTryAgain",
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                              TraceLoggingKeyword(TIL_KEYWORD_TRACE));
            // Here we're gonna just give it a quick second try.Probably not
            // definitive, but might help.
            _createMonarch();
        }

        if (_monarch == nullptr)
        {
            // See MSFT:38540483, GH#12774 for details.
            if constexpr (Feature_IsolatedMonarchMode::IsEnabled())
            {
                // Fall back to having a in proc monarch. Were now isolated from
                // other windows. This is a pretty torn state, but at least we
                // didn't just explode.
                TraceLoggingWrite(g_hRemotingProvider,
                                  "WindowManager_NullMonarchIsolateMode",
                                  TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                  TraceLoggingKeyword(TIL_KEYWORD_TRACE));

                _monarch = winrt::make<winrt::Microsoft::Terminal::Remoting::implementation::Monarch>();
            }
            else
            {
                // The monarch is null. We're hoping that we can find another,
                // hopefully us. We're gonna go back around the loop again and
                // see what happens. If this is really an infinite loop (where
                // the OS won't even give us back US as the monarch), then I
                // suppose we'll find out soon enough.
                TraceLoggingWrite(g_hRemotingProvider,
                                  "WindowManager_NullMonarchTryAgain",
                                  TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                  TraceLoggingKeyword(TIL_KEYWORD_TRACE));

                winrt::hresult_error(E_UNEXPECTED, L"Did not expect the Monarch to ever be null");
            }
        }
    }

    // NOTE: This can throw! Callers include:
    // - the constructor, who performs this in a loop until it successfully
    //   find a a monarch
    // - the performElection method, which is called in the waitOnMonarch
    //   thread. All the calls in that thread are wrapped in try/catch's
    //   already.
    // - _createOurPeasant, who might do this in a loop to establish us with the
    //   monarch.
    void WindowManager::_createMonarchAndCallbacks()
    {
        _redundantCreateMonarch();
        // We're pretty confident that we have a Monarch here.
        _createCallbacks();
    }

    // Check if we became the king, and if we are, wire up callbacks.
    void WindowManager::_createCallbacks()
    {
        // Save the result of checking if we're the king. We want to avoid
        // unnecessary calls back and forth if we can.
        _isKing = _areWeTheKing();

        TraceLoggingWrite(g_hRemotingProvider,
                          "WindowManager_ConnectedToMonarch",
                          TraceLoggingUInt64(_monarch.GetPID(), "monarchPID", "The PID of the new Monarch"),
                          TraceLoggingBoolean(_isKing, "isKing", "true if we are the new monarch"),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));

        if (_peasant)
        {
            if (const auto& lastActivated{ _peasant.GetLastActivatedArgs() })
            {
                // Inform the monarch of the time we were last activated
                _monarch.HandleActivatePeasant(lastActivated);
            }
        }

        if (!_isKing)
        {
            return;
        }
        // Here, we're the king!
        //
        // This is where you should do any additional setup that might need to be
        // done when we become the king. This will be called both for the first
        // window, and when the current monarch dies.

        _monarch.WindowCreated({ get_weak(), &WindowManager::_WindowCreatedHandlers });
        _monarch.WindowClosed({ get_weak(), &WindowManager::_WindowClosedHandlers });
        _monarch.FindTargetWindowRequested({ this, &WindowManager::_raiseFindTargetWindowRequested });
        _monarch.ShowNotificationIconRequested([this](auto&&, auto&&) { _ShowNotificationIconRequestedHandlers(*this, nullptr); });
        _monarch.HideNotificationIconRequested([this](auto&&, auto&&) { _HideNotificationIconRequestedHandlers(*this, nullptr); });
        _monarch.QuitAllRequested({ get_weak(), &WindowManager::_QuitAllRequestedHandlers });

        _BecameMonarchHandlers(*this, nullptr);
    }

    bool WindowManager::_areWeTheKing()
    {
        const auto ourPID{ GetCurrentProcessId() };
        const auto kingPID{ _monarch.GetPID() };
        return (ourPID == kingPID);
    }

    Remoting::IPeasant WindowManager::_createOurPeasant(std::optional<uint64_t> givenID,
                                                        const winrt::hstring& givenName)
    {
        auto p = winrt::make_self<Remoting::implementation::Peasant>();
        if (givenID)
        {
            p->AssignID(givenID.value());
        }

        // If the name wasn't specified, this will be an empty string.
        p->WindowName(givenName);
        _peasant = *p;

        // Try to add us to the monarch. If that fails, try to find a monarch
        // again, until we find one (we will eventually find us)
        while (true)
        {
            try
            {
                _monarch.AddPeasant(_peasant);
                break;
            }
            catch (...)
            {
                try
                {
                    // Wrap this in its own try/catch, because this can throw.
                    _createMonarchAndCallbacks();
                }
                catch (...)
                {
                }
            }
        }

        _peasant.GetWindowLayoutRequested({ get_weak(), &WindowManager::_GetWindowLayoutRequestedHandlers });

        TraceLoggingWrite(g_hRemotingProvider,
                          "WindowManager_CreateOurPeasant",
                          TraceLoggingUInt64(_peasant.GetID(), "peasantID", "The ID of our new peasant"),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));

        // If the peasant asks us to quit we should not try to act in future elections.
        _peasant.QuitRequested([weakThis{ get_weak() }](auto&&, auto&&) {
            if (auto wm = weakThis.get())
            {
                wm->_monarchWaitInterrupt.SetEvent();
            }
        });

        return _peasant;
    }

    // Method Description:
    // - Attempt to connect to the monarch process. This might be us!
    // - For the new monarch, add us to their list of peasants.
    // Arguments:
    // - <none>
    // Return Value:
    // - true iff we're the new monarch process.
    // NOTE: This can throw!
    bool WindowManager::_performElection()
    {
        _createMonarchAndCallbacks();

        // Tell the new monarch who we are. We might be that monarch!
        _monarch.AddPeasant(_peasant);

        // This method is only called when a _new_ monarch is elected. So
        // don't do anything here that needs to be done for all monarch
        // windows. This should only be for work that's done when a window
        // _becomes_ a monarch, after the death of the previous monarch.
        return _isKing;
    }

    void WindowManager::_createPeasantThread()
    {
        // If we catch an exception trying to get at the monarch ever, we can
        // set the _monarchWaitInterrupt, and use that to trigger a new
        // election. Though, we wouldn't be able to retry the function that
        // caused the exception in the first place...

        _electionThread = std::thread([this] {
            _waitOnMonarchThread();
        });
    }

    void WindowManager::_waitOnMonarchThread()
    {
        // This is the array of HANDLEs that we're going to wait on in
        // WaitForMultipleObjects below.
        // * waits[0] will be the handle to the monarch process. It gets
        //   signalled when the process exits / dies.
        // * waits[1] is the handle to our _monarchWaitInterrupt event. Another
        //   thread can use that to manually break this loop. We'll do that when
        //   we're getting torn down.
        HANDLE waits[2];
        waits[1] = _monarchWaitInterrupt.get();
        const auto peasantID = _peasant.GetID(); // safe: _peasant is in-proc.

        auto exitThreadRequested = false;
        while (!exitThreadRequested)
        {
            // At any point in all this, the current monarch might die. If it
            // does, we'll go straight to a new election, in the "jail"
            // try/catch below. Worst case, eventually, we'll become the new
            // monarch.
            try
            {
                // This might fail to even ask the monarch for its PID.
                wil::unique_handle hMonarch{ OpenProcess(PROCESS_ALL_ACCESS,
                                                         FALSE,
                                                         static_cast<DWORD>(_monarch.GetPID())) };

                // If we fail to open the monarch, then they don't exist
                //  anymore! Go straight to an election.
                if (hMonarch.get() == nullptr)
                {
                    const auto gle = GetLastError();
                    TraceLoggingWrite(g_hRemotingProvider,
                                      "WindowManager_FailedToOpenMonarch",
                                      TraceLoggingUInt64(peasantID, "peasantID", "Our peasant ID"),
                                      TraceLoggingUInt64(gle, "lastError", "The result of GetLastError"),
                                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                      TraceLoggingKeyword(TIL_KEYWORD_TRACE));

                    exitThreadRequested = _performElection();
                    continue;
                }

                waits[0] = hMonarch.get();
                auto waitResult = WaitForMultipleObjects(2, waits, FALSE, INFINITE);

                switch (waitResult)
                {
                case WAIT_OBJECT_0 + 0: // waits[0] was signaled, the handle to the monarch process

                    TraceLoggingWrite(g_hRemotingProvider,
                                      "WindowManager_MonarchDied",
                                      TraceLoggingUInt64(peasantID, "peasantID", "Our peasant ID"),
                                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                      TraceLoggingKeyword(TIL_KEYWORD_TRACE));
                    // Connect to the new monarch, which might be us!
                    // If we become the monarch, then we'll return true and exit this thread.
                    exitThreadRequested = _performElection();
                    break;

                case WAIT_OBJECT_0 + 1: // waits[1] was signaled, our manual interrupt

                    TraceLoggingWrite(g_hRemotingProvider,
                                      "WindowManager_MonarchWaitInterrupted",
                                      TraceLoggingUInt64(peasantID, "peasantID", "Our peasant ID"),
                                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                      TraceLoggingKeyword(TIL_KEYWORD_TRACE));
                    exitThreadRequested = true;
                    break;

                case WAIT_TIMEOUT:
                    // This should be impossible.
                    TraceLoggingWrite(g_hRemotingProvider,
                                      "WindowManager_MonarchWaitTimeout",
                                      TraceLoggingUInt64(peasantID, "peasantID", "Our peasant ID"),
                                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                      TraceLoggingKeyword(TIL_KEYWORD_TRACE));
                    exitThreadRequested = true;
                    break;

                default:
                {
                    // Returning any other value is invalid. Just die.
                    const auto gle = GetLastError();
                    TraceLoggingWrite(g_hRemotingProvider,
                                      "WindowManager_WaitFailed",
                                      TraceLoggingUInt64(peasantID, "peasantID", "Our peasant ID"),
                                      TraceLoggingUInt64(gle, "lastError", "The result of GetLastError"),
                                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                      TraceLoggingKeyword(TIL_KEYWORD_TRACE));
                    ExitProcess(0);
                }
                }
            }
            catch (...)
            {
                // Theoretically, if window[1] dies when we're trying to get
                // its PID we'll get here. If we just try to do the election
                // once here, it's possible we might elect window[2], but have
                // it die before we add ourselves as a peasant. That
                // _performElection call will throw, and we wouldn't catch it
                // here, and we'd die.

                // Instead, we're going to have a resilient election process.
                // We're going to keep trying an election, until one _doesn't_
                // throw an exception. That might mean burning through all the
                // other dying monarchs until we find us as the monarch. But if
                // this process is alive, then there's _someone_ in the line of
                // succession.

                TraceLoggingWrite(g_hRemotingProvider,
                                  "WindowManager_ExceptionInWaitThread",
                                  TraceLoggingUInt64(peasantID, "peasantID", "Our peasant ID"),
                                  TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                  TraceLoggingKeyword(TIL_KEYWORD_TRACE));
                auto foundNewMonarch = false;
                while (!foundNewMonarch)
                {
                    try
                    {
                        exitThreadRequested = _performElection();
                        // It doesn't matter if we're the monarch, or someone
                        // else is, but if we complete the election, then we've
                        // registered with a new one. We can escape this jail
                        // and re-enter society.
                        foundNewMonarch = true;
                    }
                    catch (...)
                    {
                        // If we fail to acknowledge the results of the election,
                        // stay in this jail until we do.
                        TraceLoggingWrite(g_hRemotingProvider,
                                          "WindowManager_ExceptionInNestedWaitThread",
                                          TraceLoggingUInt64(peasantID, "peasantID", "Our peasant ID"),
                                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));
                    }
                }
            }
        }
    }

    Remoting::Peasant WindowManager::CurrentWindow()
    {
        return _peasant;
    }

    void WindowManager::_raiseFindTargetWindowRequested(const winrt::Windows::Foundation::IInspectable& sender,
                                                        const winrt::Microsoft::Terminal::Remoting::FindTargetWindowArgs& args)
    {
        _FindTargetWindowRequestedHandlers(sender, args);
    }

    bool WindowManager::IsMonarch()
    {
        return _isKing;
    }

    void WindowManager::SummonWindow(const Remoting::SummonWindowSelectionArgs& args)
    {
        // We should only ever get called when we are the monarch, because only
        // the monarch ever registers for the global hotkey. So the monarch is
        // the only window that will be calling this.
        _monarch.SummonWindow(args);
    }

    void WindowManager::SummonAllWindows()
    {
        _monarch.SummonAllWindows();
    }

    Windows::Foundation::Collections::IVectorView<winrt::Microsoft::Terminal::Remoting::PeasantInfo> WindowManager::GetPeasantInfos()
    {
        // We should only get called when we're the monarch since the monarch
        // is the only one that knows about all peasants.
        return _monarch.GetPeasantInfos();
    }

    uint64_t WindowManager::GetNumberOfPeasants()
    {
        if (_monarch)
        {
            try
            {
                return _monarch.GetNumberOfPeasants();
            }
            CATCH_LOG()
        }
        return 0;
    }

    // Method Description:
    // - Ask the monarch to show a notification icon.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    winrt::fire_and_forget WindowManager::RequestShowNotificationIcon()
    {
        co_await winrt::resume_background();
        _peasant.RequestShowNotificationIcon();
    }

    // Method Description:
    // - Ask the monarch to hide its notification icon.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    winrt::fire_and_forget WindowManager::RequestHideNotificationIcon()
    {
        auto strongThis{ get_strong() };
        co_await winrt::resume_background();
        _peasant.RequestHideNotificationIcon();
    }

    // Method Description:
    // - Ask the monarch to quit all windows.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    winrt::fire_and_forget WindowManager::RequestQuitAll()
    {
        auto strongThis{ get_strong() };
        co_await winrt::resume_background();
        _peasant.RequestQuitAll();
    }

    bool WindowManager::DoesQuakeWindowExist()
    {
        return _monarch.DoesQuakeWindowExist();
    }

    void WindowManager::UpdateActiveTabTitle(winrt::hstring title)
    {
        winrt::get_self<implementation::Peasant>(_peasant)->ActiveTabTitle(title);
    }

    Windows::Foundation::Collections::IVector<winrt::hstring> WindowManager::GetAllWindowLayouts()
    {
        if (_monarch)
        {
            try
            {
                return _monarch.GetAllWindowLayouts();
            }
            CATCH_LOG()
        }
        return nullptr;
    }
}
