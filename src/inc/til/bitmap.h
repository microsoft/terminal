// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#ifdef UNIT_TESTING
class BitmapTests;
#endif

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    namespace details
    {
        class _bitmap_const_iterator
        {
        public:
            using iterator_category = typename std::input_iterator_tag;
            using value_type = typename const til::rectangle;
            using difference_type = typename ptrdiff_t;
            using pointer = typename const til::rectangle*;
            using reference = typename const til::rectangle&;

            _bitmap_const_iterator(const dynamic_bitset<>& values, til::rectangle rc, ptrdiff_t pos) :
                _values(values),
                _rc(rc),
                _pos(pos),
                _end(rc.size().area())
            {
                _calculateArea();
            }

            _bitmap_const_iterator& operator++()
            {
                _pos = _nextPos;
                _calculateArea();
                return (*this);
            }

            _bitmap_const_iterator operator++(int)
            {
                const auto prev = *this;
                ++*this;
                return prev;
            }

            constexpr bool operator==(const _bitmap_const_iterator& other) const noexcept
            {
                return _pos == other._pos && _values == other._values;
            }

            constexpr bool operator!=(const _bitmap_const_iterator& other) const noexcept
            {
                return !(*this == other);
            }

            constexpr bool operator<(const _bitmap_const_iterator& other) const noexcept
            {
                return _pos < other._pos;
            }

            constexpr bool operator>(const _bitmap_const_iterator& other) const noexcept
            {
                return _pos > other._pos;
            }

            constexpr reference operator*() const noexcept
            {
                return _run;
            }

            constexpr pointer operator->() const noexcept
            {
                return &_run;
            }

        private:
            const dynamic_bitset<>& _values;
            const til::rectangle _rc;
            ptrdiff_t _pos;
            ptrdiff_t _nextPos;
            const ptrdiff_t _end;
            til::rectangle _run;

            void _calculateArea()
            {
                // Backup the position as the next one.
                _nextPos = _pos;

                // Seek forward until we find an on bit.
                while (_nextPos < _end && !_values[_nextPos])
                {
                    ++_nextPos;
                }

                // If we haven't reached the end yet...
                if (_nextPos < _end)
                {
                    // pos is now at the first on bit.
                    const auto runStart = _rc.point_at(_nextPos);

                    // We'll only count up until the end of this row.
                    // a run can be a max of one row tall.
                    const ptrdiff_t rowEndIndex = _rc.index_of(til::point(_rc.right() - 1, runStart.y())) + 1;

                    // Find the length for the rectangle.
                    ptrdiff_t runLength = 0;

                    // We have at least 1 so start with a do/while.
                    do
                    {
                        ++_nextPos;
                        ++runLength;
                    } while (_nextPos < rowEndIndex && _values[_nextPos]);
                    // Keep going until we reach end of row, end of the buffer, or the next bit is off.

                    // Assemble and store that run.
                    _run = til::rectangle{ runStart, til::size{ runLength, static_cast<ptrdiff_t>(1) } };
                }
                else
                {
                    // If we reached the end, set the pos because the run is empty.
                    _pos = _nextPos;
                    _run = til::rectangle{};
                }
            }
        };
    }

    class bitmap
    {
    public:
        using const_iterator = details::_bitmap_const_iterator;

        bitmap() noexcept :
            _sz{},
            _rc{},
            _bits{},
            _runs{}
        {
        }

        bitmap(til::size sz) :
            bitmap(sz, false)
        {
        }

        bitmap(til::size sz, bool fill) :
            _sz(sz),
            _rc(sz),
            _bits(_sz.area()),
            _runs{}
        {
            if (fill)
            {
                set_all();
            }
        }

        constexpr bool operator==(const bitmap& other) const noexcept
        {
            return _sz == other._sz &&
                   _rc == other._rc &&
                   _bits == other._bits;
            // _runs excluded because it's a cache of generated state.
        }

        constexpr bool operator!=(const bitmap& other) const noexcept
        {
            return !(*this == other);
        }

        const_iterator begin() const
        {
            return const_iterator(_bits, _sz, 0);
        }

        const_iterator end() const
        {
            return const_iterator(_bits, _sz, _sz.area());
        }

        const std::vector<til::rectangle>& runs() const
        {
            // If we don't have cached runs, rebuild.
            if (!_runs.has_value())
            {
                _runs.emplace(begin(), end());
            }

            // Return a reference to the runs.
            return _runs.value();
        }

        // optional fill the uncovered area with bits.
        void translate(const til::point delta, bool fill = false)
        {
            if (delta.x() == 0)
            {
                // fast path by using bit shifting
                translate_y(delta.y(), fill);
                return;
            }

            // FUTURE: PERF: GH #4015: This could use in-place walk semantics instead of a temporary.
            til::bitmap other{ _sz };

            for (auto run : *this)
            {
                // Offset by the delta
                run += delta;

                // Intersect with the bounds of our bitmap area
                // as part of it could have slid out of bounds.
                run &= _rc;

                // Set it into the new bitmap.
                other.set(run);
            }

            // If we were asked to fill... find the uncovered region.
            if (fill)
            {
                // Original Rect of As.
                //
                // X <-- origin
                // A A A A
                // A A A A
                // A A A A
                // A A A A
                const auto originalRect = _rc;

                // If Delta = (2, 2)
                // Translated Rect of Bs.
                //
                // X <-- origin
                //
                //
                //     B B B B
                //     B B B B
                //     B B B B
                //     B B B B
                const auto translatedRect = _rc + delta;

                // Subtract the B from the A one to see what wasn't filled by the move.
                // C is the overlap of A and B:
                //
                // X <-- origin
                // A A A A                     1 1 1 1
                // A A A A                     1 1 1 1
                // A A C C B B     subtract    2 2
                // A A C C B B    --------->   2 2
                //     B B B B      A - B
                //     B B B B
                //
                // 1 and 2 are the spaces to fill that are "uncovered".
                const auto fillRects = originalRect - translatedRect;
                for (const auto& f : fillRects)
                {
                    other.set(f);
                }
            }

            // Swap us with the temporary one.
            std::swap(other, *this);
        }

        void set(const til::point pt)
        {
            THROW_HR_IF(E_INVALIDARG, !_rc.contains(pt));
            _runs.reset(); // reset cached runs on any non-const method

            _bits.set(_rc.index_of(pt));
        }

        void set(const til::rectangle rc)
        {
            THROW_HR_IF(E_INVALIDARG, !_rc.contains(rc));
            _runs.reset(); // reset cached runs on any non-const method

            for (auto row = rc.top(); row < rc.bottom(); ++row)
            {
                _bits.set(_rc.index_of(til::point{ rc.left(), row }), rc.width(), true);
            }
        }

        void set_all() noexcept
        {
            _runs.reset(); // reset cached runs on any non-const method
            _bits.set();
        }

        void reset_all() noexcept
        {
            _runs.reset(); // reset cached runs on any non-const method
            _bits.reset();
        }

        // True if we resized. False if it was the same size as before.
        // Set fill if you want the new region (on growing) to be marked dirty.
        bool resize(til::size size, bool fill = false)
        {
            _runs.reset(); // reset cached runs on any non-const method

            // Don't resize if it's not different
            if (_sz != size)
            {
                // Make a new bitmap for the other side, empty initially.
                auto newMap = bitmap(size, false);

                // Copy any regions that overlap from this map to the new one.
                // Just iterate our runs...
                for (const auto run : *this)
                {
                    // intersect them with the new map
                    // so we don't attempt to set bits that fit outside
                    // the new one.
                    const auto intersect = run & newMap._rc;

                    // and if there is still anything left, set them.
                    if (!intersect.empty())
                    {
                        newMap.set(intersect);
                    }
                }

                // Then, if we were requested to fill the new space on growing,
                // find the space in the new rectangle that wasn't in the old
                // and fill it up.
                if (fill)
                {
                    // A subtraction will yield anything in the new that isn't
                    // a part of the old.
                    const auto newAreas = newMap._rc - _rc;
                    for (const auto& area : newAreas)
                    {
                        newMap.set(area);
                    }
                }

                // Swap and return.
                std::swap(newMap, *this);

                return true;
            }
            else
            {
                return false;
            }
        }

        constexpr bool one() const noexcept
        {
            return _bits.count() == 1;
        }

        constexpr bool any() const noexcept
        {
            return !none();
        }

        constexpr bool none() const noexcept
        {
            return _bits.none();
        }

        constexpr bool all() const noexcept
        {
            return _bits.all();
        }

        constexpr til::size size() const noexcept
        {
            return _sz;
        }

        std::wstring to_string() const
        {
            std::wstringstream wss;
            wss << std::endl
                << L"Bitmap of size " << _sz.to_string() << " contains the following dirty regions:" << std::endl;
            wss << L"Runs:" << std::endl;

            for (auto& item : *this)
            {
                wss << L"\t- " << item.to_string() << std::endl;
            }

            return wss.str();
        }

    private:
        void translate_y(ptrdiff_t delta_y, bool fill)
        {
            if (delta_y == 0)
            {
                return;
            }

            const auto bitShift = delta_y * _sz.width();

#pragma warning(push)
            // we can't depend on GSL here (some libraries use BLOCK_GSL), so we use static_cast for explicit narrowing
#pragma warning(disable : 26472)
            const auto newBits = static_cast<size_t>(std::abs(bitShift));
#pragma warning(pop)
            const bool isLeftShift = bitShift > 0;

            if (newBits >= _bits.size())
            {
                if (fill)
                {
                    set_all();
                }
                else
                {
                    reset_all();
                }
                return;
            }

            if (isLeftShift)
            {
                // This operator doesn't modify the size of `_bits`: the
                // new bits are set to 0.
                _bits <<= newBits;
            }
            else
            {
                _bits >>= newBits;
            }

            if (fill)
            {
                if (isLeftShift)
                {
                    _bits.set(0, newBits, true);
                }
                else
                {
                    _bits.set(_bits.size() - newBits, newBits, true);
                }
            }

            _runs.reset(); // reset cached runs on any non-const method
        }

        til::size _sz;
        til::rectangle _rc;
        dynamic_bitset<> _bits;

        mutable std::optional<std::vector<til::rectangle>> _runs;

#ifdef UNIT_TESTING
        friend class ::BitmapTests;
#endif
    };
}

#ifdef __WEX_COMMON_H__
namespace WEX::TestExecution
{
    template<>
    class VerifyOutputTraits<::til::bitmap>
    {
    public:
        static WEX::Common::NoThrowString ToString(const ::til::bitmap& rect)
        {
            return WEX::Common::NoThrowString(rect.to_string().c_str());
        }
    };

    template<>
    class VerifyCompareTraits<::til::bitmap, ::til::bitmap>
    {
    public:
        static bool AreEqual(const ::til::bitmap& expected, const ::til::bitmap& actual) noexcept
        {
            return expected == actual;
        }

        static bool AreSame(const ::til::bitmap& expected, const ::til::bitmap& actual) noexcept
        {
            return &expected == &actual;
        }

        static bool IsLessThan(const ::til::bitmap& expectedLess, const ::til::bitmap& expectedGreater) = delete;

        static bool IsGreaterThan(const ::til::bitmap& expectedGreater, const ::til::bitmap& expectedLess) = delete;

        static bool IsNull(const ::til::bitmap& object) noexcept
        {
            return object == til::bitmap{};
        }
    };

};
#endif
