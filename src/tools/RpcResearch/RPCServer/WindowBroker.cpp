#include "pch.h"
#include "WindowBroker.h"
#include "../../../types/inc/Utils.hpp"

using namespace Microsoft::WRL;

WindowBroker::WindowBroker()
{
    printf("WindowBroker ctor\n");
}

STDMETHODIMP WindowBroker::CreateNewContent(_In_ GUID guid)
{
    printf("CreateNewContent\n");
    auto wstr = ::Microsoft::Console::Utils::GuidToString(guid);
    printf("Requested Content with GUID %ls\n", wstr.c_str());

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
    printf("\tWindow.pid = %d\n", pid);

    return S_OK;
}
