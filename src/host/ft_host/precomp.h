// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "windows.h"
#include "wincon.h"
#include "windowsx.h"

#include "WexTestClass.h"

// This includes support libraries from the CRT, STL, WIL, and GSL
#include "LibraryIncludes.h"

#include <conio.h>

// Extension API set presence checks.
#ifdef __INSIDE_WINDOWS
#include <messageext.h>
#include <windowext.h>
#include <sysparamsext.h>
#endif

#define CM_SET_KEY_STATE (WM_USER + 18)
#define CM_SET_KEYBOARD_LAYOUT (WM_USER + 19)

#include "OneCoreDelay.hpp"

// Include our common helpers
#include "common.hpp"

#include "resource.h"
