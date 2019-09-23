// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "ConsoleArguments.hpp"
#include "srvinit.h"
#include "..\server\Entrypoints.h"
#include "..\interactivity\inc\ServiceLocator.hpp"

// Define TraceLogging provider
TRACELOGGING_DEFINE_PROVIDER(
    g_ConhostLauncherProvider,
    "Microsoft.Windows.Console.Launcher",
    // {770aa552-671a-5e97-579b-151709ec0dbd}
    (0x770aa552, 0x671a, 0x5e97, 0x57, 0x9b, 0x15, 0x17, 0x09, 0xec, 0x0d, 0xbd),
    TraceLoggingOptionMicrosoftTelemetry());

static bool ConhostV2ForcedInRegistry()
{
    // If the registry value doesn't exist, or exists and is non-zero, we should default to using the v2 console.
    // Otherwise, in the case of an explicit value of 0, we should use the legacy console.
    bool fShouldUseConhostV2 = true;
    PCSTR pszErrorDescription = NULL;
    bool fIgnoreError = false;

    // open HKCU\Console
    wil::unique_hkey hConsoleSubKey;
    LONG lStatus = NTSTATUS_FROM_WIN32(RegOpenKeyExW(HKEY_CURRENT_USER, L"Console", 0, KEY_READ, &hConsoleSubKey));
    if (ERROR_SUCCESS == lStatus)
    {
        // now get the value of the ForceV2 reg value, if it exists
        DWORD dwValue;
        DWORD dwType;
        DWORD cbValue = sizeof(dwValue);
        lStatus = RegQueryValueExW(hConsoleSubKey.get(),
                                   L"ForceV2",
                                   nullptr,
                                   &dwType,
                                   (PBYTE)&dwValue,
                                   &cbValue);

        if (ERROR_SUCCESS == lStatus &&
            dwType == REG_DWORD && // response is a DWORD
            cbValue == sizeof(dwValue)) // response data exists
        {
            // Value exists. If non-zero use v2 console.
            fShouldUseConhostV2 = dwValue != 0;
        }
        else
        {
            pszErrorDescription = "RegQueryValueKey Failed";
            fIgnoreError = lStatus == ERROR_FILE_NOT_FOUND;
        }
    }
    else
    {
        pszErrorDescription = "RegOpenKey Failed";
        // ignore error caused by RegOpenKey if it's a simple case of the key not being found
        fIgnoreError = lStatus == ERROR_FILE_NOT_FOUND;
    }

    return fShouldUseConhostV2;
}

[[nodiscard]] static HRESULT ValidateServerHandle(const HANDLE handle)
{
    // Make sure this is a console file.
    FILE_FS_DEVICE_INFORMATION DeviceInformation;
    IO_STATUS_BLOCK IoStatusBlock;
    NTSTATUS const Status = NtQueryVolumeInformationFile(handle, &IoStatusBlock, &DeviceInformation, sizeof(DeviceInformation), FileFsDeviceInformation);
    if (!NT_SUCCESS(Status))
    {
        RETURN_NTSTATUS(Status);
    }
    else if (DeviceInformation.DeviceType != FILE_DEVICE_CONSOLE)
    {
        return E_INVALIDARG;
    }
    else
    {
        return S_OK;
    }
}

static bool ShouldUseLegacyConhost(const ConsoleArguments& args)
{
    if (args.InConptyMode())
    {
        return false;
    }

    if (args.GetForceV1())
    {
        return true;
    }

    // Per the documentation in ConhostV2ForcedInRegistry, it checks the value
    // of HKCU\Console:ForceV2. If it's *not found* or nonzero, "v2" is forced.
    return !ConhostV2ForcedInRegistry();
}

[[nodiscard]] static HRESULT ActivateLegacyConhost(const HANDLE handle)
{
    HRESULT hr = S_OK;

    // TraceLog that we're using the legacy console. We won't log new console
    // because there's already a count of how many total processes were launched.
    // Total - legacy = new console.
    // We expect legacy launches to be infrequent enough to not cause an issue.
    TraceLoggingWrite(g_ConhostLauncherProvider, "IsLegacyLoaded", TraceLoggingBool(true, "ConsoleLegacy"), TraceLoggingKeyword(MICROSOFT_KEYWORD_TELEMETRY));

    PCWSTR pszConhostDllName = L"ConhostV1.dll";

    // Load our implementation, and then Load/Launch the IO thread.
    wil::unique_hmodule hConhostBin(LoadLibraryExW(pszConhostDllName, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32));
    if (hConhostBin.get() != nullptr)
    {
        typedef NTSTATUS (*PFNCONSOLECREATEIOTHREAD)(__in HANDLE Server);

        PFNCONSOLECREATEIOTHREAD pfnConsoleCreateIoThread = (PFNCONSOLECREATEIOTHREAD)GetProcAddress(hConhostBin.get(), "ConsoleCreateIoThread");
        if (pfnConsoleCreateIoThread != nullptr)
        {
            hr = HRESULT_FROM_NT(pfnConsoleCreateIoThread(handle));
        }
        else
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
        }
    }
    else
    {
        // setup status error
        hr = HRESULT_FROM_WIN32(GetLastError());
    }

    if (SUCCEEDED(hr))
    {
        hConhostBin.release();
    }

    return hr;
}

// Routine Description:
// - Main entry point for EXE version of console launching.
//   This can be used as a debugging/diagnostics tool as well as a method of testing the console without
//   replacing the system binary.
// Arguments:
// - hInstance - This module instance pointer is saved for resource lookups.
// - hPrevInstance - Unused pointer to the module instances. See wWinMain definitions @ MSDN for more details.
// - pwszCmdLine - Unused variable. We will look up the command line using GetCommandLineW().
// - nCmdShow - Unused variable specifying window show/hide state for Win32 mode applications.
// Return value:
// - [[noreturn]] - This function will not return. It will kill the thread we were called from and the console server threads will take over.
int CALLBACK wWinMain(
    _In_ HINSTANCE hInstance,
    _In_ HINSTANCE /*hPrevInstance*/,
    _In_ PWSTR /*pwszCmdLine*/,
    _In_ int /*nCmdShow*/)
{
    Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals().hInstance = hInstance;

    ConsoleCheckDebug();

    // Register Trace provider by GUID
    TraceLoggingRegister(g_ConhostLauncherProvider);

    // Pass command line and standard handles at this point in time as
    // potential preferences for execution that were passed on process creation.
    ConsoleArguments args(GetCommandLineW(),
                          GetStdHandle(STD_INPUT_HANDLE),
                          GetStdHandle(STD_OUTPUT_HANDLE));

    HRESULT hr = args.ParseCommandline();
    if (SUCCEEDED(hr))
    {
        if (ShouldUseLegacyConhost(args))
        {
            if (args.ShouldCreateServerHandle())
            {
                hr = E_INVALIDARG;
            }
            else
            {
                hr = ValidateServerHandle(args.GetServerHandle());

                if (SUCCEEDED(hr))
                {
                    hr = ActivateLegacyConhost(args.GetServerHandle());
                }
            }
        }
        else
        {
            if (args.ShouldCreateServerHandle())
            {
                hr = Entrypoints::StartConsoleForCmdLine(args.GetClientCommandline().c_str(), &args);
            }
            else
            {
                hr = ValidateServerHandle(args.GetServerHandle());

                if (SUCCEEDED(hr))
                {
                    hr = Entrypoints::StartConsoleForServerHandle(args.GetServerHandle(), &args);
                }
            }
        }
    }

    // Unregister Tracelogging
    TraceLoggingUnregister(g_ConhostLauncherProvider);

    // Only do this if startup was successful. Otherwise, this will leave conhost.exe running with no hosted application.
    if (SUCCEEDED(hr))
    {
        // Since the lifetime of conhost.exe is inextricably tied to the lifetime of its client processes we set our process
        // shutdown priority to zero in order to effectively opt out of shutdown process enumeration. Conhost will exit when
        // all of its client processes do.
        SetProcessShutdownParameters(0, 0);

        ExitThread(hr);
    }

    return hr;
}
