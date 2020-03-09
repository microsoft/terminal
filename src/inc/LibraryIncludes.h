// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

// clang-format off

#pragma once


#pragma warning(push)

// C
#include <climits>
#include <cwchar>
#include <cwctype>

// STL

// Block minwindef.h min/max macros to prevent <algorithm> conflict
#define NOMINMAX

#include <algorithm>
#include <atomic>
#include <deque>
#include <list>
#include <memory>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <new>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>
#include <unordered_map>
#include <iterator>
#include <math.h>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <filesystem>
#include <functional>
#include <set>
#include <unordered_set>
#include <regex>

// WIL
#include <wil/Common.h>
#include <wil/Result.h>
#include <wil/resource.h>
#include <wil/wistd_memory.h>
#include <wil/stl.h>
#include <wil/com.h>
#include <wil/filesystem.h>
#include <wil/win32_helpers.h>

// GSL
// Block GSL Multi Span include because it both has C++17 deprecated iterators
// and uses the C-namespaced "max" which conflicts with Windows definitions.
#ifndef BLOCK_GSL
#define GSL_MULTI_SPAN_H
#include <gsl/gsl>
#endif

// CppCoreCheck
#include <CppCoreCheck/Warnings.h>

// Chromium Numerics (safe math)
#pragma warning(push)
#pragma warning(disable:4100) // unreferenced parameter
#include <base/numerics/safe_math.h>
#pragma warning(pop)

// IntSafe
#define ENABLE_INTSAFE_SIGNED_FUNCTIONS
#include <intsafe.h>

// SAL
#include <sal.h>

// WRL
#include <wrl.h>

// TIL - Terminal Implementation Library
#ifndef BLOCK_TIL // Certain projects may want to include TIL manually to gain superpowers
#include "til.h"
#endif

#pragma warning(pop)

// clang-format on
