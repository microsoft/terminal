// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include <til/generational.h>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

struct Data
{
    int value = 0;
};

class GenerationalTests
{
    TEST_CLASS(GenerationalTests);

    TEST_METHOD(Basic)
    {
        til::generational<Data> src;
        til::generational<Data> dst;

        // Reads via -> and *, just like std::optional, etc.
        VERIFY_ARE_EQUAL(0, src->value);
        VERIFY_ARE_EQUAL(0, (*src).value);

        // Writes via .write()->
        src.write()->value = 123;
        // ...which makes them not compare as equal.
        VERIFY_ARE_NOT_EQUAL(dst, src);

        // Synchronize the two objects by copying them
        dst = src;
        // ...which results in both being considered equal again
        VERIFY_ARE_EQUAL(dst, src);
        // ...and all values are copied over.
        VERIFY_ARE_EQUAL(123, dst->value);
    }
};
