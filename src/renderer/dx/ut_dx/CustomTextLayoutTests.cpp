// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "../../inc/consoletaeftemplates.hpp"

#include "../CustomTextLayout.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

using namespace Microsoft::Console::Render;

class Microsoft::Console::Render::CustomTextLayoutTests
{
    TEST_CLASS(CustomTextLayoutTests);

    TEST_METHOD(OrderRuns)
    {
        CustomTextLayout layout;

        // Create linked list runs where a --> c --> b
        CustomTextLayout::LinkedRun a;
        a.nextRunIndex = 2;
        a.textStart = 0;
        CustomTextLayout::LinkedRun b;
        b.nextRunIndex = 0;
        b.textStart = 20;
        CustomTextLayout::LinkedRun c;
        c.nextRunIndex = 1;
        c.textStart = 10;

        // but insert them into the runs as a, b, c
        layout._runs.push_back(a);
        layout._runs.push_back(b);
        layout._runs.push_back(c);

        // Now order them.
        layout._OrderRuns();

        // Validate that they've been reordered to a, c, b by index so they can be iterated to go in order.

        // The text starts should be in order 0, 10, 20.
        // The next run indexes should point at each other.
        VERIFY_ARE_EQUAL(a.textStart, layout._runs.at(0).textStart);
        VERIFY_ARE_EQUAL(1u, layout._runs.at(0).nextRunIndex);
        VERIFY_ARE_EQUAL(c.textStart, layout._runs.at(1).textStart);
        VERIFY_ARE_EQUAL(2u, layout._runs.at(1).nextRunIndex);
        VERIFY_ARE_EQUAL(b.textStart, layout._runs.at(2).textStart);
        VERIFY_ARE_EQUAL(0u, layout._runs.at(2).nextRunIndex);
    }

    TEST_METHOD(SplitCurrentRunIncludingGlyphs)
    {
        CustomTextLayout layout;

        // Put glyph data into the layout as if we've already gone through analysis.
        // This data matches the verbose comment from the CustomTextLayout.cpp file
        // and is derived from
        // https://social.msdn.microsoft.com/Forums/en-US/993365bc-8689-45ff-a675-c5ed0c011788/dwriteglyphrundescriptionclustermap-explained

        layout._text = L"fiñe";

        layout._glyphIndices.push_back(19);
        layout._glyphIndices.push_back(81);
        layout._glyphIndices.push_back(23);
        layout._glyphIndices.push_back(72);

        layout._glyphClusters.push_back(0);
        layout._glyphClusters.push_back(0);
        layout._glyphClusters.push_back(1);
        layout._glyphClusters.push_back(3);

        // Set up the layout to have a run that already has glyph data inside of it.
        CustomTextLayout::LinkedRun run;
        run.textStart = 0;
        run.textLength = 4;
        run.glyphStart = 0;
        run.glyphCount = 4;

        layout._runs.push_back(run);

        // Now split it in the middle per the comment example
        layout._SetCurrentRun(2);
        layout._SplitCurrentRun(2);

        // And validate that the split state matches what we expected.
        VERIFY_ARE_EQUAL(0u, layout._runs.at(0).textStart);
        VERIFY_ARE_EQUAL(2u, layout._runs.at(0).textLength);
        VERIFY_ARE_EQUAL(0u, layout._runs.at(0).glyphStart);
        VERIFY_ARE_EQUAL(1u, layout._runs.at(0).glyphCount);

        VERIFY_ARE_EQUAL(2u, layout._runs.at(1).textStart);
        VERIFY_ARE_EQUAL(2u, layout._runs.at(1).textLength);
        VERIFY_ARE_EQUAL(1u, layout._runs.at(1).glyphStart);
        VERIFY_ARE_EQUAL(3u, layout._runs.at(1).glyphCount);
    }
};
