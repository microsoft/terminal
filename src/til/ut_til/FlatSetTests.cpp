// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include <til/flat_set.h>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

struct Data
{
    static constexpr auto emptyMarker = std::numeric_limits<size_t>::max();
    size_t value = emptyMarker;
};

struct DataHashTrait
{
    static constexpr bool occupied(const Data& d) noexcept
    {
        return d.value != Data::emptyMarker;
    }

    static constexpr size_t hash(const size_t key) noexcept
    {
        return til::flat_set_hash_integer(key);
    }

    static constexpr size_t hash(const Data& d) noexcept
    {
        return til::flat_set_hash_integer(d.value);
    }

    static constexpr bool equals(const Data& d, size_t key) noexcept
    {
        return d.value == key;
    }

    static constexpr void assign(Data& d, size_t key) noexcept
    {
        d.value = key;
    }
};

class FlatSetTests
{
    TEST_CLASS(FlatSetTests);

    TEST_METHOD(Basic)
    {
        til::linear_flat_set<Data, DataHashTrait> set;

        // This simultaneously demonstrates how the class can't just do "heterogeneous lookups"
        // like STL does, but also insert items with a different type.
        const auto [entry1, inserted1] = set.insert(123);
        VERIFY_IS_TRUE(inserted1);

        const auto [entry2, inserted2] = set.insert(123);
        VERIFY_IS_FALSE(inserted2);

        VERIFY_ARE_EQUAL(entry1, entry2);
        VERIFY_ARE_EQUAL(123u, entry2->value);
    }
};
