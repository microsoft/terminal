//--------------------------------------------------------------------------------------
// File: AlignedNew.h
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#pragma once

#include <cstddef>
#include <cstdlib>
#include <new>

#ifdef _WIN32
#include <malloc.h>
#endif


namespace DirectX
{
    // Derive from this to customize operator new and delete for
    // types that have special heap alignment requirements.
    //
    // Example usage:
    //
    //      XM_ALIGNED_STRUCT(16) MyAlignedType : public AlignedNew<MyAlignedType>

    template<typename TDerived>
    struct AlignedNew
    {
        // Allocate aligned memory.
        static void* operator new (size_t size)
        {
            const size_t alignment = alignof(TDerived);

            static_assert(alignment > 8, "AlignedNew is only useful for types with > 8 byte alignment. Did you forget a __declspec(align) on TDerived?");
            static_assert(((alignment - 1) & alignment) == 0, "AlignedNew only works with power of two alignment");

        #ifdef _WIN32
            void* ptr = _aligned_malloc(size, alignment);
        #else
                    // This C++17 Standard Library function is currently NOT
                    // implemented for the Microsoft Standard C++ Library.
            void* ptr = aligned_alloc(alignment, size);
        #endif
            if (!ptr)
                throw std::bad_alloc();

            return ptr;
        }


        // Free aligned memory.
        static void operator delete (void* ptr)
        {
        #ifdef _WIN32
            _aligned_free(ptr);
        #else
            free(ptr);
        #endif
        }


        // Array overloads.
        static void* operator new[](size_t size)
        {
            static_assert((sizeof(TDerived) % alignof(TDerived) == 0), "AlignedNew expects type to be padded to the alignment");

            return operator new(size);
        }


            static void operator delete[](void* ptr)
        {
            operator delete(ptr);
        }
    };
}
