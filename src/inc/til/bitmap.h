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
        ptrdiff_t _pos;
        ptrdiff_t _nextPos;
        til::rectangle _run;

        // why? why isn't this just walking with a bitterator?
        til::point _indexToPoint(ptrdiff_t index)
        {
            const auto width = _size.width();
            const auto row = base::CheckDiv(index, width);
            const auto indexesConsumed = row * width;
            const auto col = base::CheckSub(index, indexesConsumed);

            ptrdiff_t x, y;
            THROW_HR_IF(E_ABORT, !row.AssignIfValid(&y));
            THROW_HR_IF(E_ABORT, !col.AssignIfValid(&x));

            return til::point{ x, y };
        }

        void _calculateArea()
        {
            const ptrdiff_t end = _size.area();

            _nextPos = _pos;

            while (_nextPos < end && !_values.at(_nextPos))
            {
                ++_nextPos;
            }

            if (_nextPos < end)
            {
                // pos is now at the first on bit.
                const auto runStart = _indexToPoint(_nextPos);
                const ptrdiff_t rowEndIndex = ((runStart.y() + 1) * _size.width());

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

    private:
        bool _empty;
        til::size _size;
        std::vector<bool> _bits;

#ifdef UNIT_TESTING
        friend class ::BitmapTests;
#endif
    };
}
