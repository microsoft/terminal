// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

class BitmapTests
{
    TEST_CLASS(BitmapTests);

    TEST_METHOD(Construct)
    {
        COORD foo;
        foo.X = 12;
        foo.Y = 14;
        til::point p(foo);
        VERIFY_ARE_EQUAL(foo.X, p.x());
        VERIFY_ARE_EQUAL(foo.Y, p.y());

        POINT pt;
        pt.x = 88;
        pt.y = 98;
        til::point q(pt);
        VERIFY_ARE_EQUAL(pt.x, q.x());
        VERIFY_ARE_EQUAL(pt.y, q.y());

        SIZE sz;
        sz.cx = 11;
        sz.cy = 13;
        til::size r(sz);
        VERIFY_ARE_EQUAL(sz.cx, r.width());
        VERIFY_ARE_EQUAL(sz.cy, r.height());

        COORD bar;
        bar.X = 57;
        bar.Y = 15;
        til::size s(bar);
        VERIFY_ARE_EQUAL(bar.X, s.width());
        VERIFY_ARE_EQUAL(bar.Y, s.height());

        SIZE mapSize{ 10,10 };
        til::bitmap x(mapSize);
        x.set({ 4, 4 });

        Log::Comment(L"Row 4!");
        for (auto it = x.begin_row(4); it < x.end_row(4); ++it)
        {
            if (*it)
            {
                Log::Comment(L"True");
            }
            else
            {
                Log::Comment(L"False");
            }
        }

        Log::Comment(L"All!");
        auto start = x.begin();
        auto end = x.end();

        for (const auto& y : x)
        {
            if (y)
            {
                Log::Comment(L"True");
            }
            else
            {
                Log::Comment(L"False");
            }
        }

    }
};
