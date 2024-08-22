// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "winconpty.h"

#ifdef __INSIDE_WINDOWS
// You need kernelbasestaging.h to be able to use wil in libraries consumed by kernelbase.dll
#include <kernelbasestaging.h>
#define RESOURCE_SUPPRESS_STL
#define WIL_SUPPORT_BITOPERATION_PASCAL_NAMES
#include <wil/resource.h>
#endif // __INSIDE_WINDOWS

#pragma warning(push)
#pragma warning(disable : 4273) // inconsistent dll linkage (we are exporting things kernel32 also exports)

using unique_nthandle = wil::unique_any_handle_null<decltype(&::NtClose), ::NtClose>;

static wil::unique_process_heap_string allocString(size_t count)
{
    const auto addr = static_cast<wchar_t*>(HeapAlloc(GetProcessHeap(), 0, count * sizeof(wchar_t)));
    return wil::unique_process_heap_string{ addr };
}

// Function Description:
// - Returns the path to conhost.exe as a process heap string.
static wil::unique_process_heap_string getInboxConhostPath()
{
    auto path = allocString(MAX_PATH);
    if (!path)
    {
        return {};
    }

    const auto cap = MAX_PATH - 12;
    const auto len = GetSystemDirectoryW(path.get(), cap);
    if (len >= cap)
    {
        return {};
    }

    memcpy(path.get() + len, L"\\conhost.exe", sizeof(L"\\conhost.exe"));
    return path;
}

static bool pathExists(const wchar_t* path)
{
    const auto attr = GetFileAttributesW(path);
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

[[maybe_unused]] static wil::unique_process_heap_string getOssConhostPath()
{
    static constexpr DWORD longestSuffixLength = 22; // "arm64\OpenConsole.exe"

    const auto module = wil::GetModuleInstanceHandle();
    wil::unique_process_heap_string path;
    DWORD basePathLength = MAX_PATH;

    for (;;)
    {
        path = allocString(basePathLength + longestSuffixLength);
        if (!path)
        {
            return {};
        }

        const auto cap = basePathLength;
        basePathLength = GetModuleFileNameW(module, path.get(), cap);

        // If the function fails, the return value is 0 (zero).
        if (basePathLength == 0)
        {
            return {};
        }

        // If the function succeeds, the return value is the length of the string that is
        // copied to the buffer, in characters, not including the terminating null character.
        if (basePathLength < cap)
        {
            break;
        }

        basePathLength *= 2;
    }

    const auto basePathBeg = path.get();
    auto basePathEnd = basePathBeg + basePathLength;
    for (; basePathEnd != basePathBeg && basePathEnd[-1] != L'\\'; --basePathEnd)
    {
    }

    memcpy(basePathEnd, L"OpenConsole.exe", sizeof(L"OpenConsole.exe"));
    if (pathExists(path.get()))
    {
        return path;
    }

    USHORT unusedImageFileMachine, nativeMachine;
    if (IsWow64Process2(GetCurrentProcess(), &unusedImageFileMachine, &nativeMachine))
    {
        // Despite being a machine type, the values IsWow64Process2 returns are *image* types
        switch (nativeMachine)
        {
        case IMAGE_FILE_MACHINE_AMD64:
            memcpy(basePathEnd, L"x64\\OpenConsole.exe", sizeof(L"x64\\OpenConsole.exe"));
            break;
        case IMAGE_FILE_MACHINE_ARM64:
            memcpy(basePathEnd, L"arm64\\OpenConsole.exe", sizeof(L"arm64\\OpenConsole.exe"));
            break;
        case IMAGE_FILE_MACHINE_I386:
            memcpy(basePathEnd, L"x86\\OpenConsole.exe", sizeof(L"x86\\OpenConsole.exe"));
            break;
        default:
            break;
        }
        if (pathExists(path.get()))
        {
            return path;
        }
    }

    return {};
}

// Function Description:
// - Returns the path to either conhost.exe or the side-by-side OpenConsole, depending on whether this
//   module is building with Windows and OpenConsole could be found.
// Return Value:
// - A pointer to permanent storage containing the path to the console host.
static wchar_t* getConhostPath()
{
    // Use the magic of magic statics to only calculate this once.
    static const auto consoleHostPath = []() {
#if !defined(__INSIDE_WINDOWS)
        if (auto path = getOssConhostPath())
        {
            return path;
        }
#endif // __INSIDE_WINDOWS
        return getInboxConhostPath();
    }();
    return consoleHostPath.get();
}

static bool _HandleIsValid(HANDLE h) noexcept
{
    return (h != INVALID_HANDLE_VALUE) && (h != nullptr);
}

template<size_t N>
static constexpr UNICODE_STRING constantUnicodeString(const wchar_t (&s)[N])
{
    return { N * 2 - 2, N * 2, const_cast<wchar_t*>(&s[0]) };
}

static NTSTATUS conhostCreateHandle(HANDLE* handle, HANDLE parent, UNICODE_STRING name, bool inherit, bool synchronous)
{
    ULONG attrFlags = OBJ_CASE_INSENSITIVE;
    WI_SetFlagIf(attrFlags, OBJ_INHERIT, inherit);

    OBJECT_ATTRIBUTES attr;
    InitializeObjectAttributes(&attr, &name, attrFlags, parent, nullptr);

    ULONG options = 0;
    WI_SetFlagIf(options, FILE_SYNCHRONOUS_IO_NONALERT, synchronous);

    IO_STATUS_BLOCK ioStatus;
    return NtCreateFile(
        /* FileHandle        */ handle,
        /* DesiredAccess     */ FILE_GENERIC_READ | FILE_GENERIC_WRITE,
        /* ObjectAttributes  */ &attr,
        /* IoStatusBlock     */ &ioStatus,
        /* AllocationSize    */ nullptr,
        /* FileAttributes    */ 0,
        /* ShareAccess       */ FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        /* CreateDisposition */ FILE_CREATE,
        /* CreateOptions     */ options,
        /* EaBuffer          */ nullptr,
        /* EaLength          */ 0);
}

HRESULT _CreatePseudoConsole(HANDLE hToken,
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

    // CreateProcessAsUserW expects the token to be either valid or null.
    if (hToken == INVALID_HANDLE_VALUE)
    {
        hToken = nullptr;
    }

    unique_nthandle serverHandle;
    unique_nthandle referenceHandle;
    RETURN_IF_NTSTATUS_FAILED(conhostCreateHandle(serverHandle.addressof(), nullptr, constantUnicodeString(L"\\Device\\ConDrv\\Server"), true, false));
    RETURN_IF_NTSTATUS_FAILED(conhostCreateHandle(referenceHandle.addressof(), serverHandle.get(), constantUnicodeString(L"\\Reference"), false, true));

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
    // This is plenty of space to hold the formatted string
    const BOOL bInheritCursor = (dwFlags & PSEUDOCONSOLE_INHERIT_CURSOR) == PSEUDOCONSOLE_INHERIT_CURSOR;

    const wchar_t* textMeasurement;
    switch (dwFlags & PSEUDOCONSOLE_GLYPH_WIDTH__MASK)
    {
    case PSEUDOCONSOLE_GLYPH_WIDTH_GRAPHEMES:
        textMeasurement = L"--textMeasurement graphemes ";
        break;
    case PSEUDOCONSOLE_GLYPH_WIDTH_WCSWIDTH:
        textMeasurement = L"--textMeasurement wcswidth ";
        break;
    case PSEUDOCONSOLE_GLYPH_WIDTH_CONSOLE:
        textMeasurement = L"--textMeasurement console ";
        break;
    default:
        textMeasurement = L"";
        break;
    }

    const auto conhostPath = getConhostPath();
    if (!conhostPath)
    {
        return E_OUTOFMEMORY;
    }

    wil::unique_process_heap_string cmd;
    RETURN_IF_FAILED(wil::str_printf_nothrow(
        cmd,
        L"\"%s\" --headless %s%s--width %hd --height %hd --signal 0x%tx --server 0x%tx",
        conhostPath,
        bInheritCursor ? L"--inheritcursor " : L"",
        textMeasurement,
        size.X,
        size.Y,
        std::bit_cast<uintptr_t>(signalPipeConhostSide.get()),
        std::bit_cast<uintptr_t>(serverHandle.get())));

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

    // Birds aren't real and DeleteProcThreadAttributeList isn't real either.
    alignas(__STDCPP_DEFAULT_NEW_ALIGNMENT__) char attrListBuffer[128];
    siEx.lpAttributeList = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(&attrListBuffer[0]);

    SIZE_T listSize = sizeof(attrListBuffer);
    RETURN_IF_WIN32_BOOL_FALSE(InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, &listSize));
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

        // Call create process
        RETURN_IF_WIN32_BOOL_FALSE(CreateProcessAsUserW(
            hToken,
            conhostPath,
            cmd.get(),
            nullptr,
            nullptr,
            TRUE,
            EXTENDED_STARTUPINFO_PRESENT,
            nullptr,
            nullptr,
            &siEx.StartupInfo,
            pi.addressof()));
    }

    pPty->hSignal = signalPipeOurSide.release();
    pPty->hPtyReference = referenceHandle.release();
    pPty->hConPtyProcess = pi.hProcess;
    pi.hProcess = nullptr;

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
void _ClosePseudoConsoleMembers(_In_ PseudoConsole* pPty)
{
    if (pPty != nullptr)
    {
        if (_HandleIsValid(pPty->hSignal))
        {
            CloseHandle(pPty->hSignal);
            pPty->hSignal = nullptr;
        }
        if (_HandleIsValid(pPty->hPtyReference))
        {
            CloseHandle(pPty->hPtyReference);
            pPty->hPtyReference = nullptr;
        }
        if (_HandleIsValid(pPty->hConPtyProcess))
        {
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
static void _ClosePseudoConsole(_In_ PseudoConsole* pPty) noexcept
{
    if (pPty != nullptr)
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
    _ClosePseudoConsole((PseudoConsole*)hPC);
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
