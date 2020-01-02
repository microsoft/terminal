// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    // The at function declares that you've already sufficiently checked that your array access
    // is in range before retrieving an item inside it at an offset.
    // This is to save double/triple/quadruple testing in circumstances where you are already
    // pivoting on the length of a set and now want to pull elements out of it by offset
    // without checking again.
    // gsl::at will do the check again. As will .at(). And using [] will have a warning in audit.
    template<class T>
    constexpr auto at(T& cont, const size_t i) -> decltype(cont[cont.size()])
    {
#pragma warning(suppress : 26482) // Suppress bounds.2 check for indexing with constant expressions
#pragma warning(suppress : 26446) // Suppress bounds.4 check for subscript operator.
        return cont[i];
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

// Due to a bug (DevDiv 441931), Warning 4297 (function marked noexcept throws exception) is detected even when the throwing code is unreachable, such as the end of scope after a return, in function-level catch.
#define CATCH_LOG_RETURN_FALSE()            \
    catch (...)                             \
    {                                       \
        __pragma(warning(suppress : 4297)); \
        LOG_CAUGHT_EXCEPTION();             \
        return false;                       \
    }
