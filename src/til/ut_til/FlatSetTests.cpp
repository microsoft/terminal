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

    constexpr operator bool() const noexcept
    {
        return value != emptyMarker;
    }

    constexpr bool operator==(int key) const noexcept
    {
        return value == static_cast<size_t>(key);
    }

    constexpr Data& operator=(int key) noexcept
    {
        value = static_cast<size_t>(key);
        return *this;
    }

    size_t value = emptyMarker;
};

template<>
struct ::std::hash<Data>
{
    constexpr size_t operator()(int key) const noexcept
    {
        return til::flat_set_hash_integer(static_cast<size_t>(key));
    }

    constexpr size_t operator()(Data d) const noexcept
    {
        return til::flat_set_hash_integer(d.value);
    }
};

class FlatSetTests
{
    TEST_CLASS(FlatSetTests);

    TEST_METHOD(Basic)
    {
        til::linear_flat_set<Data> set;

        // This simultaneously demonstrates how the class can't just do "heterogeneous lookups"
        // like STL does, but also insert items with a different type.
        const auto [entry1, inserted1] = set.insert(123);
        VERIFY_IS_TRUE(inserted1);

        const auto [entry2, inserted2] = set.insert(123);
        VERIFY_IS_FALSE(inserted2);

        VERIFY_ARE_EQUAL(&entry1, &entry2);
        VERIFY_ARE_EQUAL(123u, entry2.value);
    }
};
