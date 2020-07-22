#pragma once

#include <winrt/ScratchWinRTServer.h>
#include "HostAndProcess.g.h"

namespace winrt::ScratchIsland::implementation
{
    struct HostAndProcess : public HostAndProcessT<HostAndProcess>
    {
        HostAndProcess(ScratchWinRTServer::HostClass host, wil::unique_process_information pi);

        ScratchWinRTServer::HostClass Host() const;

        void CreateSwapChain(Windows::UI::Xaml::Controls::SwapChainPanel panel);

    private:
        ScratchWinRTServer::HostClass _host{ nullptr };
        wil::unique_process_information _pi;
        wil::unique_handle _hOurSwapchain{ INVALID_HANDLE_VALUE };
        wil::unique_handle _hTheirSwapchain{ INVALID_HANDLE_VALUE };
    };
}

namespace winrt::ScratchIsland::factory_implementation
{
    // struct HostAndProcess : HostAndProcessT<HostAndProcess, implementation::HostAndProcess>
    // {
    // };
}
