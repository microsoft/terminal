#pragma once

#include "HostClass.g.h"
#include "IMyComInterface.h"

namespace winrt::ScratchWinRTServer::implementation
{
    struct HostClass : HostClassT<HostClass, IMyComInterface>
    {
        HostClass(const winrt::guid& g);
        ~HostClass();
        void DoTheThing();

        int DoCount();
        winrt::guid Id();

        HRESULT __stdcall Call() override;

        void Attach(const Windows::UI::Xaml::Controls::SwapChainPanel& panel);
        void BeginRendering();

    private:
        int _DoCount{ 0 };
        winrt::guid _id;

        Windows::UI::Xaml::Controls::SwapChainPanel _panel{ nullptr };
    };
}

namespace winrt::ScratchWinRTServer::factory_implementation
{
    struct HostClass : HostClassT<HostClass, implementation::HostClass>
    {
    };
}
