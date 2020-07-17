#include "pch.h"
#include "HostClass.h"

#include "HostClass.g.cpp"

extern std::mutex m;
extern std::condition_variable cv;
extern bool dtored;

namespace winrt::ScratchWinRTServer::implementation
{
    HostClass::HostClass(const winrt::guid& g) :
        _id{ g }
    {
    }
    HostClass::~HostClass()
    {
        printf("~HostClass()\n");
        std::unique_lock<std::mutex> lk(m);
        dtored = true;
        cv.notify_one();
    }
    int HostClass::DoCount()
    {
        return _DoCount;
    }

    void HostClass::DoTheThing()
    {
        _DoCount++;
    }
    winrt::guid HostClass::Id()
    {
        return _id;
    }

    HRESULT __stdcall HostClass::Call()
    {
        _DoCount += 4;
        return S_OK;
    }

    void HostClass::Attach(const Windows::UI::Xaml::Controls::SwapChainPanel& panel)
    {
        _panel = panel;

        // DO NOT UNDER ANY CIRCUMSTANCE DO THIS
        //
        // winrt::Windows::UI::Xaml::Media::SolidColorBrush solidColor{};
        // til::color newBgColor{ 0x8F000000 };
        // solidColor.Color(newBgColor);
        // _panel.Background(solidColor);
        //
        // ANYTHING WE DO TO THE SWAPCHAINPANEL on this thread is NOT the UI thread. It can't _possibly_ be.
    }

    void HostClass::BeginRendering()
    {
        // IDXGISwapChain1* swapChain; // = _getSwapchainFromMyRenderer();

        // // DANGER: I'm fairly certain that this needs to be called on the
        // // `SwapChainPanel`s UI thread. So we may be slightly out of luck here.
        // // Unless we can just
        // //   co_await winrt::resume_foreground(_panel.Dispatcher());
        // // But that's a thread in another process!

        // auto nativePanel = _panel.as<ISwapChainPanelNative>();
        // nativePanel->SetSwapChain(swapChain);

        // Holy crap look at:
        // ISwapChainPanelNative2::SetSwapChainHandle method
        // https://docs.microsoft.com/en-us/windows/win32/api/windows.ui.xaml.media.dxinterop/nf-windows-ui-xaml-media-dxinterop-iswapchainpanelnative2-setswapchainhandle
        //
        // SetSwapChain(HANDLE swapChainHandle) allows a swap chain to be
        // rendered by referencing a shared handle to the swap chain. This
        // enables scenarios where a swap chain is created in one process and
        // needs to be passed to another process.
        //
        // XAML supports setting a DXGI swap chain as the content of a
        // SwapChainPanel element. Apps accomplish this by querying for the
        // ISwapChainPanelNative interface from a SwapChainPanel instance and
        // calling SetSwapChain(IDXGISwapChain *swapChain).
        //
        // This process works for pointers to in process swap chains. However,
        // this doesn’t work for VoIP apps, which use a two-process model to
        // enable continuing calls on a background process when a foreground
        // process is suspended or shut down. This two-process implementation
        // requires the ability to pass a shared handle to a swap chain, rather
        // than a pointer, created on the background process to the foreground
        // process to be rendered in a XAML SwapChainPanel in the foreground
        // app.

        // I _believe_ this will work something like:
        //
        // HANDLE hSwapChain;
        // auto nativePanel2 = _panel.as<ISwapChainPanelNative2>();
        // nativePanel2->SetSwapChainHandle(hSwapChain);
        //
        // But I'm not sure yet.
    }
}
