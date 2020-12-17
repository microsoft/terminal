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
        // Register with COM as a server for the Monarch class
        _registerAsMonarch();
        // Instantiate an instance of the Monarch. This may or may not be in-proc!
        _createMonarch();
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

    void WindowManager::ProposeCommandline(array_view<const winrt::hstring> args,
                                           const winrt::hstring cwd)
    {
        const bool isKing = _areWeTheKing();
        // If we're the king, we _definitely_ want to process the arguments, we were
        // launched with them!
        //
        // Otherwise, the King will tell us if we should make a new window
        _shouldCreateWindow = isKing ||
                              _monarch.ProposeCommandline(args, cwd);

        if (_shouldCreateWindow)
        {
            // If we should create a new window, then instantiate our Peasant
            // instance, and tell that peasant to handle that commandline.
            _createOurPeasant();

            auto eventArgs = winrt::make_self<implementation::CommandlineArgs>(args, cwd);
            _peasant.ExecuteCommandline(*eventArgs);
        }
        // Othersize, we'll do _nothing_.
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
        // * If we're running unpackaged: the .winmd but be a sibling of the .exe
        // * If we're running packaged: the .winmd must be in the package root
        _monarch = create_instance<Remoting::Monarch>(Monarch_clsid,
                                                      CLSCTX_LOCAL_SERVER);
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

        // TODO:MG Spawn a thread to wait on the monarch, and handle the election

        return _peasant;
    }

    Remoting::Peasant WindowManager::CurrentWindow()
    {
        return _peasant;
    }

}
