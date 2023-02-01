
#pragma once

#include "WindowManager2.g.h"
#include "Peasant.h"
#include "Monarch.h"

namespace winrt::Microsoft::Terminal::Remoting::implementation
{
    struct WindowManager2 : public WindowManager2T<WindowManager2>
    {
    public:
        WindowManager2();
        ~WindowManager2();
        winrt::Microsoft::Terminal::Remoting::ProposeCommandlineResult ProposeCommandline2(const winrt::Microsoft::Terminal::Remoting::CommandlineArgs& args);
        Remoting::Peasant CreateAPeasant(Remoting::WindowRequestedArgs args);

        void SignalClose(Remoting::Peasant peasant);
        void SummonWindow(const Remoting::SummonWindowSelectionArgs& args);
        void SummonAllWindows();
        Windows::Foundation::Collections::IVectorView<winrt::Microsoft::Terminal::Remoting::PeasantInfo> GetPeasantInfos();
        uint64_t GetNumberOfPeasants();
        winrt::fire_and_forget RequestShowNotificationIcon(Remoting::Peasant peasant);
        winrt::fire_and_forget RequestHideNotificationIcon(Remoting::Peasant peasant);
        winrt::fire_and_forget RequestQuitAll(Remoting::Peasant peasant);
        void UpdateActiveTabTitle(winrt::hstring title, Remoting::Peasant peasant);
        Windows::Foundation::Collections::IVector<winrt::hstring> GetAllWindowLayouts();
        bool DoesQuakeWindowExist();

        TYPED_EVENT(FindTargetWindowRequested, winrt::Windows::Foundation::IInspectable, winrt::Microsoft::Terminal::Remoting::FindTargetWindowArgs);

        TYPED_EVENT(WindowCreated, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);
        TYPED_EVENT(WindowClosed, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);
        TYPED_EVENT(ShowNotificationIconRequested, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);
        TYPED_EVENT(HideNotificationIconRequested, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);
        TYPED_EVENT(QuitAllRequested, winrt::Windows::Foundation::IInspectable, winrt::Microsoft::Terminal::Remoting::QuitAllRequestedArgs);
        TYPED_EVENT(GetWindowLayoutRequested, winrt::Windows::Foundation::IInspectable, winrt::Microsoft::Terminal::Remoting::GetWindowLayoutArgs);

        TYPED_EVENT(RequestNewWindow, winrt::Windows::Foundation::IInspectable, winrt::Microsoft::Terminal::Remoting::WindowRequestedArgs);

    private:
        DWORD _registrationHostClass{ 0 };
        winrt::Microsoft::Terminal::Remoting::IMonarch _monarch{ nullptr };

        void _createMonarch();
        void _registerAsMonarch();

        void _proposeToMonarch(const Remoting::CommandlineArgs& args,
                               std::optional<uint64_t>& givenID,
                               winrt::hstring& givenName);

        void _createCallbacks();
        void _raiseFindTargetWindowRequested(const winrt::Windows::Foundation::IInspectable& sender,
                                             const winrt::Microsoft::Terminal::Remoting::FindTargetWindowArgs& args);
        void _raiseRequestNewWindow(const winrt::Windows::Foundation::IInspectable& sender,
                                    const winrt::Microsoft::Terminal::Remoting::WindowRequestedArgs& args);
    };
}

namespace winrt::Microsoft::Terminal::Remoting::factory_implementation
{
    BASIC_FACTORY(WindowManager2);
}
