// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <vector>
#include "size.h"

#ifdef UNIT_TESTING
class BitmapTests;
#endif

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    class const_bitterator // Bit Iterator. Bitterator.
    {
    public:
        const_bitterator(const std::vector<bool>& values, size_t pos) :
            _map(values),
            _pos(pos)
        {
        }

        const_bitterator operator+(const ptrdiff_t movement)
        {
            auto copy = *this;
            copy += movement;
            return copy;
        }

        const_bitterator operator-(const ptrdiff_t movement)
        {
            auto copy = *this;
            copy -= movement;
            return copy;
        }

        const_bitterator& operator++()
        {
            ++_pos;
            return (*this);
        }

        const_bitterator& operator--()
        {
            --_pos;
            return (*this);
        }

        const_bitterator& operator+=(const ptrdiff_t& movement)
        {
            _pos += movement;
            return (*this);
        }

        const_bitterator& operator-=(const ptrdiff_t& movement)
        {
            _pos -= movement;
            return (*this);
        }

        bool operator==(const const_bitterator& other) const
        {
            return _pos == other._pos && _map == other._map;
        }

        bool operator!=(const const_bitterator& other) const
        {
            return !(*this == other);
        }

        bool operator<(const const_bitterator& other) const
        {
            return _pos < other._pos;
        }

        bool operator>(const const_bitterator& other) const
        {
            return _pos > other._pos;
        }

        bool operator*() const
        {
            return _map[_pos];
        }

    private:
        size_t _pos;
        const std::vector<bool>& _map;
    };

    class const_runerator // Run Iterator. Runerator.
    {
    public:
        const_runerator(const std::vector<bool>& values, til::size sz, size_t pos) :
            _values(values),
            _size(sz),
            _pos(pos)
        {
            _calculateArea();
        }

        const_runerator& operator++()
        {
            _pos = _nextPos;
            _calculateArea();
            return (*this);
        }

        bool operator==(const const_runerator& other) const
        {
            return _pos == other._pos && _values == other._values;
        }

        bool operator!=(const const_runerator& other) const
        {
            return !(*this == other);
        }

        bool operator<(const const_runerator& other) const
        {
            return _pos < other._pos;
        }

        bool operator>(const const_runerator& other) const
        {
            return _pos > other._pos;
        }

        til::rectangle operator*() const
        {
            return _run;
        }

    private:
        const std::vector<bool>& _values;
        const til::size _size;
        size_t _pos;
        size_t _nextPos;
        til::rectangle _run;

        til::point _indexToPoint(size_t index)
        {
            return til::point{ (ptrdiff_t)index % _size.width(), (ptrdiff_t)index / _size.width() };
        }

        void _calculateArea()
        {
            const size_t end = (size_t)_size.area();

            _nextPos = _pos;

            while (_nextPos < end && !_values.at(_nextPos))
            {
                ++_nextPos;
            }

            if (_nextPos < end)
            {
                // pos is now at the first on bit.
                const auto runStart = _indexToPoint(_nextPos);
                const size_t rowEndIndex = (size_t)((runStart.y() + 1) * _size.width());

                ptrdiff_t runLength = 0;

                do
                {
                    ++_nextPos;
                    ++runLength;
                } while (_nextPos < end && _nextPos < rowEndIndex && _values.at(_nextPos));

                _run = til::rectangle{ runStart, til::size{ runLength, static_cast<ptrdiff_t>(1) } };
            }
            else
            {
                _pos = _nextPos;
                _run = til::rectangle{};
            }
        }
    };

    class bitmap
    {
    public:
        using const_iterator = const const_bitterator;

        bitmap() :
            bitmap(0, 0)
        {
        }

        bitmap(size_t width, size_t height) :
            bitmap(til::size{ width, height })
        {
        }

        bitmap(til::size sz) :
            _size(sz),
            _bits(sz.area(), true),
            _empty(false)
        {
        }

        const_iterator begin() const
        {
            return const_bitterator(_bits, 0);
        }

        const_iterator end() const
        {
            return const_bitterator(_bits, _size.area());
        }

        const_iterator begin_row(size_t row) const
        {
            return const_bitterator(_bits, row * _size.width());
        }

        const_iterator end_row(size_t row) const
        {
            return const_bitterator(_bits, (row + 1) * _size.width());
        }

        const_runerator begin_runs() const
        {
            return const_runerator(_bits, _size, 0);
        }

        const_runerator end_runs() const
        {
            return const_runerator(_bits, _size, _size.area());
        }

        void set(til::point pt)
        {
            _bits[pt.y() * _size.width() + pt.x()] = true;
            _empty = false;
        }

        void reset(til::point pt)
        {
            _bits[pt.y() * _size.width() + pt.x()] = false;
        }

        void set(til::rectangle rc)
        {
            for (auto pt : rc)
            {
                set(pt);
            }
        }

        void reset(til::rectangle rc)
        {
            for (auto pt : rc)
            {
                reset(pt);
            }
        }

        void set_all()
        {
            // .clear() then .resize(_size(), true) throws an assert (unsupported operation)
            // .assign(_size(), true) throws an assert (unsupported operation)

            set(til::rectangle{ til::point{ 0, 0 }, _size });
        }

        void reset_all()
        {
            // .clear() then .resize(_size(), false) throws an assert (unsupported operation)
            // .assign(_size(), false) throws an assert (unsupported operation)
            reset(til::rectangle{ til::point{ 0, 0 }, _size });
            _empty = true;
        }

        void resize(til::size size)
        {
            // Don't resize if it's not different as we mark the whole thing dirty on resize.
            // TODO: marking it dirty might not be necessary or we should be smart about it
            // (mark none of it dirty on resize down, mark just the edges on up?)
            if (_size != size)
            {
                _size = size;
                // .resize(_size(), true) throws an assert (unsupported operation)
                _bits = std::vector<bool>(_size.area(), true);
            }
        }

        void resize(size_t width, size_t height)
        {
            resize(til::size{ width, height });
        }

        constexpr bool empty() const
        {
            return _empty;
        }

        const til::size& size() const
        {
            return _size;
        }

        operator bool() const noexcept
        {
            return !_bits.empty();
        }

        bitmap operator+(const point& pt) const
        {
            auto temp = *this;
            return temp += pt;
        }

        bitmap& operator+=(const point& pt)
        {
            // early return if nothing to do.
            if (pt.x() == 0 && pt.y() == 0)
            {
                return (*this);
            }

            // If we're told to shift the whole thing by an entire width or height,
            // the effect is to just clear the whole bitmap.
            if (pt.x() >= _size.width() || pt.y() >= _size.height())
            {
                reset_all();
                return (*this);
            }

            // TODO: any way to reconcile this with walk directions from scrolling apis?
            // TODO: actually implement translation.

            return (*this);
        }

#ifdef UNIT_TESTING
        friend class ::BitmapTests;
#endif

    private:
        bool _empty;
        til::size _size;
        std::vector<bool> _bits;
    };
}
