// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "point.h"
#include "size.h"

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    class recterator
    {
    public:
        recterator(til::point topLeft, til::size size) :
            _topLeft(topLeft),
            _size(size),
            _current(topLeft)
        {

        }

        recterator(til::point topLeft, til::size size, til::point start) :
            _topLeft(topLeft),
            _size(size),
            _current(start)
        {

        }

        recterator& operator++()
        {
            ++_current.x();
            if (_current.x() >= _topLeft.x() + _size.width_signed())
            {
                _current.x() = _topLeft.x();
                ++_current.y();
            }

            return (*this);
        }

        bool operator==(const recterator& other) const
        {
            return _current == other._current &&
                _topLeft == other._topLeft &&
                _size == other._size;
        }

        bool operator!=(const recterator& other) const
        {
            return !(*this == other);
        }

        bool operator<(const recterator& other) const
        {
            return _current < other._current;
        }

        bool operator>(const recterator& other) const
        {
            return _current > other._current;
        }

        til::point operator*() const
        {
            return _current;
        }

    protected:

        til::point _current;
        const til::point _topLeft;
        const til::size _size;
    };

    class rectangle
    {
    public:
        using const_iterator = recterator;

        rectangle() :
            rectangle(til::point{ 0, 0 }, til::size{ 0, 0 })
        {

        }

        rectangle(til::point topLeft, til::point bottomRight) :
            rectangle(topLeft, til::size{ static_cast<size_t>(bottomRight.x() - topLeft.x()), static_cast<size_t>(bottomRight.y() - topLeft.y()) })
        {

        }

        rectangle(til::point topLeft, til::size size) :
            _topLeft(topLeft),
            _size(size)
        {

        }

        template<typename TOther>
        constexpr rectangle(const TOther& other, std::enable_if_t<std::is_integral_v<decltype(std::declval<TOther>().Top)> && std::is_integral_v<decltype(std::declval<TOther>().Left)> && std::is_integral_v<decltype(std::declval<TOther>().Bottom)> && std::is_integral_v<decltype(std::declval<TOther>().Right)>, int> /*sentinel*/ = 0) :
            rectangle(til::point{static_cast<ptrdiff_t>(other.Left), static_cast<ptrdiff_t>(other.Top)}, til::point{static_cast<ptrdiff_t>(other.Right), static_cast<ptrdiff_t>(other.Bottom)})
        {

        }

        template<typename TOther>
        constexpr rectangle(const TOther& other, std::enable_if_t < std::is_integral_v<decltype(std::declval<TOther>().top)> && std::is_integral_v<decltype(std::declval<TOther>().left)> && std::is_integral_v<decltype(std::declval<TOther>().bottom)> && std::is_integral_v<decltype(std::declval<TOther>().right)>, int> /*sentinel*/ = 0) :
            rectangle(til::point{ static_cast<ptrdiff_t>(other.left), static_cast<ptrdiff_t>(other.top) }, til::point{ static_cast<ptrdiff_t>(other.right), static_cast<ptrdiff_t>(other.bottom)})
        {

        }

        const_iterator begin() const
        {
            return recterator(_topLeft, _size);
        }

        const_iterator end() const
        {
            return recterator(_topLeft, _size, { _topLeft.x(), _topLeft.y() + _size.height_signed() });
        }

        ptrdiff_t top() const
        {
            return _topLeft.y();
        }

        ptrdiff_t bottom() const
        {
            return top() + _size.height_signed();
        }

        ptrdiff_t bottom_inclusive() const
        {
            return bottom() - 1;
        }

        ptrdiff_t left() const
        {
            return _topLeft.x();
        }

        ptrdiff_t right() const
        {
            return left() + _size.width_signed();
        }

        ptrdiff_t right_inclusive() const
        {
            return right() - 1;
        }

#ifdef UNIT_TESTING
        friend class RectangleTests;
#endif
    protected:
        til::point _topLeft;
        til::size _size;
    };
}
