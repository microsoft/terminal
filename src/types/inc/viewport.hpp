/*++
Copyright (c) Microsoft Corporation

Module Name:
- viewport.hpp

Abstract:
- This method provides an interface for abstracting viewport operations

Author(s):
- Michael Niksa (miniksa) Nov 2015
--*/

#pragma once

namespace Microsoft::Console::Types
{
    class Viewport;

    using SomeViewports = til::small_vector<Viewport, 4>;

    class Viewport final
    {
    public:
        Viewport() = default;

        static Viewport Empty() noexcept;

        static Viewport FromInclusive(const til::inclusive_rect& sr) noexcept;
        static Viewport FromExclusive(const til::rect& sr) noexcept;
        static Viewport FromDimensions(const til::point origin, const til::size dimensions) noexcept;

        til::CoordType Left() const noexcept;
        til::CoordType RightInclusive() const noexcept;
        til::CoordType RightExclusive() const noexcept;
        til::CoordType Top() const noexcept;
        til::CoordType BottomInclusive() const noexcept;
        til::CoordType BottomExclusive() const noexcept;
        til::CoordType Height() const noexcept;
        til::CoordType Width() const noexcept;
        til::point Origin() const noexcept;
        til::point BottomRightInclusive() const noexcept;
        til::point BottomRightExclusive() const noexcept;
        til::point BottomInclusiveRightExclusive() const noexcept;
        til::point EndExclusive() const noexcept;
        til::size Dimensions() const noexcept;

        bool IsInBounds(const Viewport& other) const noexcept;
        bool IsInBounds(const til::point pos, bool allowEndExclusive = false) const noexcept;
        bool IsInExclusiveBounds(const til::point pos) const noexcept;

        void Clamp(til::point& pos) const;
        Viewport Clamp(const Viewport& other) const noexcept;

        bool IncrementInBounds(til::point& pos, bool allowEndExclusive = false) const noexcept;
        bool DecrementInBounds(til::point& pos, bool allowEndExclusive = false) const noexcept;
        bool IncrementInExclusiveBounds(til::point& pos) const noexcept;
        bool DecrementInExclusiveBounds(til::point& pos) const noexcept;
        int CompareInBounds(const til::point first, const til::point second, bool allowEndExclusive = false) const noexcept;
        int CompareInExclusiveBounds(const til::point first, const til::point second) const noexcept;

        bool WalkInBounds(til::point& pos, const til::CoordType delta, bool allowEndExclusive = false) const noexcept;
        bool WalkInExclusiveBounds(til::point& pos, const til::CoordType delta) const noexcept;
        til::point GetWalkOrigin(const til::CoordType delta) const noexcept;
        static til::CoordType DetermineWalkDirection(const Viewport& source, const Viewport& target) noexcept;

        bool TrimToViewport(_Inout_ til::rect* psr) const noexcept;
        void ConvertToOrigin(_Inout_ til::rect* psr) const noexcept;
        void ConvertToOrigin(_Inout_ til::inclusive_rect* const psr) const noexcept;
        void ConvertToOrigin(_Inout_ til::point* const pcoord) const noexcept;
        [[nodiscard]] Viewport ConvertToOrigin(const Viewport& other) const noexcept;
        void ConvertFromOrigin(_Inout_ til::inclusive_rect* const psr) const noexcept;
        void ConvertFromOrigin(_Inout_ til::point* const pcoord) const noexcept;
        [[nodiscard]] Viewport ConvertFromOrigin(const Viewport& other) const noexcept;

        til::rect ToExclusive() const noexcept;
        til::inclusive_rect ToInclusive() const noexcept;

        Viewport ToOrigin() const noexcept;

        bool IsValid() const noexcept;

        [[nodiscard]] static Viewport Offset(const Viewport& original, const til::point delta) noexcept;

        [[nodiscard]] static Viewport Union(const Viewport& lhs, const Viewport& rhs) noexcept;

        [[nodiscard]] static Viewport Intersect(const Viewport& lhs, const Viewport& rhs) noexcept;

        [[nodiscard]] static SomeViewports Subtract(const Viewport& original, const Viewport& removeMe) noexcept;

        constexpr bool operator==(const Viewport& other) const noexcept
        {
            return _sr == other._sr;
        }

        constexpr bool operator!=(const Viewport& other) const noexcept
        {
            return _sr != other._sr;
        }

    private:
        Viewport(const til::inclusive_rect& sr) noexcept;

        // This is always stored as a Inclusive rect.
        til::inclusive_rect _sr{ 0, 0, -1, -1 };

#if UNIT_TESTING
        friend class ViewportTests;
#endif
    };
}
