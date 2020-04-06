// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#ifdef UNIT_TESTING
class RectangleTests;
#endif

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    namespace details
    {
        class _rectangle_const_iterator
        {
        public:
            constexpr _rectangle_const_iterator(point topLeft, point bottomRight) :
                _topLeft(topLeft),
                _bottomRight(bottomRight),
                _current(topLeft)
            {
            }

            constexpr _rectangle_const_iterator(point topLeft, point bottomRight, point start) :
                _topLeft(topLeft),
                _bottomRight(bottomRight),
                _current(start)
            {
            }

            _rectangle_const_iterator& operator++()
            {
                ptrdiff_t nextX;
                THROW_HR_IF(E_ABORT, !::base::CheckAdd(_current.x(), 1).AssignIfValid(&nextX));

                if (nextX >= _bottomRight.x())
                {
                    ptrdiff_t nextY;
                    THROW_HR_IF(E_ABORT, !::base::CheckAdd(_current.y(), 1).AssignIfValid(&nextY));
                    // Note for the standard Left-to-Right, Top-to-Bottom walk,
                    // the end position is one cell below the bottom left.
                    // (or more accurately, on the exclusive bottom line in the inclusive left column.)
                    _current = { _topLeft.x(), nextY };
                }
                else
                {
                    _current = { nextX, _current.y() };
                }

                return (*this);
            }

            constexpr bool operator==(const _rectangle_const_iterator& other) const
            {
                return _current == other._current &&
                       _topLeft == other._topLeft &&
                       _bottomRight == other._bottomRight;
            }

            constexpr bool operator!=(const _rectangle_const_iterator& other) const
            {
                return !(*this == other);
            }

            constexpr bool operator<(const _rectangle_const_iterator& other) const
            {
                return _current < other._current;
            }

            constexpr bool operator>(const _rectangle_const_iterator& other) const
            {
                return _current > other._current;
            }

            constexpr point operator*() const
            {
                return _current;
            }

        protected:
            point _current;
            const point _topLeft;
            const point _bottomRight;

#ifdef UNIT_TESTING
            friend class ::RectangleTests;
#endif
        };
    }

    class rectangle
    {
    public:
        using const_iterator = details::_rectangle_const_iterator;

        constexpr rectangle() noexcept :
            rectangle(til::point{ 0, 0 }, til::point{ 0, 0 })
        {
        }

        // On 64-bit processors, int and ptrdiff_t are different fundamental types.
        // On 32-bit processors, they're the same which makes this a double-definition
        // with the `ptrdiff_t` one below.
#if defined(_M_AMD64) || defined(_M_ARM64)
        constexpr rectangle(int left, int top, int right, int bottom) noexcept :
            rectangle(til::point{ left, top }, til::point{ right, bottom })
        {
        }
#endif

        rectangle(size_t left, size_t top, size_t right, size_t bottom) :
            rectangle(til::point{ left, top }, til::point{ right, bottom })
        {
        }

        constexpr rectangle(ptrdiff_t left, ptrdiff_t top, ptrdiff_t right, ptrdiff_t bottom) noexcept :
            rectangle(til::point{ left, top }, til::point{ right, bottom })
        {
        }

        // Creates a 1x1 rectangle with the given top-left corner.
        rectangle(til::point topLeft) :
            _topLeft(topLeft)
        {
            _bottomRight = _topLeft + til::point{ 1, 1 };
        }

        // Creates a rectangle where you specify the top-left corner (included)
        // and the bottom-right corner (excluded)
        constexpr rectangle(til::point topLeft, til::point bottomRight) noexcept :
            _topLeft(topLeft),
            _bottomRight(bottomRight)
        {
        }

        // Creates a rectangle with the given size where the top-left corner
        // is set to 0,0.
        constexpr rectangle(til::size size) noexcept :
            _topLeft(til::point{ 0, 0 }),
            _bottomRight(til::point{ size.width(), size.height() })
        {
        }

        // Creates a rectangle at the given top-left corner point X,Y that extends
        // down (+Y direction) and right (+X direction) for the given size.
        rectangle(til::point topLeft, til::size size) :
            _topLeft(topLeft),
            _bottomRight(topLeft + til::point{ size.width(), size.height() })
        {
        }

#ifdef _WINCONTYPES_
        // This extra specialization exists for SMALL_RECT because it's the only rectangle in the world that we know of
        // with the bottom and right fields INCLUSIVE to the rectangle itself.
        // It will perform math on the way in to ensure that it is represented as EXCLUSIVE.
        rectangle(SMALL_RECT sr)
        {
            _topLeft = til::point{ static_cast<ptrdiff_t>(sr.Left), static_cast<ptrdiff_t>(sr.Top) };

            _bottomRight = til::point{ static_cast<ptrdiff_t>(sr.Right), static_cast<ptrdiff_t>(sr.Bottom) } + til::point{ 1, 1 };
        }
#endif

        // This template will convert to rectangle from anything that has a Left, Top, Right, and Bottom field that appear convertible to an integer value
        template<typename TOther>
        constexpr rectangle(const TOther& other, std::enable_if_t<std::is_integral_v<decltype(std::declval<TOther>().Top)> && std::is_integral_v<decltype(std::declval<TOther>().Left)> && std::is_integral_v<decltype(std::declval<TOther>().Bottom)> && std::is_integral_v<decltype(std::declval<TOther>().Right)>, int> /*sentinel*/ = 0) :
            rectangle(til::point{ static_cast<ptrdiff_t>(other.Left), static_cast<ptrdiff_t>(other.Top) }, til::point{ static_cast<ptrdiff_t>(other.Right), static_cast<ptrdiff_t>(other.Bottom) })
        {
        }

        // This template will convert to rectangle from anything that has a left, top, right, and bottom field that appear convertible to an integer value
        template<typename TOther>
        constexpr rectangle(const TOther& other, std::enable_if_t<std::is_integral_v<decltype(std::declval<TOther>().top)> && std::is_integral_v<decltype(std::declval<TOther>().left)> && std::is_integral_v<decltype(std::declval<TOther>().bottom)> && std::is_integral_v<decltype(std::declval<TOther>().right)>, int> /*sentinel*/ = 0) :
            rectangle(til::point{ static_cast<ptrdiff_t>(other.left), static_cast<ptrdiff_t>(other.top) }, til::point{ static_cast<ptrdiff_t>(other.right), static_cast<ptrdiff_t>(other.bottom) })
        {
        }

        // This template will convert to rectangle from anything that has a Left, Top, Right, and Bottom field that are floating-point;
        // a math type is required.
        template<typename TilMath, typename TOther>
        constexpr rectangle(TilMath, const TOther& other, std::enable_if_t<std::is_floating_point_v<decltype(std::declval<TOther>().Left)> && std::is_floating_point_v<decltype(std::declval<TOther>().Top)> && std::is_floating_point_v<decltype(std::declval<TOther>().Right)> && std::is_floating_point_v<decltype(std::declval<TOther>().Bottom)>, int> /*sentinel*/ = 0) :
            rectangle(til::point{ TilMath{}, other.Left, other.Top }, til::point{ TilMath{}, other.Right, other.Bottom })
        {
        }

        // This template will convert to rectangle from anything that has a left, top, right, and bottom field that are floating-point;
        // a math type is required.
        template<typename TilMath, typename TOther>
        constexpr rectangle(TilMath, const TOther& other, std::enable_if_t<std::is_floating_point_v<decltype(std::declval<TOther>().left)> && std::is_floating_point_v<decltype(std::declval<TOther>().top)> && std::is_floating_point_v<decltype(std::declval<TOther>().right)> && std::is_floating_point_v<decltype(std::declval<TOther>().bottom)>, int> /*sentinel*/ = 0) :
            rectangle(til::point{ TilMath{}, other.left, other.top }, til::point{ TilMath{}, other.right, other.bottom })
        {
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

        explicit constexpr operator bool() const noexcept
        {
            return _topLeft.x() < _bottomRight.x() &&
                   _topLeft.y() < _bottomRight.y();
        }

        constexpr const_iterator begin() const
        {
            return const_iterator(_topLeft, _bottomRight);
        }

        constexpr const_iterator end() const
        {
            // For the standard walk: Left-To-Right then Top-To-Bottom
            // the end box is one cell below the left most column.
            // |----|  5x2 square. Remember bottom & right are exclusive
            // |    |  while top & left are inclusive.
            // X-----  X is the end position.

            return const_iterator(_topLeft, _bottomRight, { _topLeft.x(), _bottomRight.y() });
        }

#pragma region RECTANGLE OPERATORS
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

        constexpr rectangle& operator|=(const rectangle& other) noexcept
        {
            *this = *this | other;
            return *this;
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
            const auto b = std::min(bottom(), other.bottom());

            // If the height dimension would be empty, give back empty rectangle.
            if (t >= b)
            {
                return rectangle{};
            }

            return rectangle{ l, t, r, b };
        }

        constexpr rectangle& operator&=(const rectangle& other) noexcept
        {
            *this = *this & other;
            return *this;
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
                const til::rectangle t{ left(), top(), right(), intersect.top() };
                const til::rectangle b{ left(), intersect.bottom(), right(), bottom() };
                const til::rectangle l{ left(), intersect.top(), intersect.left(), intersect.bottom() };
                const til::rectangle r{ intersect.right(), intersect.top(), right(), intersect.bottom() };

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
#pragma endregion

#pragma region RECTANGLE VS POINT
        // ADD will translate (offset) the rectangle by the point.
        rectangle operator+(const point& point) const
        {
            ptrdiff_t l, t, r, b;

            THROW_HR_IF(E_ABORT, !::base::CheckAdd(left(), point.x()).AssignIfValid(&l));
            THROW_HR_IF(E_ABORT, !::base::CheckAdd(top(), point.y()).AssignIfValid(&t));
            THROW_HR_IF(E_ABORT, !::base::CheckAdd(right(), point.x()).AssignIfValid(&r));
            THROW_HR_IF(E_ABORT, !::base::CheckAdd(bottom(), point.y()).AssignIfValid(&b));

            return til::rectangle{ til::point{ l, t }, til::point{ r, b } };
        }

        rectangle& operator+=(const point& point)
        {
            *this = *this + point;
            return *this;
        }

        // SUB will translate (offset) the rectangle by the point.
        rectangle operator-(const point& point) const
        {
            ptrdiff_t l, t, r, b;

            THROW_HR_IF(E_ABORT, !::base::CheckSub(left(), point.x()).AssignIfValid(&l));
            THROW_HR_IF(E_ABORT, !::base::CheckSub(top(), point.y()).AssignIfValid(&t));
            THROW_HR_IF(E_ABORT, !::base::CheckSub(right(), point.x()).AssignIfValid(&r));
            THROW_HR_IF(E_ABORT, !::base::CheckSub(bottom(), point.y()).AssignIfValid(&b));

            return til::rectangle{ til::point{ l, t }, til::point{ r, b } };
        }

        rectangle& operator-=(const point& point)
        {
            *this = *this - point;
            return *this;
        }

#pragma endregion

#pragma region RECTANGLE VS SIZE
        // ADD will grow the total area of the rectangle. The sign is the direction to grow.
        rectangle operator+(const size& size) const
        {
            // Fetch the pieces of the rectangle.
            auto l = left();
            auto r = right();
            auto t = top();
            auto b = bottom();

            // Fetch the scale factors we're using.
            const auto width = size.width();
            const auto height = size.height();

            // Since this is the add operation versus a size, the result
            // should grow the total rectangle area.
            // The sign determines which edge of the rectangle moves.
            // We use the magnitude as how far to move.
            if (width > 0)
            {
                // Adding the positive makes the rectangle "grow"
                // because right stretches outward (to the right).
                //
                // Example with adding width 3...
                // |-- x = origin
                // V
                // x---------|    x------------|
                // |         |    |            |
                // |         |    |            |
                // |---------|    |------------|
                // BEFORE         AFTER
                THROW_HR_IF(E_ABORT, !base::CheckAdd(r, width).AssignIfValid(&r));
            }
            else
            {
                // Adding the negative makes the rectangle "grow"
                // because left stretches outward (to the left).
                //
                // Example with adding width -3...
                // |-- x = origin
                // V
                // x---------|    |--x---------|
                // |         |    |            |
                // |         |    |            |
                // |---------|    |------------|
                // BEFORE             AFTER
                THROW_HR_IF(E_ABORT, !base::CheckAdd(l, width).AssignIfValid(&l));
            }

            if (height > 0)
            {
                // Adding the positive makes the rectangle "grow"
                // because bottom stretches outward (to the down).
                //
                // Example with adding height 2...
                // |-- x = origin
                // V
                // x---------|    x---------|
                // |         |    |         |
                // |         |    |         |
                // |---------|    |         |
                //                |         |
                //                |---------|
                // BEFORE         AFTER
                THROW_HR_IF(E_ABORT, !base::CheckAdd(b, height).AssignIfValid(&b));
            }
            else
            {
                // Adding the negative makes the rectangle "grow"
                // because top stretches outward (to the up).
                //
                // Example with adding height -2...
                // |-- x = origin
                // |
                // |              |---------|
                // V              |         |
                // x---------|    x         |
                // |         |    |         |
                // |         |    |         |
                // |---------|    |---------|
                // BEFORE         AFTER
                THROW_HR_IF(E_ABORT, !base::CheckAdd(t, height).AssignIfValid(&t));
            }

            return rectangle{ til::point{ l, t }, til::point{ r, b } };
        }

        rectangle& operator+=(const size& size)
        {
            *this = *this + size;
            return *this;
        }

        // SUB will shrink the total area of the rectangle. The sign is the direction to shrink.
        rectangle operator-(const size& size) const
        {
            // Fetch the pieces of the rectangle.
            auto l = left();
            auto r = right();
            auto t = top();
            auto b = bottom();

            // Fetch the scale factors we're using.
            const auto width = size.width();
            const auto height = size.height();

            // Since this is the subtract operation versus a size, the result
            // should shrink the total rectangle area.
            // The sign determines which edge of the rectangle moves.
            // We use the magnitude as how far to move.
            if (width > 0)
            {
                // Subtracting the positive makes the rectangle "shrink"
                // because right pulls inward (to the left).
                //
                // Example with subtracting width 3...
                // |-- x = origin
                // V
                // x---------|    x------|
                // |         |    |      |
                // |         |    |      |
                // |---------|    |------|
                // BEFORE         AFTER
                THROW_HR_IF(E_ABORT, !base::CheckSub(r, width).AssignIfValid(&r));
            }
            else
            {
                // Subtracting the negative makes the rectangle "shrink"
                // because left pulls inward (to the right).
                //
                // Example with subtracting width -3...
                // |-- x = origin
                // V
                // x---------|    x  |------|
                // |         |       |      |
                // |         |       |      |
                // |---------|       |------|
                // BEFORE         AFTER
                THROW_HR_IF(E_ABORT, !base::CheckSub(l, width).AssignIfValid(&l));
            }

            if (height > 0)
            {
                // Subtracting the positive makes the rectangle "shrink"
                // because bottom pulls inward (to the up).
                //
                // Example with subtracting height 2...
                // |-- x = origin
                // V
                // x---------|    x---------|
                // |         |    |---------|
                // |         |
                // |---------|
                // BEFORE         AFTER
                THROW_HR_IF(E_ABORT, !base::CheckSub(b, height).AssignIfValid(&b));
            }
            else
            {
                // Subtracting the positive makes the rectangle "shrink"
                // because top pulls inward (to the down).
                //
                // Example with subtracting height -2...
                // |-- x = origin
                // V
                // x---------|    x
                // |         |
                // |         |    |---------|
                // |---------|    |---------|
                // BEFORE         AFTER
                THROW_HR_IF(E_ABORT, !base::CheckSub(t, height).AssignIfValid(&t));
            }

            return rectangle{ til::point{ l, t }, til::point{ r, b } };
        }

        rectangle& operator-=(const size& size)
        {
            *this = *this - size;
            return *this;
        }

        // scale_up will scale the entire rectangle up by the size factor
        // This includes moving the origin.
        rectangle scale_up(const size& size) const
        {
            const auto topLeft = _topLeft * size;
            const auto bottomRight = _bottomRight * size;
            return til::rectangle{ topLeft, bottomRight };
        }

        // scale_down will scale the entire rectangle down by the size factor,
        // but rounds the bottom-right corner out.
        // This includes moving the origin.
        rectangle scale_down(const size& size) const
        {
            auto topLeft = _topLeft;
            auto bottomRight = _bottomRight;
            topLeft = topLeft / size;

            // Move bottom right point into a size
            // Use size specialization of divide_ceil to round up against the size given.
            // Add leading addition to point to convert it back into a point.
            bottomRight = til::point{} + til::size{ right(), bottom() }.divide_ceil(size);

            return til::rectangle{ topLeft, bottomRight };
        }

        template<typename TilMath>
        rectangle scale(TilMath, const float scale) const
        {
            return til::rectangle{ _topLeft.scale(TilMath{}, scale), _bottomRight.scale(TilMath{}, scale) };
        }

#pragma endregion

        constexpr ptrdiff_t top() const noexcept
        {
            return _topLeft.y();
        }

        template<typename T>
        T top() const
        {
            T ret;
            THROW_HR_IF(E_ABORT, !base::MakeCheckedNum(top()).AssignIfValid(&ret));
            return ret;
        }

        constexpr ptrdiff_t bottom() const noexcept
        {
            return _bottomRight.y();
        }

        template<typename T>
        T bottom() const
        {
            T ret;
            THROW_HR_IF(E_ABORT, !base::MakeCheckedNum(bottom()).AssignIfValid(&ret));
            return ret;
        }

        constexpr ptrdiff_t left() const noexcept
        {
            return _topLeft.x();
        }

        template<typename T>
        T left() const
        {
            T ret;
            THROW_HR_IF(E_ABORT, !base::MakeCheckedNum(left()).AssignIfValid(&ret));
            return ret;
        }

        constexpr ptrdiff_t right() const noexcept
        {
            return _bottomRight.x();
        }

        template<typename T>
        T right() const
        {
            T ret;
            THROW_HR_IF(E_ABORT, !base::MakeCheckedNum(right()).AssignIfValid(&ret));
            return ret;
        }

        ptrdiff_t width() const
        {
            ptrdiff_t ret;
            THROW_HR_IF(E_ABORT, !::base::CheckSub(right(), left()).AssignIfValid(&ret));
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
            THROW_HR_IF(E_ABORT, !::base::CheckSub(bottom(), top()).AssignIfValid(&ret));
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

        size size() const
        {
            return til::size{ width(), height() };
        }

        constexpr bool empty() const noexcept
        {
            return !operator bool();
        }

        constexpr bool contains(til::point pt) const
        {
            return pt.x() >= _topLeft.x() && pt.x() < _bottomRight.x() &&
                   pt.y() >= _topLeft.y() && pt.y() < _bottomRight.y();
        }

        bool contains(ptrdiff_t index) const
        {
            return index >= 0 && index < size().area();
        }

        constexpr bool contains(til::rectangle rc) const
        {
            // Union the other rectangle and ourselves.
            // If the result of that didn't grow at all, then we already
            // fully contained the rectangle we were given.
            return (*this | rc) == *this;
        }

        ptrdiff_t index_of(til::point pt) const
        {
            THROW_HR_IF(E_INVALIDARG, !contains(pt));

            // Take Y away from the top to find how many rows down
            auto check = base::CheckSub(pt.y(), top());

            // Multiply by the width because we've passed that many
            // widths-worth of indices.
            check *= width();

            // Then add in the last few indices in the x position this row
            // and subtract left to find the offset from left edge.
            check = check + pt.x() - left();

            ptrdiff_t result;
            THROW_HR_IF(E_ABORT, !check.AssignIfValid(&result));
            return result;
        }

        til::point point_at(ptrdiff_t index) const
        {
            THROW_HR_IF(E_INVALIDARG, !contains(index));

            const auto div = std::div(index, width());

            // Not checking math on these because we're presuming
            // that the point can't be in bounds of a rectangle where
            // this would overflow on addition after the division.
            return til::point{ div.rem + left(), div.quot + top() };
        }

#ifdef _WINCONTYPES_
        // NOTE: This will convert back to INCLUSIVE on the way out because
        // that is generally how SMALL_RECTs are handled in console code and via the APIs.
        operator SMALL_RECT() const
        {
            SMALL_RECT ret;
            THROW_HR_IF(E_ABORT, !base::MakeCheckedNum(left()).AssignIfValid(&ret.Left));
            THROW_HR_IF(E_ABORT, !base::MakeCheckedNum(top()).AssignIfValid(&ret.Top));
            THROW_HR_IF(E_ABORT, !base::CheckSub(right(), 1).AssignIfValid(&ret.Right));
            THROW_HR_IF(E_ABORT, !base::CheckSub(bottom(), 1).AssignIfValid(&ret.Bottom));
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

        std::wstring to_string() const
        {
            return wil::str_printf<std::wstring>(L"(L:%td, T:%td, R:%td, B:%td) [W:%td, H:%td]", left(), top(), right(), bottom(), width(), height());
        }

    protected:
        til::point _topLeft;
        til::point _bottomRight;

#ifdef UNIT_TESTING
        friend class ::RectangleTests;
#endif
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
            return WEX::Common::NoThrowString(rect.to_string().c_str());
        }
    };

    template<>
    class VerifyCompareTraits<::til::rectangle, ::til::rectangle>
    {
    public:
        static bool AreEqual(const ::til::rectangle& expected, const ::til::rectangle& actual) noexcept
        {
            return expected == actual;
        }

        static bool AreSame(const ::til::rectangle& expected, const ::til::rectangle& actual) noexcept
        {
            return &expected == &actual;
        }

        static bool IsLessThan(const ::til::rectangle& expectedLess, const ::til::rectangle& expectedGreater) = delete;

        static bool IsGreaterThan(const ::til::rectangle& expectedGreater, const ::til::rectangle& expectedLess) = delete;

        static bool IsNull(const ::til::rectangle& object) noexcept
        {
            return object == til::rectangle{};
        }
    };

};
#endif
