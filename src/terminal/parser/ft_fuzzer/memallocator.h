// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#ifndef __MEMALLOCATOR_H__
#define __MEMALLOCATOR_H__

#pragma once

namespace fuzz
{
    struct CFuzzCRTAllocator
    {
        _Ret_maybenull_ _Post_writable_byte_size_(nBytes) static void* Reallocate(
            _In_ void* p,
            _In_ size_t nBytes) throw()
        {
            return realloc(p, nBytes);
        }

        _Ret_maybenull_ _Post_writable_byte_size_(nBytes) static void* Allocate(_In_ size_t nBytes) throw()
        {
            return malloc(nBytes);
        }

        static void Free(_In_ void* p) throw()
        {
            free(p);
        }
    };
}

#endif
