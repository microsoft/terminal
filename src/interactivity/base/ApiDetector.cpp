// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "ApiDetector.hpp"

#include <VersionHelpers.h>

#pragma hdrstop

using namespace Microsoft::Console::Interactivity;

// API Sets
#define EXT_API_SET_NTUSER_WINDOW L"ext-ms-win-ntuser-window-l1-1-0"

// This may not be defined depending on the SDK version being targetted.
#ifndef LOAD_LIBRARY_SEARCH_SYSTEM32_NO_FORWARDER
#define LOAD_LIBRARY_SEARCH_SYSTEM32_NO_FORWARDER 0x00004000
#endif

#pragma region Public Methods

// Routine Description
// - This routine detects whether the system hosts the extension API set that
//   includes, among others, CreateWindowExW.
// Arguments:
// - level - pointer to an APILevel enum stating the level of support the
//           system offers for the given functionality.
[[nodiscard]] NTSTATUS ApiDetector::DetectNtUserWindow(_Out_ ApiLevel* level)
{
    // N.B.: Testing for the API set implies the function is present.
    return DetectApiSupport(EXT_API_SET_NTUSER_WINDOW, nullptr, level);
}

#pragma endregion

#pragma region Private Methods

[[nodiscard]] NTSTATUS ApiDetector::DetectApiSupport(_In_ LPCWSTR lpApiHost, _In_ LPCSTR lpProcedure, _Out_ ApiLevel* level)
{
    if (!level)
    {
        return STATUS_INVALID_PARAMETER;
    }

    NTSTATUS status = STATUS_SUCCESS;
    HMODULE hModule = nullptr;

    status = TryLoadWellKnownLibrary(lpApiHost, &hModule);
    if (NT_SUCCESS(status) && lpProcedure)
    {
        status = TryLocateProcedure(hModule, lpProcedure);
    }

    SetLevelAndFreeIfNecessary(status, hModule, level);

    return STATUS_SUCCESS;
}

[[nodiscard]] NTSTATUS ApiDetector::TryLoadWellKnownLibrary(_In_ LPCWSTR lpLibrary,
                                                            _Outptr_result_nullonfailure_ HMODULE* phModule)
{
    NTSTATUS status = STATUS_SUCCESS;

    // N.B.: Suppose we attempt to load user32.dll and locate CreateWindowExW
    //       on a Nano Server system with reverse forwarders enabled. Since the
    //       reverse forwarder modules have the same name as their regular
    //       counterparts, the loader will claim to have found the module. In
    //       addition, since reverse forwarders contain all the functions of
    //       their regular counterparts, just stubbed to return or set the last
    //       error to STATUS_NOT_IMPLEMENTED, GetProcAddress will indeed
    //       indicate that the procedure exists. Hence, we need to search for
    //       modules skipping over the reverse forwarders.
    //
    //       This however has the side-effect of not working on downlevel.
    //       LoadLibraryEx asserts that the flags passed in are valid. If any
    //       invalid flags are passed, it sets the last error to
    //       ERROR_INVALID_PARAMETER and returns. Since
    //       LOAD_LIBRARY_SEARCH_SYSTEM32_NO_FORWARDER does not exist on
    //       downlevel Windows, the call will fail there.
    //
    //       To counteract that, we try to load the library skipping forwarders
    //       first under the assumption that we are running on a sufficiently
    //       new system. If the call fails with ERROR_INVALID_PARAMETER, we
    //       know there is a problem with the flags, so we try again without
    //       the NO_FORWARDER part. Because reverse forwarders do not exist on
    //       downlevel (i.e. < Windows 10), we do not run the risk of failing
    //       to accurately detect system functionality there.
    //
    // N.B.: We cannot use IsWindowsVersionOrGreater or associated helper API's
    //       because those are subject to manifesting and may tell us we are
    //       running on Windows 8 even if we are running on Windows 10.
    //
    // TODO: MSFT 10916452 Determine what manifest magic is required to make
    //       versioning API's behave sanely.

    status = TryLoadWellKnownLibrary(lpLibrary, LOAD_LIBRARY_SEARCH_SYSTEM32_NO_FORWARDER, phModule);
    if (!NT_SUCCESS(status) && GetLastError() == ERROR_INVALID_PARAMETER)
    {
        status = TryLoadWellKnownLibrary(lpLibrary, LOAD_LIBRARY_SEARCH_SYSTEM32, phModule);
    }

    return status;
}

[[nodiscard]] NTSTATUS ApiDetector::TryLoadWellKnownLibrary(_In_ LPCWSTR lpLibrary,
                                                            _In_ DWORD dwLoaderFlags,
                                                            _Outptr_result_nullonfailure_ HMODULE* phModule)
{
    HMODULE hModule = nullptr;

    hModule = LoadLibraryExW(lpLibrary,
                             nullptr,
                             dwLoaderFlags);
    if (hModule)
    {
        *phModule = hModule;

        return STATUS_SUCCESS;
    }
    else
    {
        return STATUS_UNSUCCESSFUL;
    }
}

[[nodiscard]] NTSTATUS ApiDetector::TryLocateProcedure(_In_ HMODULE hModule, _In_ LPCSTR lpProcedure)
{
    FARPROC proc = GetProcAddress(hModule, lpProcedure);

    if (proc)
    {
        return STATUS_SUCCESS;
    }
    else
    {
        return STATUS_UNSUCCESSFUL;
    }
}

void ApiDetector::SetLevelAndFreeIfNecessary(_In_ NTSTATUS status, _In_ HMODULE hModule, _Out_ ApiLevel* level)
{
    if (NT_SUCCESS(status))
    {
        *level = ApiLevel::Win32;
    }
    else
    {
        FreeLibrary(hModule);

        *level = ApiLevel::OneCore;
    }
}

#pragma endregion
