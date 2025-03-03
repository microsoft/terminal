// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <filesystem>
#include <functional>
#include <optional>
#include <shared_mutex>
#include <span>
#include <string_view>
#include <vector>
#include <cassert>

#include <d2d1_3.h>
#include <d3d11_2.h>
#include <d3dcompiler.h>
#include <dcomp.h>
#include <dwrite_3.h>
#include <dxgi1_3.h>
#include <dxgidebug.h>
#include <VersionHelpers.h>
#include <wincodec.h>

#include <gsl/gsl_util>
#include <gsl/pointers>
#include <wil/com.h>
#include <wil/filesystem.h>
#include <wil/stl.h>

// Chromium Numerics (safe math)
#pragma warning(push)
#pragma warning(disable : 4100) // '...': unreferenced formal parameter
#pragma warning(disable : 26812) // The enum type '...' is unscoped. Prefer 'enum class' over 'enum' (Enum.3).
#include <base/numerics/safe_math.h>
#pragma warning(pop)

// {fmt}, a C++20-compatible formatting library
#pragma warning(push)
#pragma warning(disable : 4702) // unreachable code
#include <fmt/compile.h>
#include <fmt/xchar.h>
#pragma warning(pop)

#include <til.h>
