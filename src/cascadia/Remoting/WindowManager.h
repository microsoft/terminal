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

namespace winrt::Microsoft::Terminal::Remoting::implementation
{
    struct WindowManager : public WindowManagerT<WindowManager>
    {
        WindowManager();
        ~WindowManager();

        void ProposeCommandline(const winrt::Microsoft::Terminal::Remoting::CommandlineArgs& args);
        bool ShouldCreateWindow();

        winrt::Microsoft::Terminal::Remoting::Peasant CurrentWindow();
        bool IsMonarch();
        void SummonWindow(const Remoting::SummonWindowSelectionArgs& args);
        void SignalClose();

        void SummonAllWindows();
        uint64_t GetNumberOfPeasants();
        Windows::Foundation::Collections::IVectorView<winrt::Microsoft::Terminal::Remoting::PeasantInfo> GetPeasantInfos();

        winrt::fire_and_forget RequestShowNotificationIcon();
        winrt::fire_and_forget RequestHideNotificationIcon();
        winrt::fire_and_forget RequestQuitAll();
        bool DoesQuakeWindowExist();
        void UpdateActiveTabTitle(winrt::hstring title);
        Windows::Foundation::Collections::IVector<winrt::hstring> GetAllWindowLayouts();

        TYPED_EVENT(FindTargetWindowRequested, winrt::Windows::Foundation::IInspectable, winrt::Microsoft::Terminal::Remoting::FindTargetWindowArgs);
        TYPED_EVENT(BecameMonarch, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);
        TYPED_EVENT(WindowCreated, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);
        TYPED_EVENT(WindowClosed, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);
        TYPED_EVENT(ShowNotificationIconRequested, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);
        TYPED_EVENT(HideNotificationIconRequested, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);
        TYPED_EVENT(QuitAllRequested, winrt::Windows::Foundation::IInspectable, winrt::Microsoft::Terminal::Remoting::QuitAllRequestedArgs);
        TYPED_EVENT(GetWindowLayoutRequested, winrt::Windows::Foundation::IInspectable, winrt::Microsoft::Terminal::Remoting::GetWindowLayoutArgs);

    private:
        bool _shouldCreateWindow{ false };
        bool _isKing{ false };
        DWORD _registrationHostClass{ 0 };
        winrt::Microsoft::Terminal::Remoting::IMonarch _monarch{ nullptr };
        winrt::Microsoft::Terminal::Remoting::Peasant _peasant{ nullptr };

        wil::unique_event _monarchWaitInterrupt;
        std::thread _electionThread;

        void _registerAsMonarch();
        void _createMonarch();
        void _redundantCreateMonarch();
        void _createMonarchAndCallbacks();
        void _createCallbacks();
        bool _areWeTheKing();
        winrt::Microsoft::Terminal::Remoting::IPeasant _createOurPeasant(std::optional<uint64_t> givenID,
                                                                         const winrt::hstring& givenName);

        bool _performElection();
        void _createPeasantThread();
        void _waitOnMonarchThread();
        void _raiseFindTargetWindowRequested(const winrt::Windows::Foundation::IInspectable& sender,
                                             const winrt::Microsoft::Terminal::Remoting::FindTargetWindowArgs& args);

        void _proposeToMonarch(const Remoting::CommandlineArgs& args,
                               std::optional<uint64_t>& givenID,
                               winrt::hstring& givenName);
    };
}

namespace winrt::Microsoft::Terminal::Remoting::factory_implementation
{
    BASIC_FACTORY(WindowManager);
}
