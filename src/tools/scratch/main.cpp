// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <windows.h>
#include <stdio.h>

// This wmain exists for help in writing scratch programs while debugging.
int __cdecl wmain(int /*argc*/, WCHAR* /*argv[]*/)
{
    printf("Getting manager...\n");
    auto hManager = OpenSCManager(nullptr, nullptr, SERVICE_QUERY_CONFIG);
    // printf("OpenSCManager returned %d\n", static_cast<unsigned long long>(hManager));
    if (hManager == 0)
    {
        auto gle = GetLastError();
        printf("gle: %d", gle);
        return gle;
    }

    printf("Getting service...\n");
    auto hService = OpenService(hManager, L"TabletInputService", SERVICE_QUERY_CONFIG | SERVICE_QUERY_STATUS);
    // printf("OpenService returned %d\n", static_cast<unsigned long long>(hService));
    if (hService == 0)
    {
        auto gle = GetLastError();
        printf("gle: %d", gle);
        return gle;
    }

    DWORD dwBytesNeeded = 0;
    LPQUERY_SERVICE_CONFIG lpsc = nullptr;

    printf("Getting config size...\n");
    if (!QueryServiceConfig(hService,
                            NULL,
                            0,
                            &dwBytesNeeded))
    {
        auto gle = GetLastError();
        if (ERROR_INSUFFICIENT_BUFFER == gle)
        {
            auto cbBufSize = dwBytesNeeded;
            lpsc = (LPQUERY_SERVICE_CONFIG)LocalAlloc(LMEM_FIXED, cbBufSize);
        }
        else
        {
            printf("QueryServiceConfig failed (%d)", gle);
            return gle;
        }
    }

    printf("Getting config...\n");
    if (!QueryServiceConfig(hService,
                            lpsc,
                            dwBytesNeeded,
                            &dwBytesNeeded))
    {
        auto gle = GetLastError();
        printf("QueryServiceConfig failed (%d)", gle);
        return gle;
    }

    printf("Succeeded!\n");

    printf("Start Type: 0x%x\n", lpsc->dwStartType);

    SERVICE_STATUS status{ 0 };
    if (!QueryServiceStatus(hService, &status))
    {
        auto gle = GetLastError();
        printf("QueryServiceStatus failed (%d)", gle);
        return gle;
    }
    printf("State: 0x%x\n", status.dwCurrentState);

    auto state = status.dwCurrentState;
    if (state != SERVICE_RUNNING && state != SERVICE_START_PENDING)
    {
        printf("The service is stopped\n");
    }
    else
    {
        printf("The service is running\n");
    }

    return 0;
}
