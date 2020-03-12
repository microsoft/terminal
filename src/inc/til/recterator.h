// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "point.h"
#include "size.h"

#ifdef UNIT_TESTING
class RecteratorTests;
#endif

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    class recterator
    {
    public:
        recterator(point topLeft, size size) :
            _topLeft(topLeft),
            _size(size),
            _current(topLeft)
        {
        }

        recterator(point topLeft, size size, point start) :
            _topLeft(topLeft),
            _size(size),
            _current(start)
        {
        }

        recterator& operator++()
        {
            if (_current.x() + 1 >= _topLeft.x() + _size.width())
            {
                _current = { _topLeft.x(), _current.y() + 1 };
            }
            else
            {
                _current = { _current.x() + 1, _current.y() };
            }

            return (*this);
        }

        bool operator==(const recterator& other) const
        {
            return _current == other._current &&
                   _topLeft == other._topLeft &&
                   _size == other._size;
        }

        bool operator!=(const recterator& other) const
        {
            return !(*this == other);
        }

        bool operator<(const recterator& other) const
        {
            return _current < other._current;
        }

        bool operator>(const recterator& other) const
        {
            return _current > other._current;
        }

        point operator*() const
        {
            return _current;
        }

    protected:
        point _current;
        const point _topLeft;
        const size _size;

#ifdef UNIT_TESTING
        friend class ::RecteratorTests;
#endif
    };
};

#ifdef __WEX_COMMON_H__
namespace WEX::TestExecution
{
    template<>
    class VerifyOutputTraits<::til::recterator>
    {
    public:
        static WEX::Common::NoThrowString ToString(const ::til::recterator& /*rect*/)
        {
            return WEX::Common::NoThrowString().Format(L"Yep that's a recterator.");
        }
    };

    template<>
    class VerifyCompareTraits<::til::recterator, ::til::recterator>
    {
    public:
        static bool AreEqual(const ::til::recterator& expected, const ::til::recterator& actual) noexcept
        {
            return expected == actual;
        }

        static bool AreSame(const ::til::recterator& expected, const ::til::recterator& actual) noexcept
        {
            return &expected == &actual;
        }

        static bool IsLessThan(const ::til::recterator& expectedLess, const ::til::recterator& expectedGreater) = delete;

        static bool IsGreaterThan(const ::til::recterator& expectedGreater, const ::til::recterator& expectedLess) = delete;

        static bool IsNull(const ::til::recterator& object) noexcept = delete;
    };

};
#endif
