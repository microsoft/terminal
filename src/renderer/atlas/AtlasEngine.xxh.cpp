// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AtlasEngine.h"

using namespace Microsoft::Console::Render;

// XXH3 for exactly 32 bytes.
uint64_t AtlasEngine::XXH3_len_32_64b(const void* data) noexcept
{
    static constexpr uint64_t dataSize = 32;
    static constexpr auto XXH3_mul128_fold64 = [](uint64_t lhs, uint64_t rhs) noexcept {
        uint64_t lo, hi;

#if defined(_M_AMD64)
        lo = _umul128(lhs, rhs, &hi);
#elif defined(_M_ARM64)
        lo = lhs * rhs;
        hi = __umulh(lhs, rhs);
#else
// Implemented as a macro as MSVC tends to call __allmul otherwise.
#define XXH_mult32to64(x, y) __emulu(gsl::narrow_cast<uint32_t>(x), gsl::narrow_cast<uint32_t>(y))
        const uint64_t lo_lo = XXH_mult32to64(lhs, rhs);
        const uint64_t hi_lo = XXH_mult32to64(lhs >> 32, rhs);
        const uint64_t lo_hi = XXH_mult32to64(lhs, rhs >> 32);
        const uint64_t hi_hi = XXH_mult32to64(lhs >> 32, rhs >> 32);
        const uint64_t cross = (lo_lo >> 32) + (hi_lo & 0xFFFFFFFF) + lo_hi;
        hi = (hi_lo >> 32) + (cross >> 32) + hi_hi;
        lo = (cross << 32) | (lo_lo & 0xFFFFFFFF);
#undef XXH_mult32to64
#endif

        return lo ^ hi;
    };

    // If executed on little endian CPUs these 4 numbers will
    // equal the first 32 byte of the original XXH3_kSecret.
    static constexpr uint64_t XXH3_kSecret[4] = {
        UINT64_C(0xbe4ba423396cfeb8),
        UINT64_C(0x1cad21f72c81017c),
        UINT64_C(0xdb979083e96dd4de),
        UINT64_C(0x1f67b3b7a4a44072),
    };

    uint64_t inputs[4];
    memcpy(&inputs[0], data, 32);

#pragma warning(suppress : 26450) // Arithmetic overflow: '*' operation causes overflow at compile time. Use a wider type to store the operands (io.1).
    uint64_t acc = dataSize * UINT64_C(0x9E3779B185EBCA87);
    acc += XXH3_mul128_fold64(inputs[0] ^ XXH3_kSecret[0], inputs[1] ^ XXH3_kSecret[1]);
    acc += XXH3_mul128_fold64(inputs[2] ^ XXH3_kSecret[2], inputs[3] ^ XXH3_kSecret[3]);
    acc = acc ^ (acc >> 37);
    acc *= UINT64_C(0x165667919E3779F9);
    acc = acc ^ (acc >> 32);
    return acc;
}
