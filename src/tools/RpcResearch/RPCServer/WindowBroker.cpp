#include "pch.h"
#include "WindowBroker.h"

using namespace Microsoft::WRL;

STDMETHODIMP WindowBroker::CreateNewContent(_In_ GUID guid)
{
    guid;
    printf("CreateNewContent\n");
    return S_OK;
}

STDMETHODIMP WindowBroker::AddWindow(_In_ IWindowProc* pWindow)
{
    winrt::com_ptr<IWindowProc> window;
    //{ pWindow };
    winrt::attach_abi(window, pWindow);
    // pWindow;
    printf("AddWindow\n");
    int pid = 0;
    RETURN_IF_FAILED(window->GetPID(&pid));
    printf("Window.pid = %d", pid);

    return S_OK;
}
