// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _PseudoConsole
{
    HANDLE hSignal;
    HANDLE hPtyReference;
    HANDLE hConPtyProcess;
} PseudoConsole;

// Signals
// These are not defined publicly, but are used for controlling the conpty via
//      the signal pipe.
#define PTY_SIGNAL_RESIZE_WINDOW (8u)

// CreatePseudoConsole Flags
// The other flag (PSEUDOCONSOLE_INHERIT_CURSOR) is actually defined in consoleapi.h in the OS repo
// #define PSEUDOCONSOLE_INHERIT_CURSOR (0x1)
#define PSEUDOCONSOLE_RESIZE_QUIRK (0x2)
#define PSEUDOCONSOLE_WIN32_INPUT_MODE (0x4)

// Implementations of the various PseudoConsole functions.
HRESULT _CreatePseudoConsole(const HANDLE hToken,
                             const COORD size,
                             const HANDLE hInput,
                             const HANDLE hOutput,
                             const DWORD dwFlags,
                             _Inout_ PseudoConsole* pPty);

HRESULT _ResizePseudoConsole(_In_ const PseudoConsole* const pPty, _In_ const COORD size);
void _ClosePseudoConsoleMembers(_In_ PseudoConsole* pPty);
VOID _ClosePseudoConsole(_In_ PseudoConsole* pPty);

HRESULT ConptyCreatePseudoConsoleAsUser(_In_ HANDLE hToken,
                                        _In_ COORD size,
                                        _In_ HANDLE hInput,
                                        _In_ HANDLE hOutput,
                                        _In_ DWORD dwFlags,
                                        _Out_ HPCON* phPC);

#ifdef __cplusplus
}
#endif
