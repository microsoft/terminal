// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <guiddef.h>

#if !defined(_M_IX86) && !defined(_M_X64)

// ARM64 doesn't define a (__builtin_)memcmp function without CRT,
// but we need one to compile IID_GENERIC_CHECK_IID.
// Luckily we only ever use memcmp for IIDs.
#pragma function(memcmp)
inline int memcmp(const IID* a, const IID* b, size_t count)
{
    (void)(count);
    return 1 - InlineIsEqualGUID(a, b);
}

#endif
