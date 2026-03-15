/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- pch.h

Abstract:
- Contains external headers to include in the precompile phase of console build
  process.
- Avoid including internal project headers. Instead include them only in the
  classes that need them (helps with test project building).
--*/

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define NOMINMAX

#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS

#include <winternl.h>

#pragma warning(push)
#pragma warning(disable : 4430)
#include <ntstatus.h>
#pragma warning(pop)

#define BLOCK_TIL
#include "LibraryIncludes.h"

#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

#include <wil/cppwinrt.h>
#include <Unknwn.h>
#include <hstring.h>

#include <WexTestClass.h>
#include "consoletaeftemplates.hpp"

#include <winrt/Windows.System.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

// Manually include til after we include Windows.Foundation to give it winrt superpowers
#include "til.h"

#include <fstream>
#include <sstream>
#include <filesystem>
