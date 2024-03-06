// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#ifdef __cplusplus
extern "C" {
#endif

// This structure is part of an ABI shared with the rest of the operating system.
typedef struct _PseudoConsole
{
    // hSignal is a anonymous pipe used for out of band communication with conhost.
    // It's used to send the various PTY_SIGNAL_* messages.
    HANDLE hSignal;
    // The "server handle" in conhost represents the console IPC "pipe" over which all console
    // messages, all client connect and disconnect events, API calls, text output, etc. flow.
    // The full type of this handle is \Device\ConDrv\Server and is implemented in
    // /minkernel/console/condrv/server.c. If you inspect conhost's handles it'll show up
    // as a handle of name \Device\ConDrv, because that's the namespace of these handles.
    //
    // hPtyReference is derived from that handle (= a child), is named \Reference and is implemented
    // in /minkernel/console/condrv/reference.c. While conhost is the sole owner and user of the
    // "server handle", the "reference handle" is what console processes actually inherit in order
    // to communicate with the console server (= conhost). When the reference count of the
    // \Reference handle drops to 0, it'll release its reference to the server handle.
    // The server handle in turn is implemented in such a way that the IPC pipe is broken
    // once the reference count drops to 1, because then conhost must be the last one using it.
    //
    // In other words: As long as hPtyReference exists it'll keep the server handle alive
    // and thus keep conhost alive. Closing this handle will make conhost exit as soon as all
    // currently connected clients have disconnected and closed the reference handle as well.
    //
    // This benefit of this system is that it naturally works with handle inheritance in
    // CreateProcess,  which ensures that the reference handle is safely duplicated and
    // transmitted from a parent process to a new child process, even if the parent
    // process exits before the OS has even finished spawning the child process.
    HANDLE hPtyReference;
    // hConPtyProcess is a process handle to the conhost instance that we've spawned for ConPTY.
    HANDLE hConPtyProcess;
} PseudoConsole;

// Signals
// These are not defined publicly, but are used for controlling the conpty via
//      the signal pipe.
#define PTY_SIGNAL_SHOWHIDE_WINDOW (1u)
#define PTY_SIGNAL_CLEAR_WINDOW (2u)
#define PTY_SIGNAL_REPARENT_WINDOW (3u)
#define PTY_SIGNAL_RESIZE_WINDOW (8u)

// CreatePseudoConsole Flags
#ifndef PSEUDOCONSOLE_INHERIT_CURSOR
#define PSEUDOCONSOLE_INHERIT_CURSOR (0x1)
#endif
#ifndef PSEUDOCONSOLE_RESIZE_QUIRK
#define PSEUDOCONSOLE_RESIZE_QUIRK (0x2)
#endif
#ifndef PSEUDOCONSOLE_WIN32_INPUT_MODE
#define PSEUDOCONSOLE_WIN32_INPUT_MODE (0x4)
#endif
#ifndef PSEUDOCONSOLE_PASSTHROUGH_MODE
#define PSEUDOCONSOLE_PASSTHROUGH_MODE (0x8)
#endif

// Implementations of the various PseudoConsole functions.
HRESULT _CreatePseudoConsole(const HANDLE hToken,
                             const COORD size,
                             const HANDLE hInput,
                             const HANDLE hOutput,
                             const DWORD dwFlags,
                             _Inout_ PseudoConsole* pPty);

HRESULT _ResizePseudoConsole(_In_ const PseudoConsole* const pPty, _In_ const COORD size);
HRESULT _ClearPseudoConsole(_In_ const PseudoConsole* const pPty);
HRESULT _ShowHidePseudoConsole(_In_ const PseudoConsole* const pPty, const bool show);
HRESULT _ReparentPseudoConsole(_In_ const PseudoConsole* const pPty, _In_ const HWND newParent);
void _ClosePseudoConsoleMembers(_In_ PseudoConsole* pPty, _In_ DWORD dwMilliseconds);

HRESULT WINAPI ConptyCreatePseudoConsoleAsUser(_In_ HANDLE hToken,
                                               _In_ COORD size,
                                               _In_ HANDLE hInput,
                                               _In_ HANDLE hOutput,
                                               _In_ DWORD dwFlags,
                                               _Out_ HPCON* phPC);

#ifdef __cplusplus
}
#endif
