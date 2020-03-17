// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#ifdef UNIT_TESTING
class BitmapTests;
#endif

namespace til // Terminal Implementation Library. Also: "Today I Learned"
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

        bool operator==(const _bitmap_const_iterator& other) const
        {
            return _pos == other._pos && _values == other._values;
        }

        bool operator!=(const _bitmap_const_iterator& other) const
        {
            return !(*this == other);
        }

        bool operator<(const _bitmap_const_iterator& other) const
        {
            return _pos < other._pos;
        }

        bool operator>(const _bitmap_const_iterator& other) const
        {
            return _pos > other._pos;
        }

        til::rectangle operator*() const
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
                } while (_nextPos < _end && _nextPos < rowEndIndex && _values.at(_nextPos));
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

    class bitmap
    {
    public:
        using const_iterator = _bitmap_const_iterator;

        bitmap() :
            bitmap(til::size{ 0, 0 })
        {
        }

        bitmap(til::size sz) :
            _sz(sz),
            _rc(sz),
            _bits(sz.area(), false),
            _empty(true)
        {
        }

        const_iterator begin() const
        {
            return _bitmap_const_iterator(_bits, _sz, 0);
        }

        const_iterator end() const
        {
            return _bitmap_const_iterator(_bits, _sz, _sz.area());
        }

        void set(til::point pt)
        {
            THROW_HR_IF(E_INVALIDARG, !_rc.contains(pt));

            _bits[_rc.index_of(pt)] = true;
            _empty = false;
        }

        void reset(til::point pt)
        {
            THROW_HR_IF(E_INVALIDARG, !_rc.contains(pt));

            _bits[_rc.index_of(pt)] = false;
        }

        void set(til::rectangle rc)
        {
            THROW_HR_IF(E_INVALIDARG, !_rc.contains(rc));

            for (auto pt : rc)
            {
                set(pt);
            }
        }

        void reset(til::rectangle rc)
        {
            THROW_HR_IF(E_INVALIDARG, !_rc.contains(rc));

            for (auto pt : rc)
            {
                reset(pt);
            }
        }

        void set_all()
        {
            // .clear() then .resize(_size(), true) throws an assert (unsupported operation)
            // .assign(_size(), true) throws an assert (unsupported operation)

            set(_rc);
        }

        void reset_all()
        {
            // .clear() then .resize(_size(), false) throws an assert (unsupported operation)
            // .assign(_size(), false) throws an assert (unsupported operation)
            reset(_rc);
            _empty = true;
        }

        // True if we resized. False if it was the same size as before.
        bool resize(til::size size)
        {
            // Don't resize if it's not different as we mark the whole thing dirty on resize.
            if (_sz != size)
            {
                _sz = size;
                _rc = til::rectangle{ size };
                // .resize(_size(), true/false) throws an assert (unsupported operation)
                _bits = std::vector<bool>(_sz.area(), false);
                _empty = true;
                return true;
            }
            else
            {
                return false;
            }
        }

        constexpr bool empty() const
        {
            return _empty;
        }

    private:
        bool _empty;
        til::size _sz;
        til::rectangle _rc;
        std::vector<bool> _bits;

#ifdef UNIT_TESTING
        friend class ::BitmapTests;
#endif
    };
}
