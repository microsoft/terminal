// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/Viewport.hpp"

using namespace Microsoft::Console::Types;

Viewport::Viewport(const til::inclusive_rect& sr) noexcept :
    _sr(sr)
{
}

Viewport Viewport::Empty() noexcept
{
    return Viewport();
}

Viewport Viewport::FromInclusive(const til::inclusive_rect& sr) noexcept
{
    return Viewport(sr);
}

Viewport Viewport::FromExclusive(const til::rect& sr) noexcept
{
    return FromInclusive(sr.to_inclusive_rect());
}

// Function Description:
// - Creates a new Viewport at the given origin, with the given dimensions.
// Arguments:
// - origin: The origin of the new Viewport. Becomes the Viewport's Left, Top
// - dimensions: A coordinate containing the width and height of the new viewport
//      in the x and y coordinates respectively.
// Return Value:
// - a new Viewport at the given origin, with the given dimensions.
Viewport Viewport::FromDimensions(const til::point origin,
                                  const til::size dimensions) noexcept
{
    return Viewport(til::inclusive_rect{
        origin.x,
        origin.y,
        origin.x + dimensions.width - 1,
        origin.y + dimensions.height - 1,
    });
}

til::CoordType Viewport::Left() const noexcept
{
    return _sr.left;
}

til::CoordType Viewport::RightInclusive() const noexcept
{
    return _sr.right;
}

til::CoordType Viewport::RightExclusive() const noexcept
{
    return _sr.right + 1;
}

til::CoordType Viewport::Top() const noexcept
{
    return _sr.top;
}

til::CoordType Viewport::BottomInclusive() const noexcept
{
    return _sr.bottom;
}

til::CoordType Viewport::BottomExclusive() const noexcept
{
    return _sr.bottom + 1;
}

til::CoordType Viewport::Height() const noexcept
{
    return BottomExclusive() - Top();
}

til::CoordType Viewport::Width() const noexcept
{
    return RightExclusive() - Left();
}

// Method Description:
// - Get a coord representing the origin of this viewport.
// Arguments:
// - <none>
// Return Value:
// - the coordinates of this viewport's origin.
til::point Viewport::Origin() const noexcept
{
    return { Left(), Top() };
}

// Method Description:
// - Get a coord representing the bottom right of the viewport in inclusive terms.
// Arguments:
// - <none>
// Return Value:
// - the inclusive bottom right coordinates of this viewport.
til::point Viewport::BottomRightInclusive() const noexcept
{
    return { RightInclusive(), BottomInclusive() };
}

// Method Description:
// - Get a coord representing the bottom right of the viewport in exclusive terms.
// Arguments:
// - <none>
// Return Value:
// - the exclusive bottom right coordinates of this viewport.
til::point Viewport::BottomRightExclusive() const noexcept
{
    return { RightExclusive(), BottomExclusive() };
}

til::point Viewport::BottomInclusiveRightExclusive() const noexcept
{
    return { RightExclusive(), BottomInclusive() };
}

// Method Description:
// - For Accessibility, get a til::point representing the end of this viewport in exclusive terms.
// - This is needed to represent an exclusive endpoint in UiaTextRange that includes the last
//    til::point's text in the buffer at (RightInclusive(), BottomInclusive())
// Arguments:
// - <none>
// Return Value:
// - the coordinates of this viewport's end.
til::point Viewport::EndExclusive() const noexcept
{
    return { Left(), BottomExclusive() };
}

// Method Description:
// - Get a coord representing the dimensions of this viewport.
// Arguments:
// - <none>
// Return Value:
// - the dimensions of this viewport.
til::size Viewport::Dimensions() const noexcept
{
    return { Width(), Height() };
}

// Method Description:
// - Determines if the given viewport fits within this viewport.
// Arguments:
// - other - The viewport to fit inside this one
// Return Value:
// - True if it fits. False otherwise.
bool Viewport::IsInBounds(const Viewport& other) const noexcept
{
    return other.Left() >= Left() && other.Left() <= RightInclusive() &&
           other.RightInclusive() >= Left() && other.RightInclusive() <= RightInclusive() &&
           other.Top() >= Top() && other.Top() <= other.BottomInclusive() &&
           other.BottomInclusive() >= Top() && other.BottomInclusive() <= BottomInclusive();
}

// Method Description:
// - Determines if the given coordinate position lies within this viewport.
// Arguments:
// - pos - Coordinate position
// - allowEndExclusive - if true, allow the EndExclusive til::point as a valid position.
//                        Used in accessibility to signify that the exclusive end
//                        includes the last til::point in a given viewport.
// Return Value:
// - True if it lies inside the viewport. False otherwise.
bool Viewport::IsInBounds(const til::point pos, bool allowEndExclusive) const noexcept
{
    if (allowEndExclusive && pos == EndExclusive())
    {
        return true;
    }

    return pos.x >= Left() && pos.x < RightExclusive() &&
           pos.y >= Top() && pos.y < BottomExclusive();
}

// Method Description:
// - Clamps a coordinate position into the inside of this viewport.
// Arguments:
// - pos - coordinate to update/clamp
// Return Value:
// - <none>
void Viewport::Clamp(til::point& pos) const
{
    THROW_HR_IF(E_NOT_VALID_STATE, !IsValid()); // we can't clamp to an invalid viewport.

    pos.x = std::clamp(pos.x, Left(), RightInclusive());
    pos.y = std::clamp(pos.y, Top(), BottomInclusive());
}

// Method Description:
// - Clamps a viewport into the inside of this viewport.
// Arguments:
// - other - Viewport to clamp to the inside of this viewport
// Return Value:
// - Clamped viewport
Viewport Viewport::Clamp(const Viewport& other) const noexcept
{
    return Viewport::FromExclusive(ToExclusive() & other.ToExclusive());
}

// Method Description:
// - Increments the given coordinate within the bounds of this viewport.
// Arguments:
// - pos - Coordinate position that will be incremented, if it can be.
// - allowEndExclusive - if true, allow the EndExclusive til::point as a valid position.
//                        Used in accessibility to signify that the exclusive end
//                        includes the last til::point in a given viewport.
// Return Value:
// - True if it could be incremented. False if it would move outside.
bool Viewport::IncrementInBounds(til::point& pos, bool allowEndExclusive) const noexcept
{
    return WalkInBounds(pos, 1, allowEndExclusive);
}

// Method Description:
// - Decrements the given coordinate within the bounds of this viewport.
// Arguments:
// - pos - Coordinate position that will be incremented, if it can be.
// - allowEndExclusive - if true, allow the EndExclusive til::point as a valid position.
//                        Used in accessibility to signify that the exclusive end
//                        includes the last til::point in a given viewport.
// Return Value:
// - True if it could be incremented. False if it would move outside.
bool Viewport::DecrementInBounds(til::point& pos, bool allowEndExclusive) const noexcept
{
    return WalkInBounds(pos, -1, allowEndExclusive);
}

// Routine Description:
// - Compares two coordinate positions to determine whether they're the same, left, or right within the given buffer size
// Arguments:
// - first- The first coordinate position
// - second - The second coordinate position
// - allowEndExclusive - if true, allow the EndExclusive til::point as a valid position.
//                        Used in accessibility to signify that the exclusive end
//                        includes the last til::point in a given viewport.
// Return Value:
// -  Negative if First is to the left of the Second.
// -  0 if First and Second are the same coordinate.
// -  Positive if First is to the right of the Second.
// -  This is so you can do s_CompareCoords(first, second) <= 0 for "first is left or the same as second".
//    (the < looks like a left arrow :D)
// -  The magnitude of the result is the distance between the two coordinates when typing characters into the buffer (left to right, top to bottom)
#pragma warning(suppress : 4100)
int Viewport::CompareInBounds(const til::point first, const til::point second, bool allowEndExclusive) const noexcept
{
    // Assert that our coordinates are within the expected boundaries
    assert(IsInBounds(first, allowEndExclusive));
    assert(IsInBounds(second, allowEndExclusive));

    // First set the distance vertically
    //   If first is on row 4 and second is on row 6, first will be -2 rows behind second * an 80 character row would be -160.
    //   For the same row, it'll be 0 rows * 80 character width = 0 difference.
    auto retVal = (first.y - second.y) * Width();

    // Now adjust for horizontal differences
    //   If first is in position 15 and second is in position 30, first is -15 left in relation to 30.
    retVal += (first.x - second.x);

    // Further notes:
    //   If we already moved behind one row, this will help correct for when first is right of second.
    //     For example, with row 4, col 79 and row 5, col 0 as first and second respectively, the distance is -1.
    //     Assume the row width is 80.
    //     Step one will set the retVal as -80 as first is one row behind the second.
    //     Step two will then see that first is 79 - 0 = +79 right of second and add 79
    //     The total is -80 + 79 = -1.
    return retVal;
}

// Method Description:
// - Walks the given coordinate within the bounds of this viewport in the specified
//   X and Y directions.
// Arguments:
// - pos - Coordinate position that will be adjusted, if it can be.
// - dir - Walking direction specifying which direction to go when reaching the end of a row/column
// - allowEndExclusive - if true, allow the EndExclusive til::point as a valid position.
//                        Used in accessibility to signify that the exclusive end
//                        includes the last til::point in a given viewport.
// Return Value:
// - True if it could be adjusted as specified and remain in bounds. False if it would move outside.
bool Viewport::WalkInBounds(til::point& pos, const til::CoordType delta, bool allowEndExclusive) const noexcept
{
    const auto l = static_cast<ptrdiff_t>(_sr.left);
    const auto t = static_cast<ptrdiff_t>(_sr.top);
    const auto w = static_cast<ptrdiff_t>(std::max(0, _sr.right - _sr.left + 1));
    const auto h = static_cast<ptrdiff_t>(std::max(0, _sr.bottom - _sr.top + 1));
    const auto max = w * h - !allowEndExclusive;
    const auto off = w * (pos.y - t) + (pos.x - l) + delta;
    const auto offClamped = std::clamp(off, ptrdiff_t{ 0 }, max);
    pos.x = gsl::narrow_cast<til::CoordType>(offClamped % w + l);
    pos.y = gsl::narrow_cast<til::CoordType>(offClamped / w + t);
    return off == offClamped;
}

bool Viewport::IncrementInExclusiveBounds(til::point& pos) const noexcept
{
    return WalkInExclusiveBounds(pos, 1);
}

bool Viewport::DecrementInExclusiveBounds(til::point& pos) const noexcept
{
    return WalkInExclusiveBounds(pos, -1);
}

bool Viewport::IsInExclusiveBounds(const til::point pos) const noexcept
{
    return pos.x >= Left() && pos.x <= RightExclusive() &&
           pos.y >= Top() && pos.y <= BottomInclusive();
}

int Viewport::CompareInExclusiveBounds(const til::point first, const til::point second) const noexcept
{
    // Assert that our coordinates are within the expected boundaries
    assert(IsInExclusiveBounds(first));
    assert(IsInExclusiveBounds(second));

    // First set the distance vertically
    //   If first is on row 4 and second is on row 6, first will be -2 rows behind second * an 80 character row would be -160.
    //   For the same row, it'll be 0 rows * 80 character width = 0 difference.
    auto retVal = (first.y - second.y) * Width();

    // Now adjust for horizontal differences
    //   If first is in position 15 and second is in position 30, first is -15 left in relation to 30.
    retVal += (first.x - second.x);

    // Further notes:
    //   If we already moved behind one row, this will help correct for when first is right of second.
    //     For example, with row 4, col 79 and row 5, col 0 as first and second respectively, the distance is -1.
    //     Assume the row width is 80.
    //     Step one will set the retVal as -80 as first is one row behind the second.
    //     Step two will then see that first is 79 - 0 = +79 right of second and add 79
    //     The total is -80 + 79 = -1.
    return retVal;
}

bool Viewport::WalkInExclusiveBounds(til::point& pos, const til::CoordType delta) const noexcept
{
    const auto l = static_cast<ptrdiff_t>(_sr.left);
    const auto t = static_cast<ptrdiff_t>(_sr.top);
    const auto w = static_cast<ptrdiff_t>(std::max(0, _sr.right - _sr.left + 2));
    const auto h = static_cast<ptrdiff_t>(std::max(0, _sr.bottom - _sr.top + 1));
    const auto max = w * h;
    const auto off = w * (pos.y - t) + (pos.x - l) + delta;
    const auto offClamped = std::clamp(off, ptrdiff_t{ 0 }, max);
    pos.x = gsl::narrow_cast<til::CoordType>(offClamped % w + l);
    pos.y = gsl::narrow_cast<til::CoordType>(offClamped / w + t);
    return off == offClamped;
}

// Routine Description:
// - If walking through a viewport, one might want to know the origin
//   for the direction walking.
// - For example, for walking up and to the left (bottom right corner
//   to top left corner), the origin would start at the bottom right.
// Arguments:
// - dir - The direction one intends to walk through the viewport
// Return Value:
// - The origin for the walk to reach every position without circling
//   if using this same viewport with the `WalkInBounds` methods.
til::point Viewport::GetWalkOrigin(const til::CoordType delta) const noexcept
{
    til::point origin;
    origin.x = delta >= 0 ? Left() : RightInclusive();
    origin.y = delta >= 0 ? Top() : BottomInclusive();
    return origin;
}

// A text buffer is basically a list (std::vector) of characters and we just tell it where the line breaks are.
// This means that copying between overlapping ranges requires the same care as when using std::copy VS std::copy_backward:
//   std::copy (aka forward copying) can only be used within an overlapping source/target range if target is before source.
//   std::copy_backward, as the name implies, is to be used for the reverse situation: if target is after source.
// This method returns 1 for the former and -1 for the latter. It's the delta you can pass to GetWalkOrigin and WalkInBounds.
til::CoordType Viewport::DetermineWalkDirection(const Viewport& source, const Viewport& target) noexcept
{
    const auto sourceOrigin = source.Origin();
    const auto targetOrigin = target.Origin();
    return targetOrigin < sourceOrigin ? 1 : -1;
}

// Method Description:
// - Clips the input rectangle to our bounds. Assumes that the input rectangle
//is an exclusive rectangle.
// Arguments:
// - psr: a pointer to an exclusive rect to clip.
// Return Value:
// - true iff the clipped rectangle is valid (with a width and height both >0)
bool Viewport::TrimToViewport(_Inout_ til::rect* psr) const noexcept
{
    psr->left = std::max(psr->left, Left());
    psr->right = std::min(psr->right, RightExclusive());
    psr->top = std::max(psr->top, Top());
    psr->bottom = std::min(psr->bottom, BottomExclusive());

    return psr->left < psr->right && psr->top < psr->bottom;
}

// Method Description:
// - Translates the input til::rect out of our coordinate space, whose origin is
//      at (this.left, this.right)
// Arguments:
// - psr: a pointer to a til::rect the translate into our coordinate space.
// Return Value:
// - <none>
void Viewport::ConvertToOrigin(_Inout_ til::rect* psr) const noexcept
{
    const auto dx = Left();
    const auto dy = Top();
    psr->left -= dx;
    psr->right -= dx;
    psr->top -= dy;
    psr->bottom -= dy;
}

// Method Description:
// - Translates the input til::inclusive_rect out of our coordinate space, whose origin is
//      at (this.left, this.right)
// Arguments:
// - psr: a pointer to a til::inclusive_rect the translate into our coordinate space.
// Return Value:
// - <none>
void Viewport::ConvertToOrigin(_Inout_ til::inclusive_rect* const psr) const noexcept
{
    const auto dx = Left();
    const auto dy = Top();
    psr->left -= dx;
    psr->right -= dx;
    psr->top -= dy;
    psr->bottom -= dy;
}

// Method Description:
// - Translates the input coordinate out of our coordinate space, whose origin is
//      at (this.left, this.right)
// Arguments:
// - pcoord: a pointer to a coordinate the translate into our coordinate space.
// Return Value:
// - <none>
void Viewport::ConvertToOrigin(_Inout_ til::point* const pcoord) const noexcept
{
    pcoord->x -= Left();
    pcoord->y -= Top();
}

// Method Description:
// - Translates the input til::inclusive_rect to our coordinate space, whose origin is
//      at (this.left, this.right)
// Arguments:
// - psr: a pointer to a til::inclusive_rect the translate into our coordinate space.
// Return Value:
// - <none>
void Viewport::ConvertFromOrigin(_Inout_ til::inclusive_rect* const psr) const noexcept
{
    const auto dx = Left();
    const auto dy = Top();
    psr->left += dx;
    psr->right += dx;
    psr->top += dy;
    psr->bottom += dy;
}

// Method Description:
// - Translates the input coordinate to our coordinate space, whose origin is
//      at (this.left, this.right)
// Arguments:
// - pcoord: a pointer to a coordinate the translate into our coordinate space.
// Return Value:
// - <none>
void Viewport::ConvertFromOrigin(_Inout_ til::point* const pcoord) const noexcept
{
    pcoord->x += Left();
    pcoord->y += Top();
}

// Method Description:
// - Returns an exclusive til::rect equivalent to this viewport.
// Arguments:
// - <none>
// Return Value:
// - an exclusive til::rect equivalent to this viewport.
til::rect Viewport::ToExclusive() const noexcept
{
    return { Left(), Top(), RightExclusive(), BottomExclusive() };
}

// Method Description:
// - Returns an inclusive til::inclusive_rect equivalent to this viewport.
// Arguments:
// - <none>
// Return Value:
// - an inclusive til::inclusive_rect equivalent to this viewport.
til::inclusive_rect Viewport::ToInclusive() const noexcept
{
    return { Left(), Top(), RightInclusive(), BottomInclusive() };
}

// Method Description:
// - Returns a new viewport representing this viewport at the origin.
// For example:
//  this = {6, 5, 11, 11} (w, h = 5, 6)
//  result = {0, 0, 5, 6} (w, h = 5, 6)
// Arguments:
// - <none>
// Return Value:
// - a new viewport with the same dimensions as this viewport with top, left = 0, 0
Viewport Viewport::ToOrigin() const noexcept
{
    auto returnVal = *this;
    ConvertToOrigin(&returnVal._sr);
    return returnVal;
}

// Method Description:
// - Translates another viewport to this viewport's coordinate space.
// For example:
//  this = {5, 6, 7, 8} (w,h = 1, 1)
//  other = {6, 5, 11, 11} (w, h = 5, 6)
//  result = {1, -1, 6, 5} (w, h = 5, 6)
// Arguments:
// - other: the viewport to convert to this coordinate space
// Return Value:
// - the input viewport in the coordinate space with origin at (this.top, this.left)
[[nodiscard]] Viewport Viewport::ConvertToOrigin(const Viewport& other) const noexcept
{
    auto returnVal = other;
    ConvertToOrigin(&returnVal._sr);
    return returnVal;
}

// Method Description:
// - Translates another viewport out of this viewport's coordinate space.
// For example:
//  this = {5, 6, 7, 8} (w,h = 1, 1)
//  other = {0, 0, 5, 6} (w, h = 5, 6)
//  result = {5, 6, 10, 12} (w, h = 5, 6)
// Arguments:
// - other: the viewport to convert out of this coordinate space
// Return Value:
// - the input viewport in the coordinate space with origin at (0, 0)
[[nodiscard]] Viewport Viewport::ConvertFromOrigin(const Viewport& other) const noexcept
{
    auto returnVal = other;
    ConvertFromOrigin(&returnVal._sr);
    return returnVal;
}

// Function Description:
// - Translates a given Viewport by the specified coord amount. Does the
//      addition with safemath.
// Arguments:
// - original: The initial viewport to translate. Is unmodified by this operation.
// - delta: The amount to translate the original rect by, in both the x and y coordinates.
// Return Value:
// - The offset viewport by the given delta.
// - NOTE: Throws on safe math failure.
[[nodiscard]] Viewport Viewport::Offset(const Viewport& original, const til::point delta) noexcept
{
    const auto newLeft = original._sr.left + delta.x;
    const auto newTop = original._sr.top + delta.y;
    const auto newRight = original._sr.right + delta.x;
    const auto newBottom = original._sr.bottom + delta.y;
    return Viewport({ newLeft, newTop, newRight, newBottom });
}

// Function Description:
// - Returns a viewport created from the union of both the parameter viewports.
//      The result extends from the leftmost extent of either rect to the
//      rightmost extent of either rect, and from the lowest top value to the
//      highest bottom value, and everything in between.
// Arguments:
// - lhs: one of the viewports to or together
// - rhs: the other viewport to or together
// Return Value:
// - a Viewport representing the union of the other two viewports.
[[nodiscard]] Viewport Viewport::Union(const Viewport& lhs, const Viewport& rhs) noexcept
{
    const auto leftValid = lhs.IsValid();
    const auto rightValid = rhs.IsValid();

    // If neither are valid, return empty.
    if (!leftValid && !rightValid)
    {
        return Viewport::Empty();
    }
    // If left isn't valid, then return just the right.
    else if (!leftValid)
    {
        return rhs;
    }
    // If right isn't valid, then return just the left.
    else if (!rightValid)
    {
        return lhs;
    }
    // Otherwise, everything is valid. Find the actual union.
    else
    {
        const auto left = std::min(lhs.Left(), rhs.Left());
        const auto top = std::min(lhs.Top(), rhs.Top());
        const auto right = std::max(lhs.RightInclusive(), rhs.RightInclusive());
        const auto bottom = std::max(lhs.BottomInclusive(), rhs.BottomInclusive());

        return Viewport({ left, top, right, bottom });
    }
}

// Function Description:
// - Creates a viewport from the intersection of both the parameter viewports.
//      The result will be the smallest area that fits within both rectangles.
// Arguments:
// - lhs: one of the viewports to intersect
// - rhs: the other viewport to intersect
// Return Value:
// - a Viewport representing the intersection of the other two, or an empty viewport if there's no intersection.
[[nodiscard]] Viewport Viewport::Intersect(const Viewport& lhs, const Viewport& rhs) noexcept
{
    const auto left = std::max(lhs.Left(), rhs.Left());
    const auto top = std::max(lhs.Top(), rhs.Top());
    const auto right = std::min(lhs.RightInclusive(), rhs.RightInclusive());
    const auto bottom = std::min(lhs.BottomInclusive(), rhs.BottomInclusive());

    const Viewport intersection({ left, top, right, bottom });

    // What we calculated with min/max might not actually represent a valid viewport that has area.
    // If we calculated something that is nonsense (invalid), then just return the empty viewport.
    if (!intersection.IsValid())
    {
        return Viewport::Empty();
    }
    else
    {
        // If it was valid, give back whatever we created.
        return intersection;
    }
}

// Routine Description:
// - Returns a list of Viewports representing the area from the `original` Viewport that was NOT a part of
//   the given `removeMe` Viewport. It can require multiple Viewports to represent the remaining
//   area as a "region".
// Arguments:
// - original - The overall viewport to start from.
// - removeMe - The space that should be taken out of the main Viewport.
// Return Value:
// - Array of 4 Viewports representing non-overlapping segments of the remaining area
//   that was covered by `main` before the regional area of `removeMe` was taken out.
// - You must check that each viewport .IsValid() before using it.
[[nodiscard]] SomeViewports Viewport::Subtract(const Viewport& original, const Viewport& removeMe) noexcept
try
{
    SomeViewports result;

    // We could have up to four rectangles describing the area resulting when you take removeMe out of main.
    // Find the intersection of the two so we know which bits of removeMe are actually applicable
    // to the original rectangle for subtraction purposes.
    const auto intersection = Viewport::Intersect(original, removeMe);

    // If there's no intersection, there's nothing to remove.
    if (!original.IsValid())
    {
        // Nothing to do here.
    }
    else if (!intersection.IsValid())
    {
        // Just put the original rectangle into the results and return early.
        result.push_back(original);
    }
    // If the original rectangle matches the intersection, there is nothing to return.
    else if (original != intersection)
    {
        // Generate our potential four viewports that represent the region of the original that falls outside of the remove area.
        // We will bias toward generating wide rectangles over tall rectangles (if possible) so that optimizations that apply
        // to manipulating an entire row at once can be realized by other parts of the console code. (i.e. Run Length Encoding)
        // In the following examples, the found remaining regions are represented by:
        // T = Top      B = Bottom      L = Left        R = Right
        //
        // 4 Sides but Identical:
        // |---------original---------|             |---------original---------|
        // |                          |             |                          |
        // |                          |             |                          |
        // |                          |             |                          |
        // |                          |    ======>  |        intersect         |  ======>  early return of nothing
        // |                          |             |                          |
        // |                          |             |                          |
        // |                          |             |                          |
        // |---------removeMe---------|             |--------------------------|
        //
        // 4 Sides:
        // |---------original---------|             |---------original---------|           |--------------------------|
        // |                          |             |                          |           |TTTTTTTTTTTTTTTTTTTTTTTTTT|
        // |                          |             |                          |           |TTTTTTTTTTTTTTTTTTTTTTTTTT|
        // |        |---------|       |             |        |---------|       |           |LLLLLLLL|---------|RRRRRRR|
        // |        |removeMe |       |    ======>  |        |intersect|       |  ======>  |LLLLLLLL|         |RRRRRRR|
        // |        |---------|       |             |        |---------|       |           |LLLLLLLL|---------|RRRRRRR|
        // |                          |             |                          |           |BBBBBBBBBBBBBBBBBBBBBBBBBB|
        // |                          |             |                          |           |BBBBBBBBBBBBBBBBBBBBBBBBBB|
        // |--------------------------|             |--------------------------|           |--------------------------|
        //
        // 3 Sides:
        // |---------original---------|             |---------original---------|           |--------------------------|
        // |                          |             |                          |           |TTTTTTTTTTTTTTTTTTTTTTTTTT|
        // |                          |             |                          |           |TTTTTTTTTTTTTTTTTTTTTTTTTT|
        // |        |--------------------|          |        |-----------------|           |LLLLLLLL|-----------------|
        // |        |removeMe            | ======>  |        |intersect        |  ======>  |LLLLLLLL|                 |
        // |        |--------------------|          |        |-----------------|           |LLLLLLLL|-----------------|
        // |                          |             |                          |           |BBBBBBBBBBBBBBBBBBBBBBBBBB|
        // |                          |             |                          |           |BBBBBBBBBBBBBBBBBBBBBBBBBB|
        // |--------------------------|             |--------------------------|           |--------------------------|
        //
        // 2 Sides:
        // |---------original---------|             |---------original---------|           |--------------------------|
        // |                          |             |                          |           |TTTTTTTTTTTTTTTTTTTTTTTTTT|
        // |                          |             |                          |           |TTTTTTTTTTTTTTTTTTTTTTTTTT|
        // |        |--------------------|          |        |-----------------|           |LLLLLLLL|-----------------|
        // |        |removeMe            | ======>  |        |intersect        |  ======>  |LLLLLLLL|                 |
        // |        |                    |          |        |                 |           |LLLLLLLL|                 |
        // |        |                    |          |        |                 |           |LLLLLLLL|                 |
        // |        |                    |          |        |                 |           |LLLLLLLL|                 |
        // |--------|                    |          |--------------------------|           |--------------------------|
        //          |                    |
        //          |--------------------|
        //
        // 1 Side:
        // |---------original---------|             |---------original---------|           |--------------------------|
        // |                          |             |                          |           |TTTTTTTTTTTTTTTTTTTTTTTTTT|
        // |                          |             |                          |           |TTTTTTTTTTTTTTTTTTTTTTTTTT|
        // |-----------------------------|          |--------------------------|           |--------------------------|
        // |         removeMe            | ======>  |         intersect        |  ======>  |                          |
        // |                             |          |                          |           |                          |
        // |                             |          |                          |           |                          |
        // |                             |          |                          |           |                          |
        // |                             |          |--------------------------|           |--------------------------|
        // |                             |
        // |-----------------------------|
        //
        // 0 Sides:
        // |---------original---------|             |---------original---------|
        // |                          |             |                          |
        // |                          |             |                          |
        // |                          |             |                          |
        // |                          |    ======>  |                          |  ======>  early return of Original
        // |                          |             |                          |
        // |                          |             |                          |
        // |                          |             |                          |
        // |--------------------------|             |--------------------------|
        //
        //
        //         |---------------|
        //         | removeMe      |
        //         |---------------|

        // We generate these rectangles by the original and intersection points, but some of them might be empty when the intersection
        // lines up with the edge of the original. That's OK. That just means that the subtraction didn't leave anything behind.
        // We will filter those out below when adding them to the result.
        const Viewport top{ til::inclusive_rect{ original.Left(), original.Top(), original.RightInclusive(), intersection.Top() - 1 } };
        const Viewport bottom{ til::inclusive_rect{ original.Left(), intersection.BottomExclusive(), original.RightInclusive(), original.BottomInclusive() } };
        const Viewport left{ til::inclusive_rect{ original.Left(), intersection.Top(), intersection.Left() - 1, intersection.BottomInclusive() } };
        const Viewport right{ til::inclusive_rect{ intersection.RightExclusive(), intersection.Top(), original.RightInclusive(), intersection.BottomInclusive() } };

        if (top.IsValid())
        {
            result.push_back(top);
        }

        if (bottom.IsValid())
        {
            result.push_back(bottom);
        }

        if (left.IsValid())
        {
            result.push_back(left);
        }

        if (right.IsValid())
        {
            result.push_back(right);
        }
    }

    return result;
}
CATCH_FAIL_FAST()

// Method Description:
// - Returns true if the rectangle described by this Viewport has internal space
// - i.e. it has a positive, non-zero height and width.
bool Viewport::IsValid() const noexcept
{
    return static_cast<bool>(_sr);
}
