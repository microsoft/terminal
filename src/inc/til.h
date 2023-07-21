// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

// This is a copy of how DirectXMath.h determines _XM_SSE_INTRINSICS_ and _XM_ARM_NEON_INTRINSICS_.
#if (defined(_M_IX86) || defined(_M_X64) || __i386__ || __x86_64__) && !defined(_M_HYBRID_X86_ARM64) && !defined(_M_ARM64EC)
#define TIL_SSE_INTRINSICS
#elif defined(_M_ARM) || defined(_M_ARM64) || defined(_M_HYBRID_X86_ARM64) || defined(_M_ARM64EC) || __arm__ || __aarch64__
#define TIL_ARM_NEON_INTRINSICS
#else
#define TIL_NO_INTRINSICS
#endif

#define _TIL_INLINEPREFIX __declspec(noinline) inline

#include "til/at.h"
#include "til/bitmap.h"
#include "til/coalesce.h"
#include "til/color.h"
#include "til/enumset.h"
#include "til/pmr.h"
#include "til/replace.h"
#include "til/string.h"
#include "til/u8u16convert.h"

// Use keywords on TraceLogging providers to specify the category
// of event that we are emitting for filtering purposes.
// The bottom 48 bits (0..47) are definable by each provider.
// The top 16 bits are reserved by Microsoft.
// NOTE: Any provider registering TraceLoggingOptionMicrosoftTelemetry
// should also reserve bits 43..47 for telemetry controls.
//
// To ensure that providers that transmit both telemetry
// and diagnostic information do not do excess work when only
// a telemetry listener is attached, please set a keyword
// on all TraceLoggingWrite statements.
//
// Use TIL_KEYWORD_TRACE if you are basically
// using it as a printf-like debugging tool for super
// deep diagnostics reasons only.
//
// Please do NOT leave events marked without a keyword
// or filtering on intent will not be possible.
//
// See also https://osgwiki.com/wiki/TraceLogging#Semantics
//
// Note that Conhost had already defined some keywords
// between bits 0..12 so be sure to not overlap those.
// See `TraceKeywords`.
// We will therefore try to reserve 32..42 for TIL
// as common flags for the entire Terminal team projects.
#define TIL_KEYWORD_TRACE 0x0000000100000000 // bit 32

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    template<typename T>
    void manage_vector(std::vector<T>& vector, typename std::vector<T>::size_type requestedSize, float shrinkThreshold)
    {
        const auto existingCapacity = vector.capacity();
        const auto requiredCapacity = requestedSize;

        // Check by integer first as float math is way more expensive.
        if (requiredCapacity < existingCapacity)
        {
            // Now check if it's even worth shrinking. We don't want to shrink by 1 at a time, so meet a threshold first.
            if (requiredCapacity <= gsl::narrow_cast<size_t>((static_cast<float>(existingCapacity) * shrinkThreshold)))
            {
                // There's no real way to force a shrink, so make a new one.
                vector = std::vector<T>{};
            }
        }

        // Reserve won't shrink on its own and won't grow if we have enough space.
        vector.reserve(requiredCapacity);
    }
}

// These sit outside the namespace because they sit outside for WIL too.

// Inspired from RETURN_IF_WIN32_BOOL_FALSE
// WIL doesn't include a RETURN_BOOL_IF_FALSE, and RETURN_IF_WIN32_BOOL_FALSE
//  will actually return the value of GLE.
#define RETURN_BOOL_IF_FALSE(b)                     \
    do                                              \
    {                                               \
        const bool __boolRet = wil::verify_bool(b); \
        if (!__boolRet)                             \
        {                                           \
            return __boolRet;                       \
        }                                           \
    } while (0, 0)

// Due to a bug (DevDiv 441931), Warning 4297 (function marked noexcept throws
// exception) is detected even when the throwing code is unreachable, such as
// the end of scope after a return, in function-level catch.
#define CATCH_LOG_RETURN_FALSE()            \
    catch (...)                             \
    {                                       \
        __pragma(warning(suppress : 4297)); \
        LOG_CAUGHT_EXCEPTION();             \
        return false;                       \
    }

// This is like the above, but doesn't log any messages. This is for GH#10882.
#define CATCH_RETURN_FALSE() \
    catch (...)              \
    {                        \
        return false;        \
    }

// MultiByteToWideChar has a bug in it where it can return 0 and then not set last error.
// WIL has a fit if the last error is 0 when a bool false is returned.
// This macro doesn't have a fit. It just reports E_UNEXPECTED instead.
#define THROW_LAST_ERROR_IF_AND_IGNORE_BAD_GLE(condition) \
    do                                                    \
    {                                                     \
        if (condition)                                    \
        {                                                 \
            const auto gle = ::GetLastError();            \
            if (gle)                                      \
            {                                             \
                THROW_WIN32(gle);                         \
            }                                             \
            else                                          \
            {                                             \
                THROW_HR(E_UNEXPECTED);                   \
            }                                             \
        }                                                 \
    } while (0, 0)

// clang-format off
#define TIL_FAST_MATH_BEGIN                \
    _Pragma("float_control(push)")         \
    _Pragma("float_control(precise, off)") \
    _Pragma("float_control(except, off)")  \
    _Pragma("fenv_access(off)")            \
    _Pragma("fp_contract(on)")

#define TIL_FAST_MATH_END \
    _Pragma("float_control(pop)")
// clang-format on
