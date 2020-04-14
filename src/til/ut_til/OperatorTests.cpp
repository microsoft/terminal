// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "til/operators.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

class OperatorTests
{
    TEST_CLASS(OperatorTests);

    TEST_METHOD(PointAddSize)
    {
        const til::point pt{ 5, 10 };
        const til::size sz{ 2, 4 };
        const til::point expected{ 5 + 2, 10 + 4 };
        const auto actual = pt + sz;
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(PointSubSize)
    {
        const til::point pt{ 5, 10 };
        const til::size sz{ 2, 4 };
        const til::point expected{ 5 - 2, 10 - 4 };
        const auto actual = pt - sz;
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(PointMulSize)
    {
        const til::point pt{ 5, 10 };
        const til::size sz{ 2, 4 };
        const til::point expected{ 5 * 2, 10 * 4 };
        const auto actual = pt * sz;
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(PointDivSize)
    {
        const til::point pt{ 5, 10 };
        const til::size sz{ 2, 4 };
        const til::point expected{ 5 / 2, 10 / 4 };
        const auto actual = pt / sz;
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(SizeAddPoint)
    {
        const til::size pt{ 5, 10 };
        const til::point sz{ 2, 4 };
        const til::size expected{ 5 + 2, 10 + 4 };
        const auto actual = pt + sz;
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(SizeSubPoint)
    {
        const til::size pt{ 5, 10 };
        const til::point sz{ 2, 4 };
        const til::size expected{ 5 - 2, 10 - 4 };
        const auto actual = pt - sz;
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(SizeMulPoint)
    {
        const til::size pt{ 5, 10 };
        const til::point sz{ 2, 4 };
        const til::size expected{ 5 * 2, 10 * 4 };
        const auto actual = pt * sz;
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(SizeDivPoint)
    {
        const til::size pt{ 5, 10 };
        const til::point sz{ 2, 4 };
        const til::size expected{ 5 / 2, 10 / 4 };
        const auto actual = pt / sz;
        VERIFY_ARE_EQUAL(expected, actual);
    }
};
