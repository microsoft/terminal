// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <vector>
#include "size.h"

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    class bitterator // Bit Iterator. Bitterator.
    {
    public:
        bitterator(const std::vector<bool>& values, size_t pos) :
            _map(values),
            _pos(pos)
        {

        }

        bitterator& operator++()
        {
            ++_pos;
            return (*this);
        }

        bitterator& operator--()
        {
            --_pos;
            return (*this);
        }

        bitterator& operator+=(const ptrdiff_t& movement)
        {
            _pos += movement;
            return (*this);
        }

        bitterator& operator-=(const ptrdiff_t& movement)
        {
            _pos -= movement;
            return (*this);
        }

        bool operator==(const bitterator& other) const
        {
            return _pos == other._pos && _map == other._map;
        }

        bool operator!=(const bitterator& other) const
        {
            return !(*this == other);
        }

        bool operator<(const bitterator& other) const
        {
            return _pos < other._pos;
        }

        bool operator>(const bitterator& other) const
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
        using const_iterator = bitterator;
        
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
            return bitterator(_bits, 0);
        }

        const_iterator end() const
        {
            return bitterator(_bits, _size.area());
        }

        const_iterator begin_row(size_t row) const
        {
            return bitterator(_bits, row * _size.width());
        }

        const_iterator end_row(size_t row) const
        {
            return bitterator(_bits, (row + 1) * _size.width());
        }
        
        void set(til::point pt)
        {
            _bits[pt.y() * _size.width() + pt.x()] = true;
        }

        void reset(til::point pt)
        {
            _bits[pt.y() * _size.width() + pt.x()] = false;
        }

        void reset_all()
        {
            _bits.assign(_size.area(), false);
        }

#ifdef UNIT_TESTING
        friend class BitmapTests;
#endif

    private:
        const til::size _size;
        std::vector<bool> _bits;
    };
}
