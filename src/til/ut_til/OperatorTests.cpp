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

    TEST_METHOD(RectangleAdditionSize)
    {
        const til::rectangle start{ 10, 20, 30, 40 };

        Log::Comment(L"1.) Add size to bottom and right");
        {
            const til::size scale{ 3, 7 };
            const til::rectangle expected{ 10, 20, 33, 47 };
            const auto actual = start + scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"2.) Add size to top and left");
        {
            const til::size scale{ -3, -7 };
            const til::rectangle expected{ 7, 13, 30, 40 };
            const auto actual = start + scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"3.) Add size to bottom and left");
        {
            const til::size scale{ -3, 7 };
            const til::rectangle expected{ 7, 20, 30, 47 };
            const auto actual = start + scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"4.) Add size to top and right");
        {
            const til::size scale{ 3, -7 };
            const til::rectangle expected{ 10, 13, 33, 40 };
            const auto actual = start + scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }
    }

    TEST_METHOD(RectangleInplaceAdditionSize)
    {
        const til::rectangle start{ 10, 20, 30, 40 };

        Log::Comment(L"1.) Add size to bottom and right");
        {
            auto actual = start;
            const til::size scale{ 3, 7 };
            const til::rectangle expected{ 10, 20, 33, 47 };
            actual += scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"2.) Add size to top and left");
        {
            auto actual = start;
            const til::size scale{ -3, -7 };
            const til::rectangle expected{ 7, 13, 30, 40 };
            actual += scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"3.) Add size to bottom and left");
        {
            auto actual = start;
            const til::size scale{ -3, 7 };
            const til::rectangle expected{ 7, 20, 30, 47 };
            actual += scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"4.) Add size to top and right");
        {
            auto actual = start;
            const til::size scale{ 3, -7 };
            const til::rectangle expected{ 10, 13, 33, 40 };
            actual += scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }
    }

    TEST_METHOD(RectangleSubtractionSize)
    {
        const til::rectangle start{ 10, 20, 30, 40 };

        Log::Comment(L"1.) Subtract size from bottom and right");
        {
            const til::size scale{ 3, 7 };
            const til::rectangle expected{ 10, 20, 27, 33 };
            const auto actual = start - scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"2.) Subtract size from top and left");
        {
            const til::size scale{ -3, -7 };
            const til::rectangle expected{ 13, 27, 30, 40 };
            const auto actual = start - scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"3.) Subtract size from bottom and left");
        {
            const til::size scale{ -3, 7 };
            const til::rectangle expected{ 13, 20, 30, 33 };
            const auto actual = start - scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"4.) Subtract size from top and right");
        {
            const til::size scale{ 3, -6 };
            const til::rectangle expected{ 10, 26, 27, 40 };
            const auto actual = start - scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }
    }

    TEST_METHOD(RectangleInplaceSubtractionSize)
    {
        const til::rectangle start{ 10, 20, 30, 40 };

        Log::Comment(L"1.) Subtract size from bottom and right");
        {
            auto actual = start;
            const til::size scale{ 3, 7 };
            const til::rectangle expected{ 10, 20, 27, 33 };
            actual -= scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"2.) Subtract size from top and left");
        {
            auto actual = start;
            const til::size scale{ -3, -7 };
            const til::rectangle expected{ 13, 27, 30, 40 };
            actual -= scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"3.) Subtract size from bottom and left");
        {
            auto actual = start;
            const til::size scale{ -3, 7 };
            const til::rectangle expected{ 13, 20, 30, 33 };
            actual -= scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"4.) Subtract size from top and right");
        {
            auto actual = start;
            const til::size scale{ 3, -6 };
            const til::rectangle expected{ 10, 26, 27, 40 };
            actual -= scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }
    }
};
