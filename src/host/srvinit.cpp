// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "srvinit.h"

#include "dbcs.h"
#include "handle.h"
#include "registry.hpp"
#include "renderFontDefaults.hpp"
#include "../interactivity/base/ApiDetector.hpp"
#include "../interactivity/base/RemoteConsoleControl.hpp"
#include "../interactivity/inc/ServiceLocator.hpp"
#include "../server/DeviceHandle.h"
#include "../server/IoSorter.h"
#include "../types/inc/CodepointWidthDetector.hpp"

#if TIL_FEATURE_RECEIVEINCOMINGHANDOFF_ENABLED
#include "ITerminalHandoff.h"
#endif // TIL_FEATURE_RECEIVEINCOMINGHANDOFF_ENABLED

#pragma hdrstop

using namespace Microsoft::Console::Interactivity;
using namespace Microsoft::Console::Render;

const UINT CONSOLE_EVENT_FAILURE_ID = 21790;
const UINT CONSOLE_LPC_PORT_FAILURE_ID = 21791;

[[nodiscard]] HRESULT ConsoleServerInitialization(_In_ HANDLE Server, const ConsoleArguments* const args)
try
{
    auto& Globals = ServiceLocator::LocateGlobals();

    if (!Globals.pDeviceComm)
    {
        // in rare circumstances (such as in the fuzzing harness), there will already be a device comm
        Globals.pDeviceComm = new ConDrvDeviceComm(Server);
    }

    Globals.launchArgs = *args;

    Globals.uiOEMCP = GetOEMCP();
    Globals.uiWindowsCP = GetACP();

    Globals.pFontDefaultList = new RenderFontDefaults();

    FontInfoBase::s_SetFontDefaultList(Globals.pFontDefaultList);

    // Check if this conhost is allowed to delegate its activities to another.
    // If so, look up the registered default console handler.
    if (Globals.delegationPair.IsUndecided())
    {
        Globals.delegationPair = DelegationConfig::s_GetDelegationPair();

        TraceLoggingWrite(g_hConhostV2EventTraceProvider,
                          "SrvInit_FoundDelegationConsole",
                          TraceLoggingGuid(Globals.delegationPair.console, "ConsoleClsid"),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));
        TraceLoggingWrite(g_hConhostV2EventTraceProvider,
                          "SrvInit_FoundDelegationTerminal",
                          TraceLoggingGuid(Globals.delegationPair.terminal, "TerminalClsid"),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
    // If we looked up the registered defterm pair, and it was left as the default (missing or {0}),
    // AND velocity is enabled for DxD, then we switch the delegation pair to Terminal and
    // mark that we should check that class for the marker interface later.
    if (Globals.delegationPair.IsDefault())
    {
        Globals.delegationPair = DelegationConfig::TerminalDelegationPair;
        Globals.defaultTerminalMarkerCheckRequired = true;
    }

    // Create the accessibility notifier early in the startup process.
    // Only create if we're not in PTY mode.
    // The notifiers use expensive legacy MSAA events and the PTY isn't even responsible
    // for the terminal user interface, so we should set ourselves up to skip all
    // those notifications and the mathematical calculations required to send those events
    // for performance reasons.
    if (!args->InConptyMode())
    {
        RETURN_IF_FAILED(ServiceLocator::CreateAccessibilityNotifier());
    }

    // Removed allocation of scroll buffer here.
    return S_OK;
}
CATCH_RETURN()

static bool s_IsOnDesktop()
{
    // Persist this across calls so we don't dig it out a whole bunch of times. Once is good enough for the system.
    static auto fAlreadyQueried = false;
    static auto fIsDesktop = false;

    if (!fAlreadyQueried)
    {
        Microsoft::Console::Interactivity::ApiLevel level;
        const auto status = Microsoft::Console::Interactivity::ApiDetector::DetectNtUserWindow(&level);
        LOG_IF_NTSTATUS_FAILED(status);

        if (SUCCEEDED_NTSTATUS(status))
        {
            switch (level)
            {
            case Microsoft::Console::Interactivity::ApiLevel::OneCore:
                fIsDesktop = false;
                break;
            case Microsoft::Console::Interactivity::ApiLevel::Win32:
                fIsDesktop = true;
                break;
            }
        }

        fAlreadyQueried = true;
    }

    return fIsDesktop;
}

[[nodiscard]] NTSTATUS SetUpConsole(_Inout_ Settings* pStartupSettings,
                                    _In_ DWORD TitleLength,
                                    _In_reads_bytes_(TitleLength) LPWSTR Title,
                                    _In_ LPCWSTR CurDir,
                                    _In_ LPCWSTR AppName)
{
    // We will find and locate all relevant preference settings and then create the console here.
    // The precedence order for settings is:
    // 0. Launch arguments passed on the commandline.
    // 1. STARTUPINFO settings
    // 2a. Shortcut/Link settings
    // 2b. Registry specific settings
    // 3. Registry default settings
    // 4. Hardcoded default settings
    // To establish this hierarchy, we will need to load the settings and apply them in reverse order.

    // 4. Initializing Settings will establish hard-coded defaults.
    // Set to reference of global console information since that's the only place we need to hold the settings.
    auto& settings = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto& launchArgs = ServiceLocator::LocateGlobals().launchArgs;
    // 4b. On Desktop editions, we need to apply a series of Desktop-specific defaults that are better than the
    // ones from the constructor (which are great for OneCore systems.)
    if (s_IsOnDesktop())
    {
        settings.ApplyDesktopSpecificDefaults();
    }

    // Use the launch arguments to check if we're going to be started in pseudoconsole mode.
    // If we are, we don't want to load any user settings, because that could
    //      result in some strange rendering results in the end terminal.
    // Use the launch args because the VtIo hasn't been initialized yet.
    if (!launchArgs.InConptyMode())
    {
        // 3. Read the default registry values.
        Registry reg(&settings);
        reg.LoadGlobalsFromRegistry();
        reg.LoadDefaultFromRegistry();

        // 2. Read specific settings

        // Link is expecting the flags from the process to be in already, so apply that first
        settings.SetStartupFlags(pStartupSettings->GetStartupFlags());

        // We need to see if we were spawned from a link. If we were, we need to
        // call back into the shell to try to get all the console information from the link.
        ServiceLocator::LocateSystemConfigurationProvider()->GetSettingsFromLink(&settings, Title, &TitleLength, CurDir, AppName, nullptr);

        // If we weren't started from a link, this will already be set.
        // If LoadLinkInfo couldn't find anything, it will remove the flag so we can dig in the registry.
        if (!(settings.IsStartupTitleIsLinkNameSet()))
        {
            reg.LoadFromRegistry(Title);
        }
    }
    else
    {
        // microsoft/terminal#1965 - Let's just always enable VT processing by
        // default for conpty clients. This prevents peculiar differences in
        // behavior between conhost and terminal applications when the user has
        // VirtualTerminalLevel=1 in their registry.
        // We want everyone to be using VT by default anyways, so this is a
        // strong nudge in that direction. If an application _doesn't_ want VT
        // processing, it's free to disable this setting, even in conpty mode.
        settings.SetDefaultVirtTermLevel(1);

        // GH#9458 - In the case of a DefTerm handoff, the OriginalTitle might
        // be stashed in the lnk. We want to crack that lnk open, so we can get
        // that title from it, but we want to discard everything else. So build
        // a dummy Settings object here, and read the link settings into it.
        // `Title` will get filled with the title from the lnk, which we'll use
        // below.

        Settings temp;
        // We're not gonna copy over StartupFlags to the main gci settings,
        // because we generally don't think those are valuable in ConPTY mode.
        // However, we do need to apply them to the temp we've created, so that
        // GetSettingsFromLink will actually look for the link settings (it will
        // skip that if STARTF_TITLEISLINKNAME is not set).
        temp.SetStartupFlags(pStartupSettings->GetStartupFlags());
        ServiceLocator::LocateSystemConfigurationProvider()->GetSettingsFromLink(&temp, Title, &TitleLength, CurDir, AppName, nullptr);
    }

    // 1. The settings we were passed contains STARTUPINFO structure settings to be applied last.
    settings.ApplyStartupInfo(pStartupSettings);

    // 0. The settings passed in via commandline arguments. These should override anything else.
    settings.ApplyCommandlineArguments(launchArgs);

    // Validate all applied settings for correctness against final rules.
    settings.Validate();

    // As of the graphics refactoring to library based, all fonts are now DPI aware. Scaling is
    // performed at the Blt time for raster fonts.
    // Note that we can only declare our DPI awareness once per process launch.
    // Set the process's default dpi awareness context to PMv2 so that new top level windows
    // inherit their WM_DPICHANGED* broadcast mode (and more, like dialog scaling) from the thread.

    auto pHighDpiApi = ServiceLocator::LocateHighDpiApi();
    if (pHighDpiApi)
    {
        // N.B.: There is no high DPI support on OneCore (non-UAP) systems.
        //       Instead of implementing a no-op interface, just skip all high
        //       DPI configuration if it is not supported. All callers into the
        //       high DPI API are in the Win32-specific interactivity DLL.
        if (!pHighDpiApi->SetProcessDpiAwarenessContext())
        {
            // Fallback to per-monitor aware V1 if the API isn't available.
            LOG_IF_FAILED(pHighDpiApi->SetProcessPerMonitorDpiAwareness());
        }
    }

    //Save initial font name for comparison on exit. We want telemetry when the font has changed
    if (settings.IsFaceNameSet())
    {
        settings.SetLaunchFaceName(settings.GetFaceName());
    }

    // Allocate console will read the global ServiceLocator::LocateGlobals().getConsoleInformation
    // for the settings we just set.
    auto Status = CONSOLE_INFORMATION::AllocateConsole({ Title, TitleLength / sizeof(wchar_t) });
    if (FAILED_NTSTATUS(Status))
    {
        return Status;
    }

    return STATUS_SUCCESS;
}

[[nodiscard]] NTSTATUS RemoveConsole(_In_ ConsoleProcessHandle* ProcessData)
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    LockConsole();
    auto Status = STATUS_SUCCESS;

    CommandHistory::s_Free((HANDLE)ProcessData);

    const auto fRecomputeOwner = ProcessData->fRootProcess;
    gci.ProcessHandleList.FreeProcessData(ProcessData);

    if (fRecomputeOwner)
    {
        auto pWindow = ServiceLocator::LocateConsoleWindow();
        if (pWindow != nullptr)
        {
            pWindow->SetOwner();
        }
    }

    UnlockConsole();

    return Status;
}

DWORD WINAPI ConsoleIoThread(LPVOID lpParameter);

void ConsoleCheckDebug()
{
#ifdef DBG
    wil::unique_hkey hCurrentUser;
    wil::unique_hkey hConsole;
    auto status = RegistrySerialization::s_OpenConsoleKey(&hCurrentUser, &hConsole);

    if (SUCCEEDED_NTSTATUS(status))
    {
        DWORD dwData = 0;
        status = RegistrySerialization::s_QueryValue(hConsole.get(),
                                                     L"DebugLaunch",
                                                     sizeof(dwData),
                                                     REG_DWORD,
                                                     (BYTE*)&dwData,
                                                     nullptr);

        if (SUCCEEDED_NTSTATUS(status))
        {
            if (dwData != 0)
            {
                DebugBreak();
            }
        }
    }
#endif
}

// Routine Description:
// - Sets up the main driver message packet (I/O) processing
//   thread that will handle all client requests from all
//   attached command-line applications for the duration
//   of this console server session.
// - The optional arguments are only used when receiving a handoff
//   from another console server (typically in-box to the Windows OS image)
//   that has already started processing the console session.
//   They will be blank and generated internally by this method if this is the first
//   console server starting in response to a client startup or ConPTY setup
//   request.
// Arguments:
// - Server - Handle to the console driver that represents
//     our server side of the connection.
// - args - Command-line arguments from starting this console host
//    that may affect the way we host the session.
// - driverInputEvent - (Optional) Event registered with the console driver
//    that we will use to wake up input read requests that
//    are blocked because they came in when we had no input ready.
// - connectMessage - (Optional) A message received from a connecting client
//    by another console server that is being passed off to us as a part of
//    the handoff strategy.
HRESULT ConsoleCreateIoThread(_In_ HANDLE Server,
                              const ConsoleArguments* const args,
                              HANDLE driverInputEvent,
                              PCONSOLE_API_MSG connectMessage)
{
    auto& g = ServiceLocator::LocateGlobals();
    RETURN_IF_FAILED(ConsoleServerInitialization(Server, args));
    RETURN_IF_FAILED(g.hConsoleInputInitEvent.create(wil::EventOptions::None));

    if (driverInputEvent != INVALID_HANDLE_VALUE)
    {
        // Store the driver input event. It's already been told that it exists by whomever started us.
        g.hInputEvent.reset(driverInputEvent);
    }
    else
    {
        // Set up and tell the driver about the input available event.
        RETURN_IF_FAILED(g.hInputEvent.create(wil::EventOptions::ManualReset));

        CD_IO_SERVER_INFORMATION ServerInformation;
        ServerInformation.InputAvailableEvent = ServiceLocator::LocateGlobals().hInputEvent.get();
        RETURN_IF_FAILED(g.pDeviceComm->SetServerInformation(&ServerInformation));
    }

    // Ensure that whatever we're giving to the new thread is on the heap so it cannot
    // go out of scope by the time that thread starts.
    // (e.g. if someone sent us a pointer to stack memory... that could happen
    //  ask me how I know... :| )
    std::unique_ptr<CONSOLE_API_MSG> heapConnectMessage;
    if (connectMessage)
    {
        // Allocate and copy onto the heap
        heapConnectMessage = std::make_unique<CONSOLE_API_MSG>(*connectMessage);

        // Set the pointer that `CreateThread` uses to the heap space
        connectMessage = heapConnectMessage.get();
    }

    const auto hThread = CreateThread(nullptr, 0, ConsoleIoThread, connectMessage, 0, nullptr);
    RETURN_HR_IF(E_HANDLE, hThread == nullptr);

    // If we successfully started the other thread, it's that guy's problem to free the connect message.
    // (If we didn't make one, it should be no problem to release the empty unique_ptr.)
    heapConnectMessage.release();

    LOG_IF_FAILED(SetThreadDescription(hThread, L"Console Driver Message IO Thread"));
    LOG_IF_WIN32_BOOL_FALSE(CloseHandle(hThread)); // The thread will run on its own and close itself. Free the associated handle.

    // See MSFT:19918626
    // Make sure to always set up the signal thread if we need to.
    // Do this first, because breaking the signal pipe is used by the conpty API
    //  to indicate that we should close.
    // The conpty i/o threads need an actual client to be connected before they
    //      can start, so they're started below, in ConsoleAllocateConsole
    auto& gci = g.getConsoleInformation();
    RETURN_IF_FAILED(gci.GetVtIo()->Initialize(args));

    return S_OK;
}

// Routine Description:
// - Accepts a console server session from another console server
//   most commonly from the operating system in-box console to
//   a more-up-to-date and out-of-band delivered one.
// Arguments:
// - Server - Handle to the console driver that represents our server
//    side of hosting the console session
// - driverInputEvent - Handle to an event already registered with the
//    driver that clients will implicitly wait on when we don't have
//    any input to return in the queue when a request is made and is
//    signaled to unblock them when input finally arrives.
// - connectMessage - A console driver/server message as received
//    by the previous console server for us to finish processing in
//    order to complete the client's initial connection and store
//    all necessary callback information for all subsequent API calls.
// Return Value:
// - COM errors, registry errors, pipe errors, handle manipulation errors,
//   errors from the creating the thread for the
//   standard IO thread loop for the server to process messages
//   from the driver... or an S_OK success.
[[nodiscard]] HRESULT ConsoleEstablishHandoff([[maybe_unused]] _In_ HANDLE Server,
                                              [[maybe_unused]] HANDLE driverInputEvent,
                                              [[maybe_unused]] HANDLE hostSignalPipe,
                                              [[maybe_unused]] HANDLE hostProcessHandle,
                                              [[maybe_unused]] PCONSOLE_API_MSG connectMessage)
try
{
#if !TIL_FEATURE_RECEIVEINCOMINGHANDOFF_ENABLED
    TraceLoggingWrite(g_hConhostV2EventTraceProvider,
                      "SrvInit_ReceiveHandoff_Disabled",
                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                      TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
#else // TIL_FEATURE_RECEIVEINCOMINGHANDOFF_ENABLED
    auto& g = ServiceLocator::LocateGlobals();
    g.handoffTarget = true;

    g.delegationPair = DelegationConfig::s_GetDelegationPair();
    // We've been handed off to (we're OpenConsole, not conhost).
    // If we get here and there's not a custom defterm set, then it must be because
    // conhost defaulted to us for DxD. Set up Terminal as the thing to handoff too.
    if (!g.delegationPair.IsCustom())
    {
        g.delegationPair = DelegationConfig::TerminalDelegationPair;
    }

    TraceLoggingWrite(g_hConhostV2EventTraceProvider,
                      "SrvInit_ReceiveHandoff",
                      TraceLoggingGuid(g.delegationPair.terminal, "TerminalClsid"),
                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                      TraceLoggingKeyword(TIL_KEYWORD_TRACE));

    // Capture handle to the inbox process into a unique handle holder.
    g.handoffInboxConsoleHandle.reset(hostProcessHandle);

    // Set up a threadpool waiter to shutdown everything if the inbox process disappears.
    g.handoffInboxConsoleExitWait.reset(CreateThreadpoolWait(
        [](PTP_CALLBACK_INSTANCE /*callbackInstance*/, PVOID /*context*/, PTP_WAIT /*wait*/, TP_WAIT_RESULT /*waitResult*/) noexcept {
            ServiceLocator::RundownAndExit(E_APPLICATION_MANAGER_NOT_RUNNING);
        },
        nullptr,
        nullptr));
    RETURN_LAST_ERROR_IF_NULL(g.handoffInboxConsoleExitWait.get());

    SetThreadpoolWait(g.handoffInboxConsoleExitWait.get(), g.handoffInboxConsoleHandle.get(), nullptr);

    std::unique_ptr<IConsoleControl> remoteControl = std::make_unique<Microsoft::Console::Interactivity::RemoteConsoleControl>(hostSignalPipe);
    RETURN_IF_NTSTATUS_FAILED(ServiceLocator::SetConsoleControlInstance(std::move(remoteControl)));

    wil::unique_handle signalPipeTheirSide;
    wil::unique_handle signalPipeOurSide;
    RETURN_IF_WIN32_BOOL_FALSE(CreatePipe(signalPipeOurSide.addressof(), signalPipeTheirSide.addressof(), nullptr, 0));

    TraceLoggingWrite(g_hConhostV2EventTraceProvider,
                      "SrvInit_ReceiveHandoff_OpenedPipes",
                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                      TraceLoggingKeyword(TIL_KEYWORD_TRACE));

    wil::unique_handle clientProcess{ OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE, TRUE, static_cast<DWORD>(connectMessage->Descriptor.Process)) };
    RETURN_LAST_ERROR_IF_NULL(clientProcess.get());

    TraceLoggingWrite(g_hConhostV2EventTraceProvider,
                      "SrvInit_ReceiveHandoff_OpenedClient",
                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                      TraceLoggingKeyword(TIL_KEYWORD_TRACE));

    wil::unique_handle refHandle;
    RETURN_IF_NTSTATUS_FAILED(DeviceHandle::CreateClientHandle(refHandle.addressof(),
                                                               Server,
                                                               L"\\Reference",
                                                               FALSE));

    const auto serverProcess = GetCurrentProcess();

    ::Microsoft::WRL::ComPtr<ITerminalHandoff3> handoff;

    TraceLoggingWrite(g_hConhostV2EventTraceProvider,
                      "SrvInit_PrepareToCreateDelegationTerminal",
                      TraceLoggingGuid(g.delegationPair.terminal, "TerminalClsid"),
                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                      TraceLoggingKeyword(TIL_KEYWORD_TRACE));

    RETURN_IF_FAILED(CoCreateInstance(g.delegationPair.terminal, nullptr, CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&handoff)));
    TraceLoggingWrite(g_hConhostV2EventTraceProvider,
                      "SrvInit_CreatedDelegationTerminal",
                      TraceLoggingGuid(g.delegationPair.terminal, "TerminalClsid"),
                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                      TraceLoggingKeyword(TIL_KEYWORD_TRACE));

    // As a part of defterm handoff, we're gonna try to pull a lot of
    // information out of the link and startup info, so we can let the terminal
    // know these things as well.
    //
    // To let the terminal know these things, we have to look them up now,
    // before we normally would.
    //
    // Typically, we'll just go into `ConsoleCreateIoThread` below, which will
    // pull out the CONSOLE_API_CONNECTINFO from this connect message, and then
    // get the link properties out of the title later. Below are elements of
    // ConsoleAllocateConsole and SetUpConsole that get the bits of STARTUP_INFO
    // we care about for defterm handoffs.

    // A placeholder that we'll read icon information into, instead of setting
    // the globals icon state.
    Microsoft::Console::Interactivity::IconInfo icon;

    // To be able to actually process this connect message into a
    // CONSOLE_API_CONNECTINFO, we need to hook up the ConDrvDeviceComm to the
    // message. Usually, we'd create the ConDrvDeviceComm later, in
    // ConsoleServerInitialization, but we can set it up early here.
    // ConsoleServerInitialization will safely no-op if it already finds one.
    g.pDeviceComm = new ConDrvDeviceComm(Server);
    // load bearing: if you don't set this, the ConsoleInitializeConnectInfo will fail.
    connectMessage->_pDeviceComm = g.pDeviceComm;
    CONSOLE_API_CONNECTINFO Cac;
    RETURN_IF_NTSTATUS_FAILED(ConsoleInitializeConnectInfo(connectMessage, &Cac));

    // BEGIN code from SetUpConsole
    // Create a temporary Settings object to parse the settings into, rather
    // than parsing them into the global settings object (gci).
    Settings settings{};
    // We need to see if we were spawned from a link. If we were, we need to
    // call back into the OS shell to try to get all the console information from the link.
    //
    // load bearing: if you don't pass the StartupFlags, then
    // GetSettingsFromLink might not even bother attempting to check the lnk.
    settings.SetStartupFlags(Cac.ConsoleInfo.GetStartupFlags());
    ServiceLocator::LocateSystemConfigurationProvider()->GetSettingsFromLink(&settings,
                                                                             Cac.Title,
                                                                             &Cac.TitleLength,
                                                                             Cac.CurDir,
                                                                             Cac.AppName,
                                                                             &icon);

    // 1. The settings we were passed contains STARTUPINFO structure settings to be applied last.
    settings.ApplyStartupInfo(&Cac.ConsoleInfo);
    // END code from SetUpConsole

    // Take what we've collected, and bundle it up for handoff.
    auto title = wil::make_bstr(Cac.Title);
    auto iconPath = wil::make_bstr(icon.path.data());
    TERMINAL_STARTUP_INFO myStartupInfo{
        title.get(),
        iconPath.get(),
        icon.index
    };

    myStartupInfo.wShowWindow = settings.GetShowWindow();

    wil::unique_handle inPipeOurSide;
    wil::unique_handle outPipeOurSide;
    RETURN_IF_FAILED(handoff->EstablishPtyHandoff(inPipeOurSide.addressof(),
                                                  outPipeOurSide.addressof(),
                                                  signalPipeTheirSide.get(),
                                                  refHandle.get(),
                                                  serverProcess,
                                                  clientProcess.get(),
                                                  &myStartupInfo));

    TraceLoggingWrite(g_hConhostV2EventTraceProvider,
                      "SrvInit_DelegateToTerminalSucceeded",
                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                      TraceLoggingKeyword(TIL_KEYWORD_TRACE));

    signalPipeTheirSide.reset();

    // GH#13211 - Make sure the terminal obeys the resizing quirk. Otherwise,
    // defterm connections to the Terminal are going to have weird resizing.
    const auto commandLine = fmt::format(FMT_COMPILE(L" --headless --signal {:#x}"),
                                         (int64_t)signalPipeOurSide.release());

    ConsoleArguments consoleArgs(commandLine, inPipeOurSide.release(), outPipeOurSide.release());
    RETURN_IF_FAILED(consoleArgs.ParseCommandline());

    return ConsoleCreateIoThread(Server, &consoleArgs, driverInputEvent, connectMessage);
#endif // TIL_FEATURE_RECEIVEINCOMINGHANDOFF_ENABLED
}
CATCH_RETURN()

// Routine Description:
// - Creates the I/O thread for handling and processing messages from the console driver
//   as the server side of a console session.
// - This entrypoint is for all start scenarios that are not receiving a hand-off
//   from another console server. For example, getting started by kernelbase.dll from
//   the operating system as a client application realizes it needs a console server,
//   getting started to be a ConPTY host inside the OS, or being double clicked either
//   inside the OS as `conhost.exe` or outside as `OpenConsole.exe`.
// Arguments:
// - Server - The server side handle to the console driver to let us pick up messages to process for the clients.
// - args - A structure of arguments that may have been passed in on the command-line, typically only used to control the ConPTY configuration.
// Return Value:
// - S_OK if the thread starts up correctly or any number of thread, registry, windowing, or just about any other
//   failure that could possibly occur during console server initialization.
[[nodiscard]] HRESULT ConsoleCreateIoThreadLegacy(_In_ HANDLE Server, const ConsoleArguments* const args)
{
    return ConsoleCreateIoThread(Server, args, INVALID_HANDLE_VALUE, nullptr);
}

#define SYSTEM_ROOT (L"%SystemRoot%")
#define SYSTEM_ROOT_LENGTH (sizeof(SYSTEM_ROOT) - sizeof(WCHAR))

// Routine Description:
// - This routine translates path characters into '_' characters because the NT registry apis do not allow the creation of keys with
//   names that contain path characters. It also converts absolute paths into %SystemRoot% relative ones. As an example, if both behaviors were
//   specified it would convert a title like C:\WINNT\System32\cmd.exe to %SystemRoot%_System32_cmd.exe.
// Arguments:
// - ConsoleTitle - Pointer to string to translate.
// - Unexpand - Convert absolute path to %SystemRoot% relative one.
// - Substitute - Whether string-substitution ('_' for '\') should occur.
// Return Value:
// - Pointer to translated title or nullptr.
// Note:
// - This routine allocates a buffer that must be freed.
PWSTR TranslateConsoleTitle(_In_ PCWSTR pwszConsoleTitle, const BOOL fUnexpand, const BOOL fSubstitute)
{
    LPWSTR Tmp = nullptr;

    size_t cbConsoleTitle;
    size_t cbSystemRoot;

    auto pwszSysRoot = new (std::nothrow) wchar_t[MAX_PATH];
    if (nullptr != pwszSysRoot)
    {
        if (0 != GetWindowsDirectoryW(pwszSysRoot, MAX_PATH))
        {
            if (SUCCEEDED(StringCbLengthW(pwszConsoleTitle, STRSAFE_MAX_CCH, &cbConsoleTitle)) &&
                SUCCEEDED(StringCbLengthW(pwszSysRoot, MAX_PATH, &cbSystemRoot)))
            {
                const auto cchSystemRoot = (int)(cbSystemRoot / sizeof(WCHAR));
                const auto cchConsoleTitle = (int)(cbConsoleTitle / sizeof(WCHAR));
                cbConsoleTitle += sizeof(WCHAR); // account for nullptr terminator

                if (fUnexpand &&
                    cchConsoleTitle >= cchSystemRoot &&
#pragma prefast(suppress : 26018, "We've guaranteed that cchSystemRoot is equal to or smaller than cchConsoleTitle in size.")
                    (CSTR_EQUAL == CompareStringOrdinal(pwszConsoleTitle, cchSystemRoot, pwszSysRoot, cchSystemRoot, TRUE)))
                {
                    cbConsoleTitle -= cbSystemRoot;
                    pwszConsoleTitle += cchSystemRoot;
                    cbSystemRoot = SYSTEM_ROOT_LENGTH;
                }
                else
                {
                    cbSystemRoot = 0;
                }

                LPWSTR pszTranslatedConsoleTitle;
                const auto cbTranslatedConsoleTitle = cbSystemRoot + cbConsoleTitle;
                Tmp = pszTranslatedConsoleTitle = (PWSTR) new BYTE[cbTranslatedConsoleTitle];
                if (pszTranslatedConsoleTitle == nullptr)
                {
                    return nullptr;
                }

                // No need to check return here -- pszTranslatedConsoleTitle is guaranteed large enough for SYSTEM_ROOT
                (void)StringCbCopy(pszTranslatedConsoleTitle, cbTranslatedConsoleTitle, SYSTEM_ROOT);
                pszTranslatedConsoleTitle += (cbSystemRoot / sizeof(WCHAR)); // skip by characters -- not bytes

                for (UINT i = 0; i < cbConsoleTitle; i += sizeof(WCHAR))
                {
#pragma prefast(suppress : 26018, "We are reading the null portion of the buffer on purpose and will escape on reaching it below.")
                    if (fSubstitute && *pwszConsoleTitle == '\\')
                    {
#pragma prefast(suppress : 26019, "Console title must contain system root if this path was followed.")
                        *pszTranslatedConsoleTitle++ = (WCHAR)'_';
                    }
                    else
                    {
                        *pszTranslatedConsoleTitle++ = *pwszConsoleTitle;
                        if (*pwszConsoleTitle == L'\0')
                        {
                            break;
                        }
                    }

                    pwszConsoleTitle++;
                }
            }
        }
        delete[] pwszSysRoot;
    }

    return Tmp;
}

[[nodiscard]] NTSTATUS GetConsoleLangId(const UINT uiOutputCP, _Out_ LANGID* const pLangId)
{
    auto Status = STATUS_NOT_SUPPORTED;

    // -- WARNING -- LOAD BEARING CODE --
    // Only attempt to return the Lang ID if the Windows ACP on console launch was an East Asian Code Page.
    // -
    // As of right now, this is a load bearing check and causes a domino effect of errors during OEM preinstallation if removed
    // resulting in a crash on launch of CMD.exe
    // (and consequently any scripts OEMs use to customize an image during the auditUser preinstall step inside their unattend.xml files.)
    // I have no reason to believe that removing this check causes any problems on any other SKU or scenario types.
    // -
    // Returning STATUS_NOT_SUPPORTED will skip a call to SetThreadLocale inside the Windows loader. This has the effect of not
    // setting the appropriate locale on the client end of the pipe, but also avoids the error.
    // Returning STATUS_SUCCESS will trigger the call to SetThreadLocale inside the loader.
    // This method is called on process launch by the loader and on every SetConsoleOutputCP call made from the client application to
    // maintain the synchrony of the client's Thread Locale state.
    // -
    // It is important to note that a comment exists inside the loader stating that DBCS code pages (CJK languages)
    // must have the SetThreadLocale synchronized with the console in order for FormatMessage to output correctly.
    // I'm not sure of the full validity of that comment at this point in time (Nov 2016), but the least risky thing is to trust it and revert
    // the behavior to this function until it can be otherwise proven.
    // -
    // See MSFT: 9808579 for the complete story on what happened here and why this must stay until the other dominos are resolved.
    // -
    // I would also highly advise against expanding the LANGIDs returned here or modifying them in any way until the cascading impacts
    // discovered in MSFT: 9808579 are vetted against any changes.
    // -- END WARNING --
    if (IsAvailableEastAsianCodePage(ServiceLocator::LocateGlobals().uiWindowsCP))
    {
        if (pLangId != nullptr)
        {
            switch (uiOutputCP)
            {
            case CP_JAPANESE:
                *pLangId = MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT);
                break;
            case CP_KOREAN:
                *pLangId = MAKELANGID(LANG_KOREAN, SUBLANG_KOREAN);
                break;
            case CP_CHINESE_SIMPLIFIED:
                *pLangId = MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED);
                break;
            case CP_CHINESE_TRADITIONAL:
                *pLangId = MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL);
                break;
            default:
                *pLangId = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
                break;
            }
        }
        Status = STATUS_SUCCESS;
    }

    return Status;
}

[[nodiscard]] HRESULT ApiRoutines::GetConsoleLangIdImpl(LANGID& langId) noexcept
{
    try
    {
        const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        // This fails a lot and it's totally expected. It only works for a few East Asian code pages.
        // As such, just return it. Do NOT use a wil macro here. It is very noisy.
        return HRESULT_FROM_NT(GetConsoleLangId(gci.OutputCP, &langId));
    }
    CATCH_RETURN();
}

// Routine Description:
// - This routine reads the connection information from a 'connect' IO, validates it and stores them in an internal format.
// - N.B. The internal connection contains information not sent by clients in their connect IOs and initialized by other routines.
// Arguments:
// - Server - Supplies a handle to the console server.
// - Message - Supplies the message representing the connect IO.
// - Cac - Receives the connection information.
// Return Value:
// - NTSTATUS indicating if the connection information was successfully initialized.
[[nodiscard]] NTSTATUS ConsoleInitializeConnectInfo(_In_ PCONSOLE_API_MSG Message, _Out_ PCONSOLE_API_CONNECTINFO Cac)
{
    CONSOLE_SERVER_MSG Data = { 0 };
    // Try to receive the data sent by the client.
    auto Status = NTSTATUS_FROM_HRESULT(Message->ReadMessageInput(0, &Data, sizeof(Data)));
    if (FAILED_NTSTATUS(Status))
    {
        return Status;
    }

    // Validate that strings are within the buffers and null-terminated.
    if ((Data.ApplicationNameLength > (sizeof(Data.ApplicationName) - sizeof(WCHAR))) ||
        (Data.TitleLength > (sizeof(Data.Title) - sizeof(WCHAR))) ||
        (Data.CurrentDirectoryLength > (sizeof(Data.CurrentDirectory) - sizeof(WCHAR))) ||
        (Data.ApplicationName[Data.ApplicationNameLength / sizeof(WCHAR)] != UNICODE_NULL) ||
        (Data.Title[Data.TitleLength / sizeof(WCHAR)] != UNICODE_NULL) || (Data.CurrentDirectory[Data.CurrentDirectoryLength / sizeof(WCHAR)] != UNICODE_NULL))
    {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    // Initialize (partially) the connect info with the received data.
    FAIL_FAST_IF(!(sizeof(Cac->AppName) == sizeof(Data.ApplicationName)));
    FAIL_FAST_IF(!(sizeof(Cac->Title) == sizeof(Data.Title)));
    FAIL_FAST_IF(!(sizeof(Cac->CurDir) == sizeof(Data.CurrentDirectory)));

    // unused(Data.IconId)
    Cac->ConsoleInfo.SetHotKey(Data.HotKey);
    Cac->ConsoleInfo.SetStartupFlags(Data.StartupFlags);
    Cac->ConsoleInfo.SetFillAttribute(Data.FillAttribute);
    Cac->ConsoleInfo.SetShowWindow(Data.ShowWindow);
    Cac->ConsoleInfo.SetScreenBufferSize(til::wrap_coord_size(Data.ScreenBufferSize));
    Cac->ConsoleInfo.SetWindowSize(til::wrap_coord_size(Data.WindowSize));
    Cac->ConsoleInfo.SetWindowOrigin(til::wrap_coord_size(Data.WindowOrigin));
    Cac->ProcessGroupId = Data.ProcessGroupId;
    Cac->ConsoleApp = Data.ConsoleApp;
    Cac->WindowVisible = Data.WindowVisible;
    Cac->TitleLength = Data.TitleLength;
    Cac->AppNameLength = Data.ApplicationNameLength;
    Cac->CurDirLength = Data.CurrentDirectoryLength;

    memmove(Cac->AppName, Data.ApplicationName, sizeof(Cac->AppName));
    memmove(Cac->Title, Data.Title, sizeof(Cac->Title));
    memmove(Cac->CurDir, Data.CurrentDirectory, sizeof(Cac->CurDir));

    return STATUS_SUCCESS;
}

[[nodiscard]] bool ConsoleConnectionDeservesVisibleWindow(PCONSOLE_API_CONNECTINFO p)
{
    auto& g = ServiceLocator::LocateGlobals();
    // processes that are created ...
    //  ... with CREATE_NO_WINDOW never get a window.
    //  ... on Desktop, with a visible window always get one (even a fake one)
    //  ... not on Desktop, with a visible window only get one if we are headful (not ConPTY).
    //  This prevents pseudoconsole-hosted applications from taking over the screen,
    //  even if they really beg us for a window.
    return p->WindowVisible && (s_IsOnDesktop() || !g.IsHeadless());
}

[[nodiscard]] NTSTATUS ConsoleAllocateConsole(PCONSOLE_API_CONNECTINFO p)
{
    // AllocConsole is outside our codebase, but we should be able to mostly track the call here.
    auto& g = ServiceLocator::LocateGlobals();

    auto& gci = g.getConsoleInformation();

    // No matter what, create a renderer.
    try
    {
        if (!gci.IsInVtIoMode())
        {
            g.pRender = new Renderer(gci.GetRenderSettings(), &gci.renderData);

            // Set up the renderer to be used to calculate the width of a glyph,
            //      should we be unable to figure out its width another way.
            CodepointWidthDetector::Singleton().SetFallbackMethod([](const std::wstring_view& glyph) {
                return ServiceLocator::LocateGlobals().pRender->IsGlyphWideByFont(glyph);
            });
        }
    }
    catch (...)
    {
        return NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
    }

    // Note that the order of initialization is important here. SetUpConsole is
    // where the TextBuffer is created (ultimately in the SCREEN_INFORMATION
    // CreateInstance method), and the TextBuffer needs to be constructed with
    // a reference to the renderer, so the renderer must be created first.
    auto Status = SetUpConsole(&p->ConsoleInfo, p->TitleLength, p->Title, p->CurDir, p->AppName);
    if (FAILED_NTSTATUS(Status))
    {
        return Status;
    }

    // Allow the renderer to paint once the rest of the console is hooked up.
    if (g.pRender)
    {
        g.pRender->EnablePainting();
    }

    if (SUCCEEDED_NTSTATUS(Status) && ConsoleConnectionDeservesVisibleWindow(p))
    {
        HANDLE Thread = nullptr;

        IConsoleInputThread* pNewThread = nullptr;
        LOG_IF_FAILED(ServiceLocator::CreateConsoleInputThread(&pNewThread));

        FAIL_FAST_IF_NULL(pNewThread);

        Thread = pNewThread->Start();
        if (Thread == nullptr)
        {
            Status = STATUS_NO_MEMORY;
        }
        else
        {
            ServiceLocator::LocateGlobals().dwInputThreadId = pNewThread->GetThreadId();

            // The ConsoleInputThread needs to lock the console so we must first unlock it ourselves.
            UnlockConsole();
            g.hConsoleInputInitEvent.wait();
            LockConsole();

            // OK, we've been told that the input thread is done initializing under lock.
            // Cleanup the handles and events we used to maintain our virtual lock passing dance.

            CloseHandle(Thread); // This doesn't stop the thread from running.

            if (FAILED_NTSTATUS(g.ntstatusConsoleInputInitStatus))
            {
                Status = g.ntstatusConsoleInputInitStatus;
            }
            else
            {
                Status = STATUS_SUCCESS;
            }

            // If we're not headless, we'll make a real window.
            // Allow UI Access to the real window but not the little
            // fake window we would make in headless mode.
            if (!g.launchArgs.IsHeadless())
            {
                /*
                 * Tell driver to allow clients with UIAccess to connect
                 * to this server even if the security descriptor doesn't
                 * allow it.
                 *
                 * N.B. This allows applications like narrator.exe to have
                 *      access to the console. This is ok because they already
                 *      have access to the console window anyway - this function
                 *      is only called when a window is created.
                 */

                LOG_IF_FAILED(g.pDeviceComm->AllowUIAccess());
            }
        }
    }

    // Potentially start the VT IO (if needed)
    // Make sure to do this after the i/o buffers have been created.
    // We'll need the size of the screen buffer in the vt i/o initialization
    if (SUCCEEDED_NTSTATUS(Status))
    {
        // Actually start the VT I/O threads
        auto hr = gci.GetVtIo()->StartIfNeeded();
        // Don't convert S_FALSE to an NTSTATUS - the equivalent NTSTATUS
        //      is treated as an error
        if (FAILED(hr))
        {
            Status = NTSTATUS_FROM_HRESULT(hr);
        }
    }

    return Status;
}

// Routine Description:
// - This routine is the main one in the console server IO thread.
// - It reads IO requests submitted by clients through the driver, services and completes them in a loop.
// Arguments:
// - lpParameter - PCONSOLE_API_MSG being handed off to us from the previous I/O.
// Return Value:
// - This routine never returns. The process exits when no more references or clients exist.
DWORD WINAPI ConsoleIoThread(LPVOID lpParameter)
{
    auto& globals = ServiceLocator::LocateGlobals();

    CONSOLE_API_MSG ReceiveMsg;
    ReceiveMsg._pApiRoutines = globals.api;
    ReceiveMsg._pDeviceComm = globals.pDeviceComm;
    PCONSOLE_API_MSG ReplyMsg = nullptr;

    // If we were given a message on startup, process that in our context and then continue with the IO loop normally.
    if (lpParameter)
    {
        // Capture the incoming lpParameter into a unique_ptr so we can appropriately
        // free the heap memory when we're done getting the important bits out of it below.
        std::unique_ptr<CONSOLE_API_MSG> capturedMessage{ static_cast<PCONSOLE_API_MSG>(lpParameter) };

        ReceiveMsg = *capturedMessage.get();
        ReceiveMsg._pApiRoutines = globals.api;
        ReceiveMsg._pDeviceComm = globals.pDeviceComm;
        IoSorter::ServiceIoOperation(&ReceiveMsg, &ReplyMsg);
    }

    auto fShouldExit = false;
    while (!fShouldExit)
    {
        if (ReplyMsg != nullptr)
        {
            ReplyMsg->ReleaseMessageBuffers();
        }

        // TODO: 9115192 correct mixed NTSTATUS/HRESULT
        auto hr = ServiceLocator::LocateGlobals().pDeviceComm->ReadIo(ReplyMsg, &ReceiveMsg);
        if (FAILED(hr))
        {
            if (hr == HRESULT_FROM_WIN32(ERROR_PIPE_NOT_CONNECTED))
            {
                fShouldExit = true;

                // This will not return. Terminate immediately when disconnected.
                ServiceLocator::RundownAndExit(STATUS_SUCCESS);
            }
            LOG_HR_MSG(hr, "DeviceIoControl failed");
            ReplyMsg = nullptr;
            continue;
        }
        ReceiveMsg._pApiRoutines = globals.api;
        IoSorter::ServiceIoOperation(&ReceiveMsg, &ReplyMsg);
    }

    return 0;
}
