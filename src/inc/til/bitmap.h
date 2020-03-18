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
            _bitmap_const_iterator(const std::vector<bool>& values, til::rectangle rc, ptrdiff_t pos) :
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

            constexpr til::rectangle operator*() const noexcept
            {
                return _run;
            }

        private:
            const std::vector<bool>& _values;
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
                while (_nextPos < _end && !_values.at(_nextPos))
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
                    } while (_nextPos < rowEndIndex && _values.at(_nextPos));
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
            _dirty{}
        {
        }

        bitmap(til::size sz) :
            bitmap(sz, false)
        {
        }

        bitmap(til::size sz, bool fill) :
            _sz(sz),
            _rc(sz),
            _bits(sz.area(), fill),
            _dirty(fill ? sz : til::rectangle{})
        {
        }

        const_iterator begin() const
        {
            return const_iterator(_bits, _sz, 0);
        }

        const_iterator end() const
        {
            return const_iterator(_bits, _sz, _sz.area());
        }

        void set(const til::point pt)
        {
            THROW_HR_IF(E_INVALIDARG, !_rc.contains(pt));

            til::at(_bits, _rc.index_of(pt)) = true;
            _dirty |= til::rectangle{ pt };
        }

        void set(const til::rectangle rc)
        {
            THROW_HR_IF(E_INVALIDARG, !_rc.contains(rc));

            for (const auto pt : rc)
            {
                til::at(_bits, _rc.index_of(pt)) = true;
            }

            _dirty |= rc;
        }

        void set_all()
        {
            // .clear() then .resize(_size(), true) throws an assert (unsupported operation)
            // .assign(_size(), true) throws an assert (unsupported operation)
            _bits = std::vector<bool>(_sz.area(), true);
            _dirty = _rc;
        }

        void reset_all()
        {
            // .clear() then .resize(_size(), false) throws an assert (unsupported operation)
            // .assign(_size(), false) throws an assert (unsupported operation)

            _bits = std::vector<bool>(_sz.area(), false);
            _dirty = {};
        }

        // True if we resized. False if it was the same size as before.
        // Set fill if you want the new region (on growing) to be marked dirty.
        bool resize(til::size size, bool fill = false)
        {
            // FYI .resize(_size(), true/false) throws an assert (unsupported operation)

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

        bool one() const
        {
            return _dirty.size() == til::size{ 1, 1 };
        }

        constexpr bool any() const noexcept
        {
            return !none();
        }

        constexpr bool none() const noexcept
        {
            return _dirty.empty();
        }

        constexpr bool all() const noexcept
        {
            return _dirty == _rc;
        }

    private:
        til::rectangle _dirty;
        til::size _sz;
        til::rectangle _rc;
        std::vector<bool> _bits;

#ifdef UNIT_TESTING
        friend class ::BitmapTests;
#endif
    };
}
