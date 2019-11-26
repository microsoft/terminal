// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "winconpty.h"

#ifdef __INSIDE_WINDOWS
#include <strsafe.h>
#include <consoleinternal.h>
// You need kernelbasestaging.h to be able to use wil in libraries consumed by kernelbase.dll
#include <kernelbasestaging.h>
#define RESOURCE_SUPPRESS_STL
#define WIL_SUPPORT_BITOPERATION_PASCAL_NAMES
#include <wil/resource.h>
#else
#include "device.h"
#include <filesystem>
#endif // __INSIDE_WINDOWS

#pragma warning(push)
#pragma warning(disable : 4273) // inconsistent dll linkage (we are exporting things kernel32 also exports)

// Function Description:
// - Returns the path to either conhost.exe or the side-by-side OpenConsole, depending on whether this
//   module is building with Windows.
// Return Value:
// - A pointer to permanent storage containing the path to the console host.
static wchar_t* _ConsoleHostPath()
{
    // Use the magic of magic statics to only calculate this once.
    static wil::unique_process_heap_string consoleHostPath = []() {
#ifdef __INSIDE_WINDOWS
        wil::unique_process_heap_string systemDirectory;
        wil::GetSystemDirectoryW<wil::unique_process_heap_string>(systemDirectory);
        return wil::str_concat_failfast<wil::unique_process_heap_string>(L"\\\\?\\", systemDirectory, L"\\conhost.exe");
#else
        // Use the STL only if we're not building in Windows.
        std::filesystem::path modulePath{ wil::GetModuleFileNameW<std::wstring>(wil::GetModuleInstanceHandle()) };
        modulePath.replace_filename(L"OpenConsole.exe");
        auto modulePathAsString{ modulePath.wstring() };
        return wil::make_process_heap_string_nothrow(modulePathAsString.data(), modulePathAsString.size());
#endif // __INSIDE_WINDOWS
    }();
    return consoleHostPath.get();
}

static bool _HandleIsValid(HANDLE h) noexcept
{
    return (h != INVALID_HANDLE_VALUE) && (h != nullptr);
}

HRESULT _CreatePseudoConsole(const HANDLE hToken,
                             const COORD size,
                             const HANDLE hInput,
                             const HANDLE hOutput,
                             const DWORD dwFlags,
                             _Inout_ PseudoConsole* pPty)
{
    if (pPty == NULL)
    {
        return E_INVALIDARG;
    }
    if (size.X == 0 || size.Y == 0)
    {
        return E_INVALIDARG;
    }

    wil::unique_handle serverHandle;
    RETURN_IF_NTSTATUS_FAILED(CreateServerHandle(serverHandle.addressof(), TRUE));

    wil::unique_handle signalPipeConhostSide;
    wil::unique_handle signalPipeOurSide;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    // Mark inheritable for signal handle when creating. It'll have the same value on the other side.
    sa.bInheritHandle = FALSE;
    sa.lpSecurityDescriptor = NULL;

    RETURN_IF_WIN32_BOOL_FALSE(CreatePipe(signalPipeConhostSide.addressof(), signalPipeOurSide.addressof(), &sa, 0));
    RETURN_IF_WIN32_BOOL_FALSE(SetHandleInformation(signalPipeConhostSide.get(), HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT));

    const wchar_t* pwszFormat = L"%s --headless %s--width %hu --height %hu --signal 0x%x --server 0x%x";
    // This is plenty of space to hold the formatted string
    wchar_t cmd[MAX_PATH];
    const BOOL bInheritCursor = (dwFlags & PSEUDOCONSOLE_INHERIT_CURSOR) == PSEUDOCONSOLE_INHERIT_CURSOR;
    swprintf_s(cmd,
               MAX_PATH,
               pwszFormat,
               _ConsoleHostPath(),
               bInheritCursor ? L"--inheritcursor " : L"",
               size.X,
               size.Y,
               signalPipeConhostSide.get(),
               serverHandle.get());

    STARTUPINFOEXW siEx{ 0 };
    siEx.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    siEx.StartupInfo.hStdInput = hInput;
    siEx.StartupInfo.hStdOutput = hOutput;
    siEx.StartupInfo.hStdError = hOutput;
    siEx.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;

    // Only pass the handles we actually want the conhost to know about to it:
    const size_t INHERITED_HANDLES_COUNT = 4;
    HANDLE inheritedHandles[INHERITED_HANDLES_COUNT];
    inheritedHandles[0] = serverHandle.get();
    inheritedHandles[1] = hInput;
    inheritedHandles[2] = hOutput;
    inheritedHandles[3] = signalPipeConhostSide.get();

    // Get the size of the attribute list. We need one attribute, the handle list.
    SIZE_T listSize = 0;
    InitializeProcThreadAttributeList(NULL, 1, 0, &listSize);

    // I have to use a HeapAlloc here because kernelbase can't link new[] or delete[]
    PPROC_THREAD_ATTRIBUTE_LIST attrList = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, listSize));
    RETURN_IF_NULL_ALLOC(attrList);
    auto attrListDelete = wil::scope_exit([&] {
        HeapFree(GetProcessHeap(), 0, attrList);
    });

    siEx.lpAttributeList = attrList;
    RETURN_IF_WIN32_BOOL_FALSE(InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, &listSize));
    // Set cleanup data for ProcThreadAttributeList when successful.
    auto cleanupProcThreadAttribute = wil::scope_exit([&] {
        DeleteProcThreadAttributeList(siEx.lpAttributeList);
    });
    RETURN_IF_WIN32_BOOL_FALSE(UpdateProcThreadAttribute(siEx.lpAttributeList,
                                                         0,
                                                         PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                                         inheritedHandles,
                                                         (INHERITED_HANDLES_COUNT * sizeof(HANDLE)),
                                                         NULL,
                                                         NULL));
    wil::unique_process_information pi;
    { // wow64 disabled filesystem redirection scope
#if defined(BUILD_WOW6432)
        PVOID RedirectionFlag;
        RETURN_IF_NTSTATUS_FAILED(RtlWow64EnableFsRedirectionEx(
            WOW64_FILE_SYSTEM_DISABLE_REDIRECT,
            &RedirectionFlag));
        auto resetFsRedirection = wil::scope_exit([&] {
            RtlWow64EnableFsRedirectionEx(RedirectionFlag, &RedirectionFlag);
        });
#endif
        if (hToken == INVALID_HANDLE_VALUE || hToken == NULL)
        {
            // Call create process
            RETURN_IF_WIN32_BOOL_FALSE(CreateProcessW(NULL,
                                                      cmd,
                                                      NULL,
                                                      NULL,
                                                      TRUE,
                                                      EXTENDED_STARTUPINFO_PRESENT,
                                                      NULL,
                                                      NULL,
                                                      &siEx.StartupInfo,
                                                      pi.addressof()));
        }
        else
        {
            // Call create process
            RETURN_IF_WIN32_BOOL_FALSE(CreateProcessAsUserW(hToken,
                                                            NULL,
                                                            cmd,
                                                            NULL,
                                                            NULL,
                                                            TRUE,
                                                            EXTENDED_STARTUPINFO_PRESENT,
                                                            NULL,
                                                            NULL,
                                                            &siEx.StartupInfo,
                                                            pi.addressof()));
        }
    }

    // Move the process handle out of the PROCESS_INFORMATION into out Pseudoconsole
    pPty->hConPtyProcess = pi.hProcess;
    pi.hProcess = NULL;

    RETURN_IF_NTSTATUS_FAILED(CreateClientHandle(&pPty->hPtyReference,
                                                 serverHandle.get(),
                                                 L"\\Reference",
                                                 FALSE));

    pPty->hSignal = signalPipeOurSide.release();

    return S_OK;
}

// Function Description:
// - Resizes the conpty
// Arguments:
// - hSignal: A signal pipe as returned by CreateConPty.
// - size: The new dimenstions of the conpty, in characters.
// Return Value:
// - S_OK if the call succeeded, else an appropriate HRESULT for failing to
//      write the resize message to the pty.
HRESULT _ResizePseudoConsole(_In_ const PseudoConsole* const pPty, _In_ const COORD size)
{
    if (pPty == NULL)
    {
        return E_INVALIDARG;
    }

    unsigned short signalPacket[3];
    signalPacket[0] = PTY_SIGNAL_RESIZE_WINDOW;
    signalPacket[1] = size.X;
    signalPacket[2] = size.Y;

    BOOL fSuccess = WriteFile(pPty->hSignal, signalPacket, sizeof(signalPacket), NULL, NULL);
    return fSuccess ? S_OK : HRESULT_FROM_WIN32(GetLastError());
}

// Function Description:
// - This closes each of the members of a PseudoConsole. It does not free the
//      data associated with the PseudoConsole. This is helpful for testing,
//      where we might stack allocate a PseudoConsole (instead of getting a
//      HPCON via the API).
// Arguments:
// - pPty: A pointer to a PseudoConsole struct.
// Return Value:
// - <none>
void _ClosePseudoConsoleMembers(_In_ PseudoConsole* pPty)
{
    if (pPty != NULL)
    {
        // See MSFT:19918626
        // First break the signal pipe - this will trigger conhost to tear itself down
        if (_HandleIsValid(pPty->hSignal))
        {
            CloseHandle(pPty->hSignal);
            pPty->hSignal = 0;
        }
        // Then, wait on the conhost process before killing it.
        // We do this to make sure the conhost finishes flushing any output it
        //      has yet to send before we hard kill it.
        if (_HandleIsValid(pPty->hConPtyProcess))
        {
            // If the conhost is already dead, then that's fine. Presumably
            //      it's finished flushing it's output already.
            DWORD dwExit = 0;
            // If GetExitCodeProcess failed, it's likely conhost is already dead
            //      If so, skip waiting regardless of whatever error
            //      GetExitCodeProcess returned.
            //      We'll just go straight to killing conhost.
            if (GetExitCodeProcess(pPty->hConPtyProcess, &dwExit) && dwExit == STILL_ACTIVE)
            {
                WaitForSingleObject(pPty->hConPtyProcess, INFINITE);
            }

            TerminateProcess(pPty->hConPtyProcess, 0);
            pPty->hConPtyProcess = 0;
        }
        // Then take care of the reference handle.
        // TODO GH#1810: Closing the reference handle late leaves conhost thinking
        // that we have an outstanding connected client.
        if (_HandleIsValid(pPty->hPtyReference))
        {
            CloseHandle(pPty->hPtyReference);
            pPty->hPtyReference = 0;
        }
    }
}

// Function Description:
// - This closes each of the members of a PseudoConsole, and HeapFree's the
//      memory allocated to it. This should be used to cleanup any
//      PseudoConosles that were created with CreatePseudoConsole.
// Arguments:
// - pPty: A pointer to a PseudoConsole struct.
// Return Value:
// - <none>
VOID _ClosePseudoConsole(_In_ PseudoConsole* pPty)
{
    if (pPty != NULL)
    {
        _ClosePseudoConsoleMembers(pPty);
        HeapFree(GetProcessHeap(), 0, pPty);
    }
}

// These functions are defined in the console l1 apiset, which is generated from
//      the consoleapi.apx file in minkernel\apiset\libs\Console.

// Function Description:
// Creates a "Pseudo-console" (conpty) with dimensions (in characters)
//      provided by the `size` parameter. The caller should provide two handles:
// - `hInput` is used for writing input to the pty, encoded as UTF-8 and VT sequences.
// - `hOutput` is used for reading the output of the pty, encoded as UTF-8 and VT sequences.
// Once the call completes, `phPty` will receive a token value to identify this
//      conpty object. This value should be used in conjunction with the other
//      Pseudoconsole API's.
// `dwFlags` is used to specify optional behavior to the created pseudoconsole.
// The flags can be combinations of the following values:
//  INHERIT_CURSOR: This will cause the created conpty to attempt to inherit the
//      cursor position of the parent terminal application. This can be useful
//      for applications like `ssh`, where ssh (currently running in a terminal)
//      might want to create a pseudoterminal session for an child application
//      and the child inherit the cursor position of ssh.
//      The creted conpty will immediately emit a "Device Status Request" VT
//      sequence to hOutput, that should be replied to on hInput in the format
//      "\x1b[<r>;<c>R", where `<r>` is the row and `<c>` is the column of the
//      cursor position.
//      This requires a cooperating terminal application - if a caller does not
//      reply to this message, the conpty will not process any input until it
//      does. Most *nix terminals and the Windows Console (after Windows 10
//      Anniversary Update) will be able to handle such a message.

extern "C" HRESULT WINAPI ConptyCreatePseudoConsole(_In_ COORD size,
                                                    _In_ HANDLE hInput,
                                                    _In_ HANDLE hOutput,
                                                    _In_ DWORD dwFlags,
                                                    _Out_ HPCON* phPC)
{
    return ConptyCreatePseudoConsoleAsUser(INVALID_HANDLE_VALUE, size, hInput, hOutput, dwFlags, phPC);
}

extern "C" HRESULT ConptyCreatePseudoConsoleAsUser(_In_ HANDLE hToken,
                                                   _In_ COORD size,
                                                   _In_ HANDLE hInput,
                                                   _In_ HANDLE hOutput,
                                                   _In_ DWORD dwFlags,
                                                   _Out_ HPCON* phPC)
{
    if (phPC == NULL)
    {
        return E_INVALIDARG;
    }
    *phPC = NULL;
    if ((!_HandleIsValid(hInput)) && (!_HandleIsValid(hOutput)))
    {
        return E_INVALIDARG;
    }

    PseudoConsole* pPty = (PseudoConsole*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(PseudoConsole));
    RETURN_IF_NULL_ALLOC(pPty);
    auto cleanupPty = wil::scope_exit([&] {
        _ClosePseudoConsole(pPty);
    });

    wil::unique_handle duplicatedInput;
    wil::unique_handle duplicatedOutput;
    RETURN_IF_WIN32_BOOL_FALSE(DuplicateHandle(GetCurrentProcess(), hInput, GetCurrentProcess(), duplicatedInput.addressof(), 0, TRUE, DUPLICATE_SAME_ACCESS));
    RETURN_IF_WIN32_BOOL_FALSE(DuplicateHandle(GetCurrentProcess(), hOutput, GetCurrentProcess(), duplicatedOutput.addressof(), 0, TRUE, DUPLICATE_SAME_ACCESS));

    RETURN_IF_FAILED(_CreatePseudoConsole(hToken, size, duplicatedInput.get(), duplicatedOutput.get(), dwFlags, pPty));

    *phPC = (HPCON)pPty;
    cleanupPty.release();

    return S_OK;
}

// Function Description:
// Resizes the given conpty to the specified size, in characters.
extern "C" HRESULT WINAPI ConptyResizePseudoConsole(_In_ HPCON hPC, _In_ COORD size)
{
    PseudoConsole* const pPty = (PseudoConsole*)hPC;
    HRESULT hr = pPty == NULL ? E_INVALIDARG : S_OK;
    if (SUCCEEDED(hr))
    {
        hr = _ResizePseudoConsole(pPty, size);
    }
    return hr;
}

// Function Description:
// Closes the conpty and all associated state.
// Client applications attached to the conpty will also behave as though the
//      console window they were running in was closed.
// This can fail if the conhost hosting the pseudoconsole failed to be
//      terminated, or if the pseudoconsole was already terminated.
extern "C" VOID WINAPI ConptyClosePseudoConsole(_In_ HPCON hPC)
{
    PseudoConsole* const pPty = (PseudoConsole*)hPC;
    if (pPty != NULL)
    {
        _ClosePseudoConsole(pPty);
    }
}

#pragma warning(pop)
