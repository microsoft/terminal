#pragma once

#include "WindowManager.g.h"
#include "Peasant.h"
#include "Monarch.h"
#include "../cascadia/inc/cppwinrt_utils.h"

namespace winrt::Microsoft::Terminal::Remoting::implementation
{
    struct WindowManager final : public WindowManagerT<WindowManager>
    {
        WindowManager();
        ~WindowManager();

        void ProposeCommandline(const winrt::Microsoft::Terminal::Remoting::CommandlineArgs& args);
        bool ShouldCreateWindow();

        winrt::Microsoft::Terminal::Remoting::Peasant CurrentWindow();

    private:
        bool _shouldCreateWindow{ false };
        DWORD _registrationHostClass{ 0 };
        winrt::Microsoft::Terminal::Remoting::Monarch _monarch{ nullptr };
        winrt::Microsoft::Terminal::Remoting::Peasant _peasant{ nullptr };

        wil::unique_event _monarchWaitInterrupt;

        std::thread _electionThread;

        void _registerAsMonarch();
        void _createMonarch();
        void _createMonarchAndCallbacks();
        bool _areWeTheKing();
        winrt::Microsoft::Terminal::Remoting::IPeasant _createOurPeasant();

        bool _electionNight2020();
        void _createPeasantThread();
        void _waitOnMonarchThread();
    };
}

namespace winrt::Microsoft::Terminal::Remoting::factory_implementation
{
    BASIC_FACTORY(WindowManager);
}
