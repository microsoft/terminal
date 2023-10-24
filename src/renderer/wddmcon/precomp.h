// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifdef __INSIDE_WINDOWS
#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <dxgidwm.h>
#include <condrv.h>
#else
#include <Windows.h>
#include <unknwn.h>
#endif

#include <dxgi1_2.h>
#include <d3d11.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>

#ifndef __INSIDE_WINDOWS
#include "oss_shim.h"
#endif

// This includes support libraries from the CRT, STL, WIL, and GSL
#include "LibraryIncludes.h"
