// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "til/bitmap.h"

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

        SIZE mapSize{ 10, 10 };
        til::bitmap x(mapSize);
        x.set({ 4, 4 });

        til::rectangle area(til::point{ 5, 5 }, til::size{ 2, 2 });
        x.set(area);

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

        SMALL_RECT smrc;
        smrc.Top = 31;
        smrc.Bottom = 41;
        smrc.Left = 59;
        smrc.Right = 265;

        til::rectangle smrectangle(smrc);

        VERIFY_ARE_EQUAL(smrc.Top, smrectangle.top());
        VERIFY_ARE_EQUAL(smrc.Bottom, smrectangle.bottom());
        VERIFY_ARE_EQUAL(smrc.Left, smrectangle.left());
        VERIFY_ARE_EQUAL(smrc.Right, smrectangle.right());

        RECT bgrc;
        bgrc.top = 3;
        bgrc.bottom = 5;
        bgrc.left = 8;
        bgrc.right = 9;

        til::rectangle bgrectangle(bgrc);

        VERIFY_ARE_EQUAL(bgrc.top, bgrectangle.top());
        VERIFY_ARE_EQUAL(bgrc.bottom, bgrectangle.bottom());
        VERIFY_ARE_EQUAL(bgrc.left, bgrectangle.left());
        VERIFY_ARE_EQUAL(bgrc.right, bgrectangle.right());
    }

    TEST_METHOD(Runerator)
    {
        til::bitmap foo{ til::size{ 4, 8 } };
        foo.reset_all();
        foo.set(til::rectangle{ til::point{ 1, 1 }, til::size{ 2, 2 } });

        foo.set(til::rectangle{ til::point{ 3, 5 } });
        foo.set(til::rectangle{ til::point{ 0, 6 } });

        std::deque<til::rectangle> expectedRects;
        expectedRects.push_back(til::rectangle{ til::point{ 1, 1 }, til::size{ 2, 1 } });
        expectedRects.push_back(til::rectangle{ til::point{ 1, 2 }, til::size{ 2, 1 } });
        expectedRects.push_back(til::rectangle{ til::point{ 3, 5 } });
        expectedRects.push_back(til::rectangle{ til::point{ 0, 6 } });

        for (auto it = foo.begin_runs(); it < foo.end_runs(); ++it)
        {
            const auto actual = *it;
            const auto expected = expectedRects.front();

            VERIFY_ARE_EQUAL(expected, actual);

            expectedRects.pop_front();
        }
    }
};
