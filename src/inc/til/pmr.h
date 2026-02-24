// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

// The Windows build doesn't fully support std::pmr, but we can
// work around that limitation by providing our own default resource.
//
// This is a reimplementation of the default aligned allocator in
// <memory_resource>.
//
// Outside of Windows, til::pmr will use pmr directly.
namespace til::pmr
{
#ifdef __INSIDE_WINDOWS
    // I really, really didn't want to have to stick this function in here.
    // However, the default constructor for optional (really, "optional construct base")
    // insists on being able to default-construct its type. We have an instance in til
    // of an optional<vector<..., pmr allocator>> that forces _Aligned_get_default_resource
    // to get pulled in. This pragma mollifies the linker, and using this function in
    // get_default_resource below forces it to be included by the compiler.
    // I *believe* that if the VC++ Runtime is updated to include the PMR source,
    // this will safely no-op (since it's an ALTERNATENAME).
#if defined(_M_AMD64) || defined(_M_ARM64) || defined(_M_ARM)
#pragma comment(linker, "/ALTERNATENAME:_Aligned_get_default_resource=TIL_PMR_Aligned_get_default_resource")
#else
#pragma comment(linker, "/ALTERNATENAME:__Aligned_get_default_resource=_TIL_PMR_Aligned_get_default_resource")
#endif
    extern "C" _TIL_INLINEPREFIX std::pmr::memory_resource* __cdecl TIL_PMR_Aligned_get_default_resource() noexcept
    {
        class aligned_new_delete_resource : public std::pmr::memory_resource
        {
            bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override
            {
                return this == &other;
            }

            void* do_allocate(const size_t bytes, const size_t align) override
            {
                if (align > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
                {
                    return ::operator new(bytes, std::align_val_t{ align });
                }

                return ::operator new(bytes);
            }

            void do_deallocate(void* const ptr, const size_t bytes, const size_t align) noexcept override
            {
                if (align > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
                {
                    return ::operator delete(ptr, bytes, std::align_val_t{ align });
                }

                ::operator delete(ptr, bytes);
            }
        };

        static aligned_new_delete_resource resource;
        return &resource;
    }

    [[nodiscard]] inline std::pmr::memory_resource* get_default_resource() noexcept
    {
        return TIL_PMR_Aligned_get_default_resource();
    }
#else // !__INSIDE_WINDOWS
    [[nodiscard]] inline std::pmr::memory_resource* get_default_resource() noexcept
    {
        return std::pmr::get_default_resource();
    }
#endif
}
