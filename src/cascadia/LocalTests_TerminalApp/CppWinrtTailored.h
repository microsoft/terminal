// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Module Name:
// - CppWinrtTailored.h
//
// Abstract:
// - This is effectively a copy-paste of TAEF's Tailored.h, ported to cppwinrt.
//   Unfortunately, TAEF only provides a CX or pure c++ version of the
//   RunOnUIThread function, which doesn't compile for us. This version has been
//   ported to cppwinrt for our uses.
// - RunOnUIThread is a helper function for running test code on the UI thread
//
// Author:
// - Mike Griese (zadjii-msft) 04-Dec-2019
#pragma once
#include "pch.h"

extern "C" __declspec(dllimport) HRESULT __stdcall Thread_Wait_For(HANDLE handle, unsigned long milliseconds);

namespace details
{
    class Event
    {
    public:
        Event() :
            m_handle(::CreateEvent(nullptr, FALSE, FALSE, nullptr))
        {
        }

        ~Event()
        {
            if (IsValid())
            {
                ::CloseHandle(m_handle);
            }
        }

        void Set()
        {
            ::SetEvent(m_handle);
        }

        HRESULT Wait()
        {
            return Thread_Wait_For(m_handle, INFINITE);
        }

        bool IsValid()
        {
            return m_handle != nullptr;
        }

        HANDLE m_handle;
    };
};

// Function Description:
// - This is a helper function for running a bit of test code on the UI thread.
//   It will synchronously dispatch the provided function to the UI thread, and
//   wait for that function to complete, before returning to the caller. Callers
//   should make sure to VERIFY_SUCCEEDED the result of this function, to ensure
//   the code executed successfully.
// Arguments:
// - function: A pointer to some function to run. This should accept no params,
//   and any return value will be ignored.
// Return Value:
// - S_OK _after_ we successfully execute the provided function, or another
//   HRESULT indicating failure.
template<typename TFunction>
HRESULT RunOnUIThread(const TFunction& function)
{
    auto m = winrt::Windows::ApplicationModel::Core::CoreApplication::MainView();
    auto cw = m.CoreWindow();
    auto d = cw.Dispatcher();

    // Create an event so we can wait for the callback to complete
    details::Event completedEvent;
    if (!completedEvent.IsValid())
    {
        return HRESULT_FROM_WIN32(::GetLastError());
    }

    auto invokeResult = E_FAIL;

    auto asyncAction = d.RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,
                                  [&invokeResult, &function]() {
                                      invokeResult = WEX::SafeInvoke([&]() -> bool { function(); return true; });
                                  });

    asyncAction.Completed([&completedEvent](auto&&, auto&&) {
        completedEvent.Set();
        return S_OK;
    });

    // Wait for the callback to complete
    auto hr = completedEvent.Wait();
    if (FAILED(hr))
    {
        return hr;
    }
    return invokeResult;
}
