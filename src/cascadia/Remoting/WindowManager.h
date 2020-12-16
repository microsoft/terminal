#pragma once

#include "WindowManager.g.h"
#include "Peasant.h"
#include "Monarch.h"
#include "../cascadia/inc/cppwinrt_utils.h"

namespace winrt::Microsoft::Terminal::Remoting::implementation
{
    struct WindowManager : public WindowManagerT<WindowManager>
    {
        WindowManager();
        ~WindowManager();

        void ProposeCommandline();
        bool ShouldCreateWindow();

    private:
        bool _shouldCreateWindow{ false };
        DWORD _registrationHostClass{ 0 };
        winrt::Microsoft::Terminal::Remoting::Monarch _monarch{ nullptr };

        void _RegisterAsMonarch();
        void _CreateMonarch();
    };
}

namespace winrt::Microsoft::Terminal::Remoting::factory_implementation
{
    BASIC_FACTORY(WindowManager);
}
