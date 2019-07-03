// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// pch.h
// Header for platform projection include files
//

#pragma once

#define WIN32_LEAN_AND_MEAN
#define _SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING

#include <LibraryIncludes.h>

// Must be included before any WinRT headers.
#include <unknwn.h>

#include "winrt/Windows.Foundation.h"
#include <Windows.h>
