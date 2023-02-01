// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "WindowManager2.h"

#include "../inc/WindowingBehavior.h"
#include "MonarchFactory.h"

#include "CommandlineArgs.h"
#include "FindTargetWindowArgs.h"
#include "ProposeCommandlineResult.h"

#include "WindowManager2.g.cpp"
#include "../../types/inc/utils.hpp"

using namespace winrt;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Windows::Foundation;
using namespace ::Microsoft::Console;

namespace winrt::Microsoft::Terminal::Remoting::implementation
{
    WindowManager2::WindowManager2()
    {
    }
    WindowManager2::~WindowManager2()
    {
        // IMPORTANT! Tear down the registration as soon as we exit. If we're not a
        // real peasant window (the monarch passed our commandline to someone else),
        // then the monarch dies, we don't want our registration becoming the active
        // monarch!
        CoRevokeClassObject(_registrationHostClass);
        _registrationHostClass = 0;
    }

    void WindowManager2::_createMonarch()
    {
        // Heads up! This only works because we're using
        // "metadata-based-marshalling" for our WinRT types. That means the OS is
        // using the .winmd file we generate to figure out the proxy/stub
        // definitions for our types automatically. This only works in the following
        // cases:
        //
        // * If we're running unpackaged: the .winmd must be a sibling of the .exe
        // * If we're running packaged: the .winmd must be in the package root
        _monarch = try_create_instance<Remoting::IMonarch>(Monarch_clsid,
                                                       CLSCTX_LOCAL_SERVER);
    }

    void WindowManager2::_registerAsMonarch()
    {
        winrt::check_hresult(CoRegisterClassObject(Monarch_clsid,
                                                   winrt::make<::MonarchFactory>().get(),
                                                   CLSCTX_LOCAL_SERVER,
                                                   REGCLS_MULTIPLEUSE,
                                                   &_registrationHostClass));
    }

    Remoting::ProposeCommandlineResult WindowManager2::ProposeCommandline2(const Remoting::CommandlineArgs& args)
    {
        bool shouldCreateWindow = false;

        _createMonarch();
        if (_monarch == nullptr)
        {
            // No pre-existing instance.

            // Raise an event, to ask how to handle this commandline. We can't ask
            // the app ourselves - we exist isolated from that knowledge (and
            // dependency hell). The WindowManager will raise this up to the app
            // host, which will then ask the AppLogic, who will then parse the
            // commandline and determine the provided ID of the window.
            auto findWindowArgs{ winrt::make_self<Remoting::implementation::FindTargetWindowArgs>(args) };

            // This is handled by some handler in-proc
            _FindTargetWindowRequestedHandlers(*this, *findWindowArgs);
            // {
            //     const auto targetWindow = _appLogic.FindTargetWindow(findWindowArgs->Args().Commandline());
            //     findWindowArgs->ResultTargetWindow(targetWindow.WindowId());
            //     findWindowArgs->ResultTargetWindowName(targetWindow.WindowName());
            // }

            // After the event was handled, ResultTargetWindow() will be filled with
            // the parsed result.
            const auto targetWindow = findWindowArgs->ResultTargetWindow();
            const auto targetWindowName = findWindowArgs->ResultTargetWindowName();

            if (targetWindow == WindowingBehaviorUseNone)
            {
                // This commandline doesn't deserve a window. Don't make a monarch
                // either.
                shouldCreateWindow = false;

                return *winrt::make_self<ProposeCommandlineResult>(shouldCreateWindow);
                // return;
            }
            else
            {
                // This commandline _does_ want a window, which means we do want
                // to create a window, and a monarch.
                //
                // Congrats! This is now THE PROCESS. It's the only one that's
                // getting any windows.
                _registerAsMonarch();
                _createMonarch();
                if (!_monarch)
                {
                    // TODO! something catastrophically bad happened here.
                }

                // I don't _really_ think we need to propose this again. WE
                // basically did that work above. We've already gotten the
                // result -
                //  * we don't care about the name or ID, we want a new window.

                //Remoting::ProposeCommandlineResult result = _monarch.ProposeCommandline(args);

                // TODO! So, we wanted a new peasant. Cool!
                //
                // We need to fill in args.ResultTargetWindow,
                // args.ResultTargetWindowName so that we can create the new window
                // with those values.
                shouldCreateWindow = true;
                auto result = winrt::make_self<ProposeCommandlineResult>(shouldCreateWindow);
                result->Id();
                result->WindowName();
                return *result;
            }
        }
        else
        {
            // We connectecd to a monarch instance, not us though.

            shouldCreateWindow = false;
            std::optional<uint64_t> givenID;
            winrt::hstring givenName{};

            // Send the commandline over to the monarch process
            _proposeToMonarch(args, givenID, givenName);

            // Our job is done. Either the monarch is going to run the
            // commandline in an existing window, or a new one, but either way,
            // this process doesn't need to make a new window.

            return *winrt::make_self<ProposeCommandlineResult>(shouldCreateWindow);
        }
    }

    void WindowManager2::_proposeToMonarch(const Remoting::CommandlineArgs& args,
                                           std::optional<uint64_t>& /*givenID*/,
                                           winrt::hstring& /*givenName*/)
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
                    // TODO!
                    break;
                }
                // if (attempts >= 10)
                // {
                //     // We've tried 10 times to find the monarch, failing each
                //     // time. Since we have no idea why, we're guessing that in
                //     // this case, there's just a Monarch registered that's
                //     // misbehaving. In this case, just fall back to
                //     // "IsolatedMonarchMode" - we can't trust the currently
                //     // registered one.
                //     TraceLoggingWrite(g_hRemotingProvider,
                //                       "WindowManager_TooManyAttempts_NullMonarchIsolateMode",
                //                       TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                //                       TraceLoggingKeyword(TIL_KEYWORD_TRACE));

                //     _monarch = winrt::make<winrt::Microsoft::Terminal::Remoting::implementation::Monarch>();
                //     _createCallbacks();
                // }
                // else
                // {
                //     // We failed to ask the monarch. It must have died. Try and
                //     // find the real monarch. Don't perform an election, that
                //     // assumes we have a peasant, which we don't yet.
                //     _createMonarchAndCallbacks();
                //     // _createMonarchAndCallbacks will initialize _isKing
                // }
                // if (_isKing)
                // {
                //     // We became the king. We don't need to ProposeCommandline to ourself, we're just
                //     // going to do it.
                //     //
                //     // Return early, because there's nothing else for us to do here.
                //     TraceLoggingWrite(g_hRemotingProvider,
                //                       "WindowManager_proposeToMonarch_becameKing",
                //                       TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                //                       TraceLoggingKeyword(TIL_KEYWORD_TRACE));

                //     // In WindowManager::ProposeCommandline, had we been the
                //     // king originally, we would have started by setting
                //     // this to true. We became the monarch here, so set it
                //     // here as well.
                //     _shouldCreateWindow = true;
                //     return;
                // }

                // // Here, we created the new monarch, it wasn't us, so we're
                // // gonna go through the while loop again and ask the new
                // // king.
                // TraceLoggingWrite(g_hRemotingProvider,
                //                   "WindowManager_proposeToMonarch_tryAgain",
                //                   TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                //                   TraceLoggingKeyword(TIL_KEYWORD_TRACE));
            }
        }

        // // Here, the monarch (not us) has replied to the message. Get the
        // // valuables out of the response:
        // _shouldCreateWindow = result.ShouldCreateWindow();
        // if (result.Id())
        // {
        //     givenID = result.Id().Value();
        // }
        // givenName = result.WindowName();

        // // TraceLogging doesn't have a good solution for logging an
        // // optional. So we have to repeat the calls here:
        // if (givenID)
        // {
        //     TraceLoggingWrite(g_hRemotingProvider,
        //                       "WindowManager_ProposeCommandline",
        //                       TraceLoggingBoolean(_shouldCreateWindow, "CreateWindow", "true iff we should create a new window"),
        //                       TraceLoggingUInt64(givenID.value(), "Id", "The ID we should assign our peasant"),
        //                       TraceLoggingWideString(givenName.c_str(), "Name", "The name we should assign this window"),
        //                       TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
        //                       TraceLoggingKeyword(TIL_KEYWORD_TRACE));
        // }
        // else
        // {
        //     TraceLoggingWrite(g_hRemotingProvider,
        //                       "WindowManager_ProposeCommandline",
        //                       TraceLoggingBoolean(_shouldCreateWindow, "CreateWindow", "true iff we should create a new window"),
        //                       TraceLoggingPointer(nullptr, "Id", "No ID provided"),
        //                       TraceLoggingWideString(givenName.c_str(), "Name", "The name we should assign this window"),
        //                       TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
        //                       TraceLoggingKeyword(TIL_KEYWORD_TRACE));
        // }
    }

    Remoting::Peasant WindowManager2::CreateAPeasant(Remoting::WindowRequestedArgs args)
    {
        auto p = winrt::make_self<Remoting::implementation::Peasant>();
        if (args.Id())
        {
            p->AssignID(args.Id().Value());
        }

        // If the name wasn't specified, this will be an empty string.
        p->WindowName(args.WindowName());

        p->ExecuteCommandline(*winrt::make_self<CommandlineArgs>(args.Commandline(), args.CurrentDirectory()));

        _monarch.AddPeasant(*p);

        // TODO!
        // _peasant.GetWindowLayoutRequested({ get_weak(), &WindowManager::_GetWindowLayoutRequestedHandlers });

        TraceLoggingWrite(g_hRemotingProvider,
                          "WindowManager_CreateOurPeasant",
                          TraceLoggingUInt64(p->GetID(), "peasantID", "The ID of our new peasant"),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));

        // If the peasant asks us to quit we should not try to act in future elections.
        p->QuitRequested([weakThis{ get_weak() }](auto&&, auto&&) {
            // if (auto wm = weakThis.get())
            // {
            //     wm->_monarchWaitInterrupt.SetEvent();
            // }
        });

        return *p;
    }

    void WindowManager2::SignalClose(Remoting::Peasant peasant)
    {
        if (_monarch)
        {
            try
            {
                _monarch.SignalClose(peasant.GetID());
            }
            CATCH_LOG()
        }
    }

    void WindowManager2::SummonWindow(const Remoting::SummonWindowSelectionArgs& args)
    {
        // We should only ever get called when we are the monarch, because only
        // the monarch ever registers for the global hotkey. So the monarch is
        // the only window that will be calling this.
        _monarch.SummonWindow(args);
    }

    void WindowManager2::SummonAllWindows()
    {
        _monarch.SummonAllWindows();
    }

    Windows::Foundation::Collections::IVectorView<winrt::Microsoft::Terminal::Remoting::PeasantInfo> WindowManager2::GetPeasantInfos()
    {
        // We should only get called when we're the monarch since the monarch
        // is the only one that knows about all peasants.
        return _monarch.GetPeasantInfos();
    }

    uint64_t WindowManager2::GetNumberOfPeasants()
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
    winrt::fire_and_forget WindowManager2::RequestShowNotificationIcon(Remoting::Peasant peasant)
    {
        co_await winrt::resume_background();
        peasant.RequestShowNotificationIcon();
    }

    // Method Description:
    // - Ask the monarch to hide its notification icon.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    winrt::fire_and_forget WindowManager2::RequestHideNotificationIcon(Remoting::Peasant peasant)
    {
        auto strongThis{ get_strong() };
        co_await winrt::resume_background();
        peasant.RequestHideNotificationIcon();
    }

    // Method Description:
    // - Ask the monarch to quit all windows.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    winrt::fire_and_forget WindowManager2::RequestQuitAll(Remoting::Peasant peasant)
    {
        auto strongThis{ get_strong() };
        co_await winrt::resume_background();
        peasant.RequestQuitAll();
    }

    bool WindowManager2::DoesQuakeWindowExist()
    {
        return _monarch.DoesQuakeWindowExist();
    }

    void WindowManager2::UpdateActiveTabTitle(winrt::hstring title, Remoting::Peasant peasant)
    {
        winrt::get_self<implementation::Peasant>(peasant)->ActiveTabTitle(title);
    }

    Windows::Foundation::Collections::IVector<winrt::hstring> WindowManager2::GetAllWindowLayouts()
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
