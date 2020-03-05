// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "rectangle.h"
#include "size.h"
#include "bitmap.h"

#define _INLINEPREFIX __declspec(noinline) inline

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    _INLINEPREFIX rectangle operator+(const rectangle& lhs, const size& rhs)
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
            r += width;
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
            l += width;
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
            b += height;
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
            t += height;
        }

        return rectangle{ til::point{l, t}, til::point{r, b} };
    }

    _INLINEPREFIX rectangle& operator+=(rectangle& lhs, const size& rhs)
    {
        lhs = lhs + rhs;
        return lhs;
    }

    _INLINEPREFIX rectangle operator-(const rectangle& lhs, const size& rhs)
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
            r -= width;
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
            l -= width;
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
            b -= height;
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
            t -= height;
        }

        return rectangle{ til::point{l, t}, til::point{r, b} };
    }

    _INLINEPREFIX rectangle& operator-=(rectangle& lhs, const size& rhs)
    {
        lhs = lhs - rhs;
        return lhs;
    }

    //rectangle operator+(const rectangle& lhs, const point& rhs)
    //{
    //    const auto l = lhs.left() + rhs.x();
    //    const auto r = lhs.right() + rhs.x();
    //    const auto t = lhs.top() + rhs.y();
    //    const auto b = lhs.bottom() + rhs.y();
    //    return rectangle{ til::point{l, t}, til::point{r, b} };
    //}

    //rectangle& operator+=(rectangle& lhs, const point& rhs)
    //{
    //    lhs = lhs + rhs;
    //    return lhs;
    //}

    //rectangle operator+(const rectangle& lhs, const rectangle& rhs)
    //{
    //    const auto l = lhs.left() + rhs.left();
    //    const auto r = lhs.right() + rhs.right();
    //    const auto t = lhs.top() + rhs.top();
    //    const auto b = lhs.bottom() + rhs.bottom();
    //    return rectangle{ til::point{l, t}, til::point{r, b} };
    //}

    //rectangle& operator+=(rectangle& lhs, const rectangle& rhs)
    //{
    //    lhs = lhs + rhs;
    //    return lhs;
    //}

    //rectangle operator/(const rectangle& rect, const size& size)
    //{
    //    const auto l = rect.left() / size.width_signed();
    //    const auto r = rect.right() / size.width_signed();
    //    const auto t = rect.top() / size.height_signed();
    //    const auto b = rect.bottom() / size.height_signed();
    //    return rectangle{ til::point{l, t}, til::point{r, b} };
    //}

    //rectangle& operator/=(rectangle& lhs, const size& rhs)
    //{
    //    lhs = lhs / rhs;
    //    return lhs;
    //}

    //rectangle operator%(const rectangle& rect, const size& size)
    //{
    //    const auto l = rect.left() % size.width_signed();
    //    const auto r = rect.right() % size.width_signed();
    //    const auto t = rect.top() % size.height_signed();
    //    const auto b = rect.bottom() % size.height_signed();
    //    return rectangle{ til::point{l, t}, til::point{r, b} };
    //}

    //rectangle& operator%=(rectangle& lhs, const size& rhs)
    //{
    //    lhs = lhs % rhs;
    //    return lhs;
    //}

    //rectangle operator*(const rectangle& rect, const bool& flag)
    //{
    //    const auto l = static_cast<decltype(rect.left())>(!(flag ^ static_cast<bool>(rect.left())));
    //    const auto r = static_cast<decltype(rect.right())>(!(flag ^ static_cast<bool>(rect.right())));
    //    const auto t = static_cast<decltype(rect.top())>(!(flag ^ static_cast<bool>(rect.top())));
    //    const auto b = static_cast<decltype(rect.bottom())>(!(flag ^ static_cast<bool>(rect.bottom())));
    //    return rectangle{ til::point{l, t}, til::point{r, b} };
    //}

    //rectangle& operator*=(rectangle& lhs, const bool& rhs)
    //{
    //    lhs = lhs * rhs;
    //    return lhs;
    //}
}
