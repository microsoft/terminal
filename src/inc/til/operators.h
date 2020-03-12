// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "rectangle.h"
#include "size.h"
#include "bitmap.h"

#define _TIL_INLINEPREFIX __declspec(noinline) inline

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    // RECTANGLE VS SIZE
    // ADD will grow the total area of the rectangle. The sign is the direction to grow.
    _TIL_INLINEPREFIX rectangle operator+(const rectangle& lhs, const size& rhs)
    {
        // Fetch the pieces of the rectangle.
        auto l = lhs.left();
        auto r = lhs.right();
        auto t = lhs.top();
        auto b = lhs.bottom();

        // Fetch the scale factors we're using.
        const auto width = rhs.width();
        const auto height = rhs.height();

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

    _TIL_INLINEPREFIX rectangle& operator+=(rectangle& lhs, const size& rhs)
    {
        lhs = lhs + rhs;
        return lhs;
    }

    // SUB will shrink the total area of the rectangle. The sign is the direction to shrink.
    _TIL_INLINEPREFIX rectangle operator-(const rectangle& lhs, const size& rhs)
    {
        // Fetch the pieces of the rectangle.
        auto l = lhs.left();
        auto r = lhs.right();
        auto t = lhs.top();
        auto b = lhs.bottom();

        // Fetch the scale factors we're using.
        const auto width = rhs.width();
        const auto height = rhs.height();

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

    _TIL_INLINEPREFIX rectangle& operator-=(rectangle& lhs, const size& rhs)
    {
        lhs = lhs - rhs;
        return lhs;
    }

    // MUL will scale the entire rectangle by the size L/R * WIDTH and T/B * HEIGHT.
    _TIL_INLINEPREFIX rectangle operator*(const rectangle& lhs, const size& rhs)
    {
        ptrdiff_t l;
        THROW_HR_IF(E_ABORT, !(base::MakeCheckedNum(lhs.left()) * rhs.width()).AssignIfValid(&l));

        ptrdiff_t t;
        THROW_HR_IF(E_ABORT, !(base::MakeCheckedNum(lhs.top()) * rhs.height()).AssignIfValid(&t));

        ptrdiff_t r;
        THROW_HR_IF(E_ABORT, !(base::MakeCheckedNum(lhs.right()) * rhs.width()).AssignIfValid(&r));

        ptrdiff_t b;
        THROW_HR_IF(E_ABORT, !(base::MakeCheckedNum(lhs.bottom()) * rhs.height()).AssignIfValid(&b));

        return til::rectangle{ l, t, r, b };
    }

    // POINT VS SIZE
    // This is a convenience and will take X vs WIDTH and Y vs HEIGHT.
    _TIL_INLINEPREFIX point operator+(const point& lhs, const size& rhs)
    {
        return lhs + til::point{ rhs.width(), rhs.height() };
    }

    _TIL_INLINEPREFIX point operator-(const point& lhs, const size& rhs)
    {
        return lhs - til::point{ rhs.width(), rhs.height() };
    }

    _TIL_INLINEPREFIX point operator*(const point& lhs, const size& rhs)
    {
        return lhs * til::point{ rhs.width(), rhs.height() };
    }

    _TIL_INLINEPREFIX point operator/(const point& lhs, const size& rhs)
    {
        return lhs / til::point{ rhs.width(), rhs.height() };
    }

    // SIZE VS POINT
    // This is a convenience and will take WIDTH vs X and HEIGHT vs Y.
    _TIL_INLINEPREFIX size operator+(const size& lhs, const point& rhs)
    {
        return lhs + til::size(rhs.x(), rhs.y());
    }

    _TIL_INLINEPREFIX size operator-(const size& lhs, const point& rhs)
    {
        return lhs - til::size(rhs.x(), rhs.y());
    }

    _TIL_INLINEPREFIX size operator*(const size& lhs, const point& rhs)
    {
        return lhs * til::size(rhs.x(), rhs.y());
    }

    _TIL_INLINEPREFIX size operator/(const size& lhs, const point& rhs)
    {
        return lhs / til::size(rhs.x(), rhs.y());
    }
}
