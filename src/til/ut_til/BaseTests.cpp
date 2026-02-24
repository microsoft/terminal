// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "til.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

// Tests for things in the TIL base class.
class BaseTests
{
    TEST_CLASS(BaseTests);

    TEST_METHOD(ManageVector)
    {
        constexpr auto shrinkThreshold = 0.5f;

        std::vector<int> foo;
        foo.reserve(20);

        Log::Comment(L"Expand vector.");
        til::manage_vector(foo, 30, shrinkThreshold);
        VERIFY_ARE_EQUAL(30u, foo.capacity());

        Log::Comment(L"Try shrink but by not enough for threshold.");
        til::manage_vector(foo, 18, shrinkThreshold);
        VERIFY_ARE_EQUAL(30u, foo.capacity());

        Log::Comment(L"Shrink because it is meeting threshold.");
        til::manage_vector(foo, 15, shrinkThreshold);
        VERIFY_ARE_EQUAL(15u, foo.capacity());
    }
};
