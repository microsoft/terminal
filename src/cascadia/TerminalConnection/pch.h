// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// pch.h
// Header for platform projection include files
//

#pragma once

// Needs to be defined or we get redeclaration errors
#define WIN32_LEAN_AND_MEAN

#include <LibraryIncludes.h>

// Must be included before any WinRT headers.
#include <unknwn.h>

#include "winrt/Windows.Foundation.h"
#include "winrt/Windows.Security.Credentials.h"
#include "winrt/Windows.Foundation.Collections.h"
#include <Windows.h>
