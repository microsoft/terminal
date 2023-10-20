#pragma once

#define WIN32_LEAN_AND_MEAN
// Manually include til after we include Windows.Foundation to give it winrt superpowers
#define BLOCK_TIL
#include <wil/cppwinrt.h>
#undef max
#undef min
#include "LibraryIncludes.h"

// This is inexplicable, but for whatever reason, cppwinrt conflicts with the
//      SDK definition of this function, so the only fix is to undef it.
// from WinBase.h
// Windows::UI::Xaml::Media::Animation::IStoryboard::GetCurrentTime
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

#include <Unknwn.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

// Manually include til after we include Windows.Foundation to give it winrt superpowers
#include "til.h"

#include <conio.h>

#include <cppwinrt_utils.h>
