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
#include <memory_resource>
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
#include <cmath>
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
#define GSL_MULTI_SPAN_H
#include <gsl/gsl>
#include <gsl/span_ext>

// CppCoreCheck
#include <CppCoreCheck/Warnings.h>

// Chromium Numerics (safe math)
#pragma warning(push)
#pragma warning(disable:4100) // unreferenced parameter
#include <base/numerics/safe_math.h>
#pragma warning(pop)

// Boost
#include "boost/container/small_vector.hpp"

// IntSafe
#define ENABLE_INTSAFE_SIGNED_FUNCTIONS
#include <intsafe.h>

// LibPopCnt - Fast C/C++ bit population count library (on bits in an array)
#include <libpopcnt.h>

// Dynamic Bitset (optional dependency on LibPopCnt for perf at bit counting)
// Variable-size compressed-storage header-only bit flag storage library.
#pragma warning(push)
#pragma warning(disable:4702) // unreachable code
#include <dynamic_bitset.hpp>
#pragma warning(pop)

// {fmt}, a C++20-compatible formatting library
#include <fmt/format.h>
#include <fmt/compile.h>

#define USE_INTERVAL_TREE_NAMESPACE
#include <IntervalTree.h>

// SAL
#include <sal.h>

// WRL
#include <wrl.h>

// WEX/TAEF testing
// Include before TIL if we're unit testing so it can light up WEX/TAEF template extensions
#ifdef UNIT_TESTING
#include <WexTestClass.h>
#endif

// TIL - Terminal Implementation Library
#ifndef BLOCK_TIL // Certain projects may want to include TIL manually to gain superpowers
#include "til.h"
#endif

#pragma warning(pop)

// clang-format on
