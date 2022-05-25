// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til
{
    // bit_cast is a backport of the STL's std::bit_cast to C++17.
    template<class To, class From, std::enable_if_t<std::conjunction_v<std::bool_constant<sizeof(To) == sizeof(From)>, std::is_trivially_copyable<To>, std::is_trivially_copyable<From>>, int> = 0>
    [[nodiscard]] constexpr To bit_cast(const From& _Val) noexcept
    {
        // TODO: Replace til::bit_cast and __builtin_bit_cast with std::bit_cast
        return __builtin_bit_cast(To, _Val);
    }

    // When you cast a signed integer to an unsigned one, the compiler will use "sign extension"
    // so that -1 translates to all bits being set, no matter the size of the target type.
    // Sometimes you don't need or want that, which is when you can use this function.
    template<typename T>
    [[nodiscard]] constexpr auto as_unsigned(const T& v) noexcept
    {
        return bit_cast<std::make_unsigned_t<T>>(v);
    }
}
