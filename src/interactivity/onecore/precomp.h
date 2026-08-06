// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#ifdef __INSIDE_WINDOWS

#define NOMINMAX

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS
#include "wchar.h"

// Extension presence detection
#include <sysparamsext.h>

#define _DDK_INCLUDED
#define NO_WINTERNL_INBOX_BUILD
#include "../../host/precomp.h"

#else

#include "../../host/precomp.h"

#endif
