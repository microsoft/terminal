// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "../server/DeviceHandle.h"
#include "../server/winbasep.h"
__declspec(dllexport) BOOL WINAPI EnableChildWindowDpiMessage(HWND, BOOL)
{
    return FALSE;
}

[[nodiscard]] static inline NTSTATUS CreateClientHandle(PHANDLE Handle, HANDLE ServerHandle, PCWSTR Name, BOOLEAN Inheritable)
{
    return DeviceHandle::CreateClientHandle(Handle, ServerHandle, Name, Inheritable);
}

[[nodiscard]] static inline NTSTATUS CreateServerHandle(PHANDLE Handle, BOOLEAN Inheritable)
{
    return DeviceHandle::CreateServerHandle(Handle, Inheritable);
}

[[nodiscard]] static HRESULT StartConsoleHostEXE(PCWSTR conhostPath, HANDLE serverHandle)
{
    STARTUPINFOEX StartupInformation = { 0 };
    StartupInformation.StartupInfo.cb = sizeof(STARTUPINFOEX);
    StartupInformation.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    StartupInformation.StartupInfo.hStdInput = nullptr;
    StartupInformation.StartupInfo.hStdOutput = nullptr;
    StartupInformation.StartupInfo.hStdError = nullptr;

    SIZE_T AttributeListSize;
    InitializeProcThreadAttributeList(NULL,
                                      1,
                                      0,
                                      &AttributeListSize);

    // Alloc space
    wistd::unique_ptr<BYTE[]> AttributeList = wil::make_unique_nothrow<BYTE[]>(AttributeListSize);
    RETURN_IF_NULL_ALLOC(AttributeList);

    StartupInformation.lpAttributeList = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(AttributeList.get());

    // Call second time to actually initialize space.
    RETURN_IF_WIN32_BOOL_FALSE(InitializeProcThreadAttributeList(StartupInformation.lpAttributeList,
                                                                 1, // This represents the length of the list. We will call UpdateProcThreadAttribute twice so this is 2.
                                                                 0,
                                                                 &AttributeListSize));
    // Set cleanup data for ProcThreadAttributeList when successful.
    auto CleanupProcThreadAttribute = wil::scope_exit([&] {
        DeleteProcThreadAttributeList(StartupInformation.lpAttributeList);
    });

    // UpdateProcThreadAttributes wants this as a bare array of handles and doesn't like our smart structures,
    // so set it up for its use.
    HANDLE HandleList[1];
    HandleList[0] = serverHandle;

    RETURN_IF_WIN32_BOOL_FALSE(UpdateProcThreadAttribute(StartupInformation.lpAttributeList,
                                                         0,
                                                         PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                                         &HandleList[0],
                                                         sizeof HandleList,
                                                         NULL,
                                                         NULL));

    if (!conhostPath || wcslen(conhostPath) == 0)
    {
        conhostPath = L"\\\\?\\%WINDIR%\\System32\\conhost.exe";
    }

    // Expand any environment variables present in the command line string.
    std::wstring cmdLine = wil::ExpandEnvironmentStringsW<std::wstring>(conhostPath);
    std::wstring expandedConhostPath{ cmdLine };
    cmdLine += wil::str_printf<std::wstring>(L" 0x%x", reinterpret_cast<uintptr_t>(HandleList[0]));
    // Call create process
    wil::unique_process_information ProcessInformation;
    RETURN_IF_WIN32_BOOL_FALSE(CreateProcessW(expandedConhostPath.data(),
                                              cmdLine.data(),
                                              NULL,
                                              NULL,
                                              TRUE,
                                              EXTENDED_STARTUPINFO_PRESENT,
                                              NULL,
                                              NULL,
                                              &StartupInformation.StartupInfo,
                                              ProcessInformation.addressof()));

    return S_OK;
}

[[nodiscard]] static HRESULT StartConsoleHostDLL(PCWSTR conhostPath, HANDLE serverHandle)
{
    typedef NTSTATUS (*PFNCONSOLECREATEIOTHREAD)(__in HANDLE Server);
    wil::unique_hmodule conhost{ LoadLibrary(conhostPath) };
    RETURN_LAST_ERROR_IF_NULL(conhost);
    PFNCONSOLECREATEIOTHREAD pfnConsoleCreateIoThread = (PFNCONSOLECREATEIOTHREAD)GetProcAddress(conhost.get(), "ConsoleCreateIoThread");
    RETURN_HR_IF_NULL(E_UNEXPECTED, pfnConsoleCreateIoThread);
    RETURN_IF_NTSTATUS_FAILED(pfnConsoleCreateIoThread(serverHandle));
    conhost.release(); // keep the module loaded
    return S_OK;
}

[[nodiscard]] static HRESULT StartConsoleHost(PCWSTR conhostPath, HANDLE serverHandle, bool& keepRunning)
{
    if (!conhostPath || conhostPath[0] == L'\0')
    {
        return StartConsoleHostEXE(conhostPath, serverHandle);
    }

    std::filesystem::path p{ conhostPath };
    auto ext{ p.extension() };
    if (ext == L".dll" || ext == L".DLL")
    {
        keepRunning = true;
        return StartConsoleHostDLL(conhostPath, serverHandle);
    }
    return StartConsoleHostEXE(conhostPath, serverHandle);
}

[[nodiscard]] static HRESULT
StartConsoleForCmdLine(PCWSTR conhostPath, PCWSTR pwszCmdLine)
{
    bool keepRunning{ false };
    // Create a scope because we're going to exit thread if everything goes well.
    // This scope will ensure all C++ objects and smart pointers get a chance to destruct before ExitThread is called.
    {
        // Create the server and reference handles and create the console object.
        wil::unique_handle ServerHandle;
        RETURN_IF_NTSTATUS_FAILED(CreateServerHandle(ServerHandle.addressof(), TRUE));

        wil::unique_handle ReferenceHandle;
        RETURN_IF_NTSTATUS_FAILED(CreateClientHandle(ReferenceHandle.addressof(),
                                                     ServerHandle.get(),
                                                     L"\\Reference",
                                                     FALSE));

        RETURN_IF_FAILED(StartConsoleHost(conhostPath, ServerHandle.get(), keepRunning));
        //if (!keepRunning)
        {
            Sleep(500); // let the host quiesce
        }

        auto hServer = ServerHandle.release();

        // Now that the console object was created, we're in a state that lets us
        // create the default io objects.
        wil::unique_handle ClientHandle[3];

        // Input
        RETURN_IF_NTSTATUS_FAILED(CreateClientHandle(ClientHandle[0].addressof(),
                                                     hServer,
                                                     L"\\Input",
                                                     TRUE));

        // Output
        RETURN_IF_NTSTATUS_FAILED(CreateClientHandle(ClientHandle[1].addressof(),
                                                     hServer,
                                                     L"\\Output",
                                                     TRUE));

        // Error is a copy of Output
        RETURN_IF_WIN32_BOOL_FALSE(DuplicateHandle(GetCurrentProcess(),
                                                   ClientHandle[1].get(),
                                                   GetCurrentProcess(),
                                                   ClientHandle[2].addressof(),
                                                   0,
                                                   TRUE,
                                                   DUPLICATE_SAME_ACCESS));

        // Create the child process. We will temporarily overwrite the values in the
        // PEB to force them to be inherited.

        STARTUPINFOEX StartupInformation = { 0 };
        StartupInformation.StartupInfo.cb = sizeof(STARTUPINFOEX);
        StartupInformation.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        StartupInformation.StartupInfo.hStdInput = ClientHandle[0].get();
        StartupInformation.StartupInfo.hStdOutput = ClientHandle[1].get();
        StartupInformation.StartupInfo.hStdError = ClientHandle[2].get();

        // Get the parent startup info for this process. It might contain LNK data we need to pass to the child.
        {
            STARTUPINFO HostStartupInfo = { 0 };
            HostStartupInfo.cb = sizeof(STARTUPINFO);
            GetStartupInfoW(&HostStartupInfo);

            // Pass the title we were started with down to our child process.
            // Conhost itself absolutely doesn't care about this value, but the
            // child might.
            StartupInformation.StartupInfo.lpTitle = HostStartupInfo.lpTitle;
            // If we were started with Title is Link Name, then pass the flag
            // down to the child. (the link name was already passed down above)
            if (WI_IsFlagSet(HostStartupInfo.dwFlags, STARTF_TITLEISLINKNAME))
            {
                StartupInformation.StartupInfo.dwFlags |= STARTF_TITLEISLINKNAME;
            }
        }

        // Create the extended attributes list that will pass the console server information into the child process.

        // Call first time to find size
        SIZE_T AttributeListSize;
        InitializeProcThreadAttributeList(NULL,
                                          2,
                                          0,
                                          &AttributeListSize);

        // Alloc space
        wistd::unique_ptr<BYTE[]> AttributeList = wil::make_unique_nothrow<BYTE[]>(AttributeListSize);
        RETURN_IF_NULL_ALLOC(AttributeList);

        StartupInformation.lpAttributeList = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(AttributeList.get());

        // Call second time to actually initialize space.
        RETURN_IF_WIN32_BOOL_FALSE(InitializeProcThreadAttributeList(StartupInformation.lpAttributeList,
                                                                     2, // This represents the length of the list. We will call UpdateProcThreadAttribute twice so this is 2.
                                                                     0,
                                                                     &AttributeListSize));
        // Set cleanup data for ProcThreadAttributeList when successful.
        auto CleanupProcThreadAttribute = wil::scope_exit([&] {
            DeleteProcThreadAttributeList(StartupInformation.lpAttributeList);
        });

        RETURN_IF_WIN32_BOOL_FALSE(UpdateProcThreadAttribute(StartupInformation.lpAttributeList,
                                                             0,
                                                             PROC_THREAD_ATTRIBUTE_CONSOLE_REFERENCE,
                                                             ReferenceHandle.addressof(),
                                                             sizeof(HANDLE),
                                                             NULL,
                                                             NULL));

        // UpdateProcThreadAttributes wants this as a bare array of handles and doesn't like our smart structures,
        // so set it up for its use.
        HANDLE HandleList[3];
        HandleList[0] = StartupInformation.StartupInfo.hStdInput;
        HandleList[1] = StartupInformation.StartupInfo.hStdOutput;
        HandleList[2] = StartupInformation.StartupInfo.hStdError;

        RETURN_IF_WIN32_BOOL_FALSE(UpdateProcThreadAttribute(StartupInformation.lpAttributeList,
                                                             0,
                                                             PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                                             &HandleList[0],
                                                             sizeof HandleList,
                                                             NULL,
                                                             NULL));

        // We have to copy the command line string we're given because CreateProcessW has to be called with mutable data.
        if (!pwszCmdLine || wcslen(pwszCmdLine) == 0)
        {
            // If they didn't give us one, just launch cmd.exe.
            pwszCmdLine = L"%WINDIR%\\system32\\cmd.exe";
        }

        // Expand any environment variables present in the command line string.
        std::wstring cmdLine = wil::ExpandEnvironmentStringsW<std::wstring>(pwszCmdLine);
        // Call create process
        wil::unique_process_information ProcessInformation;
        RETURN_IF_WIN32_BOOL_FALSE(CreateProcessW(NULL,
                                                  cmdLine.data(),
                                                  NULL,
                                                  NULL,
                                                  TRUE,
                                                  EXTENDED_STARTUPINFO_PRESENT,
                                                  NULL,
                                                  NULL,
                                                  &StartupInformation.StartupInfo,
                                                  ProcessInformation.addressof()));
    }

    // Exit the process since we've done our jobs
    if (!keepRunning)
    {
        ExitProcess(S_OK);
    }

    return S_OK;
}

int CALLBACK wWinMain(
    _In_ HINSTANCE /*hInstance*/,
    _In_ HINSTANCE /*hPrevInstance*/,
    _In_ PWSTR /*pwszCmdLine*/,
    _In_ int /*nCmdShow*/)
{
    int argc{ 0 };
    auto commandline{ GetCommandLineW() };
    auto argv{ CommandLineToArgvW(commandline, &argc) };
    PCWSTR conhost = argc >= 2 ? argv[1] : nullptr;
    PCWSTR cmdLine = argc >= 3 ? argv[2] : nullptr;
    THROW_IF_FAILED(StartConsoleForCmdLine(conhost, cmdLine));
    SetProcessShutdownParameters(0, 0);
    ExitThread(S_OK);
}
