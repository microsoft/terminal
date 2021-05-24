// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "ConsoleArguments.hpp"
#include "srvinit.h"
#include "../server/Entrypoints.h"
#include "../interactivity/inc/ServiceLocator.hpp"
#include "../inc/conint.h"

#ifndef __INSIDE_WINDOWS
#include "CConsoleHandoff.h"
#endif

// Define TraceLogging provider
TRACELOGGING_DEFINE_PROVIDER(
    g_ConhostLauncherProvider,
    "Microsoft.Windows.Console.Launcher",
    // {770aa552-671a-5e97-579b-151709ec0dbd}
    (0x770aa552, 0x671a, 0x5e97, 0x57, 0x9b, 0x15, 0x17, 0x09, 0xec, 0x0d, 0xbd),
    TraceLoggingOptionMicrosoftTelemetry());

// Define a specialization of WRL::Module so we can specify a REGCLS_SINGLEUSE type server.
// We would like to use all the conveniences afforded to us by WRL::Module<T>, but it only
// creates REGCLS_MULTIPLEUSE with no override. This makes an override for it by taking advantage
// of its existing virtual declarations.
#pragma region Single Use Out of Proc Specialization
template<int RegClsType>
class DefaultOutOfProcModuleWithRegistrationFlag;

template<int RegClsType, typename ModuleT = DefaultOutOfProcModuleWithRegistrationFlag<RegClsType>>
class OutOfProcModuleWithRegistrationFlag : public Microsoft::WRL::Module<Microsoft::WRL::ModuleType::OutOfProc, ModuleT>
{
    using Elsewhere = Microsoft::WRL::Module<Microsoft::WRL::ModuleType::OutOfProc, ModuleT>;
    using Super = Microsoft::WRL::Details::OutOfProcModuleBase<ModuleT>;

public:
    STDMETHOD(RegisterCOMObject)
    (_In_opt_z_ const wchar_t* serverName, _In_reads_(count) IID* clsids, _In_reads_(count) IClassFactory** factories, _Inout_updates_(count) DWORD* cookies, unsigned int count)
    {
        return Microsoft::WRL::Details::RegisterCOMObject<RegClsType>(serverName, clsids, factories, cookies, count);
    }
};

template<int RegClsType>
class DefaultOutOfProcModuleWithRegistrationFlag : public OutOfProcModuleWithRegistrationFlag<RegClsType, DefaultOutOfProcModuleWithRegistrationFlag<RegClsType>>
{
};
#pragma endregion

// Holds the wwinmain open until COM tells us there are no more server connections
wil::unique_event _comServerExitEvent;

static bool useV2 = true;
static bool ConhostV2ForcedInRegistry()
{
    // If the registry value doesn't exist, or exists and is non-zero, we should default to using the v2 console.
    // Otherwise, in the case of an explicit value of 0, we should use the legacy console.
    bool fShouldUseConhostV2 = true;
    PCSTR pszErrorDescription = nullptr;
    bool fIgnoreError = false;
    DWORD dwValue;
    DWORD dwType;
    DWORD cbValue = sizeof(dwValue);

    // open HKCU\Console
    wil::unique_hkey hConsoleSubKey;
    LONG lStatus = NTSTATUS_FROM_WIN32(RegOpenKeyExW(HKEY_CURRENT_USER, L"Console", 0, KEY_READ, &hConsoleSubKey));
    if (ERROR_SUCCESS == lStatus)
    {
        // now get the value of the ForceV2 reg value, if it exists
        cbValue = sizeof(dwValue);
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
    TraceLoggingWrite(g_ConhostLauncherProvider,
                      "IsLegacyLoaded",
                      TraceLoggingBool(true, "ConsoleLegacy"),
                      TraceLoggingKeyword(MICROSOFT_KEYWORD_TELEMETRY),
                      TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));

    const PCWSTR pszConhostDllName = L"ConhostV1.dll";

    // Load our implementation, and then Load/Launch the IO thread.
    wil::unique_hmodule hConhostBin(LoadLibraryExW(pszConhostDllName, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32));
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
        // fallback to V2 if conhostv1.dll cannot be loaded.
        useV2 = true;
    }

    if (SUCCEEDED(hr))
    {
        hConhostBin.release();
    }

    return hr;
}

// Routine Description:
// - Called back when COM says there is nothing left for our server to do and we can tear down.
#pragma warning(suppress : 4505) // this is unused, and therefore discarded, when built inside windows
static void _releaseNotifier() noexcept
{
    _comServerExitEvent.SetEvent();
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

    // Set up OutOfProc COM server stuff in case we become one.
    // WRL Module gets going right before winmain is called, so if we don't
    // set this up appropriately... other things using WRL that aren't us
    // could get messed up by the singleton module and cause unexpected errors.
    _comServerExitEvent.create();

    // We will use a single use server to ensure that each out-of-box console that
    // gets activated to take over a session from the OS console will only be responsible
    // for ONE console server session. This ensures that we, as the handoff target, are
    // responsible for only one session and one server handle to the driver and we maintain
    // the one-to-one relationship between console sessions and servers just like the inbox
    // one. Theoretically we could combine them if we had any way of keeping track of all
    // of the console state separately per server connection... but that's not how this is
    // all designed (so many globals) and it would potentially risk one session's crash
    // taking down some completely unrelated command-line clients.
    // ----
    // The general flow is...
    // 1. The in-box console looks up the registered delegation console
    // 2. An OpenConsole.exe is typically found which is a newer version of the
    //    same code that is in-box and may have more bug fixes or features (especially
    //    an improved VT dialect or something of that ilk).
    // 3. By activating the registered CLSID, the in-box console will be starting `openconsole.exe -Embedding`
    //    through the OutOfProc COM server infrastructure.
    // 4. The `openconsole.exe -Embedding` that starts will come through right here and register
    //    `CConsoleHandoff` to accept ONE connection.
    // 5. The in-box console will then receive an `IConsoleHandoff` to this `CConsoleHandoff` and the registration
    //    immediately expires, letting no one else in. The next caller will start another new `openconsole.exe -Embedding` process.
    // 6. The in-box console invokes the handoff method on the interface and it transfers some data into `CConsoleHandoff`
    //    of the `OpenConsole.exe` which will then stand up its own server IO thread and handle all console server session
    //    messages going forward.
    // 7. The out-of-box `OpenConsole.exe` can then attempt to lookup and invoke a `CTerminalHandoff` to ask a registered
    //    Terminal to become the UI. This OpenConsole.exe will put itself in PTY mode and let the Terminal handle user interaction.
#ifndef __INSIDE_WINDOWS
    auto& module = OutOfProcModuleWithRegistrationFlag<REGCLS_SINGLEUSE>::Create(&_releaseNotifier);
#endif

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
        // Only try to register as a handoff target if we are NOT a part of Windows.
#ifndef __INSIDE_WINDOWS
        bool defAppEnabled = false;
        if (args.ShouldRunAsComServer() && SUCCEEDED(Microsoft::Console::Internal::DefaultApp::CheckDefaultAppPolicy(defAppEnabled)) && defAppEnabled)
        {
            try
            {
                // OK we have to do this here and not in another method because
                // we would either have to store the module ref above in some accessible
                // variable (which would be awful because of the gigantic template name)
                // or we would have to come up with some creativity to extract it out
                // of the singleton module base without accidentally having WRL
                // think we're recreating it (and then assert because it's already created.)
                //
                // Also this is all a problem because the decrementing count of used objects
                // in this module in WRL::Module base doesn't null check the release notifier
                // callback function in the OutOfProc variant in the 18362 SDK. So if anything
                // else uses WRL directly or indirectly, it'll crash if the refcount
                // ever hits 0.
                // It does in the 19041 SDK so this can be cleaned into its own class if
                // we ever build with 19041 or later.
                auto comScope{ wil::CoInitializeEx(COINIT_MULTITHREADED) };

                RETURN_IF_FAILED(module.RegisterObjects());
                _comServerExitEvent.wait();
                RETURN_IF_FAILED(module.UnregisterObjects());
            }
            CATCH_RETURN()
        }
        else
#endif
        {
            if (ShouldUseLegacyConhost(args))
            {
                useV2 = false;
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
            if (useV2)
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
