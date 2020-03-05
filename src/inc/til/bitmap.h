// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <vector>
#include "size.h"

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
            _size(sz)
        {
            _bits.resize(_size.area());
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

        void set(til::point pt)
        {
            _bits[pt.y() * _size.width() + pt.x()] = true;
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
            _bits.assign(_size.area(), true);
        }

        void reset_all()
        {
            _bits.assign(_size.area(), false);
        }

        const til::size& size() const
        {
            return _size;
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
                return(*this);
            }

            // TODO: any way to reconcile this with walk directions from scrolling apis?
            // TODO: actually implement translation.

            return (*this);
        }

#ifdef UNIT_TESTING
        friend class BitmapTests;
#endif

    private:
        const til::size _size;
        std::vector<bool> _bits;
    };
}
