// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "point.h"
#include "size.h"
#include "some.h"

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
            if (_current.x() + 1 >= _topLeft.x() + _size.width())
            {
                _current = { _topLeft.x(), _current.y() + 1 };
            }
            else
            {
                _current = { _current.x() + 1, _current.y() };
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

        constexpr rectangle() noexcept :
            rectangle(til::point{ 0, 0 }, til::point{ 0, 0 })
        {

        }

        constexpr rectangle(ptrdiff_t left, ptrdiff_t top, ptrdiff_t right, ptrdiff_t bottom) noexcept:
            rectangle(til::point{ left, top }, til::point{ right, bottom })
        {

        }

        rectangle(til::point topLeft):
            _topLeft(topLeft)
        {
            _bottomRight = _topLeft + 1;
        }

        constexpr rectangle(til::point topLeft, til::point bottomRight) noexcept :
            _topLeft(topLeft),
            _bottomRight(bottomRight)
        {

        }

        rectangle(til::point topLeft, til::size size) :
            _topLeft(topLeft)
        {
            ptrdiff_t right;
            ptrdiff_t bottom;

            THROW_HR_IF(E_ABORT, !(base::MakeCheckedNum(topLeft.x()) + size.width()).AssignIfValid(&right));
            THROW_HR_IF(E_ABORT, !(base::MakeCheckedNum(topLeft.y()) + size.height()).AssignIfValid(&bottom));

            _bottomRight = til::point{ right, bottom };
        }

#ifdef _WINCONTYPES_
        rectangle(SMALL_RECT sr)
        {
            _topLeft = til::point{ static_cast<ptrdiff_t>(sr.Left), static_cast<ptrdiff_t>(sr.Top) };

            _bottomRight = til::point{ static_cast<ptrdiff_t>(sr.Right), static_cast<ptrdiff_t>(sr.Bottom) } + til::point{ 1, 1 };
        }
#endif

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
            return recterator(_topLeft, size());
        }

        const_iterator end() const
        {
            return recterator(_topLeft, size(), { _topLeft.x(), _topLeft.y() + height() });
        }

        constexpr ptrdiff_t top() const noexcept
        {
            return _topLeft.y();
        }

        constexpr ptrdiff_t bottom() const noexcept
        {
            return _bottomRight.y();
        }

        ptrdiff_t bottom_inclusive() const
        {
            ptrdiff_t ret;
            THROW_HR_IF(E_ABORT, !(::base::MakeCheckedNum(bottom()) - 1).AssignIfValid(&ret));
            return ret;
        }

        constexpr ptrdiff_t left() const noexcept
        {
            return _topLeft.x();
        }

        constexpr ptrdiff_t right() const noexcept
        {
            return _bottomRight.x();
        }

        ptrdiff_t right_inclusive() const
        {
            ptrdiff_t ret;
            THROW_HR_IF(E_ABORT, !(::base::MakeCheckedNum(right()) - 1).AssignIfValid(&ret));
            return ret;
        }

        ptrdiff_t width() const
        {
            ptrdiff_t ret;
            THROW_HR_IF(E_ABORT, !(::base::MakeCheckedNum(right()) - left()).AssignIfValid(&ret));
            return ret;
        }

        template<typename T>
        T width() const
        {
            T ret;
            THROW_HR_IF(E_ABORT, !base::MakeCheckedNum(width()).AssignIfValid(&ret));
            return ret;
        }

        ptrdiff_t height() const 
        {
            ptrdiff_t ret;
            THROW_HR_IF(E_ABORT, !(::base::MakeCheckedNum(bottom()) - top()).AssignIfValid(&ret));
            return ret;
        }

        template<typename T>
        T height() const
        {
            T ret;
            THROW_HR_IF(E_ABORT, !base::MakeCheckedNum(height()).AssignIfValid(&ret));
            return ret;
        }
        
        constexpr point origin() const noexcept
        {
            return _topLeft;
        }

        constexpr bool empty() const noexcept
        {
            return _topLeft.x() >= _bottomRight.x() ||
                _topLeft.y() >= _bottomRight.y();
        }

        size size() const 
        {
            return til::size{ width(), height() };
        }

        constexpr rectangle& operator=(const rectangle other) noexcept
        {
            _topLeft = other._topLeft;
            _bottomRight = other._bottomRight;
            return (*this);
        }

        constexpr bool operator==(const rectangle& other) const noexcept
        {
            return _topLeft == other._topLeft &&
                _bottomRight == other._bottomRight;
        }

        constexpr bool operator!=(const rectangle& other) const noexcept
        {
            return !(*this == other);
        }

        rectangle operator+(const rectangle& other) const
        {
            return rectangle{ _topLeft + other._topLeft, _bottomRight + other._bottomRight };
        }

        rectangle operator*(const rectangle& other) const
        {
            return rectangle{ _topLeft * other._topLeft, _bottomRight * other._bottomRight };
        }

        // OR = union
        constexpr rectangle operator|(const rectangle& other) const noexcept
        {
            const auto thisEmpty = empty();
            const auto otherEmpty = other.empty();

            // If both are empty, return empty rect.
            if (thisEmpty && otherEmpty)
            {
                return rectangle{};
            }

            // If this is empty but not the other one, then give the other.
            if (thisEmpty)
            {
                return other;
            }

            // If the other is empty but not this, give this.
            if (otherEmpty)
            {
                return *this;
            }

            // If we get here, they're both not empty. Do math.
            const auto l = std::min(left(), other.left());
            const auto t = std::min(top(), other.top());
            const auto r = std::max(right(), other.right());
            const auto b = std::max(bottom(), other.bottom());
            return rectangle{ l, t, r, b };
        }

        // AND = intersect
        constexpr rectangle operator&(const rectangle& other) const noexcept
        {
            const auto l = std::max(left(), other.left());
            const auto r = std::min(right(), other.right());

            // If the width dimension would be empty, give back empty rectangle.
            if (l >= r)
            {
                return rectangle{};
            }

            const auto t = std::max(top(), other.top());
            const auto b = std::max(bottom(), other.bottom());

            // If the height dimension would be empty, give back empty rectangle.
            if (t >= b)
            {
                return rectangle{};
            }

            return rectangle{ l, t, r, b };
        }

        // - = subtract
        some<rectangle, 4> operator-(const rectangle& other) const
        {
            some<rectangle, 4> result;

            // We could have up to four rectangles describing the area resulting when you take removeMe out of main.
            // Find the intersection of the two so we know which bits of removeMe are actually applicable
            // to the original rectangle for subtraction purposes.
            const auto intersect = *this & other;

            // If there's no intersect, there's nothing to remove.
            if (intersect.empty())
            {
                // Just put the original rectangle into the results and return early.
                result.push_back(*this);
            }
            // If the original rectangle matches the intersect, there is nothing to return.
            else if (*this != intersect)
            {
                // Generate our potential four viewports that represent the region of the original that falls outside of the remove area.
                // We will bias toward generating wide rectangles over tall rectangles (if possible) so that optimizations that apply
                // to manipulating an entire row at once can be realized by other parts of the console code. (i.e. Run Length Encoding)
                // In the following examples, the found remaining regions are represented by:
                // T = Top      B = Bottom      L = Left        R = Right
                //
                // 4 Sides but Identical:
                // |-----------this-----------|             |-----------this-----------|
                // |                          |             |                          |
                // |                          |             |                          |
                // |                          |             |                          |
                // |                          |    ======>  |        intersect         |  ======>  early return of nothing
                // |                          |             |                          |
                // |                          |             |                          |
                // |                          |             |                          |
                // |-----------other----------|             |--------------------------|
                //
                // 4 Sides:
                // |-----------this-----------|             |-----------this-----------|           |--------------------------|
                // |                          |             |                          |           |TTTTTTTTTTTTTTTTTTTTTTTTTT|
                // |                          |             |                          |           |TTTTTTTTTTTTTTTTTTTTTTTTTT|
                // |        |---------|       |             |        |---------|       |           |LLLLLLLL|---------|RRRRRRR|
                // |        |other    |       |    ======>  |        |intersect|       |  ======>  |LLLLLLLL|         |RRRRRRR|
                // |        |---------|       |             |        |---------|       |           |LLLLLLLL|---------|RRRRRRR|
                // |                          |             |                          |           |BBBBBBBBBBBBBBBBBBBBBBBBBB|
                // |                          |             |                          |           |BBBBBBBBBBBBBBBBBBBBBBBBBB|
                // |--------------------------|             |--------------------------|           |--------------------------|
                //
                // 3 Sides:
                // |-----------this-----------|             |-----------this-----------|           |--------------------------|
                // |                          |             |                          |           |TTTTTTTTTTTTTTTTTTTTTTTTTT|
                // |                          |             |                          |           |TTTTTTTTTTTTTTTTTTTTTTTTTT|
                // |        |--------------------|          |        |-----------------|           |LLLLLLLL|-----------------|
                // |        |other               | ======>  |        |intersect        |  ======>  |LLLLLLLL|                 |
                // |        |--------------------|          |        |-----------------|           |LLLLLLLL|-----------------|
                // |                          |             |                          |           |BBBBBBBBBBBBBBBBBBBBBBBBBB|
                // |                          |             |                          |           |BBBBBBBBBBBBBBBBBBBBBBBBBB|
                // |--------------------------|             |--------------------------|           |--------------------------|
                //
                // 2 Sides:
                // |-----------this-----------|             |-----------this-----------|           |--------------------------|
                // |                          |             |                          |           |TTTTTTTTTTTTTTTTTTTTTTTTTT|
                // |                          |             |                          |           |TTTTTTTTTTTTTTTTTTTTTTTTTT|
                // |        |--------------------|          |        |-----------------|           |LLLLLLLL|-----------------|
                // |        |other               | ======>  |        |intersect        |  ======>  |LLLLLLLL|                 |
                // |        |                    |          |        |                 |           |LLLLLLLL|                 |
                // |        |                    |          |        |                 |           |LLLLLLLL|                 |
                // |        |                    |          |        |                 |           |LLLLLLLL|                 |
                // |--------|                    |          |--------------------------|           |--------------------------|
                //          |                    |
                //          |--------------------|
                //
                // 1 Side:
                // |-----------this-----------|             |-----------this-----------|           |--------------------------|
                // |                          |             |                          |           |TTTTTTTTTTTTTTTTTTTTTTTTTT|
                // |                          |             |                          |           |TTTTTTTTTTTTTTTTTTTTTTTTTT|
                // |-----------------------------|          |--------------------------|           |--------------------------|
                // |         other               | ======>  |         intersect        |  ======>  |                          |
                // |                             |          |                          |           |                          |
                // |                             |          |                          |           |                          |
                // |                             |          |                          |           |                          |
                // |                             |          |--------------------------|           |--------------------------|
                // |                             |
                // |-----------------------------|
                //
                // 0 Sides:
                // |-----------this-----------|             |-----------this-----------|
                // |                          |             |                          |
                // |                          |             |                          |
                // |                          |             |                          |
                // |                          |    ======>  |                          |  ======>  early return of this
                // |                          |             |                          |
                // |                          |             |                          |
                // |                          |             |                          |
                // |--------------------------|             |--------------------------|
                //
                //
                //         |---------------|
                //         | other         |
                //         |---------------|

                // We generate these rectangles by the original and intersect points, but some of them might be empty when the intersect
                // lines up with the edge of the original. That's OK. That just means that the subtraction didn't leave anything behind.
                // We will filter those out below when adding them to the result.
                const auto t = rectangle({ left(), top(), right(), intersect.top()});
                const auto b = rectangle({ left(), intersect.bottom(), right(), bottom() });
                const auto l = rectangle({ left(), intersect.top(), intersect.left(), intersect.bottom() });
                const auto r = rectangle({ intersect.right(), intersect.top(), right(), intersect.bottom() });

                if (!t.empty())
                {
                    result.push_back(t);
                }

                if (!b.empty())
                {
                    result.push_back(b);
                }

                if (!l.empty())
                {
                    result.push_back(l);
                }

                if (!r.empty())
                {
                    result.push_back(r);
                }
            }

            return result;
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
        til::point _bottomRight;
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
