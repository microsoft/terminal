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
            _sz(sz),
            _rc(sz),
            _bits(sz.area(), false),
            _dirty()
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
        bool resize(til::size size)
        {
            // Don't resize if it's not different
            if (_sz != size)
            {
                _sz = size;
                _rc = til::rectangle{ size };
                // .resize(_size(), true/false) throws an assert (unsupported operation)
                reset_all();
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

        bool any() const
        {
            return !none();
        }

        bool none() const
        {
            return _dirty.empty();
        }

        bool all() const
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
