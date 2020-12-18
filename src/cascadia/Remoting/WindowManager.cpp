#include "pch.h"
#include "WindowManager.h"
#include "MonarchFactory.h"
#include "CommandlineArgs.h"

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
        // _peasantListenerInterrupt.create();

        // wil::unique_event peasantListenerInterrupt;
        // peasantListenerInterrupt.create();
        // _peasantHandles.emplace_back(std::move(peasantListenerInterrupt));

        // Register with COM as a server for the Monarch class
        _registerAsMonarch();
        // Instantiate an instance of the Monarch. This may or may not be in-proc!
        _createMonarchAndCallbacks();
    }

    WindowManager::~WindowManager()
    {
        // IMPORTANT! Tear down the registration as soon as we exit. If we're not a
        // real peasant window (the monarch passed our commandline to someone else),
        // then the monarch dies, we don't want our registration becoming the active
        // monarch!
        CoRevokeClassObject(_registrationHostClass);
        _registrationHostClass = 0;
        _monarchWaitInterrupt.SetEvent();
        // _peasantListenerInterrupt.SetEvent();
        if (_electionThread.joinable())
        {
            _electionThread.join();
        }
        // if (_peasantListenerThread.joinable())
        // {
        //     _peasantListenerThread.join();
        // }
    }

    void WindowManager::ProposeCommandline(const Remoting::CommandlineArgs& args)
    {
        const bool isKing = _areWeTheKing();
        // If we're the king, we _definitely_ want to process the arguments, we were
        // launched with them!
        //
        // Otherwise, the King will tell us if we should make a new window
        _shouldCreateWindow = isKing ||
                              _monarch.ProposeCommandline(args);

        if (_shouldCreateWindow)
        {
            // If we should create a new window, then instantiate our Peasant
            // instance, and tell that peasant to handle that commandline.
            _createOurPeasant();

            // TODO:MG Spawn a thread to wait on the monarch, and handle the election
            if (!isKing)
            {
                _createPeasantThread();
            }

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
        // "metadata-based-marshalling" for our WinRT types. THat means the OS is
        // using the .winmd file we generate to figure out the proxy/stub
        // definitions for our types automatically. This only works in the following
        // cases:
        //
        // * If we're running unpackaged: the .winmd must be a sibling of the .exe
        // * If we're running packaged: the .winmd must be in the package root
        _monarch = create_instance<Remoting::Monarch>(Monarch_clsid,
                                                      CLSCTX_LOCAL_SERVER);
    }

    void WindowManager::_createMonarchAndCallbacks()
    {
        _createMonarch();
        const auto isKing = _areWeTheKing();
        if (!isKing)
        {
            return;
        }
        // Here, we're the king!

        // TODO:MG Add an even handler to the monarch's PeasantAdded event.
        // We'll use that callback as a chance to start waiting on the peasant's
        // PID. If they die, we'll tell the monarch to remove them from the
        // list.
        // _peasantHandles.emplace_back(_peasantListenerInterrupt.get());

        // _peasantListenerThread = std::thread([this]() {

        //     bool exitRequested = false;
        //     while (!exitRequested)
        //     {
        //     }
        // });

        // Wait, don't. Let's just have the monarch try/catch any accesses to
        // peasants. If the peasant dies, then it can't get the peasant's
        // anything. In that case, _remove it_.
    }

    bool WindowManager::_areWeTheKing()
    {
        auto kingPID = _monarch.GetPID();
        auto ourPID = GetCurrentProcessId();
        return (ourPID == kingPID);
    }

    Remoting::IPeasant WindowManager::_createOurPeasant()
    {
        auto p = winrt::make_self<Remoting::implementation::Peasant>();
        _peasant = *p;
        _monarch.AddPeasant(_peasant);

        return _peasant;
    }

    bool WindowManager::_electionNight2020()
    {
        _createMonarchAndCallbacks();
        if (_areWeTheKing())
        {
            return true;
        }
        return false;
    }

    void WindowManager::_createPeasantThread()
    {
        // If we catch an exception trying to get at the monarch ever, we can
        // set the _monarchWaitInterrupt, and use that to trigger a new
        // election. Though, we wouldn't be able to retry the function that
        // caused the exception in the first place...

        _electionThread = std::thread([this] {
            HANDLE waits[2];
            waits[1] = _monarchWaitInterrupt.get();

            bool exitRequested = false;
            while (!exitRequested)
            {
                wil::unique_handle hMonarch{ OpenProcess(PROCESS_ALL_ACCESS, FALSE, static_cast<DWORD>(_monarch.GetPID())) };
                // TODO:MG If we fail to open the monarch, then they don't exist
                //  anymore! Go straight to an election.
                //
                // if (hMonarch.get() == nullptr)
                // {
                //     const auto gle = GetLastError();
                //     return false;
                // }
                waits[0] = hMonarch.get();
                auto waitResult = WaitForMultipleObjects(2, waits, FALSE, INFINITE);

                switch (waitResult)
                {
                case WAIT_OBJECT_0 + 0: // waits[0] was signaled
                    // Connect to the new monarch, which might be us!
                    // If we become the monarch, then we'll return true and exit this thread.
                    exitRequested = _electionNight2020();
                    break;
                case WAIT_OBJECT_0 + 1: // waits[1] was signaled
                    exitRequested = true;
                    break;

                case WAIT_TIMEOUT:
                    printf("Wait timed out. This should be impossible.\n");
                    exitRequested = true;
                    break;

                // Return value is invalid.
                default:
                {
                    auto gle = GetLastError();
                    printf("WaitForMultipleObjects returned: %d\n", waitResult);
                    printf("Wait error: %d\n", gle);
                    ExitProcess(0);
                }
                }
            }
        });
    }

    Remoting::Peasant WindowManager::CurrentWindow()
    {
        return _peasant;
    }

}
