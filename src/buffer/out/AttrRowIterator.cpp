// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "AttrRowIterator.hpp"
#include "AttrRow.hpp"

AttrRowIterator AttrRowIterator::CreateEndIterator(const ATTR_ROW* const attrRow) noexcept
{
    AttrRowIterator it{ attrRow };
    it._setToEnd();
    return it;
}

AttrRowIterator::AttrRowIterator(const ATTR_ROW* const attrRow) noexcept :
    _pAttrRow{ attrRow },
    _run{ attrRow->_list.cbegin() },
    _currentAttributeIndex{ 0 }
{
}

AttrRowIterator::operator bool() const
{
    return _run < _pAttrRow->_list.cend();
}

bool AttrRowIterator::operator==(const AttrRowIterator& it) const
{
    return (_pAttrRow == it._pAttrRow &&
            _run == it._run &&
            _currentAttributeIndex == it._currentAttributeIndex);
}

bool AttrRowIterator::operator!=(const AttrRowIterator& it) const
{
    return !(*this == it);
}

AttrRowIterator& AttrRowIterator::operator++()
{
    _increment(1);
    return *this;
}

AttrRowIterator AttrRowIterator::operator++(int)
{
    auto copy = *this;
    _increment(1);
    return copy;
}

AttrRowIterator& AttrRowIterator::operator+=(const ptrdiff_t& movement)
{
    if (movement >= 0)
    {
        _increment(gsl::narrow<size_t>(movement));
    }
    else
    {
        _decrement(gsl::narrow<size_t>(-movement));
    }

    return *this;
}

AttrRowIterator& AttrRowIterator::operator-=(const ptrdiff_t& movement)
{
    return this->operator+=(-movement);
}

AttrRowIterator& AttrRowIterator::operator--()
{
    _decrement(1);
    return *this;
}

AttrRowIterator AttrRowIterator::operator--(int)
{
    auto copy = *this;
    _decrement(1);
    return copy;
}

const TextAttribute* AttrRowIterator::operator->() const
{
    return &_run->GetAttributes();
}

const TextAttribute& AttrRowIterator::operator*() const
{
    return _run->GetAttributes();
}

// Routine Description:
// - increments the index the iterator points to
// Arguments:
// - count - the amount to increment by
void AttrRowIterator::_increment(size_t count)
{
    while (count > 0)
    {
        const size_t runLength = _run->GetLength();
        if (count + _currentAttributeIndex < runLength)
        {
            _currentAttributeIndex += count;
            return;
        }
        else
        {
            count -= runLength - _currentAttributeIndex;
            ++_run;
            _currentAttributeIndex = 0;
        }
    }
}

// Routine Description:
// - decrements the index the iterator points to
// Arguments:
// - count - the amount to decrement by
void AttrRowIterator::_decrement(size_t count)
{
    while (count > 0)
    {
        if (count <= _currentAttributeIndex)
        {
            _currentAttributeIndex -= count;
            return;
        }
        else
        {
            count -= _currentAttributeIndex;
            --_run;
            _currentAttributeIndex = _run->GetLength() - 1;
        }
    }
}

// Routine Description:
// - sets fields on the iterator to describe the end() state of the ATTR_ROW
void AttrRowIterator::_setToEnd() noexcept
{
    _run = _pAttrRow->_list.cend();
    _currentAttributeIndex = 0;
}
