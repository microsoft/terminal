// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

// clang-format off

// Custom window messages
#define CM_SET_WINDOW_SIZE       (WM_USER + 2)
#define CM_BEEP                  (WM_USER + 3)
#define CM_UPDATE_SCROLL_BARS    (WM_USER + 4)
#define CM_UPDATE_TITLE          (WM_USER + 5)
// CM_MODE_TRANSITION is hard-coded to WM_USER + 6 in kernel\winmgr.c
// unused (CM_MODE_TRANSITION)   (WM_USER + 6)
// unused (CM_CONSOLE_SHUTDOWN)  (WM_USER + 7)
// unused (CM_HIDE_WINDOW)       (WM_USER + 8)
#define CM_CONIME_CREATE         (WM_USER+9)
#define CM_SET_CONSOLEIME_WINDOW (WM_USER+10)
#define CM_WAIT_CONIME_PROCESS   (WM_USER+11)
// unused CM_SET_IME_CODEPAGE      (WM_USER+12)
// unused CM_SET_NLSMODE           (WM_USER+13)
// unused CM_GET_NLSMODE           (WM_USER+14)
#define CM_CONIME_KL_ACTIVATE    (WM_USER+15)
#define CM_CONSOLE_MSG           (WM_USER+16)
#define CM_UPDATE_EDITKEYS       (WM_USER+17)

#ifdef DBG
#define CM_SET_KEY_STATE         (WM_USER+18)
#define CM_SET_KEYBOARD_LAYOUT   (WM_USER+19)
#endif

// clang-format on
