// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#ifdef __INSIDE_WINDOWS

#define NOMINMAX

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <windows.h>
#include "wchar.h"

// Extension presence detection
#include <sysparamsext.h>

#define _DDK_INCLUDED
#include "../../host/precomp.h"

#else

#include "../../host/precomp.h"

#endif
