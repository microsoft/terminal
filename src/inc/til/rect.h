// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "bit.h"
#include "some.h"
#include "math.h"
#include "size.h"
#include "point.h"
#include "operators.h"

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    struct inclusive_rect
    {
        CoordType left = 0;
        CoordType top = 0;
        CoordType right = 0;
        CoordType bottom = 0;

        constexpr bool operator==(const inclusive_rect& rhs) const noexcept
        {
            return __builtin_memcmp(this, &rhs, sizeof(rhs)) == 0;
        }

        constexpr bool operator!=(const inclusive_rect& rhs) const noexcept
        {
            return __builtin_memcmp(this, &rhs, sizeof(rhs)) != 0;
        }

        explicit constexpr operator bool() const noexcept
        {
            return (left >= 0) & (top >= 0) &
                   (right >= left) & (bottom >= top);
        }
    };

    constexpr inclusive_rect wrap_small_rect(const SMALL_RECT& rect) noexcept
    {
        return { rect.Left, rect.Top, rect.Right, rect.Bottom };
    }

    constexpr SMALL_RECT unwrap_small_rect(const inclusive_rect& rect)
    {
        return {
            gsl::narrow<short>(rect.left),
            gsl::narrow<short>(rect.top),
            gsl::narrow<short>(rect.right),
            gsl::narrow<short>(rect.bottom),
        };
    }

    constexpr HRESULT unwrap_small_rect_hr(const inclusive_rect& rect, SMALL_RECT& out) noexcept
    {
        short l = 0;
        short t = 0;
        short r = 0;
        short b = 0;
        if (narrow_maybe(rect.left, l) && narrow_maybe(rect.top, t) && narrow_maybe(rect.right, r) && narrow_maybe(rect.bottom, b))
        {
            out.Left = l;
            out.Top = t;
            out.Right = r;
            out.Bottom = b;
            return S_OK;
        }
        RETURN_WIN32(ERROR_UNHANDLED_EXCEPTION);
    }

    namespace details
    {
        class _rectangle_const_iterator
        {
        public:
            constexpr _rectangle_const_iterator(point topLeft, point bottomRight) :
                _topLeft{ topLeft },
                _bottomRight{ bottomRight },
                _current{ topLeft }
            {
            }

            constexpr _rectangle_const_iterator(point topLeft, point bottomRight, point start) :
                _topLeft{ topLeft },
                _bottomRight{ bottomRight },
                _current{ start }
            {
            }

            _rectangle_const_iterator& operator++()
            {
                const auto nextX = details::extract(::base::CheckAdd(_current.x, 1));

                if (nextX >= _bottomRight.x)
                {
                    const auto nextY = details::extract(::base::CheckAdd(_current.y, 1));
                    // Note for the standard Left-to-Right, Top-to-Bottom walk,
                    // the end position is one cell below the bottom left.
                    // (or more accurately, on the exclusive bottom line in the inclusive left column.)
                    _current = { _topLeft.x, nextY };
                }
                else
                {
                    _current = { nextX, _current.y };
                }

                return (*this);
            }

            constexpr bool operator==(const _rectangle_const_iterator& rhs) const noexcept
            {
                // `__builtin_memcmp` isn't an official standard, but it's the
                // only way at the time of writing to get a constexpr `memcmp`.
                return __builtin_memcmp(this, &rhs, sizeof(rhs)) == 0;
            }

            constexpr bool operator!=(const _rectangle_const_iterator& rhs) const noexcept
            {
                return __builtin_memcmp(this, &rhs, sizeof(rhs)) != 0;
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
        };
    }

    struct rect
    {
        using const_iterator = details::_rectangle_const_iterator;

        CoordType left = 0;
        CoordType top = 0;
        CoordType right = 0;
        CoordType bottom = 0;

        constexpr rect() noexcept = default;

        constexpr rect(CoordType left, CoordType top, CoordType right, CoordType bottom) noexcept :
            left{ left }, top{ top }, right{ right }, bottom{ bottom }
        {
        }

        // This template will convert to point from floating-point args;
        // a math type is required. If you _don't_ provide one, you're going to
        // get a compile-time error about "cannot convert from initializer-list to til::point"
        template<typename TilMath, typename T>
        constexpr rect(TilMath, T left, T top, T right, T bottom) :
            left{ TilMath::template cast<CoordType>(left) },
            top{ TilMath::template cast<CoordType>(top) },
            right{ TilMath::template cast<CoordType>(right) },
            bottom{ TilMath::template cast<CoordType>(bottom) }
        {
        }

        // Creates a rect where you specify the top-left corner (included)
        // and the bottom-right corner (excluded)
        constexpr rect(point topLeft, point bottomRight) noexcept :
            left{ topLeft.x }, top{ topLeft.y }, right{ bottomRight.x }, bottom{ bottomRight.y }
        {
        }

        // Creates a rect with the given size where the top-left corner
        // is set to 0,0.
        explicit constexpr rect(size size) noexcept :
            right{ size.width }, bottom{ size.height }
        {
        }

        // Creates a rect at the given top-left corner point X,Y that extends
        // down (+Y direction) and right (+X direction) for the given size.
        constexpr rect(point topLeft, size size) :
            rect{ topLeft, topLeft + size }
        {
        }

        constexpr bool operator==(const rect& rhs) const noexcept
        {
            // `__builtin_memcmp` isn't an official standard, but it's the
            // only way at the time of writing to get a constexpr `memcmp`.
            return __builtin_memcmp(this, &rhs, sizeof(rhs)) == 0;
        }

        constexpr bool operator!=(const rect& rhs) const noexcept
        {
            return __builtin_memcmp(this, &rhs, sizeof(rhs)) != 0;
        }

        explicit constexpr operator bool() const noexcept
        {
            return (left >= 0) & (top >= 0) &
                   (right > left) & (bottom > top);
        }

        constexpr const_iterator begin() const
        {
            return const_iterator({ left, top }, { right, bottom });
        }

        constexpr const_iterator end() const
        {
            // For the standard walk: Left-To-Right then Top-To-Bottom
            // the end box is one cell below the left most column.
            // |----|  5x2 square. Remember bottom & right are exclusive
            // |    |  while top & left are inclusive.
            // X-----  X is the end position.

            return const_iterator({ left, top }, { right, bottom }, { left, bottom });
        }

#pragma region RECTANGLE OPERATORS
        // OR = union
        constexpr rect operator|(const rect& other) const noexcept
        {
            const auto thisEmpty = empty();
            const auto otherEmpty = other.empty();

            // If both are empty, return empty rect.
            if (thisEmpty && otherEmpty)
            {
                return rect{};
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
            const auto l = std::min(left, other.left);
            const auto t = std::min(top, other.top);
            const auto r = std::max(right, other.right);
            const auto b = std::max(bottom, other.bottom);
            return rect{ l, t, r, b };
        }

        constexpr rect& operator|=(const rect& other) noexcept
        {
            *this = *this | other;
            return *this;
        }

        // AND = intersect
        constexpr rect operator&(const rect& other) const noexcept
        {
            const auto l = std::max(left, other.left);
            const auto r = std::min(right, other.right);

            // If the width dimension would be empty, give back empty rect.
            if (l >= r)
            {
                return rect{};
            }

            const auto t = std::max(top, other.top);
            const auto b = std::min(bottom, other.bottom);

            // If the height dimension would be empty, give back empty rect.
            if (t >= b)
            {
                return rect{};
            }

            return rect{ l, t, r, b };
        }

        constexpr rect& operator&=(const rect& other) noexcept
        {
            *this = *this & other;
            return *this;
        }

        // - = subtract
        constexpr some<rect, 4> operator-(const rect& other) const
        {
            some<rect, 4> result;

            // We could have up to four rectangles describing the area resulting when you take removeMe out of main.
            // Find the intersection of the two so we know which bits of removeMe are actually applicable
            // to the original rect for subtraction purposes.
            const auto intersect = *this & other;

            // If there's no intersect, there's nothing to remove.
            if (intersect.empty())
            {
                // Just put the original rect into the results and return early.
                result.push_back(*this);
            }
            // If the original rect matches the intersect, there is nothing to return.
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
                const rect t{ left, top, right, intersect.top };
                const rect b{ left, intersect.bottom, right, bottom };
                const rect l{ left, intersect.top, intersect.left, intersect.bottom };
                const rect r{ intersect.right, intersect.top, right, intersect.bottom };

                if (t)
                {
                    result.push_back(t);
                }

                if (b)
                {
                    result.push_back(b);
                }

                if (l)
                {
                    result.push_back(l);
                }

                if (r)
                {
                    result.push_back(r);
                }
            }

            return result;
        }
#pragma endregion

#pragma region RECTANGLE VS POINT
        // ADD will translate (offset) the rect by the point.
        constexpr rect operator+(const point point) const
        {
            const auto l = details::extract(::base::CheckAdd(left, point.x));
            const auto t = details::extract(::base::CheckAdd(top, point.y));
            const auto r = details::extract(::base::CheckAdd(right, point.x));
            const auto b = details::extract(::base::CheckAdd(bottom, point.y));
            return { l, t, r, b };
        }

        constexpr rect& operator+=(const point point)
        {
            *this = *this + point;
            return *this;
        }

        // SUB will translate (offset) the rect by the point.
        constexpr rect operator-(const point point) const
        {
            const auto l = details::extract(::base::CheckSub(left, point.x));
            const auto t = details::extract(::base::CheckSub(top, point.y));
            const auto r = details::extract(::base::CheckSub(right, point.x));
            const auto b = details::extract(::base::CheckSub(bottom, point.y));
            return { l, t, r, b };
        }

        constexpr rect& operator-=(const point point)
        {
            *this = *this - point;
            return *this;
        }

#pragma endregion

#pragma region RECTANGLE VS SIZE

        // scale_up will scale the entire rect up by the size factor
        constexpr rect scale_up(const size size) const
        {
            return rect{
                details::extract(::base::CheckMul(left, size.width)),
                details::extract(::base::CheckMul(top, size.height)),
                details::extract(::base::CheckMul(right, size.width)),
                details::extract(::base::CheckMul(bottom, size.height)),
            };
        }

        // scale_down will scale the entire rect down by the size factor.
        // The top/left corner is rounded down (floor) and
        // the bottom/right corner is rounded up (ceil).
        constexpr rect scale_down(const size size) const
        {
            // The integer ceil division `((a - 1) / b) + 1` only works for numbers >0.
            // Support for negative numbers wasn't deemed useful at this point.
            if ((left < 0) | (top < 0) | (right < 0) | (bottom < 0) | (size.width <= 0) | (size.height <= 0))
            {
                throw std::invalid_argument{ "invalid til::rect::scale_down" };
            }

            // Imagine a terminal of 120x30 "cells" with each cell being
            // 5x10 pixels large. The terminal is therefore 600x300 pixels.
            // Given a rectangle in pixel coordinates, what's the rectangle in cell coordinates?
            // Clearly this requires us to floor() top/left and ceil() bottom/right to cover all pixels.
            // And thus:
            //   {17, 24, 31, 38}.scale_down({5, 10}) == {3, 2, 7, 4}
            //   {3, 2, 7, 4}.scale_up({5, 10}) == {15, 20, 35, 40}
            return rect{
                left / size.width,
                top / size.height,
                right != 0 ? (right - 1) / size.width + 1 : 0,
                bottom != 0 ? (bottom - 1) / size.height + 1 : 0,
            };
        }

#pragma endregion

        template<typename T>
        constexpr T narrow_left() const
        {
            return gsl::narrow<T>(left);
        }

        template<typename T>
        constexpr T narrow_top() const
        {
            return gsl::narrow<T>(top);
        }

        template<typename T>
        constexpr T narrow_right() const
        {
            return gsl::narrow<T>(right);
        }

        template<typename T>
        constexpr T narrow_bottom() const
        {
            return gsl::narrow<T>(bottom);
        }

        constexpr CoordType width() const
        {
            return details::extract(::base::CheckSub(right, left));
        }

        template<typename T>
        constexpr T narrow_width() const
        {
            return details::extract<CoordType, T>(::base::CheckSub(right, left));
        }

        constexpr CoordType height() const
        {
            return details::extract(::base::CheckSub(bottom, top));
        }

        template<typename T>
        constexpr T narrow_height() const
        {
            return details::extract<CoordType, T>(::base::CheckSub(bottom, top));
        }

        constexpr point origin() const noexcept
        {
            return { left, top };
        }

        constexpr size size() const noexcept
        {
            return { width(), height() };
        }

        constexpr bool empty() const noexcept
        {
            return !operator bool();
        }

        constexpr bool contains(point pt) const noexcept
        {
            return (pt.x >= left) & (pt.x < right) &
                   (pt.y >= top) & (pt.y < bottom);
        }

        constexpr bool contains(const rect& rc) const noexcept
        {
            return (rc.left >= left) & (rc.top >= top) &
                   (rc.right <= right) & (rc.bottom <= bottom);
        }

        template<typename T = CoordType>
        constexpr T index_of(point pt) const
        {
            THROW_HR_IF(E_INVALIDARG, !contains(pt));

            // Take Y away from the top to find how many rows down
            auto check = ::base::CheckSub(pt.y, top);

            // Multiply by the width because we've passed that many
            // widths-worth of indices.
            check *= width();

            // Then add in the last few indices in the x position this row
            // and subtract left to find the offset from left edge.
            check = check + pt.x - left;

            return details::extract<CoordType, T>(check);
        }

        point point_at(size_t index) const
        {
            const auto width = details::extract<CoordType, size_t>(::base::CheckSub(right, left));
            const auto area = details::extract<CoordType, size_t>(::base::CheckSub(bottom, top) * width);

            THROW_HR_IF(E_INVALIDARG, index >= area);

            // Not checking math on these because we're presuming
            // that the point can't be in bounds of a rect where
            // this would overflow on addition after the division.
            const auto quot = gsl::narrow_cast<CoordType>(index / width);
            const auto rem = gsl::narrow_cast<CoordType>(index % width);
            return point{ left + rem, top + quot };
        }

#ifdef _WINCONTYPES_
        // NOTE: This will convert from INCLUSIVE on the way in because
        // that is generally how SMALL_RECTs are handled in console code and via the APIs.
        explicit constexpr rect(const inclusive_rect& other) noexcept :
            rect{ other.left, other.top, other.right + 1, other.bottom + 1 }
        {
        }

        // NOTE: This will convert back to INCLUSIVE on the way out because
        // that is generally how SMALL_RECTs are handled in console code and via the APIs.
        constexpr inclusive_rect to_inclusive_rect() const
        {
            return { left, top, right - 1, bottom - 1 };
        }
#endif

#ifdef _WINDEF_
        explicit constexpr rect(const RECT& other) noexcept :
            rect{ other.left, other.top, other.right, other.bottom }
        {
        }

        constexpr RECT to_win32_rect() const noexcept
        {
            return { left, top, right, bottom };
        }

        // til::rect and RECT have the exact same layout at the time of writing,
        // so this function lets you unsafely "view" this rect as a RECT
        // if you need to pass it to a Win32 function.
        //
        // Use as_win32_rect() as sparingly as possible because it'll be a pain to hack
        // it out of this code base once til::rect and RECT aren't the same anymore.
        // Prefer casting to RECT and back to til::rect instead if possible.
        RECT* as_win32_rect() noexcept
        {
#pragma warning(suppress : 26490) // Don't use reinterpret_cast (type.1).
            return std::launder(reinterpret_cast<RECT*>(this));
        }

        // til::rect and POINT[2] have the exact same layout at the time of writing,
        // so this function lets you unsafely "view" this rect as a POINT[2] array
        // if you need to pass it to a Win32 function.
        //
        // Use as_win32_points() as sparingly as possible because it'll be a pain to hack
        // it out of this code base once til::rect and POINT[2] aren't the same anymore.
        // Prefer casting to POINT and back to til::rect instead if possible.
        POINT* as_win32_points() noexcept
        {
#pragma warning(suppress : 26490) // Don't use reinterpret_cast (type.1).
            return std::launder(reinterpret_cast<POINT*>(this));
        }
#endif

#ifdef DCOMMON_H_INCLUDED
        template<typename TilMath>
        constexpr rect(TilMath&& math, const D2D1_RECT_F& other) :
            rect{ std::forward<TilMath>(math), other.left, other.top, other.right, other.bottom }
        {
        }

        constexpr D2D1_RECT_F to_d2d_rect() const noexcept
        {
            return {
                static_cast<float>(left),
                static_cast<float>(top),
                static_cast<float>(right),
                static_cast<float>(bottom),
            };
        }
#endif

#ifdef WINRT_Windows_Foundation_H
        template<typename TilMath>
        constexpr rect(TilMath&& math, const winrt::Windows::Foundation::Rect& other) :
            rect{ std::forward<TilMath>(math), other.X, other.Y, other.X + other.Width, other.Y + other.Height }
        {
        }

        winrt::Windows::Foundation::Rect to_winrt_rect() const noexcept
        {
            return {
                static_cast<float>(left),
                static_cast<float>(top),
                static_cast<float>(width()),
                static_cast<float>(height()),
            };
        }
#endif

#ifdef WINRT_Microsoft_Terminal_Core_H
        template<typename TilMath>
        constexpr rect(TilMath&& math, const winrt::Microsoft::Terminal::Core::Padding& other) :
            rect{ std::forward<TilMath>(math), other.Left, other.Top, other.Right, other.Bottom }
        {
        }

        winrt::Microsoft::Terminal::Core::Padding to_core_padding() const noexcept
        {
            return {
                static_cast<float>(left),
                static_cast<float>(top),
                static_cast<float>(right),
                static_cast<float>(bottom),
            };
        }
#endif

        std::wstring to_string() const
        {
            return wil::str_printf<std::wstring>(L"(L:%d, T:%d, R:%d, B:%d) [W:%d, H:%d]", left, top, right, bottom, width(), height());
        }
    };

    constexpr rect wrap_exclusive_small_rect(const SMALL_RECT& rect) noexcept
    {
        return { rect.Left, rect.Top, rect.Right, rect.Bottom };
    }

    constexpr SMALL_RECT unwrap_exclusive_small_rect(const rect& rect)
    {
        return {
            gsl::narrow<short>(rect.left),
            gsl::narrow<short>(rect.top),
            gsl::narrow<short>(rect.right),
            gsl::narrow<short>(rect.bottom),
        };
    }

    constexpr HRESULT unwrap_exclusive_small_rect_hr(const rect& rect, SMALL_RECT& out) noexcept
    {
        short l = 0;
        short t = 0;
        short r = 0;
        short b = 0;
        if (narrow_maybe(rect.left, l) && narrow_maybe(rect.top, t) && narrow_maybe(rect.right, r) && narrow_maybe(rect.bottom, b))
        {
            out.Left = l;
            out.Top = t;
            out.Right = r;
            out.Bottom = b;
            return S_OK;
        }
        RETURN_WIN32(ERROR_UNHANDLED_EXCEPTION);
    }
}

#ifdef __WEX_COMMON_H__
namespace WEX::TestExecution
{
    template<>
    class VerifyOutputTraits<til::inclusive_rect>
    {
    public:
        static WEX::Common::NoThrowString ToString(const til::inclusive_rect& rect)
        {
            return WEX::Common::NoThrowString().Format(L"(L:%d, T:%d, R:%d, B:%d) [W:%d, H:%d]", rect.left, rect.top, rect.right, rect.bottom, rect.right - rect.left, rect.bottom - rect.top);
        }
    };

    template<>
    class VerifyCompareTraits<til::inclusive_rect, til::inclusive_rect>
    {
    public:
        static bool AreEqual(const til::inclusive_rect& expected, const til::inclusive_rect& actual) noexcept
        {
            return expected == actual;
        }

        static bool AreSame(const til::inclusive_rect& expected, const til::inclusive_rect& actual) noexcept
        {
            return &expected == &actual;
        }

        static bool IsLessThan(const til::inclusive_rect& expectedLess, const til::inclusive_rect& expectedGreater) = delete;

        static bool IsGreaterThan(const til::inclusive_rect& expectedGreater, const til::inclusive_rect& expectedLess) = delete;

        static bool IsNull(const til::inclusive_rect& object) noexcept
        {
            return object == til::inclusive_rect{};
        }
    };

    template<>
    class VerifyOutputTraits<til::rect>
    {
    public:
        static WEX::Common::NoThrowString ToString(const til::rect& rect)
        {
            return WEX::Common::NoThrowString(rect.to_string().c_str());
        }
    };

    template<>
    class VerifyCompareTraits<til::rect, til::rect>
    {
    public:
        static bool AreEqual(const til::rect& expected, const til::rect& actual) noexcept
        {
            return expected == actual;
        }

        static bool AreSame(const til::rect& expected, const til::rect& actual) noexcept
        {
            return &expected == &actual;
        }

        static bool IsLessThan(const til::rect& expectedLess, const til::rect& expectedGreater) = delete;

        static bool IsGreaterThan(const til::rect& expectedGreater, const til::rect& expectedLess) = delete;

        static bool IsNull(const til::rect& object) noexcept
        {
            return object == til::rect{};
        }
    };

};
#endif
