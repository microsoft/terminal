#include "pch.h"
// #include "../RPCServer/ICalculatorComponent.h"
#include "ICalculatorComponent_h.h"
#include "WindowProc.h"
#include <conio.h>
#include "../../../types/inc/Utils.hpp"

#include <wrl.h>
using namespace Microsoft::WRL;

#define PRINTF_RETURN_IF_FAILED(hr, msg) \
    do                                   \
    {                                    \
        auto __res = hr;                 \
        if (FAILED(__res))               \
        {                                \
            printf(msg "%d\n", __res);   \
            return __res;                \
        }                                \
    } while (0);

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

struct AppState
{
public:
    std::vector<winrt::hstring> args{};
    winrt::com_ptr<IWindowBroker> broker{ nullptr };
};
AppState state;

int contentProcessMain(winrt::guid ourGuid)
{
    printf("Started as a content proc\n");
    auto wstr = ::Microsoft::Console::Utils::GuidToString(ourGuid);
    printf("Started Content proc with GUID %ls\n", wstr.c_str());
    printf("Press a key to exit\n");
    auto ch = _getch();
    ch;

    return 0;
}

int windowProcessMain()
{
    printf("Started as a window proc\n");

    ComPtr<WindowProc> p = Make<WindowProc>();

    int pid;
    RETURN_IF_FAILED(p->GetPID(&pid));
    printf("our pid=%d\n", pid);

    state.broker->AddWindow(p.Get());
    printf("Added our window to the broker\n");

    printf("Requesting new content proc\n");

    winrt::guid contentGuid = ::Microsoft::Console::Utils::CreateGuid();
    PRINTF_RETURN_IF_FAILED(state.broker->CreateNewContent(contentGuid), "CreateNewContent");
    printf("Press a key to exit\n");
    auto ch = _getch();
    ch;

    return 0;
}

int main(int argc, char** argv)
{
    auto hr = S_OK;
    printf("Top of main()\n");
    hr = CoInitialize(nullptr);
    printf("CoInitialize -> %d\n", hr);

    try
    {
        winrt::com_ptr<ICalculatorComponent> calc = winrt::create_instance<ICalculatorComponent>(_uuidof(CalculatorComponentImpl), CLSCTX_LOCAL_SERVER);
        printf("Created a calculator\n");
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
    catch (winrt::hresult_error const& e)
    {
        printf("Error: %ls\n", e.message().c_str());
    }
    catch (...)
    {
        printf("Some other exception happened\n");
        LOG_CAUGHT_EXCEPTION();
    }

    {
        ComPtr<ICalculatorComponent> calc; // Interface to COM component.
        hr = CoCreateInstance(__uuidof(CalculatorComponentImpl),
                              nullptr,
                              CLSCTX_LOCAL_SERVER,
                              IID_PPV_ARGS(calc.GetAddressOf()));
        if (SUCCEEDED(hr))
        {
            // Test the component by adding two numbers.
            int result;
            hr = calc->Add(10, 11, &result);

            if (FAILED(hr))
            {
                printf("Add failed: %d\n", hr);
            }
            else
            {
                printf("10+11=%d\n", result);
            }
        }
        else
        {
            // Object creation failed. Print a message.
            printf("CoCreateInstance: %d\n", hr);
        }
    }

    // CoCreateInstance(__uuidof(CalculatorComponentImpl), CLSCTX_LOCAL_SERVER, )
    // Microsoft::WRL::Make<ICalculatorComponent>()

    state.broker = winrt::create_instance<IWindowBroker>(_uuidof(WindowBrokerImpl), CLSCTX_LOCAL_SERVER);
    if (state.broker)
    {
        printf("Got broker\n");
    }
    else
    {
        auto gle = GetLastError();
        printf("Failed to create connection to broker. This is unexpected - COM should start it for us.\n");
        printf("GLE: %d\n", gle);
        return gle;
    }

    for (auto& elem : wil::make_range(argv, argc))
    {
        // printf("%s, ", elem);
        // This is obviously a bad way of converting args to a vector of
        // hstrings, but it'll do for now.
        state.args.emplace_back(winrt::to_hstring(elem));
    }

    if (state.args.size() > 2)
    {
        if (state.args[1] == L"-c")
        {
            // try to get the GUID
            try
            {
                auto g = ::Microsoft::Console::Utils::GuidFromString(state.args[2].c_str());
                contentProcessMain(g);
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
                return -1;
            }
        }
    }
    else
    {
        // Start as a window process
        windowProcessMain();
    }

    // ComPtr<ICalculatorComponent> calc; // Interface to COM component.
    // // hr = CoCreateInstance(CLSID_CalculatorComponent,
    // hr = CoCreateInstance(__uuidof(CalculatorComponentImpl),
    //                       // auto id = __uuidof(ICalculatorComponent);
    //                       // id;
    //                       // hr = CoCreateInstance(__uuidof(ICalculatorComponent),
    //                       nullptr,
    //                       CLSCTX_LOCAL_SERVER,
    //                       IID_PPV_ARGS(calc.GetAddressOf()));
    // if (SUCCEEDED(hr))
    // {
    //     // Test the component by adding two numbers.
    //     int result;
    //     hr = calc->Add(4, 5, &result);

    //     if (FAILED(hr))
    //     {
    //         printf("Add failed: %d\n", hr);
    //     }
    //     else
    //     {
    //         printf("4+5=%d\n", result);
    //     }
    // }
    // else
    // {
    //     // Object creation failed. Print a message.
    //     printf("CoCreateInstance: %d\n", hr);
    // }

    //     // auto f = winrt::make<WindowProc>();
    //     // WindowProc* f = new WindowProc();
    //     ComPtr<WindowProc> p = Make<WindowProc>();
    //     int pid;
    //     RETURN_IF_FAILED(p->GetPID(&pid));
    //     printf("our pid=%d\n", pid);
    //     broker->AddWindow(p.Get());
    // }

    // printf("Press a key to exit\n");
    // auto ch = _getch();
    // ch;

    return 0;
}
