#include "pch.h"
#include "WindowManager.h"
#include "MonarchFactory.h"

#include "WindowManager.g.cpp"
#include "../../types/inc/utils.hpp"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace ::Microsoft::Console;

namespace winrt::Microsoft::Terminal::Remoting::implementation
{
    WindowManager::WindowManager()
    {
        _RegisterAsMonarch();
        _CreateMonarch();
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

    void WindowManager::ProposeCommandline()
    {
        _shouldCreateWindow = true;
    }

    bool WindowManager::ShouldCreateWindow()
    {
        return _shouldCreateWindow;
    }

    void WindowManager::_RegisterAsMonarch()
    {
        winrt::check_hresult(CoRegisterClassObject(Monarch_clsid,
                                                   winrt::make<::MonarchFactory>().get(),
                                                   CLSCTX_LOCAL_SERVER,
                                                   REGCLS_MULTIPLEUSE,
                                                   &_registrationHostClass));
    }

    void WindowManager::_CreateMonarch()
    {
        // Heads up! This only works because we're using
        // "metadata-based-marshalling" for our WinRT types. THat means the OS is
        // using the .winmd file we generate to figure out the proxy/stub
        // definitions for our types automatically. This only works in the following
        // cases:
        //
        // * If we're running unpackaged: the .winmd but be a sibling of the .exe
        // * If we're running packaged: the .winmd must be in the package root
        _monarch = create_instance<winrt::Microsoft::Terminal::Remoting::Monarch>(Monarch_clsid,
                                                                                  CLSCTX_LOCAL_SERVER);
    }

    // bool AppHost::_ProposeCommandlineToMonarch()
    // {
    //     // returns true if we should create a new window
    //     return true;
    // }
}
