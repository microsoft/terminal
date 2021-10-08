// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <pcg_random.hpp>

namespace til
{
    namespace details
    {
        typedef BOOLEAN(APIENTRY* RtlGenRandom)(PVOID RandomBuffer, ULONG RandomBufferLength);

        struct RtlGenRandomLoader
        {
            RtlGenRandomLoader() noexcept :
                // The documentation states to use advapi32.dll, but technically
                // SystemFunction036 lives in cryptbase.dll since Windows 7.
                module{ LoadLibraryExW(L"cryptbase.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32) },
                proc{ reinterpret_cast<decltype(proc)>(GetProcAddress(module.get(), "SystemFunction036")) }
            {
                FAIL_FAST_LAST_ERROR_IF(!proc);
            }

            inline void operator()(PVOID RandomBuffer, ULONG RandomBufferLength)
            {
                proc(RandomBuffer, RandomBufferLength);
            }

        private:
            wil::unique_hmodule module;
            RtlGenRandom proc;
        };
    }

    inline void gen_random(void* data, uint32_t length)
    {
        static details::RtlGenRandomLoader loader;
        loader(data, length);
    }

    template<typename T, typename = std::enable_if_t<std::is_standard_layout_v<T>>>
    T gen_random()
    {
        T value;
        gen_random(&value, sizeof(T));
        return value;
    }
}
