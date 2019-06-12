/*++
Copyright (c) Microsoft Corporation.
Licensed under the MIT license.

Module Name:
- ColorsPage.h

Abstract:
- This module contains the definitions for console colors dialog.

Author(s):
    Mike Griese (migrie) Oct-2016
--*/

#pragma once

void ToggleV2ColorControls(__in const HWND hDlg);
INT_PTR WINAPI ColorDlgProc(HWND hDlg, UINT wMsg, WPARAM wParam, LPARAM lParam);
void SetOpacitySlider(__in HWND hDlg);
void PreviewOpacity(HWND hDlg, BYTE bOpacity);
[[nodiscard]] LRESULT CALLBACK ColorTableControlProc(HWND hColor, UINT wMsg, WPARAM wParam, LPARAM lParam);
