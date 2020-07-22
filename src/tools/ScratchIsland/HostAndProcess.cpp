#include "pch.h"
#include "HostAndProcess.h"

#include "HostAndProcess.g.cpp"
#include "../../types/inc/utils.hpp"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace ::Microsoft::Console;

namespace winrt::ScratchIsland::implementation
{
    HostAndProcess::HostAndProcess(ScratchWinRTServer::HostClass host,
                                   wil::unique_process_information pi /*, wil::unique_handle hSwapchain*/) :
        _host{ host },
        _pi{ std::move(pi) } /*,
        _hOurSwapchain {std::move(hSwapchain)}*/
    {
    }

    ScratchWinRTServer::HostClass HostAndProcess::Host() const
    {
        return _host;
    }

    void HostAndProcess::CreateSwapChain(winrt::Windows::UI::Xaml::Controls::SwapChainPanel panel)
    {
        // wil::unique_handle _swapChainHandle;
        DCompositionCreateSurfaceHandle(GENERIC_ALL, nullptr, &_hOurSwapchain);
        // wil::unique_handle otherProcess = OpenProcess(contentProcessPid);
        // HANDLE otherGuysHandleId = INVALID_HANDLE_VALUE;
        BOOL ret = DuplicateHandle(GetCurrentProcess(),
                                   _hOurSwapchain.get(),
                                   _pi.hProcess,
                                   &_hTheirSwapchain,
                                   0,
                                   FALSE,
                                   DUPLICATE_SAME_ACCESS);

        // {
        auto panelNative = panel.as<ISwapChainPanelNative2>();
        panelNative;
        panelNative->SetSwapChainHandle(_hOurSwapchain.get());
        // }

        _host.ThisIsInsane((uint64_t)_hTheirSwapchain.get());

        ret;
    }

}
