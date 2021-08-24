// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AppHost.h"
#include "resource.h"
#include "../types/inc/User32Utils.hpp"
#include <WilErrorReporting.h>

using namespace winrt;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Composition;
using namespace winrt::Windows::UI::Xaml::Hosting;
using namespace winrt::Windows::Foundation::Numerics;

bool checkIfContentProcess(winrt::guid& contentProcessGuid, HANDLE& eventHandle)
{
    std::vector<std::wstring> args;

    if (auto commandline{ GetCommandLineW() })
    {
        int argc = 0;

        // Get the argv, and turn them into a hstring array to pass to the app.
        wil::unique_any<LPWSTR*, decltype(&::LocalFree), ::LocalFree> argv{ CommandLineToArgvW(commandline, &argc) };
        if (argv)
        {
            for (auto& elem : wil::make_range(argv.get(), argc))
            {
                args.emplace_back(elem);
            }
        }
    }
    if (args.size() == 5 &&
        args.at(1) == L"--content" &&
        args.at(3) == L"--signal")
    {
        auto& guidString{ args.at(2) };
        auto canConvert = guidString.length() == 38 && guidString.front() == '{' && guidString.back() == '}';
        if (canConvert)
        {
            GUID result{};
            THROW_IF_FAILED(IIDFromString(guidString.c_str(), &result));
            contentProcessGuid = result;

            eventHandle = reinterpret_cast<HANDLE>(wcstoul(args.at(4).c_str(),
                                                           nullptr /*endptr*/,
                                                           16 /*base*/));
            return true;
        }
    }
    return false;
}

std::mutex m;
std::condition_variable cv;
bool dtored = false;
winrt::weak_ref<winrt::Microsoft::Terminal::Control::ContentProcess> g_weak{ nullptr };

struct HostClassFactory : implements<HostClassFactory, IClassFactory>
{
    HostClassFactory(winrt::guid g) :
        _guid{ g } {};

    HRESULT __stdcall CreateInstance(IUnknown* outer, GUID const& iid, void** result) noexcept final
    {
        *result = nullptr;
        if (outer)
        {
            return CLASS_E_NOAGGREGATION;
        }

        if (!g_weak)
        {
            winrt::Microsoft::Terminal::Control::ContentProcess strong{}; // = winrt::make<winrt::Microsoft::Terminal::Control::ContentProcess>();
            strong.Destructed([]() {
                std::unique_lock<std::mutex> lk(m);
                dtored = true;
                cv.notify_one();
            });
            winrt::weak_ref<winrt::Microsoft::Terminal::Control::ContentProcess> weak{ strong };
            g_weak = weak;
            return strong.as(iid, result);
        }
        else
        {
            auto strong = g_weak.get();
            return strong.as(iid, result);
        }
    }

    HRESULT __stdcall LockServer(BOOL) noexcept final
    {
        return S_OK;
    }

private:
    winrt::guid _guid;
};

void doContentProcessThing(const winrt::guid& contentProcessGuid, const HANDLE& eventHandle)
{
    // !! LOAD BEARING !! - important to be a MTA
    winrt::init_apartment();

    DWORD registrationHostClass{};
    check_hresult(CoRegisterClassObject(contentProcessGuid,
                                        make<HostClassFactory>(contentProcessGuid).get(),
                                        CLSCTX_LOCAL_SERVER,
                                        REGCLS_MULTIPLEUSE,
                                        &registrationHostClass));

    // Signal the event handle that was passed to us that we're now set up and
    // ready to go.
    SetEvent(eventHandle);
    CloseHandle(eventHandle);

    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, [] { return dtored; });
}
