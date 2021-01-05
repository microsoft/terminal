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
#include "../../inc/operators.hpp"

namespace Microsoft::Console::Types
{
    class Viewport;

    using SomeViewports = til::some<Viewport, 4>;

    class Viewport final
    {
    public:
        ~Viewport() {}
        constexpr Viewport() noexcept :
            _sr({ 0, 0, -1, -1 }){};
        Viewport(const Viewport& other) = default;
        Viewport(Viewport&&) = default;
        Viewport& operator=(const Viewport&) & = default;
        Viewport& operator=(Viewport&&) & = default;

        static Viewport Empty() noexcept;

        static Viewport FromInclusive(const SMALL_RECT sr) noexcept;

        static Viewport FromExclusive(const SMALL_RECT sr) noexcept;

        static Viewport FromDimensions(const COORD origin,
                                       const short width,
                                       const short height) noexcept;

        static Viewport FromDimensions(const COORD origin,
                                       const COORD dimensions) noexcept;

        static Viewport FromDimensions(const COORD dimensions) noexcept;

        static Viewport FromCoord(const COORD origin) noexcept;

        SHORT Left() const noexcept;
        SHORT RightInclusive() const noexcept;
        SHORT RightExclusive() const noexcept;
        SHORT Top() const noexcept;
        SHORT BottomInclusive() const noexcept;
        SHORT BottomExclusive() const noexcept;
        SHORT Height() const noexcept;
        SHORT Width() const noexcept;
        COORD Origin() const noexcept;
        COORD BottomRightExclusive() const noexcept;
        COORD EndExclusive() const noexcept;
        COORD Dimensions() const noexcept;

        bool IsInBounds(const Viewport& other) const noexcept;
        bool IsInBounds(const COORD& pos, bool allowEndExclusive = false) const noexcept;

        void Clamp(COORD& pos) const;
        Viewport Clamp(const Viewport& other) const noexcept;

        bool MoveInBounds(const ptrdiff_t move, COORD& pos) const noexcept;
        bool IncrementInBounds(COORD& pos, bool allowEndExclusive = false) const noexcept;
        bool IncrementInBoundsCircular(COORD& pos) const noexcept;
        bool DecrementInBounds(COORD& pos, bool allowEndExclusive = false) const noexcept;
        bool DecrementInBoundsCircular(COORD& pos) const noexcept;
        int CompareInBounds(const COORD& first, const COORD& second, bool allowEndExclusive = false) const noexcept;

        enum class XWalk
        {
            LeftToRight,
            RightToLeft
        };

        enum class YWalk
        {
            TopToBottom,
            BottomToTop
        };

        struct WalkDir final
        {
            const XWalk x;
            const YWalk y;
        };

        bool WalkInBounds(COORD& pos, const WalkDir dir, bool allowEndExclusive = false) const noexcept;
        bool WalkInBoundsCircular(COORD& pos, const WalkDir dir, bool allowEndExclusive = false) const noexcept;
        COORD GetWalkOrigin(const WalkDir dir) const noexcept;
        static WalkDir DetermineWalkDirection(const Viewport& source, const Viewport& target) noexcept;

        bool TrimToViewport(_Inout_ SMALL_RECT* const psr) const noexcept;
        void ConvertToOrigin(_Inout_ SMALL_RECT* const psr) const noexcept;
        void ConvertToOrigin(_Inout_ COORD* const pcoord) const noexcept;
        [[nodiscard]] Viewport ConvertToOrigin(const Viewport& other) const noexcept;
        void ConvertFromOrigin(_Inout_ SMALL_RECT* const psr) const noexcept;
        void ConvertFromOrigin(_Inout_ COORD* const pcoord) const noexcept;
        [[nodiscard]] Viewport ConvertFromOrigin(const Viewport& other) const noexcept;

        SMALL_RECT ToExclusive() const noexcept;
        SMALL_RECT ToInclusive() const noexcept;
        RECT ToRect() const noexcept;

        Viewport ToOrigin() const noexcept;

        bool IsValid() const noexcept;

        [[nodiscard]] static Viewport Offset(const Viewport& original, const COORD delta);

        [[nodiscard]] static Viewport Union(const Viewport& lhs, const Viewport& rhs) noexcept;

        [[nodiscard]] static Viewport Intersect(const Viewport& lhs, const Viewport& rhs) noexcept;

        [[nodiscard]] static SomeViewports Subtract(const Viewport& original, const Viewport& removeMe) noexcept;

    private:
        Viewport(const SMALL_RECT sr) noexcept;

        // This is always stored as a Inclusive rect.
        SMALL_RECT _sr;

#if UNIT_TESTING
        friend class ViewportTests;
#endif
    };
}

inline COORD operator-(const COORD& a, const COORD& b) noexcept
{
    return { a.X - b.X, a.Y - b.Y };
}

inline COORD operator-(const COORD& c) noexcept
{
    return { -c.X, -c.Y };
}

inline bool operator==(const Microsoft::Console::Types::Viewport& a,
                       const Microsoft::Console::Types::Viewport& b) noexcept
{
    return a.ToInclusive() == b.ToInclusive();
}

inline bool operator!=(const Microsoft::Console::Types::Viewport& a,
                       const Microsoft::Console::Types::Viewport& b) noexcept
{
    return !(a == b);
}
