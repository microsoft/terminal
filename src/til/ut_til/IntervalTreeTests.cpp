// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "til/point.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

// These tests cover our use of oss/interval_tree with a non-arithmetic Scalar.
// See oss/interval_tree/MAINTAINER_README.md for the local deviations involved.
class IntervalTreeTests
{
    TEST_CLASS(IntervalTreeTests);

    using PointTree = interval_tree::IntervalTree<til::point, size_t>;

    // The tree only splits into subtrees once it holds at least 64 intervals (its
    // default minimum bucket size), so anything below that never exercised the
    // subtree constraints.
    static PointTree _makeTree(size_t count)
    {
        PointTree::interval_vector intervals;
        intervals.reserve(count);
        for (size_t i = 0; i < count; ++i)
        {
            const auto y = gsl::narrow<til::CoordType>(i);
            intervals.push_back(PointTree::interval({ 0, y }, { 10, y }, i));
        }
        return PointTree{ std::move(intervals) };
    }

    // GH#20486: is_valid() seeds its bounds accumulators from
    // std::numeric_limits<Scalar>. Without a specialization the primary template
    // answers til::point{0,0} for both ends, which is not a usable sentinel.
    TEST_METHOD(NumericLimitsAreUsableSentinels)
    {
        VERIFY_IS_TRUE(std::numeric_limits<til::point>::is_specialized);
        VERIFY_IS_TRUE(std::numeric_limits<til::point>::min() < til::point{});
        VERIFY_IS_TRUE(std::numeric_limits<til::point>::max() > til::point{});
        VERIFY_ARE_EQUAL(std::numeric_limits<til::point>::min(), std::numeric_limits<til::point>::lowest());

        VERIFY_IS_TRUE(std::numeric_limits<til::size>::is_specialized);
        VERIFY_ARE_EQUAL(til::CoordTypeMax, std::numeric_limits<til::size>::max().width);
        VERIFY_ARE_EQUAL(til::CoordTypeMin, std::numeric_limits<til::size>::min().height);
    }

    // Before GH#20486 this asserted in Debug builds for any tree with 64 or more
    // intervals -- i.e. whenever 64+ URLs were autodetected on screen.
    TEST_METHOD(IsValidWithSubtrees)
    {
        for (const size_t count : { 1u, 63u, 64u, 65u, 512u })
        {
            const auto tree = _makeTree(count);
            VERIFY_IS_TRUE(tree.is_valid().first, NoThrowString().Format(L"%zu intervals", count));
        }
    }

    // A tree that reports itself valid must also still find what it contains.
    TEST_METHOD(FindsIntervalsAcrossSubtrees)
    {
        static constexpr size_t count = 200;
        const auto tree = _makeTree(count);

        for (size_t i = 0; i < count; ++i)
        {
            const auto y = gsl::narrow<til::CoordType>(i);
            const auto results = tree.findOverlapping({ 5, y }, { 5, y });
            VERIFY_ARE_EQUAL(1u, results.size(), NoThrowString().Format(L"row %zu", i));
            VERIFY_ARE_EQUAL(i, results.front().value);
        }
    }
};
