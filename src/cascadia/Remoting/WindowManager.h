/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Class Name:
- WindowManager.h

Abstract:
- The Window Manager takes care of coordinating the monarch and peasant for this
  process.
- It's responsible for registering as a potential future monarch. It's also
  responsible for creating the Peasant for this process when it's determined
  this process should become a window process.
- If we aren't the monarch, it's responsible for watching the current monarch
  process, and finding the new one if the current monarch dies.
- When the monarch needs to ask the TerminalApp about how to parse a
  commandline, it'll ask by raising an event that we'll bubble up to the
  AppHost.

--*/

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
        bool IsMonarch();
        void SummonWindow(const Remoting::SummonWindowSelectionArgs& args);

        TYPED_EVENT(FindTargetWindowRequested, winrt::Windows::Foundation::IInspectable, winrt::Microsoft::Terminal::Remoting::FindTargetWindowArgs);
        TYPED_EVENT(BecameMonarch, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);

    private:
        bool _shouldCreateWindow{ false };
        bool _isKing{ false };
        DWORD _registrationHostClass{ 0 };
        winrt::Microsoft::Terminal::Remoting::Monarch _monarch{ nullptr };
        winrt::Microsoft::Terminal::Remoting::Peasant _peasant{ nullptr };

        wil::unique_event _monarchWaitInterrupt;
        std::thread _electionThread;

        void _registerAsMonarch();
        void _createMonarch();
        void _createMonarchAndCallbacks();
        bool _areWeTheKing();
        winrt::Microsoft::Terminal::Remoting::IPeasant _createOurPeasant(std::optional<uint64_t> givenID,
                                                                         const winrt::hstring& givenName);

        bool _performElection();
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
