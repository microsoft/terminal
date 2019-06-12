/*++
Copyright (c) Microsoft Corporation.
Licensed under the MIT license.

Module Name:
- ColorControl.h

Abstract:
- This module contains the definitions for a color control on the prop sheet.

Author(s):
    Mike Griese (migrie) July-2018
--*/

#pragma once

[[nodiscard]] LRESULT CALLBACK SimpleColorControlProc(HWND hColor, UINT wMsg, WPARAM wParam, LPARAM lParam);
void SimpleColorDoPaint(HWND hColor, PAINTSTRUCT& ps, int ColorId);
