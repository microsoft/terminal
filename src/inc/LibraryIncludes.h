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
// Exclude rarely-used stuff from Windows headers
#define WIN32_LEAN_AND_MEAN

#include <algorithm>
#include <atomic>
#include <cmath>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iterator>
#include <list>
#include <map>
#include <memory_resource>
#include <memory>
#include <mutex>
#include <new>
#include <numeric>
#include <optional>
#include <queue>
#include <regex>
#include <set>
#include <shared_mutex>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// WIL
#include <wil/com.h>
#include <wil/stl.h>
#include <wil/filesystem.h>
// Due to the use of RESOURCE_SUPPRESS_STL in result.h, we need to include resource.h first, which happens
// implicitly through the includes above. If RESOURCE_SUPPRESS_STL is gone, the order doesn't matter anymore.
#include <wil/result.h>
#include <wil/nt_result_macros.h>

// GSL
// Block GSL Multi Span include because it both has C++17 deprecated iterators
// and uses the C-namespaced "max" which conflicts with Windows definitions.
#include <gsl/gsl_util>
#include <gsl/pointers>

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
// Microsoft::WRL::Details::StaticStorage contains a programming error.
// The author attempted to create a properly aligned backing storage for a type T,
// but instead of giving the member the proper alignas, the struct got it.
// The compiler doesn't like that. --> Suppress the warning.
#pragma warning(push)
#pragma warning(disable: 4324) // structure was padded due to alignment specifier
#include <wrl.h>
#pragma warning(pop)

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
