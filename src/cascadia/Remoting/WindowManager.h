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
    public:
        WindowManager();
        ~WindowManager();
        winrt::Microsoft::Terminal::Remoting::ProposeCommandlineResult ProposeCommandline(const winrt::Microsoft::Terminal::Remoting::CommandlineArgs& args, const bool isolatedMode);
        Remoting::Peasant CreatePeasant(const Remoting::WindowRequestedArgs& args);

        void SignalClose(const Remoting::Peasant& peasant);
        void SummonWindow(const Remoting::SummonWindowSelectionArgs& args);
        void SummonAllWindows();
        Windows::Foundation::Collections::IVectorView<winrt::Microsoft::Terminal::Remoting::PeasantInfo> GetPeasantInfos();

        uint64_t GetNumberOfPeasants();

        void UpdateActiveTabTitle(const winrt::hstring& title, const Remoting::Peasant& peasant);

        bool DoesQuakeWindowExist();

        winrt::fire_and_forget RequestMoveContent(winrt::hstring window, winrt::hstring content, uint32_t tabIndex, Windows::Foundation::IReference<Windows::Foundation::Rect> windowBounds);
        winrt::fire_and_forget RequestSendContent(Remoting::RequestReceiveContentArgs args);

        til::typed_event<winrt::Windows::Foundation::IInspectable, winrt::Microsoft::Terminal::Remoting::FindTargetWindowArgs> FindTargetWindowRequested;

        til::typed_event<> WindowCreated;
        til::typed_event<> WindowClosed;
        til::typed_event<winrt::Windows::Foundation::IInspectable, winrt::Microsoft::Terminal::Remoting::WindowRequestedArgs> RequestNewWindow;

    private:
        DWORD _registrationHostClass{ 0 };
        winrt::Microsoft::Terminal::Remoting::IMonarch _monarch{ nullptr };

        void _createMonarch();
        void _registerAsMonarch();

        bool _proposeToMonarch(const Remoting::CommandlineArgs& args);

        void _createCallbacks();
        void _raiseFindTargetWindowRequested(const winrt::Windows::Foundation::IInspectable& sender,
                                             const winrt::Microsoft::Terminal::Remoting::FindTargetWindowArgs& args);
        void _raiseRequestNewWindow(const winrt::Windows::Foundation::IInspectable& sender,
                                    const winrt::Microsoft::Terminal::Remoting::WindowRequestedArgs& args);
        void _bubbleWindowCreated(const winrt::Windows::Foundation::IInspectable& s, const winrt::Windows::Foundation::IInspectable& e);
        void _bubbleWindowClosed(const winrt::Windows::Foundation::IInspectable& s, const winrt::Windows::Foundation::IInspectable& e);
    };
}

namespace winrt::Microsoft::Terminal::Remoting::factory_implementation
{
    BASIC_FACTORY(WindowManager);
}
