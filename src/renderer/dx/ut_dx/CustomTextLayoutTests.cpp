// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "..\..\inc\consoletaeftemplates.hpp"

#include "..\CustomTextLayout.h"

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
};
