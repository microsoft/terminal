// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "WindowManager.h"

#include "../inc/WindowingBehavior.h"
#include "MonarchFactory.h"

#include "CommandlineArgs.h"
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
    }
    WindowManager::~WindowManager()
    {
        // IMPORTANT! Tear down the registration as soon as we exit. If we're not a
        // real peasant window (the monarch passed our commandline to someone else),
        // then the monarch dies, we don't want our registration becoming the active
        // monarch!
        CoRevokeClassObject(_registrationHostClass);
        _registrationHostClass = 0;
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
        _monarch = try_create_instance<Remoting::IMonarch>(Monarch_clsid,
                                                           CLSCTX_LOCAL_SERVER);
    }

    // Check if we became the king, and if we are, wire up callbacks.
    void WindowManager::_createCallbacks()
    {
        assert(_monarch);
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

        _monarch.RequestNewWindow({ get_weak(), &WindowManager::_raiseRequestNewWindow });
    }

    void WindowManager::_registerAsMonarch()
    {
        winrt::check_hresult(CoRegisterClassObject(Monarch_clsid,
                                                   winrt::make<::MonarchFactory>().get(),
                                                   CLSCTX_LOCAL_SERVER,
                                                   REGCLS_MULTIPLEUSE,
                                                   &_registrationHostClass));
    }

    void WindowManager::_raiseFindTargetWindowRequested(const winrt::Windows::Foundation::IInspectable& sender,
                                                        const winrt::Microsoft::Terminal::Remoting::FindTargetWindowArgs& args)
    {
        _FindTargetWindowRequestedHandlers(sender, args);
    }
    void WindowManager::_raiseRequestNewWindow(const winrt::Windows::Foundation::IInspectable& sender,
                                               const winrt::Microsoft::Terminal::Remoting::WindowRequestedArgs& args)
    {
        _RequestNewWindowHandlers(sender, args);
    }

    Remoting::ProposeCommandlineResult WindowManager::ProposeCommandline2(const Remoting::CommandlineArgs& args)
    {
        bool shouldCreateWindow = false;

        _createMonarch();
        if (_monarch)
        {
            // We connectecd to a monarch instance, not us though.

            shouldCreateWindow = false;
            std::optional<uint64_t> givenID;
            winrt::hstring givenName{};

            // Send the commandline over to the monarch process
            if (_proposeToMonarch(args, givenID, givenName))
            {
                // If that succeeded, then we don't need to make a new window.
                // Our job is done. Either the monarch is going to run the
                // commandline in an existing window, or a new one, but either way,
                // this process doesn't need to make a new window.

                return *winrt::make_self<ProposeCommandlineResult>(shouldCreateWindow);
            }
            // Otherwise, we'll try to handle this ourselves.
        }

        // Theoretically, this condition is always true here:
        //
        // if (_monarch == nullptr)
        //
        // If we do still have a _monarch at this point, then we must have
        // successfully proposed to it in _proposeToMonarch, so we can't get
        // here with a monarch.
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
                    // Something catastrophically bad happened here. But we
                    // don't want to just exit immediately. No, instead, we'll
                    // just instantiate a local Monarch instance. We're firmly
                    // in the realm of undefined behavior, but better to have
                    // some window than not.
                    _monarch = winrt::make<Monarch>();
                    TraceLoggingWrite(g_hRemotingProvider,
                                      "WindowManager_FaileToCoCreate",
                                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                      TraceLoggingKeyword(TIL_KEYWORD_TRACE));
                }
                _createCallbacks();

                // So, we wanted a new peasant. Cool!
                //
                // We need to fill in args.ResultTargetWindow,
                // args.ResultTargetWindowName so that we can create the new
                // window with those values. Otherwise, the very first window
                // won't obey the given name / ID.
                //
                // So let's just ask the monarch (ourselves) to get those values.
                return _monarch.ProposeCommandline(args);
            }
        }
    }

    // Method Description:
    // - Helper attempting to call to the monarch multiple times. If the monarch
    //   fails to respond, or we encounter any sort of error, we'll try again
    //   until we find one, or decisively determine there isn't one.
    bool WindowManager::_proposeToMonarch(const Remoting::CommandlineArgs& args,
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

                _monarch.ProposeCommandline(args);
                return true;
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

                    // Set the monarch to null, so that we'll create a new one
                    // (or just generally check if we need to even make a window
                    // for this commandline.)
                    _monarch = nullptr;
                    return false;
                }
                else
                {
                    // We failed to ask the monarch. It must have died. Try and
                    // find another monarch.
                    _createMonarch();
                    if (!_monarch)
                    {
                        // We failed to create a monarch. That means there
                        // aren't any other windows, and we can become the monarch.
                        return false;
                    }
                    // Go back around the loop.
                    TraceLoggingWrite(g_hRemotingProvider,
                                      "WindowManager_proposeToMonarch_tryAgain",
                                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                      TraceLoggingKeyword(TIL_KEYWORD_TRACE));
                }
            }
        }

        // I don't think we can ever get here, but the compiler doesn't know
        return false;
    }

    Remoting::Peasant WindowManager::CreateAPeasant(Remoting::WindowRequestedArgs args)
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

        p->GetWindowLayoutRequested({ get_weak(), &WindowManager::_GetWindowLayoutRequestedHandlers });

        TraceLoggingWrite(g_hRemotingProvider,
                          "WindowManager_CreateOurPeasant",
                          TraceLoggingUInt64(p->GetID(), "peasantID", "The ID of our new peasant"),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));

        return *p;
    }

    void WindowManager::SignalClose(Remoting::Peasant peasant)
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
    winrt::fire_and_forget WindowManager::RequestShowNotificationIcon(Remoting::Peasant peasant)
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
    winrt::fire_and_forget WindowManager::RequestHideNotificationIcon(Remoting::Peasant peasant)
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
    winrt::fire_and_forget WindowManager::RequestQuitAll(Remoting::Peasant peasant)
    {
        auto strongThis{ get_strong() };
        co_await winrt::resume_background();
        peasant.RequestQuitAll();
    }

    bool WindowManager::DoesQuakeWindowExist()
    {
        return _monarch.DoesQuakeWindowExist();
    }

    void WindowManager::UpdateActiveTabTitle(winrt::hstring title, Remoting::Peasant peasant)
    {
        winrt::get_self<implementation::Peasant>(peasant)->ActiveTabTitle(title);
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

    winrt::fire_and_forget WindowManager::RequestMoveContent(winrt::hstring window,
                                                             winrt::hstring content,
                                                             uint32_t tabIndex)
    {
        co_await winrt::resume_background();
        _monarch.RequestMoveContent(window, content, tabIndex);
    }

}
