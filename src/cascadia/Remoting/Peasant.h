// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Peasant.g.h"
#include "RenameRequestArgs.h"

namespace RemotingUnitTests
{
    class RemotingTests;
};
namespace winrt::Microsoft::Terminal::Remoting::implementation
{
    struct Peasant : public PeasantT<Peasant>
    {
        Peasant();

        void AssignID(uint64_t id);
        uint64_t GetID();
        uint64_t GetPID();

        bool ExecuteCommandline(const winrt::Microsoft::Terminal::Remoting::CommandlineArgs& args);
        void ActivateWindow(const winrt::Microsoft::Terminal::Remoting::WindowActivatedArgs& args);

        void Summon(const Remoting::SummonWindowBehavior& summonBehavior);
        void RequestIdentifyWindows();
        void DisplayWindowId();
        void RequestRename(const winrt::Microsoft::Terminal::Remoting::RenameRequestArgs& args);
        void RequestShowNotificationIcon();
        void RequestHideNotificationIcon();
        void RequestQuitAll();
        void Quit();

        winrt::Microsoft::Terminal::Remoting::WindowActivatedArgs GetLastActivatedArgs();

        winrt::Microsoft::Terminal::Remoting::CommandlineArgs InitialArgs();

        winrt::hstring GetWindowLayout();

        WINRT_PROPERTY(winrt::hstring, WindowName);
        WINRT_PROPERTY(winrt::hstring, ActiveTabTitle);

        TYPED_EVENT(WindowActivated, winrt::Windows::Foundation::IInspectable, winrt::Microsoft::Terminal::Remoting::WindowActivatedArgs);
        TYPED_EVENT(ExecuteCommandlineRequested, winrt::Windows::Foundation::IInspectable, winrt::Microsoft::Terminal::Remoting::CommandlineArgs);
        TYPED_EVENT(IdentifyWindowsRequested, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);
        TYPED_EVENT(DisplayWindowIdRequested, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);
        TYPED_EVENT(RenameRequested, winrt::Windows::Foundation::IInspectable, winrt::Microsoft::Terminal::Remoting::RenameRequestArgs);
        TYPED_EVENT(SummonRequested, winrt::Windows::Foundation::IInspectable, winrt::Microsoft::Terminal::Remoting::SummonWindowBehavior);
        TYPED_EVENT(ShowNotificationIconRequested, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);
        TYPED_EVENT(HideNotificationIconRequested, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);
        TYPED_EVENT(QuitAllRequested, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);
        TYPED_EVENT(QuitRequested, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);
        TYPED_EVENT(GetWindowLayoutRequested, winrt::Windows::Foundation::IInspectable, winrt::Microsoft::Terminal::Remoting::GetWindowLayoutArgs);

    private:
        Peasant(const uint64_t testPID);
        uint64_t _ourPID;

        uint64_t _id{ 0 };

        winrt::Microsoft::Terminal::Remoting::CommandlineArgs _initialArgs{ nullptr };
        winrt::Microsoft::Terminal::Remoting::WindowActivatedArgs _lastActivatedArgs{ nullptr };

        friend class RemotingUnitTests::RemotingTests;
    };
}

namespace winrt::Microsoft::Terminal::Remoting::factory_implementation
{
    BASIC_FACTORY(Peasant);
}
