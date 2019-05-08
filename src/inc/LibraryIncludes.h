// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <CppCoreCheck/Warnings.h>

#pragma warning(push)
#pragma warning(disable: ALL_CPPCORECHECK_WARNINGS)

// C
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
#include <thread>
#include <tuple>
#include <utility>
#include <vector>
#include <unordered_map>
#include <iterator>
#include <math.h>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <functional>

// WIL

#include <wil/Common.h>
#include <wil/Result.h>
#include <wil/resource.h>
#include <wil/wistd_memory.h>

// GSL
// Block GSL Multi Span include because it both has C++17 deprecated iterators
// and uses the C-namespaced "max" which conflicts with Windows definitions.
#define GSL_MULTI_SPAN_H
#include <gsl/gsl>

// IntSafe
#define ENABLE_INTSAFE_SIGNED_FUNCTIONS
#include <intsafe.h>

// SAL
#include <sal.h>

#pragma warning(pop)
