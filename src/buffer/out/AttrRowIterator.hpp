/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- AttrRowIterator.hpp

Abstract:
- iterator for ATTR_ROW to walk the TextAttributes of the run
- read only iterator

Author(s):
- Austin Diviness (AustDi) 04-Jun-2018
--*/

#pragma once

#include "TextAttribute.hpp"
#include "TextAttributeRun.hpp"

class ATTR_ROW;

class AttrRowIterator final
{
public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = TextAttribute;
    using difference_type = std::ptrdiff_t;
    using pointer = TextAttribute*;
    using reference = TextAttribute&;

    static AttrRowIterator CreateEndIterator(const ATTR_ROW* const attrRow) noexcept;

    AttrRowIterator(const ATTR_ROW* const attrRow) noexcept;

    operator bool() const noexcept;

    bool operator==(const AttrRowIterator& it) const noexcept;
    bool operator!=(const AttrRowIterator& it) const noexcept;

    AttrRowIterator& operator++() noexcept
    {
        _increment(1);
        return *this;
    }
    AttrRowIterator operator++(int) noexcept
    {
        auto copy = *this;
        _increment(1);
        return copy;
    }

    AttrRowIterator& operator+=(const ptrdiff_t& movement);
    AttrRowIterator& operator-=(const ptrdiff_t& movement);

    AttrRowIterator& operator--() noexcept
    {
        _decrement(1);
        return *this;
    }
    AttrRowIterator operator--(int) noexcept
    {
        auto copy = *this;
        _decrement(1);
        return copy;
    }

    const TextAttribute* operator->() const;
    const TextAttribute& operator*() const;

private:
    boost::container::small_vector_base<TextAttributeRun>::const_iterator _run;
    const ATTR_ROW* _pAttrRow;
    size_t _currentAttributeIndex; // index of TextAttribute within the current TextAttributeRun
    bool _exceeded;

    void _increment(size_t count) noexcept;
    void _decrement(size_t count) noexcept;
    void _setToEnd() noexcept;
};
