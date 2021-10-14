// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til
{
    template<class To, class From, std::enable_if_t<std::conjunction_v<std::bool_constant<sizeof(To) == sizeof(From)>, std::is_trivially_copyable<To>, std::is_trivially_copyable<From>>, int> = 0>
    [[nodiscard]] constexpr To bit_cast(const From& _Val) noexcept
    {
#ifdef __cpp_lib_bit_cast
#warning "Replace til::bit_cast and __builtin_bit_cast with std::bit_cast"
#endif
        return __builtin_bit_cast(To, _Val);
    }
}
