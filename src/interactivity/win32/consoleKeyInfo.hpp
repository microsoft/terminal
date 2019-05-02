/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- consoleKeyInfo.hpp

Abstract:
- This module represents a queue of stored WM_KEYDOWN messages
  that we will match up with the WM_CHARs that arrive later in the window message queue
  after being posted by TranslateMessageEx.
  This is necessary because the scan code data that arrives on WM_CHAR cannot be accurately recreated
  later and may be necessary for us to place into the input queue that client applications can read.
- This class can be removed completely when the console takes over complete handling of WM_KEYDOWN translation.
- The future vision for WM_KEYDOWN translation would be to instead use the export ToUnicode/ToUnicodeEx to 
  create a console-internal version of what TranslateMessageEx does, but instead of posting the product back into
  the window message queue (and needing this class to help line it up later) we would just immediately dispatch it to
  our WM_CHAR routines while we still have the context.

Author(s):
- Michael Niksa (MiNiksa) 11-Jan-2017

Revision History:
- From components of input.h/input.c by Therese Stowell (ThereseS) 1990-1991
--*/

#pragma once

void StoreKeyInfo(_In_ PMSG msg);
void RetrieveKeyInfo(_In_ HWND hWnd, _Out_ PWORD pwVirtualKeyCode, _Inout_ PWORD pwVirtualScanCode, _In_ BOOL FreeKeyInfo);
void ClearKeyInfo(const HWND hWnd);
