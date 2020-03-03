// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    class point
    {
    public:
        constexpr point() noexcept :
            point(0, 0)
        {

        }

        constexpr point(ptrdiff_t x, ptrdiff_t y) noexcept :
            _x(x),
            _y(y)
        {

        }

        template<typename TOther>
        constexpr point(const TOther& other, std::enable_if_t<std::is_integral_v<decltype(std::declval<TOther>().X)> && std::is_integral_v<decltype(std::declval<TOther>().Y)>, int> /*sentinel*/ = 0) :
            point(static_cast<ptrdiff_t>(other.X), static_cast<ptrdiff_t>(other.Y))
        {

        }

        template<typename TOther>
        constexpr point(const TOther& other, std::enable_if_t<std::is_integral_v<decltype(std::declval<TOther>().x)> && std::is_integral_v<decltype(std::declval<TOther>().y)>, int> /*sentinel*/ = 0) :
            point(static_cast<ptrdiff_t>(other.x), static_cast<ptrdiff_t>(other.y))
        {

        }

        constexpr ptrdiff_t x()
        {
            return _x;
        }

        constexpr ptrdiff_t y()
        {
            return _y;
        }

#ifdef UNIT_TESTING
        friend class PointTests;
#endif

    protected:
        ptrdiff_t _x;
        ptrdiff_t _y;
    };
}
