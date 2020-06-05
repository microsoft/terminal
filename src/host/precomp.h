/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- precomp.h

Abstract:
- Contains external headers to include in the precompile phase of console build process.
- Avoid including internal project headers. Instead include them only in the classes that need them (helps with test project building).
--*/

#pragma once

#define DEFINE_CONSOLEV2_PROPERTIES

// Ignore checked iterators warning from VC compiler.
#define _SCL_SECURE_NO_WARNINGS

// Block minwindef.h min/max macros to prevent <algorithm> conflict
#define NOMINMAX

// This includes a lot of common headers needed by both the host and the propsheet
// including: windows.h, winuser, ntstatus, assert, and the DDK
#include "HostAndPropsheetIncludes.h"

// This includes support libraries from the CRT, STL, WIL, and GSL
#include "LibraryIncludes.h"

#define SCREEN_BUFFER_POINTER(X, Y, XSIZE, CELLSIZE) (((XSIZE * (Y)) + (X)) * (ULONG)CELLSIZE)
#include <shellapi.h>

#include <securityappcontainer.h>

#include <condrv.h>

#include <conmsgl1.h>
#include <conmsgl2.h>
#include <conmsgl3.h>

#include <propvarutil.h>
#include <UIAutomation.h>

#include <winuser.h>
#include <winconp.h>
#include <ntcon.h>
#include <windowsx.h>
#include <dde.h>
#include "conserv.h"

#include "conv.h"

#pragma prefast(push)
#pragma prefast(disable : 26071, "Range violation in Intsafe. Not ours.")
#define ENABLE_INTSAFE_SIGNED_FUNCTIONS // Only unsigned intsafe math/casts available without this def
#include <intsafe.h>
#pragma prefast(pop)
#include <strsafe.h>
#include <wchar.h>
#include <mmsystem.h>
#include "utils.hpp"

// Including TraceLogging essentials for the binary
#include <TraceLoggingProvider.h>
#include <winmeta.h>
TRACELOGGING_DECLARE_PROVIDER(g_hConhostV2EventTraceProvider);
#include <telemetry\ProjectTelemetry.h>
#include <TraceLoggingActivity.h>
#include "telemetry.hpp"
#include "tracing.hpp"

#ifdef BUILDING_INSIDE_WINIDE
#define DbgRaiseAssertionFailure() __int2c()
#endif

#include <ShellScalingApi.h>
#include "..\propslib\conpropsp.hpp"

// Comment to build against the private SDK.
#define CON_BUILD_PUBLIC

#ifdef CON_BUILD_PUBLIC
#define CON_USERPRIVAPI_INDIRECT
#define CON_DPIAPI_INDIRECT
#endif

#include "..\inc\contsf.h"
#include "..\inc\operators.hpp"
#include "..\inc\conattrs.hpp"

// TODO: MSFT 9355094 Find a better way of doing this. http://osgvsowi/9355094
[[nodiscard]] inline NTSTATUS NTSTATUS_FROM_HRESULT(HRESULT hr)
{
    return NTSTATUS_FROM_WIN32(HRESULT_CODE(hr));
}
