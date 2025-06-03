// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

// This header prototypes the Pseudoconsole symbols from conpty.lib with their original names.
// This is required because we cannot import __imp_CreatePseudoConsole from a static library
// as it doesn't produce an import lib.
// We can't use an /ALTERNATENAME trick because it seems that that name is only resolved when the
// linker cannot otherwise find the symbol.

#pragma once

#include <consoleapi.h>

#ifndef CONPTY_IMPEXP
#define CONPTY_IMPEXP __declspec(dllimport)
#endif

#ifndef CONPTY_EXPORT
#ifdef __cplusplus
#define CONPTY_EXPORT extern "C" CONPTY_IMPEXP
#else
#define CONPTY_EXPORT extern CONPTY_IMPEXP
#endif
#endif

// CreatePseudoConsole Flags
#ifndef PSEUDOCONSOLE_INHERIT_CURSOR
#define PSEUDOCONSOLE_INHERIT_CURSOR (0x1)
#endif
#ifndef PSEUDOCONSOLE_GLYPH_WIDTH__MASK
#define PSEUDOCONSOLE_GLYPH_WIDTH__MASK 0x18
#define PSEUDOCONSOLE_GLYPH_WIDTH_GRAPHEMES 0x08
#define PSEUDOCONSOLE_GLYPH_WIDTH_WCSWIDTH 0x10
#define PSEUDOCONSOLE_GLYPH_WIDTH_CONSOLE 0x18
#endif

CONPTY_EXPORT HRESULT WINAPI ConptyCreatePseudoConsole(COORD size, HANDLE hInput, HANDLE hOutput, DWORD dwFlags, HPCON* phPC);
CONPTY_EXPORT HRESULT WINAPI ConptyCreatePseudoConsoleAsUser(HANDLE hToken, COORD size, HANDLE hInput, HANDLE hOutput, DWORD dwFlags, HPCON* phPC);

CONPTY_EXPORT HRESULT WINAPI ConptyResizePseudoConsole(HPCON hPC, COORD size);
CONPTY_EXPORT HRESULT WINAPI ConptyClearPseudoConsole(HPCON hPC, BOOL keepCursorRow);
CONPTY_EXPORT HRESULT WINAPI ConptyShowHidePseudoConsole(HPCON hPC, bool show);
CONPTY_EXPORT HRESULT WINAPI ConptyReparentPseudoConsole(HPCON hPC, HWND newParent);
CONPTY_EXPORT HRESULT WINAPI ConptyReleasePseudoConsole(HPCON hPC);

CONPTY_EXPORT VOID WINAPI ConptyClosePseudoConsole(HPCON hPC);

CONPTY_EXPORT HRESULT WINAPI ConptyPackPseudoConsole(HANDLE hServerProcess, HANDLE hRef, HANDLE hSignal, HPCON* phPC);
