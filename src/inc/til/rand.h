// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <pcg_random.hpp>

namespace til
{
    void gen_random(void* data, uint32_t length)
    {
        static const auto RtlGenRandom = []() {
            // The documentation states to use advapi32.dll, but technically
            // SystemFunction036 lives in cryptbase.dll since Windows 7.
            const auto cryptbase = LoadLibraryExW(L"cryptbase.dll", nullptr, 0);
            return reinterpret_cast<BOOLEAN(APIENTRY*)(PVOID, ULONG)>(GetProcAddress(cryptbase, "SystemFunction036"));
        }();
        RtlGenRandom(data, length);
    }

    template<typename T, typename = std::enable_if_t<std::is_standard_layout_v<T>>>
    T gen_random()
    {
        T value;
        gen_random(&value, sizeof(T));
        return value;
    }
}
