// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    // pair is a simple clone of std::pair, with one difference:
    // copy and move constructors and operators are explicitly defaulted.
    // This allows pair to be std::is_trivially_copyable, if both T and S are.
    // --> pair can be used with memcpy(), unlike std::pair.
    template<typename T, typename S>
    struct pair
    {
        using first_type = T;
        using second_type = S;

        pair() = default;

        pair(const pair&) = default;
        pair& operator=(const pair&) = default;

        pair(pair&&) = default;
        pair& operator=(pair&&) = default;

        constexpr pair(const T& first, const S& second) noexcept(std::is_nothrow_copy_constructible_v<T>&& std::is_nothrow_copy_constructible_v<S>) :
            first(first), second(second)
        {
        }

        constexpr pair(T&& first, S&& second) noexcept(std::is_nothrow_constructible_v<T>&& std::is_nothrow_constructible_v<S>) :
            first(std::forward<T>(first)), second(std::forward<S>(second))
        {
        }

        constexpr void swap(pair& other) noexcept(std::is_nothrow_swappable_v<T>&& std::is_nothrow_swappable_v<S>)
        {
            if (this != std::addressof(other))
            {
                std::swap(first, other.first);
                std::swap(second, other.second);
            }
        }

        first_type first{};
        second_type second{};
    };

    template<typename T, typename S>
    [[nodiscard]] constexpr bool operator==(const pair<T, S>& lhs, const pair<T, S>& rhs)
    {
        return lhs.first == rhs.first && lhs.second == rhs.second;
    }

    template<typename T, typename S>
    [[nodiscard]] constexpr bool operator!=(const pair<T, S>& lhs, const pair<T, S>& rhs)
    {
        return !(lhs == rhs);
    }
};
