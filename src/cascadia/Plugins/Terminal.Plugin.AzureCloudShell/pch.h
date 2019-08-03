#pragma once

#include "targetver.h"

#ifndef WINRT_WINDOWS_ABI
#define WINRT_WINDOWS_ABI
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <LibraryIncludes.h>

// Must be included before any WinRT headers.
#include <unknwn.h>

#include "winrt/Windows.Foundation.h"
#include "winrt/Windows.Security.Credentials.h"
#include "winrt/Windows.Foundation.Collections.h"
#include <Windows.h>
