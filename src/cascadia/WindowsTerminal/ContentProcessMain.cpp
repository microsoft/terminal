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

// We keep a weak ref to our ContentProcess singleton here.
// Why?
//
// We need to always return the _same_ ContentProcess when someone comes to
// instantiate this class. So we want to track the single instance we make. We
// also want to track when the last outstanding reference to this object is
// removed. If we're keeping a strong ref, then the ref count will always be > 1

static winrt::weak_ref<winrt::Microsoft::Terminal::Control::ContentProcess> g_weak{ nullptr };
static wil::unique_event g_canExitThread;

struct ContentProcessFactory : implements<ContentProcessFactory, IClassFactory>
{
    ContentProcessFactory(winrt::guid g) :
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
            // Instantiate the ContentProcess here
            winrt::Microsoft::Terminal::Control::ContentProcess strong{ _guid };

            // Now, create a weak ref to that ContentProcess object.
            winrt::weak_ref<winrt::Microsoft::Terminal::Control::ContentProcess> weak{ strong };

            // Stash away that weak ref for future callers.
            g_weak = weak;
            return strong.as(iid, result);
        }
        else
        {
            auto strong = g_weak.get();
            // !! LOAD BEARING !! If you set this event in the _first_ branch
            // here, when we first create the object, then there will be _no_
            // references to the ContentProcess object for a small slice. We'll
            // stash the ContentProcess in the weak_ptr, and return it, and at
            // that moment, there will be 0 outstanding references, it'll dtor,
            // and we'll ExitProcess.
            //
            // Instead, set the event here, once there's already a reference
            // outside of just the weak one we keep. Experimentation showed this
            // was always hit when creating the ContentProcess at least once.
            g_canExitThread.SetEvent();
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

static winrt::com_ptr<ContentProcessFactory> g_contentProcFactory{ nullptr };

static bool checkIfContentProcess(winrt::guid& contentProcessGuid, HANDLE& eventHandle)
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

static void doContentProcessThing(const winrt::guid& contentProcessGuid, const HANDLE& eventHandle)
{
    // !! LOAD BEARING !! - important to be a MTA for these COM calls.
    winrt::init_apartment();
    DWORD registrationHostClass{};
    g_contentProcFactory = make_self<ContentProcessFactory>(contentProcessGuid);
    check_hresult(CoRegisterClassObject(contentProcessGuid,
                                        g_contentProcFactory.get(),
                                        CLSCTX_LOCAL_SERVER,
                                        REGCLS_MULTIPLEUSE,
                                        &registrationHostClass));

    // Signal the event handle that was passed to us that we're now set up and
    // ready to go.
    SetEvent(eventHandle);
    CloseHandle(eventHandle);
}

void TryRunAsContentProcess()
{
    winrt::guid contentProcessGuid{};
    HANDLE eventHandle{ INVALID_HANDLE_VALUE };
    if (checkIfContentProcess(contentProcessGuid, eventHandle))
    {
        g_canExitThread = wil::unique_event{ CreateEvent(nullptr, true, false, nullptr) };

        doContentProcessThing(contentProcessGuid, eventHandle);

        WaitForSingleObject(g_canExitThread.get(), INFINITE);
        // This is the conhost thing - if we ExitThread the main thread, the
        // other threads can keep running till one calls ExitProcess.
        ExitThread(0);
    }
}
