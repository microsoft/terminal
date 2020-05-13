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

// clang-format off

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

// Windows Header Files:
#include <windows.h>

#define BLOCK_TIL
// This includes support libraries from the CRT, STL, WIL, and GSL
#include "LibraryIncludes.h"

#include "WexTestClass.h"

// Include DirectX common structures so we can test them.
// (before TIL so its support lights up)
#include <dcommon.h>

// Include some things structs from WinRT without including WinRT
// because I just want to make sure it fills structs correctly.
// Adapted from C:\Program Files (x86)\Windows Kits\10\Include\10.0.18362.0\winrt\windows.foundation.h
#define WINRT_Windows_Foundation_H
namespace winrt {
    namespace Windows {
        namespace Foundation {
            struct Rect
            {
                FLOAT X;
                FLOAT Y;
                FLOAT Width;
                FLOAT Height;
            };

            struct Point
            {
                FLOAT X;
                FLOAT Y;
            };

            struct Size
            {
                FLOAT Width;
                FLOAT Height;
            };
        } /* Foundation */
    } /* Windows */
} /* winrt */



// Include TIL after Wex to get test comparators.
#include "til.h"

// clang-format on
