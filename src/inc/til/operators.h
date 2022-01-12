// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    // Operators go here when they involve two headers that can't/don't include each other.

#pragma region POINT VS SIZE
    // This is a convenience and will take X vs WIDTH and Y vs HEIGHT.
    constexpr point operator+(const point lhs, const size rhs)
    {
        return lhs + til::point{ rhs.width, rhs.height };
    }

    constexpr point operator-(const point lhs, const size rhs)
    {
        return lhs - til::point{ rhs.width, rhs.height };
    }

    constexpr point operator*(const point lhs, const size rhs)
    {
        return lhs * til::point{ rhs.width, rhs.height };
    }

    constexpr point operator/(const point lhs, const size rhs)
    {
        return lhs / til::point{ rhs.width, rhs.height };
    }
#pragma endregion

#pragma region SIZE VS POINT
    // This is a convenience and will take WIDTH vs X and HEIGHT vs Y.
    constexpr size operator+(const size lhs, const point rhs)
    {
        return lhs + til::size(rhs.x, rhs.y);
    }

    constexpr size operator-(const size lhs, const point rhs)
    {
        return lhs - til::size(rhs.x, rhs.y);
    }

    constexpr size operator*(const size lhs, const point rhs)
    {
        return lhs * til::size(rhs.x, rhs.y);
    }

    constexpr size operator/(const size lhs, const point rhs)
    {
        return lhs / til::size(rhs.x, rhs.y);
    }
#pragma endregion
}
