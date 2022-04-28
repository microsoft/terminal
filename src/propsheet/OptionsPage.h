/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- OptionsPage.h

Abstract:
- This module contains the definitions for console options dialog.

Author(s):
    Mike Griese (migrie) Oct-2016
--*/

#pragma once

void ToggleV2OptionsControls(const __in HWND hDlg);
INT_PTR WINAPI SettingsDlgProc(HWND hDlg, UINT wMsg, WPARAM wParam, LPARAM lParam);
void InitializeCursorSize(const HWND hOptionsDlg);
