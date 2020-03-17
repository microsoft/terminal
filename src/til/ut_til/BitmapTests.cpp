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

    TEST_METHOD(DefaultConstruct)
    {
        const til::bitmap bitmap;
        const til::size expectedSize{ 0, 0 };
        const til::rectangle expectedRect{ 0, 0, 0, 0 };
        VERIFY_ARE_EQUAL(expectedSize, bitmap._sz);
        VERIFY_ARE_EQUAL(expectedRect, bitmap._rc);
        VERIFY_ARE_EQUAL(0, bitmap._bits.size());
        VERIFY_ARE_EQUAL(true, bitmap._empty);
    }

    TEST_METHOD(SizeConstruct)
    {
        const til::size expectedSize{ 5, 10 };
        const til::rectangle expectedRect{ 0, 0, 5, 10 };
        const til::bitmap bitmap{ expectedSize };
        VERIFY_ARE_EQUAL(expectedSize, bitmap._sz);
        VERIFY_ARE_EQUAL(expectedRect, bitmap._rc);
        VERIFY_ARE_EQUAL(50, bitmap._bits.size());
        VERIFY_ARE_EQUAL(true, bitmap._empty);
    }

    TEST_METHOD(SetReset)
    {
        const til::size sz{ 4, 4 };
        til::bitmap bitmap{ sz };

        // Every bit should be false.
        Log::Comment(L"All bits false on creation.");
        for (auto bit : bitmap._bits)
        {
            VERIFY_IS_FALSE(bit);
        }

        const til::point point{ 2, 2 };
        bitmap.set(point);

        // Point 2,2 is this index in a 4x4 rectangle.
        const auto index = 4 + 4 + 2;

        // Run through every bit. Only the one we set should be true.
        Log::Comment(L"Only the bit we set should be true.");
        for (size_t i = 0; i < bitmap._bits.size(); ++i)
        {
            if (i == index)
            {
                VERIFY_IS_TRUE(bitmap._bits[i]);
            }
            else
            {
                VERIFY_IS_FALSE(bitmap._bits[i]);
            }
        }

        // Reset the same one.
        bitmap.reset(point);

        // Now every bit should be false.
        Log::Comment(L"Unsetting the one we just set should make it false again.");
        for (auto bit : bitmap._bits)
        {
            VERIFY_IS_FALSE(bit);
        }

        Log::Comment(L"Setting all should mean they're all true.");
        bitmap.set_all();

        for (auto bit : bitmap._bits)
        {
            VERIFY_IS_TRUE(bit);
        }

        Log::Comment(L"Resetting just the one should leave a false in a field of true.");
        bitmap.reset(point);
        for (size_t i = 0; i < bitmap._bits.size(); ++i)
        {
            if (i == index)
            {
                VERIFY_IS_FALSE(bitmap._bits[i]);
            }
            else
            {
                VERIFY_IS_TRUE(bitmap._bits[i]);
            }
        }

        Log::Comment(L"Now reset them all.");
        bitmap.reset_all();

        for (auto bit : bitmap._bits)
        {
            VERIFY_IS_FALSE(bit);
        }

        til::rectangle totalZone{ sz };
        Log::Comment(L"Set a rectangle of bits and test they went on.");
        // 0 0 0 0       |1 1|0 0
        // 0 0 0 0  --\  |1 1|0 0
        // 0 0 0 0  --/  |1 1|0 0
        // 0 0 0 0        0 0 0 0
        til::rectangle setZone{ til::point{ 0, 0 }, til::size{ 2, 3 } };
        bitmap.set(setZone);

        for (auto pt : totalZone)
        {
            const auto expected = setZone.contains(pt);
            const auto actual = bitmap._bits[totalZone.index_of(pt)];
            // Do it this way and not with equality so you can see it in output.
            if (expected)
            {
                VERIFY_IS_TRUE(actual);
            }
            else
            {
                VERIFY_IS_FALSE(actual);
            }
        }

        Log::Comment(L"Reset a rectangle of bits not totally overlapped and check only they were cleared.");
        // 1 1 0 0       1 1 0 0
        // 1 1 0 0  --\  1|0 0|0
        // 1 1 0 0  --/  1|0 0|0
        // 0 0 0 0       0 0 0 0
        til::rectangle resetZone{ til::point{ 1, 1 }, til::size{ 2, 2 } };
        bitmap.reset(resetZone);

        for (auto pt : totalZone)
        {
            const auto expected = setZone.contains(pt) && !resetZone.contains(pt);
            const auto actual = bitmap._bits[totalZone.index_of(pt)];
            // Do it this way and not with equality so you can see it in output.
            if (expected)
            {
                VERIFY_IS_TRUE(actual);
            }
            else
            {
                VERIFY_IS_FALSE(actual);
            }
        }
    }

    TEST_METHOD(SetResetExceptions)
    {
        til::bitmap map{ til::size{ 4, 4 } };
        Log::Comment(L"1.) SetPoint out of bounds.");
        {
            auto fn = [&]() {
                map.set(til::point{ 10, 10 });
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_INVALIDARG; });
        }

        Log::Comment(L"2.) SetRectangle out of bounds.");
        {
            auto fn = [&]() {
                map.set(til::rectangle{ til::point{
                                            2,
                                            2,
                                        },
                                        til::size{ 10, 10 } });
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_INVALIDARG; });
        }

        Log::Comment(L"3.) ResetPoint out of bounds.");
        {
            auto fn = [&]() {
                map.reset(til::point{ 10, 10 });
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_INVALIDARG; });
        }

        Log::Comment(L"4.) ResetRectangle out of bounds.");
        {
            auto fn = [&]() {
                map.reset(til::rectangle{ til::point{
                                              2,
                                              2,
                                          },
                                          til::size{ 10, 10 } });
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_INVALIDARG; });
        }
    }

    TEST_METHOD(Resize)
    {
        Log::Comment(L"Set up a bitmap with every location flagged.");
        const til::size originalSize{ 2, 2 };
        til::bitmap bitmap{ originalSize };

        bitmap.set_all();

        Log::Comment(L"Attempt resize to the same size.");
        VERIFY_IS_FALSE(bitmap.resize(originalSize));

        Log::Comment(L"Attempt resize to a new size.");
        VERIFY_IS_TRUE(bitmap.resize(til::size{ 3, 3 }));
    }

    TEST_METHOD(Empty)
    {
        Log::Comment(L"When created, it should be empty.");
        til::bitmap bitmap{ til::size{ 2, 2 } };
        VERIFY_IS_TRUE(bitmap.empty());

        Log::Comment(L"When it is modified with a set, it should be non-empty.");
        bitmap.set(til::point{ 0, 0 });
        VERIFY_IS_FALSE(bitmap.empty());

        // We don't track it becoming empty again by resetting points because
        // we don't know for sure it's entirely empty unless we seek through the whole thing.
        // It's just supposed to be a short-circuit optimization for between frames
        // when the whole thing has been processed and cleared.
        Log::Comment(L"Resetting the same point, it will still report non-empty.");
        bitmap.reset(til::point{ 0, 0 });
        VERIFY_IS_FALSE(bitmap.empty());

        Log::Comment(L"But resetting all, it will report empty again.");
        bitmap.reset_all();
        VERIFY_IS_TRUE(bitmap.empty());

        Log::Comment(L"And setting all will be non-empty again.");
        bitmap.set_all();
        VERIFY_IS_FALSE(bitmap.empty());

        // FUTURE: We could make resize smart and
        // crop off the shrink or only invalidate the new bits added.
        Log::Comment(L"And resizing should make it empty*");
        bitmap.resize(til::size{ 3, 3 });
        VERIFY_IS_TRUE(bitmap.empty());
    }

    TEST_METHOD(Iterate)
    {
        // This map --> Those runs
        // 1 1 0 1      A A _ B
        // 1 0 1 1      C _ D D
        // 0 0 1 0      _ _ E _
        // 0 1 1 0      _ F F _
        Log::Comment(L"Set up a bitmap with some runs.");

        // Note: we'll test setting/resetting some overlapping rects and points.

        til::bitmap map{ til::size{ 4, 4 } };

        // 0 0 0 0     |1 1 1 1|
        // 0 0 0 0     |1 1 1 1|
        // 0 0 0 0 -->  0 0 0 0
        // 0 0 0 0      0 0 0 0
        map.set(til::rectangle{ til::point{ 0, 0 }, til::size{ 4, 2 } });

        // 1 1 1 1     1 1 1 1
        // 1 1 1 1     1|1 1|1
        // 0 0 0 0 --> 0|1 1|0
        // 0 0 0 0     0|1 1|0
        map.set(til::rectangle{ til::point{ 1, 1 }, til::size{ 2, 3 } });

        // 1 1 1 1     1 1 1 1
        // 1 1 1 1     1|0|1 1
        // 0 1 1 0 --> 0|0|1 0
        // 0 1 1 0     0 1 1 0
        map.reset(til::rectangle{ til::point{ 1, 1 }, til::size{ 1, 2 } });

        // 1 1 1 1     1 1|0|1
        // 1 0 1 1     1 0 1 1
        // 0 0 1 0 --> 0 0 1 0
        // 0 1 1 0     0 1 1 0
        map.reset(til::point{ 2, 0 });

        Log::Comment(L"Building the expected run rectangles.");

        // Reminder, we're making 6 rectangle runs A-F like this:
        // A A _ B
        // C _ D D
        // _ _ E _
        // _ F F _
        til::some<til::rectangle, 6> expected;
        expected.push_back(til::rectangle{ til::point{ 0, 0 }, til::size{ 2, 1 } });
        expected.push_back(til::rectangle{ til::point{ 3, 0 }, til::size{ 1, 1 } });
        expected.push_back(til::rectangle{ til::point{ 0, 1 }, til::size{ 1, 1 } });
        expected.push_back(til::rectangle{ til::point{ 2, 1 }, til::size{ 2, 1 } });
        expected.push_back(til::rectangle{ til::point{ 2, 2 }, til::size{ 1, 1 } });
        expected.push_back(til::rectangle{ til::point{ 1, 3 }, til::size{ 2, 1 } });

        Log::Comment(L"Run the iterator and collect the runs.");
        til::some<til::rectangle, 6> actual;
        for (auto run : map)
        {
            actual.push_back(run);
        }

        Log::Comment(L"Verify they match what we expected.");
        VERIFY_ARE_EQUAL(expected, actual);

        Log::Comment(L"Clear the map and iterate and make sure we get no results.");
        map.reset_all();

        expected.clear();
        actual.clear();
        for (auto run : map)
        {
            actual.push_back(run);
        }

        Log::Comment(L"Verify they're empty.");
        VERIFY_ARE_EQUAL(expected, actual);
    }
};
