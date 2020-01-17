// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til
{
    template<typename T>
    using checked_numeric = base::internal::CheckedNumeric<T>;

    template<typename T>
    using clamped_numeric = base::internal::ClampedNumeric<T>;

    template<typename T>
    constexpr T check_add(T a, T b)
    {
        return base::CheckAdd(a, b).ValueOrDie();
    }

    template<typename T>
    constexpr T clamp_add(T a, T b)
    {
        return base::ClampAdd(a, b).ValueOrDie();
    }
}
