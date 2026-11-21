// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <array>
#include <windows.h>
#include <wil/result.h>

namespace Microsoft::Console
{
    inline HRESULT SetForegroundBoostProcesses(HWND window, DWORD count, HANDLE* processes) noexcept
    {
        using SetProcesses = BOOL(WINAPI*)(HWND, DWORD, HANDLE*);
        static const auto setProcesses = reinterpret_cast<SetProcesses>(
            GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetAdditionalForegroundBoostProcesses"));
        RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_CALL_NOT_IMPLEMENTED), !setProcesses);
        RETURN_IF_WIN32_BOOL_FALSE(setProcesses(window, count, processes));
        return S_OK;
    }

    inline constexpr DWORD ForegroundBoostProtocol = 1;
    inline constexpr unsigned short ForegroundBoostSignal = 9;
    inline constexpr size_t ForegroundBoostClientCapacity = 31; // Reserve one API slot for OpenConsole.

    struct ForegroundBoostClient
    {
        DWORD processId{};
        FILETIME creationTime{};
    };

    struct ForegroundBoostSnapshot
    {
        HRESULT status = S_OK;
        LONG count{};
        std::array<ForegroundBoostClient, ForegroundBoostClientCapacity> clients{};
    };

    struct ForegroundBoostSharedState
    {
        struct Entry
        {
            volatile LONG processId{};
            volatile LONG creationLow{};
            volatile LONG creationHigh{};
        };

        volatile LONG version{ ForegroundBoostProtocol };
        volatile LONG revision{};
        volatile LONG status{ S_OK };
        volatile LONG count{};
        std::array<Entry, ForegroundBoostClientCapacity> clients{};

        // The console lock serializes publishers; Interlocked operations provide cross-process barriers.
        void Publish(const ForegroundBoostSnapshot& snapshot) noexcept
        {
            InterlockedIncrement(&revision);
            for (size_t i = 0; i < clients.size(); ++i)
            {
                InterlockedExchange(&clients[i].processId, static_cast<LONG>(snapshot.clients[i].processId));
                InterlockedExchange(&clients[i].creationLow, static_cast<LONG>(snapshot.clients[i].creationTime.dwLowDateTime));
                InterlockedExchange(&clients[i].creationHigh, static_cast<LONG>(snapshot.clients[i].creationTime.dwHighDateTime));
            }
            InterlockedExchange(&count, snapshot.count);
            InterlockedExchange(&status, snapshot.status);
            InterlockedIncrement(&revision);
        }

        HRESULT Read(ForegroundBoostSnapshot& snapshot) noexcept
        {
            RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH),
                         static_cast<DWORD>(InterlockedCompareExchange(&version, 0, 0)) != ForegroundBoostProtocol);
            for (size_t attempt = 0; attempt < 8; ++attempt)
            {
                const auto before = InterlockedCompareExchange(&revision, 0, 0);
                if (before & 1)
                {
                    YieldProcessor();
                    continue;
                }
                snapshot.status = InterlockedCompareExchange(&status, 0, 0);
                snapshot.count = InterlockedCompareExchange(&count, 0, 0);
                for (size_t i = 0; i < clients.size(); ++i)
                {
                    snapshot.clients[i].processId = static_cast<DWORD>(InterlockedCompareExchange(&clients[i].processId, 0, 0));
                    snapshot.clients[i].creationTime.dwLowDateTime = static_cast<DWORD>(InterlockedCompareExchange(&clients[i].creationLow, 0, 0));
                    snapshot.clients[i].creationTime.dwHighDateTime = static_cast<DWORD>(InterlockedCompareExchange(&clients[i].creationHigh, 0, 0));
                }
                if (before == InterlockedCompareExchange(&revision, 0, 0))
                {
                    RETURN_IF_FAILED(snapshot.status);
                    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_MORE_DATA),
                                 snapshot.count < 0 || static_cast<size_t>(snapshot.count) > clients.size());
                    return S_OK;
                }
            }
            return HRESULT_FROM_WIN32(ERROR_RETRY);
        }
    };
}
