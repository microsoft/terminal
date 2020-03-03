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

        bool operator==(const size& other) const
        {
            return _width == other._width &&
                _height == other._height;
        }

        bool operator!=(const size& other) const
        {
            return !(*this == other);
        }

        constexpr size_t width() const
        {
            return _width;
        }

        constexpr ptrdiff_t width_signed() const
        {
            return static_cast<ptrdiff_t>(_width);
        }

        constexpr size_t height() const
        {
            return _height;
        }

        constexpr ptrdiff_t height_signed() const
        {
            return static_cast<ptrdiff_t>(_height);
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
