// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#ifdef NODEFAULTLIB

#include <intrin.h>

#pragma function(memcpy)
void* memcpy(void* dst, const void* src, size_t size)
{
    __movsb(static_cast<BYTE*>(dst), static_cast<const BYTE*>(src), size);
    return dst;
}

#pragma function(memset)
void* memset(void* dst, int val, size_t size)
{
    __stosb(static_cast<BYTE*>(dst), static_cast<BYTE>(val), size);
    return dst;
}

#endif
