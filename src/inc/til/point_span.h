// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    // point_span can be pictured as a "selection" range inside our text buffer. So given
    // a text buffer of 10x4, a start of 4,1 and end of 7,3 the span might look like this:
    //   +----------+
    //   |          |
    //   |    xxxxxx|
    //   |xxxxxxxxxx|
    //   |xxxxxxxx  |
    //   +----------+
    // At the time of writing there's a push to make selections have an exclusive end coordinate,
    // so the interpretation of end might change soon (making this comment potentially outdated).
    struct point_span
    {
        til::point start;
        til::point end;

        constexpr bool operator==(const point_span& rhs) const noexcept
        {
            // `__builtin_memcmp` isn't an official standard, but it's the
            // only way at the time of writing to get a constexpr `memcmp`.
            return __builtin_memcmp(this, &rhs, sizeof(rhs)) == 0;
        }

        constexpr bool operator!=(const point_span& rhs) const noexcept
        {
            return __builtin_memcmp(this, &rhs, sizeof(rhs)) != 0;
        }

        // Calls func(row, begX, endX) for each row and begX and begY are inclusive coordinates,
        // because point_span itself also uses inclusive coordinates.
        // In other words, it turns a
        // ```
        //   +----------------+
        //   |       #########|
        //   |################|
        //   |####            |
        //   +----------------+
        // ```
        // into:
        // ```
        //   func(0, 8, 15)
        //   func(1, 0, 15)
        //   func(2, 0, 4)
        // ```
        constexpr void iterate_rows(til::CoordType width, auto&& func) const
        {
            // Copy the members so that the compiler knows it doesn't
            // need to re-read them on every loop iteration.
            const auto w = width - 1;
            const auto ax = std::clamp(start.x, 0, w);
            const auto ay = start.y;
            const auto bx = std::clamp(end.x, 0, w);
            const auto by = end.y;

            for (auto y = ay; y <= by; ++y)
            {
                const auto x1 = y != ay ? 0 : ax;
                const auto x2 = y != by ? w : bx;
                func(y, x1, x2);
            }
        }

        til::small_vector<til::inclusive_rect, 3> split_rects(til::CoordType width) const
        {
            // Copy the members so that the compiler knows it doesn't
            // need to re-read them on every loop iteration.
            const auto w = width - 1;
            const auto ax = std::clamp(start.x, 0, w);
            const auto ay = start.y;
            const auto bx = std::clamp(end.x, 0, w);
            const auto by = end.y;
            auto y = ay;

            til::small_vector<til::inclusive_rect, 3> rects;

            //   +----------------+
            //   |       #########| <-- A
            //   |################| <-- B
            //   |####            | <-- C
            //   +----------------+
            //
            //   +----------------+
            //   |################| <-- B
            //   |################| <-- B
            //   |####            | <-- C
            //   +----------------+
            //
            //   +----------------+
            //   |       #########| <-- A
            //   |################| <-- C
            //   |################| <-- C
            //   +----------------+
            //
            //   +----------------+
            //   |################| <-- C
            //   |################| <-- C
            //   |################| <-- C
            //   +----------------+

            if (y <= by && ax > 0) // A
            {
                const auto x2 = y == by ? bx : w;
                rects.emplace_back(ax, y, x2, y);
                y += 1;
            }

            if (y < by && bx < w) // B
            {
                rects.emplace_back(0, y, w, by - 1);
                y = by - 1;
            }

            if (y <= by) // C
            {
                rects.emplace_back(0, y, bx, by);
            }

            return rects;
        }
    };
}
