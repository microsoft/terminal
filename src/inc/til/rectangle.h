// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "point.h"
#include "size.h"

#ifdef UNIT_TESTING
class RectangleTests;
#endif

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
            if (_current.x() >= _topLeft.x() + _size.width())
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

        rectangle(ptrdiff_t left, ptrdiff_t top, ptrdiff_t right, ptrdiff_t bottom):
            rectangle(til::point{ left, top }, til::point{ right, bottom })
        {

        }

        rectangle(til::point topLeft, til::point bottomRight) :
            rectangle(topLeft, til::size{ bottomRight.x() - topLeft.x(), bottomRight.y() - topLeft.y() })
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
            return recterator(_topLeft, _size, { _topLeft.x(), _topLeft.y() + _size.height() });
        }

        ptrdiff_t top() const
        {
            return _topLeft.y();
        }

        ptrdiff_t bottom() const
        {
            return top() + _size.height();
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
            return left() + _size.width();
        }

        ptrdiff_t right_inclusive() const
        {
            return right() - 1;
        }

        ptrdiff_t width() const
        {
            return _size.width();
        }

        ptrdiff_t height() const
        {
            return _size.height();
        }

        rectangle& operator=(const rectangle other)
        {
            _size = other._size;
            _topLeft = other._topLeft;
            return (*this);
        }

        bool operator==(const til::rectangle& other) const
        {
            return _size == other._size &&
                _topLeft == other._topLeft;
        }

#ifdef _WINCONTYPES_
        operator SMALL_RECT() const
        {
            SMALL_RECT ret;
            THROW_HR_IF(E_ABORT, !base::MakeCheckedNum(left()).AssignIfValid(&ret.Left));
            THROW_HR_IF(E_ABORT, !base::MakeCheckedNum(top()).AssignIfValid(&ret.Top));
            THROW_HR_IF(E_ABORT, !base::MakeCheckedNum(right_inclusive()).AssignIfValid(&ret.Right));
            THROW_HR_IF(E_ABORT, !base::MakeCheckedNum(bottom_inclusive()).AssignIfValid(&ret.Bottom));
            return ret;
        }
#endif

#ifdef _WINDEF_
        operator RECT() const
        {
            RECT ret;
            THROW_HR_IF(E_ABORT, !base::MakeCheckedNum(left()).AssignIfValid(&ret.left));
            THROW_HR_IF(E_ABORT, !base::MakeCheckedNum(top()).AssignIfValid(&ret.top));
            THROW_HR_IF(E_ABORT, !base::MakeCheckedNum(right()).AssignIfValid(&ret.right));
            THROW_HR_IF(E_ABORT, !base::MakeCheckedNum(bottom()).AssignIfValid(&ret.bottom));
            return ret;
        }
#endif

#ifdef DCOMMON_H_INCLUDED
        constexpr operator D2D1_RECT_F() const noexcept
        {
            return D2D1_RECT_F{ gsl::narrow_cast<FLOAT>(left()), gsl::narrow_cast<FLOAT>(top()), gsl::narrow_cast<FLOAT>(right()), gsl::narrow_cast<FLOAT>(bottom()) };
        }
#endif

#ifdef UNIT_TESTING
        friend class ::RectangleTests;
#endif
    protected:
        til::point _topLeft;
        til::size _size;
    };
}

#ifdef __WEX_COMMON_H__
namespace WEX::TestExecution
{
    template<>
    class VerifyOutputTraits<::til::rectangle>
    {
    public:
        static WEX::Common::NoThrowString ToString(const ::til::rectangle& rect)
        {
            return WEX::Common::NoThrowString().Format(L"(L:%td, T:%td, R:%td, B:%td) [W:%td, H:%td]", rect.left(), rect.top(), rect.right(), rect.bottom(), rect.width(), rect.height());
        }
    };

    template<>
    class VerifyCompareTraits<::til::rectangle, ::til::rectangle>
    {
    public:
        static bool AreEqual(const ::til::rectangle& expected, const ::til::rectangle& actual)
        {
            return expected == actual;
        }

        static bool AreSame(const ::til::rectangle& expected, const ::til::rectangle& actual)
        {
            return &expected == &actual;
        }

        static bool IsLessThan(const ::til::rectangle& expectedLess, const ::til::rectangle& expectedGreater) = delete;

        static bool IsGreaterThan(const ::til::rectangle& expectedGreater, const ::til::rectangle& expectedLess) = delete;

        static bool IsNull(const ::til::rectangle& object)
        {
            return object == til::rectangle{};
        }
    };

};
#endif
