// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til
{
    // When you cast a signed integer to an unsigned one, the compiler will use "sign extension"
    // so that -1 translates to all bits being set, no matter the size of the target type.
    // Sometimes you don't need or want that, which is when you can use this function.
    template<typename T>
    [[nodiscard]] constexpr auto as_unsigned(const T& v) noexcept
    {
        return std::bit_cast<std::make_unsigned_t<T>>(v);
    }
}
