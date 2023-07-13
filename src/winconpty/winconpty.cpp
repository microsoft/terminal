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
#pragma warning(disable : 26485) // array-to-pointer decay is virtually impossible to avoid when we can't use STL.

// Function Description:
// - Returns the path to conhost.exe as a process heap string.
static wil::unique_process_heap_string _InboxConsoleHostPath()
{
    wil::unique_process_heap_string systemDirectory;
    wil::GetSystemDirectoryW<wil::unique_process_heap_string>(systemDirectory);
    return wil::str_concat_failfast<wil::unique_process_heap_string>(L"\\\\?\\", systemDirectory, L"\\conhost.exe");
}

// Function Description:
// - Returns the path to either conhost.exe or the side-by-side OpenConsole, depending on whether this
//   module is building with Windows and OpenConsole could be found.
// Return Value:
// - A pointer to permanent storage containing the path to the console host.
static wchar_t* _ConsoleHostPath()
{
    // Use the magic of magic statics to only calculate this once.
    static auto consoleHostPath = []() {
#if defined(__INSIDE_WINDOWS)
        return _InboxConsoleHostPath();
#else
        // Use the STL only if we're not building in Windows.
        std::filesystem::path modulePath{ wil::GetModuleFileNameW<std::wstring>(wil::GetModuleInstanceHandle()) };
        modulePath.replace_filename(L"OpenConsole.exe");
        if (!std::filesystem::exists(modulePath))
        {
            std::wstring_view architectureInfix{};
            USHORT unusedImageFileMachine{}, nativeMachine{};
            if (IsWow64Process2(GetCurrentProcess(), &unusedImageFileMachine, &nativeMachine))
            {
                // Despite being a machine type, the values IsWow64Process2 returns are *image* types
                switch (nativeMachine)
                {
                case IMAGE_FILE_MACHINE_AMD64:
                    architectureInfix = L"x64";
                    break;
                case IMAGE_FILE_MACHINE_ARM64:
                    architectureInfix = L"arm64";
                    break;
                case IMAGE_FILE_MACHINE_I386:
                    architectureInfix = L"x86";
                    break;
                default:
                    break;
                }
            }
            if (architectureInfix.empty())
            {
                // WHAT?
                return _InboxConsoleHostPath();
            }
            modulePath.replace_filename(architectureInfix);
            modulePath.append(L"OpenConsole.exe");
        }
        if (!std::filesystem::exists(modulePath))
        {
            // We tried the architecture infix version and failed, fall back to conhost.
            return _InboxConsoleHostPath();
        }
        const auto& modulePathAsString = modulePath.native();
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
    if (pPty == nullptr)
    {
        return E_INVALIDARG;
    }
    if (size.X == 0 || size.Y == 0)
    {
        return E_INVALIDARG;
    }

    wil::unique_handle serverHandle;
    RETURN_IF_NTSTATUS_FAILED(CreateServerHandle(serverHandle.addressof(), TRUE));

    // The hPtyReference we create here is used when the PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE attribute is processed.
    // This ensures that conhost's client processes inherit the correct (= our) console handle.
    wil::unique_handle referenceHandle;
    RETURN_IF_NTSTATUS_FAILED(CreateClientHandle(referenceHandle.addressof(),
                                                 serverHandle.get(),
                                                 L"\\Reference",
                                                 FALSE));

    wil::unique_handle signalPipeConhostSide;
    wil::unique_handle signalPipeOurSide;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    // Mark inheritable for signal handle when creating. It'll have the same value on the other side.
    sa.bInheritHandle = FALSE;
    sa.lpSecurityDescriptor = nullptr;

    RETURN_IF_WIN32_BOOL_FALSE(CreatePipe(signalPipeConhostSide.addressof(), signalPipeOurSide.addressof(), &sa, 0));
    RETURN_IF_WIN32_BOOL_FALSE(SetHandleInformation(signalPipeConhostSide.get(), HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT));

    // GH4061: Ensure that the path to executable in the format is escaped so C:\Program.exe cannot collide with C:\Program Files
    auto pwszFormat = L"\"%s\" --headless %s%s%s--width %hu --height %hu --signal 0x%x --server 0x%x";
    // This is plenty of space to hold the formatted string
    wchar_t cmd[MAX_PATH]{};
    const BOOL bInheritCursor = (dwFlags & PSEUDOCONSOLE_INHERIT_CURSOR) == PSEUDOCONSOLE_INHERIT_CURSOR;
    const BOOL bResizeQuirk = (dwFlags & PSEUDOCONSOLE_RESIZE_QUIRK) == PSEUDOCONSOLE_RESIZE_QUIRK;
    const BOOL bPassthroughMode = (dwFlags & PSEUDOCONSOLE_PASSTHROUGH_MODE) == PSEUDOCONSOLE_PASSTHROUGH_MODE;
    swprintf_s(cmd,
               MAX_PATH,
               pwszFormat,
               _ConsoleHostPath(),
               bInheritCursor ? L"--inheritcursor " : L"",
               bResizeQuirk ? L"--resizeQuirk " : L"",
               bPassthroughMode ? L"--passthrough " : L"",
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
    InitializeProcThreadAttributeList(nullptr, 1, 0, &listSize);

    // I have to use a HeapAlloc here because kernelbase can't link new[] or delete[]
    auto attrList = static_cast<PPROC_THREAD_ATTRIBUTE_LIST>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, listSize));
    RETURN_IF_NULL_ALLOC(attrList);
    auto attrListDelete = wil::scope_exit([&]() noexcept {
        HeapFree(GetProcessHeap(), 0, attrList);
    });

    siEx.lpAttributeList = attrList;
    RETURN_IF_WIN32_BOOL_FALSE(InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, &listSize));
    // Set cleanup data for ProcThreadAttributeList when successful.
    auto cleanupProcThreadAttribute = wil::scope_exit([&]() noexcept {
        DeleteProcThreadAttributeList(siEx.lpAttributeList);
    });
    RETURN_IF_WIN32_BOOL_FALSE(UpdateProcThreadAttribute(siEx.lpAttributeList,
                                                         0,
                                                         PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                                         inheritedHandles,
                                                         (INHERITED_HANDLES_COUNT * sizeof(HANDLE)),
                                                         nullptr,
                                                         nullptr));
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
        if (hToken == INVALID_HANDLE_VALUE || hToken == nullptr)
        {
            // Call create process
            RETURN_IF_WIN32_BOOL_FALSE(CreateProcessW(_ConsoleHostPath(),
                                                      cmd,
                                                      nullptr,
                                                      nullptr,
                                                      TRUE,
                                                      EXTENDED_STARTUPINFO_PRESENT,
                                                      nullptr,
                                                      nullptr,
                                                      &siEx.StartupInfo,
                                                      pi.addressof()));
        }
        else
        {
            // Call create process
            RETURN_IF_WIN32_BOOL_FALSE(CreateProcessAsUserW(hToken,
                                                            _ConsoleHostPath(),
                                                            cmd,
                                                            nullptr,
                                                            nullptr,
                                                            TRUE,
                                                            EXTENDED_STARTUPINFO_PRESENT,
                                                            nullptr,
                                                            nullptr,
                                                            &siEx.StartupInfo,
                                                            pi.addressof()));
        }
    }

    pPty->hSignal = signalPipeOurSide.release();
    pPty->hPtyReference = referenceHandle.release();
    pPty->hConPtyProcess = std::exchange(pi.hProcess, nullptr);

    return S_OK;
}

// Function Description:
// - Resizes the conpty
// Arguments:
// - hSignal: A signal pipe as returned by CreateConPty.
// - size: The new dimensions of the conpty, in characters.
// Return Value:
// - S_OK if the call succeeded, else an appropriate HRESULT for failing to
//      write the resize message to the pty.
HRESULT _ResizePseudoConsole(_In_ const PseudoConsole* const pPty, _In_ const COORD size)
{
    if (pPty == nullptr || size.X < 0 || size.Y < 0)
    {
        return E_INVALIDARG;
    }

    unsigned short signalPacket[3];
    signalPacket[0] = PTY_SIGNAL_RESIZE_WINDOW;
    signalPacket[1] = size.X;
    signalPacket[2] = size.Y;

    const auto fSuccess = WriteFile(pPty->hSignal, signalPacket, sizeof(signalPacket), nullptr, nullptr);
    return fSuccess ? S_OK : HRESULT_FROM_WIN32(GetLastError());
}

// Function Description:
// - Clears the conpty
// Arguments:
// - hSignal: A signal pipe as returned by CreateConPty.
// Return Value:
// - S_OK if the call succeeded, else an appropriate HRESULT for failing to
//      write the clear message to the pty.
HRESULT _ClearPseudoConsole(_In_ const PseudoConsole* const pPty)
{
    if (pPty == nullptr)
    {
        return E_INVALIDARG;
    }

    unsigned short signalPacket[1];
    signalPacket[0] = PTY_SIGNAL_CLEAR_WINDOW;

    const auto fSuccess = WriteFile(pPty->hSignal, signalPacket, sizeof(signalPacket), nullptr, nullptr);
    return fSuccess ? S_OK : HRESULT_FROM_WIN32(GetLastError());
}

// Function Description:
// - Shows or hides the internal HWND used by ConPTY. This should be kept in
//   sync with the hosting application's window.
// Arguments:
// - hSignal: A signal pipe as returned by CreateConPty.
// - show: true if the window should be shown, false to mark it as iconic.
// Return Value:
// - S_OK if the call succeeded, else an appropriate HRESULT for failing to
//      write the clear message to the pty.
HRESULT _ShowHidePseudoConsole(_In_ const PseudoConsole* const pPty, const bool show)
{
    if (pPty == nullptr)
    {
        return E_INVALIDARG;
    }
    unsigned short signalPacket[2];
    signalPacket[0] = PTY_SIGNAL_SHOWHIDE_WINDOW;
    signalPacket[1] = show;

    const BOOL fSuccess = WriteFile(pPty->hSignal, signalPacket, sizeof(signalPacket), nullptr, nullptr);
    return fSuccess ? S_OK : HRESULT_FROM_WIN32(GetLastError());
}

// - Sends a message to the pseudoconsole informing it that it should use the
//   given window handle as the owner for the conpty's pseudo window. This
//   allows the response given to GetConsoleWindow() to be a HWND that's owned
//   by the actual hosting terminal's HWND.
// Arguments:
// - pPty: A pointer to a PseudoConsole struct.
// - newParent: The new owning window
// Return Value:
// - S_OK if the call succeeded, else an appropriate HRESULT for failing to
//      write the resize message to the pty.
#pragma warning(suppress : 26461)
// an HWND is technically a void*, but that confuses static analysis here.
HRESULT _ReparentPseudoConsole(_In_ const PseudoConsole* const pPty, _In_ const HWND newParent)
{
    if (pPty == nullptr)
    {
        return E_INVALIDARG;
    }

    // sneaky way to pack a short and a uint64_t in a relatively literal way.
#pragma pack(push, 1)
    struct _signal
    {
        const unsigned short id;
        const uint64_t hwnd;
    };
#pragma pack(pop)

    const _signal data{
        PTY_SIGNAL_REPARENT_WINDOW,
        (uint64_t)(newParent),
    };
    const auto fSuccess = WriteFile(pPty->hSignal, &data, sizeof(data), nullptr, nullptr);

    return fSuccess ? S_OK : HRESULT_FROM_WIN32(GetLastError());
}

// Function Description:
// - This closes each of the members of a PseudoConsole. It does not free the
//      data associated with the PseudoConsole. This is helpful for testing,
//      where we might stack allocate a PseudoConsole (instead of getting a
//      HPCON via the API).
// Arguments:
// - pPty: A pointer to a PseudoConsole struct.
// - wait: If true, waits for conhost/OpenConsole to exit first.
// Return Value:
// - <none>
void _ClosePseudoConsoleMembers(_In_ PseudoConsole* pPty, _In_ DWORD dwMilliseconds)
{
    if (pPty != nullptr)
    {
        // See MSFT:19918626
        // First break the signal pipe - this will trigger conhost to tear itself down
        if (_HandleIsValid(pPty->hSignal))
        {
            CloseHandle(pPty->hSignal);
            pPty->hSignal = nullptr;
        }
        // The reference handle ensures that conhost keeps running unless ClosePseudoConsole is called.
        // We have to call it before calling WaitForSingleObject however in order to not deadlock,
        // Due to conhost waiting for all clients to disconnect, while we wait for conhost to exit.
        if (_HandleIsValid(pPty->hPtyReference))
        {
            CloseHandle(pPty->hPtyReference);
            pPty->hPtyReference = nullptr;
        }
        // Then, wait on the conhost process before killing it.
        // We do this to make sure the conhost finishes flushing any output it
        //      has yet to send before we hard kill it.
        if (_HandleIsValid(pPty->hConPtyProcess))
        {
            if (dwMilliseconds)
            {
                WaitForSingleObject(pPty->hConPtyProcess, dwMilliseconds);
            }

            CloseHandle(pPty->hConPtyProcess);
            pPty->hConPtyProcess = nullptr;
        }
    }
}

// Function Description:
// - This closes each of the members of a PseudoConsole, and HeapFree's the
//      memory allocated to it. This should be used to cleanup any
//      PseudoConsoles that were created with CreatePseudoConsole.
// Arguments:
// - pPty: A pointer to a PseudoConsole struct.
// - wait: If true, waits for conhost/OpenConsole to exit first.
// Return Value:
// - <none>
static void _ClosePseudoConsole(_In_ PseudoConsole* pPty, _In_ DWORD dwMilliseconds) noexcept
{
    if (pPty != nullptr)
    {
        _ClosePseudoConsoleMembers(pPty, dwMilliseconds);
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
//      The created conpty will immediately emit a "Device Status Request" VT
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

extern "C" HRESULT WINAPI ConptyCreatePseudoConsoleAsUser(_In_ HANDLE hToken,
                                                          _In_ COORD size,
                                                          _In_ HANDLE hInput,
                                                          _In_ HANDLE hOutput,
                                                          _In_ DWORD dwFlags,
                                                          _Out_ HPCON* phPC)
{
    if (phPC == nullptr)
    {
        return E_INVALIDARG;
    }
    *phPC = nullptr;
    if ((!_HandleIsValid(hInput)) && (!_HandleIsValid(hOutput)))
    {
        return E_INVALIDARG;
    }

    auto pPty = (PseudoConsole*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(PseudoConsole));
    RETURN_IF_NULL_ALLOC(pPty);
    auto cleanupPty = wil::scope_exit([&]() noexcept {
        _ClosePseudoConsole(pPty, 0);
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
    const PseudoConsole* const pPty = (PseudoConsole*)hPC;
    auto hr = pPty == nullptr ? E_INVALIDARG : S_OK;
    if (SUCCEEDED(hr))
    {
        hr = _ResizePseudoConsole(pPty, size);
    }
    return hr;
}

// Function Description:
// - Clear the contents of the conpty buffer, leaving the cursor row at the top
//   of the viewport.
// - This is used exclusively by ConPTY to support GH#1193, GH#1882. This allows
//   a terminal to clear the contents of the ConPTY buffer, which is important
//   if the user would like to be able to clear the terminal-side buffer.
extern "C" HRESULT WINAPI ConptyClearPseudoConsole(_In_ HPCON hPC)
{
    const PseudoConsole* const pPty = (PseudoConsole*)hPC;
    auto hr = pPty == nullptr ? E_INVALIDARG : S_OK;
    if (SUCCEEDED(hr))
    {
        hr = _ClearPseudoConsole(pPty);
    }
    return hr;
}

// Function Description:
// - Tell the ConPTY about the state of the hosting window. This should be used
//   to keep ConPTY's internal HWND state in sync with the state of whatever the
//   hosting window is.
// - For more information, refer to GH#12515.
extern "C" HRESULT WINAPI ConptyShowHidePseudoConsole(_In_ HPCON hPC, bool show)
{
    // _ShowHidePseudoConsole will return E_INVALIDARG for us if the hPC is nullptr.
    return _ShowHidePseudoConsole((PseudoConsole*)hPC, show);
}

// - Sends a message to the pseudoconsole informing it that it should use the
//   given window handle as the owner for the conpty's pseudo window. This
//   allows the response given to GetConsoleWindow() to be a HWND that's owned
//   by the actual hosting terminal's HWND.
// - Used to support GH#2988
extern "C" HRESULT WINAPI ConptyReparentPseudoConsole(_In_ HPCON hPC, HWND newParent)
{
    return _ReparentPseudoConsole((PseudoConsole*)hPC, newParent);
}

// The \Reference handle ensures that conhost keeps running by keeping the ConDrv server pipe open.
// After you've finished setting up your PTY via PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, this method may be called
// to release that handle, allowing conhost to shut down automatically once the last client has disconnected.
// You'll know when this happens, because a ReadFile() on the output pipe will return ERROR_BROKEN_PIPE.
extern "C" HRESULT WINAPI ConptyReleasePseudoConsole(_In_ HPCON hPC)
{
    const auto pPty = (PseudoConsole*)hPC;
    if (pPty == nullptr)
    {
        return E_INVALIDARG;
    }

    if (_HandleIsValid(pPty->hPtyReference))
    {
        CloseHandle(pPty->hPtyReference);
        pPty->hPtyReference = nullptr;
    }

    return S_OK;
}

// Function Description:
// Closes the conpty and all associated state.
// Client applications attached to the conpty will also behave as though the
//      console window they were running in was closed.
// This can fail if the conhost hosting the pseudoconsole failed to be
//      terminated, or if the pseudoconsole was already terminated.
// Waits for conhost/OpenConsole to exit first.
extern "C" VOID WINAPI ConptyClosePseudoConsole(_In_ HPCON hPC)
{
    _ClosePseudoConsole((PseudoConsole*)hPC, INFINITE);
}

// Function Description:
// Closes the conpty and all associated state.
// Client applications attached to the conpty will also behave as though the
//      console window they were running in was closed.
// This can fail if the conhost hosting the pseudoconsole failed to be
//      terminated, or if the pseudoconsole was already terminated.
// Doesn't wait for conhost/OpenConsole to exit.
extern "C" VOID WINAPI ConptyClosePseudoConsoleTimeout(_In_ HPCON hPC, _In_ DWORD dwMilliseconds)
{
    _ClosePseudoConsole((PseudoConsole*)hPC, dwMilliseconds);
}

// NOTE: This one is not defined in the Windows headers but is
// necessary for our outside recipient in the Terminal
// to set up a PTY session in fundamentally the same way as the
// Creation functions. Using the same HPCON pack enables
// resizing and closing to "just work."

// Function Description:
// Packs loose handle information for an inbound ConPTY
//  session into the same HPCON as a created session.
extern "C" HRESULT WINAPI ConptyPackPseudoConsole(_In_ HANDLE hProcess,
                                                  _In_ HANDLE hRef,
                                                  _In_ HANDLE hSignal,
                                                  _Out_ HPCON* phPC)
{
    RETURN_HR_IF(E_INVALIDARG, nullptr == phPC);
    *phPC = nullptr;
    RETURN_HR_IF(E_INVALIDARG, !_HandleIsValid(hProcess));
    RETURN_HR_IF(E_INVALIDARG, !_HandleIsValid(hRef));
    RETURN_HR_IF(E_INVALIDARG, !_HandleIsValid(hSignal));

    auto pPty = (PseudoConsole*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(PseudoConsole));
    RETURN_IF_NULL_ALLOC(pPty);

    pPty->hConPtyProcess = hProcess;
    pPty->hPtyReference = hRef;
    pPty->hSignal = hSignal;

    *phPC = (HPCON)pPty;
    return S_OK;
}

#pragma warning(pop)
