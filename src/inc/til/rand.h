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
                // The documentation states:
                //   This function has no associated import library. This function is available
                //   as a resource named SystemFunction036 in Advapi32.dll. You must use the
                //   LoadLibrary and GetProcAddress functions to dynamically link to Advapi32.dll.
                //
                // There's two downsides to using advapi32.dll however:
                // * The actual implementation resides in cryptbase.dll and...
                // * In older versions of Windows (7 and older) advapi32.dll didn't use forwarding to
                //   cryptbase, instead it was using LoadLibrary()/GetProcAddress() on every call.
                // * advapi32.dll doesn't exist on MinWin, cryptbase.dll however does.
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
