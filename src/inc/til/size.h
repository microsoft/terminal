// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    class size
    {
    public:
        constexpr size() noexcept :
            size(0, 0)
        {

        }

        constexpr size(size_t width, size_t height) noexcept :
            _width(width),
            _height(height)
        {

        }

        template<typename TOther>
        constexpr size(const TOther& other, std::enable_if_t<std::is_integral_v<decltype(std::declval<TOther>().X)> && std::is_integral_v<decltype(std::declval<TOther>().Y)>, int> /*sentinel*/ = 0) :
            size(static_cast<size_t>(other.X), static_cast<size_t>(other.Y))
        {

        }

        template<typename TOther>
        constexpr size(const TOther& other, std::enable_if_t<std::is_integral_v<decltype(std::declval<TOther>().cx)> && std::is_integral_v<decltype(std::declval<TOther>().cy)>, int> /*sentinel*/ = 0) :
            size(static_cast<size_t>(other.cx), static_cast<size_t>(other.cy))
        {

        }

        constexpr size_t width() const
        {
            return _width;
        }

        constexpr size_t height() const
        {
            return _height;
        }

        constexpr size_t area() const
        {
            return _width * _height;
        }

#ifdef UNIT_TESTING
        friend class SizeTests;
#endif

    protected:
        size_t _width;
        size_t _height;
    };
}
