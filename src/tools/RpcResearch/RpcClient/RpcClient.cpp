#include "pch.h"
// #include "../RPCServer/ICalculatorComponent.h"
#include "ICalculatorComponent_h.h"
#include "WindowProc.h"
#include <conio.h>

#include <wrl.h>
using namespace Microsoft::WRL;

// const CLSID CLSID_CalculatorComponent = { 0xE68F5EDD, 0x6257, 0x4E72, 0xA1, 0x0B, 0x40, 0x67, 0xED, 0x8E, 0x85, 0xF2 };
// const CLSID CLSID_WindowBroker = { 0xc4e46d11, 0xdd74, 0x43e8, 0xA1, 0x0B, 0x40, 0x67, 0xED, 0x8E, 0x85, 0xF2 };
// const auto CLSID_CalculatorComponent = uuid("E68F5EDD-6257-4E72-A10B-4067ED8E85F2");
//
// HAX: There's no good GUID literal in C++, because we've abandoned this
// platform for the last decade. So this big-brain move will fwdecl a
// placeholder implementation class, that we can use to get the __uuidof,
// letting us define the UUID in a sensible format.:
class __declspec(uuid("E68F5EDD-6257-4E72-A10B-4067ED8E85F2")) CalculatorComponentImpl;
class __declspec(uuid("c4e46d11-dd74-43e8-a4b9-d0f789ad3751")) WindowBrokerImpl;

int main()
{
    auto hr = S_OK;
    printf("Top of main()\n");
    hr = CoInitialize(nullptr);
    printf("CoInitialize -> %d\n", hr);

    ComPtr<ICalculatorComponent> calc; // Interface to COM component.
    // hr = CoCreateInstance(CLSID_CalculatorComponent,
    hr = CoCreateInstance(__uuidof(CalculatorComponentImpl),
                          // auto id = __uuidof(ICalculatorComponent);
                          // id;
                          // hr = CoCreateInstance(__uuidof(ICalculatorComponent),
                          nullptr,
                          CLSCTX_LOCAL_SERVER,
                          IID_PPV_ARGS(calc.GetAddressOf()));
    if (SUCCEEDED(hr))
    {
        // Test the component by adding two numbers.
        int result;
        hr = calc->Add(4, 5, &result);

        if (FAILED(hr))
        {
            printf("Add failed: %d\n", hr);
        }
        else
        {
            printf("4+5=%d\n", result);
        }
    }
    else
    {
        // Object creation failed. Print a message.
        printf("CoCreateInstance: %d\n", hr);
    }

    winrt::com_ptr<IWindowBroker> broker = winrt::create_instance<IWindowBroker>(_uuidof(WindowBrokerImpl), CLSCTX_LOCAL_SERVER);
    if (broker)
    {
        printf("Got broker\n");
        // auto f = winrt::make<WindowProc>();
        // WindowProc* f = new WindowProc();
        ComPtr<WindowProc> p = Make<WindowProc>();
        int pid;
        RETURN_IF_FAILED(p->GetPID(&pid));
        printf("our pid=%d\n", pid);
        broker->AddWindow(p.Get());
    }

    printf("Press a key to exit\n");
    auto ch = _getch();
    ch;

    return 0;
}
