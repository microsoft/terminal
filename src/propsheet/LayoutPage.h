/*++
Copyright (c) Microsoft Corporation.
Licensed under the MIT license.

Module Name:
- LayoutPage.h

Abstract:
- This module contains the definitions for console layout dialog. 

Author(s):
    Mike Griese (migrie) Oct-2016
--*/

#pragma once

void ToggleV2LayoutControls(__in const HWND hDlg);
INT_PTR WINAPI ScreenSizeDlgProc(HWND hDlg, UINT wMsg, WPARAM wParam, LPARAM lParam);
