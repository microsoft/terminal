// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til
{
    template<typename T, std::size_t N, typename I>
    [[gsl::suppress(bounds)]] constexpr T& at(T (&arr)[N], const I i)
    {
        if (i < 0 || i >= N)
        {
            std::terminate();
        }
        return arr[i];
    }

    template<typename Cont, typename I>
    [[gsl::suppress(bounds)]] constexpr auto at(Cont& cont, const I i) -> decltype(auto)
    {
        if (i < 0 || i >= cont.size())
        {
            std::terminate();
        }
        return cont[i];
    }

    // The at function declares that you've already sufficiently checked that your array access
    // is in range before retrieving an item inside it at an offset.
    // This is to save double/triple/quadruple testing in circumstances where you are already
    // pivoting on the length of a set and now want to pull elements out of it by offset
    // without checking again.
    // til::at will do the check again. As will .at(). And using [] will have a warning in audit.
    // This template is explicitly disabled if T is of type std::span, as it would interfere with
    // the overload below.
    template<typename T, typename I>
    [[gsl::suppress(bounds)]] constexpr auto at_unchecked(T&& cont, const I i) noexcept -> decltype(auto)
    {
        return cont[i];
    }
}
