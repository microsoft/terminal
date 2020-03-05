// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "til/point.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

class PointTests
{
    TEST_CLASS(PointTests);

    TEST_METHOD(CastToCoord)
    {
        Log::Comment(L"0.) Typical situation.");
        {
            const til::point pt{ 5, 10 };
            COORD val = pt;
            VERIFY_ARE_EQUAL(5, val.X);
            VERIFY_ARE_EQUAL(10, val.Y);
        }

        Log::Comment(L"1.) Overflow on width.");
        {
            constexpr ptrdiff_t x = std::numeric_limits<ptrdiff_t>().max();
            const ptrdiff_t y = 10;
            const til::point pt{ x, y};

            auto fn = [&]()
            {
                COORD val = pt;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"2.) Overflow on height.");
        {
            constexpr ptrdiff_t y = std::numeric_limits<ptrdiff_t>().max();
            const ptrdiff_t x = 10;
            const til::point pt{ x, y};

            auto fn = [&]()
            {
                COORD val = pt;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(CastToPoint)
    {
        Log::Comment(L"0.) Typical situation.");
        {
            const til::point pt{ 5, 10 };
            POINT val = pt;
            VERIFY_ARE_EQUAL(5, val.x);
            VERIFY_ARE_EQUAL(10, val.y);
        }

        Log::Comment(L"1.) Overflow on width.");
        {
            constexpr ptrdiff_t x = std::numeric_limits<ptrdiff_t>().max();
            const ptrdiff_t y = 10;
            const til::point pt{ x, y};

            auto fn = [&]()
            {
                POINT val = pt;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"2.) Overflow on height.");
        {
            constexpr ptrdiff_t y = std::numeric_limits<ptrdiff_t>().max();
            const ptrdiff_t x = 10;
            const til::point pt{ x, y};

            auto fn = [&]()
            {
                POINT val = pt;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(CastToD2D1Point2F)
    {
        Log::Comment(L"0.) Typical situation.");
        {
            const til::point pt{ 5, 10 };
            D2D1_POINT_2F val = pt;
            VERIFY_ARE_EQUAL(5, val.x);
            VERIFY_ARE_EQUAL(10, val.y);
        }

        // All ptrdiff_ts fit into a float, so there's no exception tests.
    }
};
