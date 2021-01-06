// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

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

        // Don't do this, the monarch can and will change over time
        // FORWARDED_TYPED_EVENT(FindTargetWindowRequested, winrt::Windows::Foundation::IInspectable, winrt::Microsoft::Terminal::Remoting::FindTargetWindowArgs, _monarch, FindTargetWindowRequested);
        TYPED_EVENT(FindTargetWindowRequested, winrt::Windows::Foundation::IInspectable, winrt::Microsoft::Terminal::Remoting::FindTargetWindowArgs);

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
        void _raiseFindTargetWindowRequested(const winrt::Windows::Foundation::IInspectable& sender,
                                             const winrt::Microsoft::Terminal::Remoting::FindTargetWindowArgs& args);
    };
}

namespace winrt::Microsoft::Terminal::Remoting::factory_implementation
{
    BASIC_FACTORY(WindowManager);
}
