#pragma once

#define WIN32_LEAN_AND_MEAN
// Manually include til after we include Windows.Foundation to give it winrt superpowers
#define BLOCK_TIL
#include <wil/cppwinrt.h>
#undef max
#undef min
#include "LibraryIncludes.h"

#include <Unknwn.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

// Manually include til after we include Windows.Foundation to give it winrt superpowers
#include "til.h"
