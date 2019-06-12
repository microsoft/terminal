// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <SDKDDKVer.h>
// Exclude rarely-used stuff from Windows headers
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
// This is inexplicable, but for whatever reason, cppwinrt conflicts with the
//      SDK definition of this function, so the only fix is to undef it.
// from WinBase.h
// Windows::UI::Xaml::Media::Animation::IStoryboard::GetCurrentTime
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

// To enable support for non-WinRT interfaces, unknwn.h must be included before any C++/WinRT headers.
#include <unknwn.h>

#include <winrt/Windows.Foundation.h>
