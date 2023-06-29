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

    template<typename T>
    void _checkBits(const til::rect& bitsOn,
                    const til::details::bitmap<T>& map)
    {
        _checkBits(std::vector<til::rect>{ bitsOn }, map);
    }

    template<typename T>
    void _checkBits(const std::vector<til::rect>& bitsOn,
                    const til::details::bitmap<T>& map)
    {
        Log::Comment(L"Check all bits in map.");
        // For every point in the map...
        for (const auto pt : map._rc)
        {
            // If any of the rectangles we were given contains this point, we expect it should be on.
            const auto expected = std::any_of(bitsOn.cbegin(), bitsOn.cend(), [&pt](auto bitRect) { return bitRect.contains(pt); });

            // Get the actual bit out of the map.
            const auto actual = map._bits[map._rc.index_of(pt)];

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

    TEST_METHOD(DefaultConstruct)
    {
        const til::bitmap bitmap;
        const til::size expectedSize{ 0, 0 };
        const til::rect expectedRect{ 0, 0, 0, 0 };
        VERIFY_ARE_EQUAL(expectedSize, bitmap._sz);
        VERIFY_ARE_EQUAL(expectedRect, bitmap._rc);
        VERIFY_ARE_EQUAL(0u, bitmap._bits.size());

        // The find will go from begin to end in the bits looking for a "true".
        // It should miss so the result should be "cend" and turn out true here.
        VERIFY_IS_TRUE(bitmap._bits.none());
    }

    TEST_METHOD(SizeConstruct)
    {
        const til::size expectedSize{ 5, 10 };
        const til::rect expectedRect{ 0, 0, 5, 10 };
        const til::bitmap bitmap{ expectedSize };
        VERIFY_ARE_EQUAL(expectedSize, bitmap._sz);
        VERIFY_ARE_EQUAL(expectedRect, bitmap._rc);
        VERIFY_ARE_EQUAL(50u, bitmap._bits.size());

        // The find will go from begin to end in the bits looking for a "true".
        // It should miss so the result should be "cend" and turn out true here.
        VERIFY_IS_TRUE(bitmap._bits.none());
    }

    TEST_METHOD(SizeConstructWithFill)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:fill", L"{true, false}")
        END_TEST_METHOD_PROPERTIES()

        bool fill;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"fill", fill));

        const til::size expectedSize{ 5, 10 };
        const til::rect expectedRect{ 0, 0, 5, 10 };
        const til::bitmap bitmap{ expectedSize, fill };
        VERIFY_ARE_EQUAL(expectedSize, bitmap._sz);
        VERIFY_ARE_EQUAL(expectedRect, bitmap._rc);
        VERIFY_ARE_EQUAL(50u, bitmap._bits.size());

        if (!fill)
        {
            VERIFY_IS_TRUE(bitmap._bits.none());
        }
        else
        {
            VERIFY_IS_TRUE(bitmap._bits.all());
        }
    }

    TEST_METHOD(Equality)
    {
        Log::Comment(L"0.) Defaults are equal");
        {
            const til::bitmap one;
            const til::bitmap two;
            VERIFY_IS_TRUE(one == two);
        }

        Log::Comment(L"1.) Different sizes are unequal");
        {
            const til::bitmap one{ til::size{ 2, 2 } };
            const til::bitmap two{ til::size{ 3, 3 } };
            VERIFY_IS_FALSE(one == two);
        }

        Log::Comment(L"2.) Same bits set are equal");
        {
            til::bitmap one{ til::size{ 2, 2 } };
            til::bitmap two{ til::size{ 2, 2 } };
            one.set(til::point{ 0, 1 });
            one.set(til::point{ 1, 0 });
            two.set(til::point{ 0, 1 });
            two.set(til::point{ 1, 0 });
            VERIFY_IS_TRUE(one == two);
        }

        Log::Comment(L"3.) Different bits set are not equal");
        {
            til::bitmap one{ til::size{ 2, 2 } };
            til::bitmap two{ til::size{ 2, 2 } };
            one.set(til::point{ 0, 1 });
            two.set(til::point{ 1, 0 });
            VERIFY_IS_FALSE(one == two);
        }
    }

    TEST_METHOD(Inequality)
    {
        Log::Comment(L"0.) Defaults are equal");
        {
            const til::bitmap one;
            const til::bitmap two;
            VERIFY_IS_FALSE(one != two);
        }

        Log::Comment(L"1.) Different sizes are unequal");
        {
            const til::bitmap one{ til::size{ 2, 2 } };
            const til::bitmap two{ til::size{ 3, 3 } };
            VERIFY_IS_TRUE(one != two);
        }

        Log::Comment(L"2.) Same bits set are equal");
        {
            til::bitmap one{ til::size{ 2, 2 } };
            til::bitmap two{ til::size{ 2, 2 } };
            one.set(til::point{ 0, 1 });
            one.set(til::point{ 1, 0 });
            two.set(til::point{ 0, 1 });
            two.set(til::point{ 1, 0 });
            VERIFY_IS_FALSE(one != two);
        }

        Log::Comment(L"3.) Different bits set are not equal");
        {
            til::bitmap one{ til::size{ 2, 2 } };
            til::bitmap two{ til::size{ 2, 2 } };
            one.set(til::point{ 0, 1 });
            two.set(til::point{ 1, 0 });
            VERIFY_IS_TRUE(one != two);
        }
    }

    TEST_METHOD(Translate)
    {
        const til::size mapSize{ 4, 4 };
        til::bitmap map{ mapSize };

        // set the middle four bits of the map.
        // 0 0 0 0
        // 0 1 1 0
        // 0 1 1 0
        // 0 0 0 0
        map.set(til::rect{ til::point{ 1, 1 }, til::size{ 2, 2 } });

        Log::Comment(L"1.) Move down and right");
        {
            auto actual = map;
            // Move all contents
            // |
            // v
            // |
            // v --> -->
            const til::point delta{ 2, 2 };

            til::bitmap expected{ mapSize };
            // Expected:
            // 0 0 0 0         0 0 0 0          0 0 0 0
            // 0 1 1 0         0 0 0 0          0 0 0 0
            // 0 1 1 0 v  -->  0 0 0 0   -->    0 0 0 0
            // 0 0 0 0 v       0 1 1 0          0 0 0 1
            //                     ->->
            expected.set(til::point{ 3, 3 });

            actual.translate(delta);

            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"2.) Move down");
        {
            auto actual = map;
            // Move all contents
            // |
            // v
            // |
            // v
            const til::point delta{ 0, 2 };

            til::bitmap expected{ mapSize };
            // Expected:
            // 0 0 0 0         0 0 0 0
            // 0 1 1 0         0 0 0 0
            // 0 1 1 0 v  -->  0 0 0 0
            // 0 0 0 0 v       0 1 1 0
            expected.set(til::rect{ til::point{ 1, 3 }, til::size{ 2, 1 } });

            actual.translate(delta);

            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"3.) Move down and left");
        {
            auto actual = map;
            // Move all contents
            // |
            // v
            // |
            // v <-- <--
            const til::point delta{ -2, 2 };

            til::bitmap expected{ mapSize };
            // Expected:
            // 0 0 0 0         0 0 0 0          0 0 0 0
            // 0 1 1 0         0 0 0 0          0 0 0 0
            // 0 1 1 0 v  -->  0 0 0 0   -->    0 0 0 0
            // 0 0 0 0 v       0 1 1 0          1 0 0 0
            //                 <-<-
            expected.set(til::point{ 0, 3 });

            actual.translate(delta);

            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"4.) Move left");
        {
            auto actual = map;
            // Move all contents
            // <-- <--
            const til::point delta{ -2, 0 };

            til::bitmap expected{ mapSize };
            // Expected:
            // 0 0 0 0         0 0 0 0
            // 0 1 1 0         1 0 0 0
            // 0 1 1 0    -->  1 0 0 0
            // 0 0 0 0         0 0 0 0
            // <--<--
            expected.set(til::rect{ til::point{ 0, 1 }, til::size{ 1, 2 } });

            actual.translate(delta);

            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"5.) Move up and left");
        {
            auto actual = map;
            // Move all contents
            // ^
            // |
            // ^
            // | <-- <--
            const til::point delta{ -2, -2 };

            til::bitmap expected{ mapSize };
            // Expected:
            // 0 0 0 0 ^       0 1 1 0          1 0 0 0
            // 0 1 1 0 ^       0 0 0 0          0 0 0 0
            // 0 1 1 0    -->  0 0 0 0   -->    0 0 0 0
            // 0 0 0 0         0 0 0 0          0 0 0 0
            //                 <-<-
            expected.set(til::point{ 0, 0 });

            actual.translate(delta);

            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"6.) Move up");
        {
            auto actual = map;
            // Move all contents
            // ^
            // |
            // ^
            // |
            const til::point delta{ 0, -2 };

            til::bitmap expected{ mapSize };
            // Expected:
            // 0 0 0 0 ^       0 1 1 0
            // 0 1 1 0 ^       0 0 0 0
            // 0 1 1 0    -->  0 0 0 0
            // 0 0 0 0         0 0 0 0
            expected.set(til::rect{ til::point{ 1, 0 }, til::size{ 2, 1 } });

            actual.translate(delta);

            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"7.) Move up and right");
        {
            auto actual = map;
            // Move all contents
            // ^
            // |
            // ^
            // | --> -->
            const til::point delta{ 2, -2 };

            til::bitmap expected{ mapSize };
            // Expected:
            // 0 0 0 0 ^       0 1 1 0          0 0 0 1
            // 0 1 1 0 ^       0 0 0 0          0 0 0 0
            // 0 1 1 0    -->  0 0 0 0   -->    0 0 0 0
            // 0 0 0 0         0 0 0 0          0 0 0 0
            //                     ->->
            expected.set(til::point{ 3, 0 });

            actual.translate(delta);

            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"8.) Move right");
        {
            auto actual = map;
            // Move all contents
            // --> -->
            const til::point delta{ 2, 0 };

            til::bitmap expected{ mapSize };
            // Expected:
            // 0 0 0 0         0 0 0 0
            // 0 1 1 0         0 0 0 1
            // 0 1 1 0    -->  0 0 0 1
            // 0 0 0 0         0 0 0 0
            //     ->->
            expected.set(til::rect{ til::point{ 3, 1 }, til::size{ 1, 2 } });

            actual.translate(delta);

            VERIFY_ARE_EQUAL(expected, actual);
        }
    }

    TEST_METHOD(TranslateWithFill)
    {
        const til::size mapSize{ 4, 4 };
        til::bitmap map{ mapSize };

        // set the middle four bits of the map.
        // 0 0 0 0
        // 0 1 1 0
        // 0 1 1 0
        // 0 0 0 0
        map.set(til::rect{ til::point{ 1, 1 }, til::size{ 2, 2 } });

        Log::Comment(L"1.) Move down and right");
        {
            auto actual = map;
            // Move all contents
            // |
            // v
            // |
            // v --> -->
            const til::point delta{ 2, 2 };

            til::bitmap expected{ mapSize };
            // Expected: (F is filling uncovered value)
            // 0 0 0 0         F F F F          F F F F
            // 0 1 1 0         F F F F          F F F F
            // 0 1 1 0 v  -->  0 0 0 0   -->    F F 0 0
            // 0 0 0 0 v       0 1 1 0          F F 0 1
            //                     ->->
            expected.set(til::rect{ til::point{ 0, 0 }, til::size{ 4, 2 } });
            expected.set(til::rect{ til::point{ 0, 2 }, til::size{ 2, 2 } });
            expected.set(til::point{ 3, 3 });

            actual.translate(delta, true);

            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"2.) Move down");
        {
            auto actual = map;
            // Move all contents
            // |
            // v
            // |
            // v
            const til::point delta{ 0, 2 };

            til::bitmap expected{ mapSize };
            // Expected: (F is filling uncovered value)
            // 0 0 0 0         F F F F
            // 0 1 1 0         F F F F
            // 0 1 1 0 v  -->  0 0 0 0
            // 0 0 0 0 v       0 1 1 0
            expected.set(til::rect{ til::point{ 0, 0 }, til::size{ 4, 2 } });
            expected.set(til::rect{ til::point{ 1, 3 }, til::size{ 2, 1 } });

            actual.translate(delta, true);

            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"3.) Move down and left");
        {
            auto actual = map;
            // Move all contents
            // |
            // v
            // |
            // v <-- <--
            const til::point delta{ -2, 2 };

            til::bitmap expected{ mapSize };
            // Expected: (F is filling uncovered value)
            // 0 0 0 0         F F F F          F F F F
            // 0 1 1 0         F F F F          F F F F
            // 0 1 1 0 v  -->  0 0 0 0   -->    0 0 F F
            // 0 0 0 0 v       0 1 1 0          1 0 F F
            //                 <-<-
            expected.set(til::rect{ til::point{ 0, 0 }, til::size{ 4, 2 } });
            expected.set(til::rect{ til::point{ 2, 2 }, til::size{ 2, 2 } });
            expected.set(til::point{ 0, 3 });

            actual.translate(delta, true);

            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"4.) Move left");
        {
            auto actual = map;
            // Move all contents
            // <-- <--
            const til::point delta{ -2, 0 };

            til::bitmap expected{ mapSize };
            // Expected: (F is filling uncovered value)
            // 0 0 0 0         0 0 F F
            // 0 1 1 0         1 0 F F
            // 0 1 1 0    -->  1 0 F F
            // 0 0 0 0         0 0 F F
            // <--<--
            expected.set(til::rect{ til::point{ 2, 0 }, til::size{ 2, 4 } });
            expected.set(til::rect{ til::point{ 0, 1 }, til::size{ 1, 2 } });

            actual.translate(delta, true);

            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"5.) Move up and left");
        {
            auto actual = map;
            // Move all contents
            // ^
            // |
            // ^
            // | <-- <--
            const til::point delta{ -2, -2 };

            til::bitmap expected{ mapSize };
            // Expected: (F is filling uncovered value)
            // 0 0 0 0 ^       0 1 1 0          1 0 F F
            // 0 1 1 0 ^       0 0 0 0          0 0 F F
            // 0 1 1 0    -->  F F F F   -->    F F F F
            // 0 0 0 0         F F F F          F F F F
            //                 <-<-
            expected.set(til::rect{ til::point{ 2, 0 }, til::size{ 2, 2 } });
            expected.set(til::rect{ til::point{ 0, 2 }, til::size{ 4, 2 } });
            expected.set(til::point{ 0, 0 });

            actual.translate(delta, true);

            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"6.) Move up");
        {
            auto actual = map;
            // Move all contents
            // ^
            // |
            // ^
            // |
            const til::point delta{ 0, -2 };

            til::bitmap expected{ mapSize };
            // Expected: (F is filling uncovered value)
            // 0 0 0 0 ^       0 1 1 0
            // 0 1 1 0 ^       0 0 0 0
            // 0 1 1 0    -->  F F F F
            // 0 0 0 0         F F F F
            expected.set(til::rect{ til::point{ 1, 0 }, til::size{ 2, 1 } });
            expected.set(til::rect{ til::point{ 0, 2 }, til::size{ 4, 2 } });

            actual.translate(delta, true);

            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"7.) Move up and right");
        {
            auto actual = map;
            // Move all contents
            // ^
            // |
            // ^
            // | --> -->
            const til::point delta{ 2, -2 };

            til::bitmap expected{ mapSize };
            // Expected: (F is filling uncovered value)
            // 0 0 0 0 ^       0 1 1 0          F F 0 1
            // 0 1 1 0 ^       0 0 0 0          F F 0 0
            // 0 1 1 0    -->  F F F F   -->    F F F F
            // 0 0 0 0         F F F F          F F F F
            //                     ->->
            expected.set(til::point{ 3, 0 });
            expected.set(til::rect{ til::point{ 0, 2 }, til::size{ 4, 2 } });
            expected.set(til::rect{ til::point{ 0, 0 }, til::size{ 2, 2 } });

            actual.translate(delta, true);

            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"8.) Move right");
        {
            auto actual = map;
            // Move all contents
            // --> -->
            const til::point delta{ 2, 0 };

            til::bitmap expected{ mapSize };
            // Expected:  (F is filling uncovered value)
            // 0 0 0 0         F F 0 0
            // 0 1 1 0         F F 0 1
            // 0 1 1 0    -->  F F 0 1
            // 0 0 0 0         F F 0 0
            //     ->->
            expected.set(til::rect{ til::point{ 3, 1 }, til::size{ 1, 2 } });
            expected.set(til::rect{ til::point{ 0, 0 }, til::size{ 2, 4 } });

            actual.translate(delta, true);

            VERIFY_ARE_EQUAL(expected, actual);
        }
    }

    TEST_METHOD(SetReset)
    {
        const til::size sz{ 4, 4 };
        til::bitmap bitmap{ sz };

        // Every bit should be false.
        Log::Comment(L"All bits false on creation.");
        VERIFY_IS_TRUE(bitmap._bits.none());

        const til::point point{ 2, 2 };
        bitmap.set(point);

        std::vector<til::rect> expectedSet;
        expectedSet.emplace_back(til::rect{ 2, 2, 3, 3 });

        // Run through every bit. Only the one we set should be true.
        Log::Comment(L"Only the bit we set should be true.");
        _checkBits(expectedSet, bitmap);

        Log::Comment(L"Setting all should mean they're all true.");
        bitmap.set_all();

        expectedSet.clear();
        expectedSet.emplace_back(til::rect{ bitmap._rc });
        _checkBits(expectedSet, bitmap);

        Log::Comment(L"Now reset them all.");
        bitmap.reset_all();

        expectedSet.clear();
        _checkBits(expectedSet, bitmap);

        til::rect totalZone{ sz };
        Log::Comment(L"Set a rectangle of bits and test they went on.");
        // 0 0 0 0       |1 1|0 0
        // 0 0 0 0  --\  |1 1|0 0
        // 0 0 0 0  --/  |1 1|0 0
        // 0 0 0 0        0 0 0 0
        til::rect setZone{ til::point{ 0, 0 }, til::size{ 2, 3 } };
        bitmap.set(setZone);

        expectedSet.clear();
        expectedSet.emplace_back(setZone);
        _checkBits(expectedSet, bitmap);

        Log::Comment(L"Reset all.");
        bitmap.reset_all();

        expectedSet.clear();
        _checkBits(expectedSet, bitmap);
    }

    TEST_METHOD(SetResetOutOfBounds)
    {
        til::bitmap map{ til::size{ 4, 4 } };
        Log::Comment(L"1.) SetPoint out of bounds.");
        map.set(til::point{ 10, 10 });

        Log::Comment(L"2.) SetRectangle out of bounds.");
        map.set(til::rect{ til::point{ 2, 2 }, til::size{ 10, 10 } });

        const auto runs = map.runs();
        VERIFY_ARE_EQUAL(2u, runs.size());
        VERIFY_ARE_EQUAL(til::rect(2, 2, 4, 3), runs[0]);
        VERIFY_ARE_EQUAL(til::rect(2, 3, 4, 4), runs[1]);
    }

    TEST_METHOD(Resize)
    {
        Log::Comment(L"Set up a bitmap with every location flagged.");
        const til::size originalSize{ 2, 2 };
        til::bitmap bitmap{ originalSize, true };

        std::vector<til::rect> expectedFillRects;

        // 1 1
        // 1 1
        expectedFillRects.emplace_back(til::rect{ originalSize });
        _checkBits(expectedFillRects, bitmap);

        Log::Comment(L"Attempt resize to the same size.");
        VERIFY_IS_FALSE(bitmap.resize(originalSize));

        // 1 1
        // 1 1
        _checkBits(expectedFillRects, bitmap);

        Log::Comment(L"Attempt resize to a new size where both dimensions grow and we didn't ask for fill.");
        VERIFY_IS_TRUE(bitmap.resize(til::size{ 3, 3 }));

        // 1 1 0
        // 1 1 0
        // 0 0 0
        _checkBits(expectedFillRects, bitmap);

        Log::Comment(L"Set a bit out in the new space and check it.");
        const til::point spaceBit{ 1, 2 };
        expectedFillRects.emplace_back(til::rect{ 1, 2, 2, 3 });
        bitmap.set(spaceBit);

        // 1 1 0
        // 1 1 0
        // 0 1 0
        _checkBits(expectedFillRects, bitmap);

        Log::Comment(L"Grow vertically and shrink horizontally at the same time. Fill any new space.");
        expectedFillRects.emplace_back(til::rect{ til::point{ 0, 3 }, til::size{ 2, 1 } });
        bitmap.resize(til::size{ 2, 4 }, true);

        // 1 1
        // 1 1
        // 0 1
        // 1 1
        _checkBits(expectedFillRects, bitmap);
    }

    TEST_METHOD(One)
    {
        Log::Comment(L"When created, it should be not be one.");
        til::bitmap bitmap{ til::size{ 2, 2 } };
        VERIFY_IS_FALSE(bitmap.one());

        Log::Comment(L"When a single point is set, it should be one.");
        bitmap.set(til::point{ 1, 0 });
        VERIFY_IS_TRUE(bitmap.one());

        Log::Comment(L"Setting the same point again, should still be one.");
        bitmap.set(til::point{ 1, 0 });
        VERIFY_IS_TRUE(bitmap.one());

        Log::Comment(L"Setting another point, it should no longer be one.");
        bitmap.set(til::point{ 0, 0 });
        VERIFY_IS_FALSE(bitmap.one());

        Log::Comment(L"Clearing it, still not one.");
        bitmap.reset_all();
        VERIFY_IS_FALSE(bitmap.one());

        Log::Comment(L"Set one point, one again.");
        bitmap.set(til::point{ 1, 0 });
        VERIFY_IS_TRUE(bitmap.one());

        Log::Comment(L"And setting all will no longer be one again.");
        bitmap.set_all();
        VERIFY_IS_FALSE(bitmap.one());
    }

    TEST_METHOD(Any)
    {
        Log::Comment(L"When created, it should be not be any.");
        til::bitmap bitmap{ til::size{ 2, 2 } };
        VERIFY_IS_FALSE(bitmap.any());

        Log::Comment(L"When a single point is set, it should be any.");
        bitmap.set(til::point{ 1, 0 });
        VERIFY_IS_TRUE(bitmap.any());

        Log::Comment(L"Setting the same point again, should still be any.");
        bitmap.set(til::point{ 1, 0 });
        VERIFY_IS_TRUE(bitmap.any());

        Log::Comment(L"Setting another point, it should still be any.");
        bitmap.set(til::point{ 0, 0 });
        VERIFY_IS_TRUE(bitmap.any());

        Log::Comment(L"Clearing it, no longer any.");
        bitmap.reset_all();
        VERIFY_IS_FALSE(bitmap.any());

        Log::Comment(L"Set one point, one again, it's any.");
        bitmap.set(til::point{ 1, 0 });
        VERIFY_IS_TRUE(bitmap.any());

        Log::Comment(L"And setting all will be any as well.");
        bitmap.set_all();
        VERIFY_IS_TRUE(bitmap.any());
    }

    TEST_METHOD(None)
    {
        Log::Comment(L"When created, it should be none.");
        til::bitmap bitmap{ til::size{ 2, 2 } };
        VERIFY_IS_TRUE(bitmap.none());

        Log::Comment(L"When it is modified with a set, it should no longer be none.");
        bitmap.set(til::point{ 0, 0 });
        VERIFY_IS_FALSE(bitmap.none());

        Log::Comment(L"Resetting all, it will report none again.");
        bitmap.reset_all();
        VERIFY_IS_TRUE(bitmap.none());

        Log::Comment(L"And setting all will no longer be none again.");
        bitmap.set_all();
        VERIFY_IS_FALSE(bitmap.none());
    }

    TEST_METHOD(All)
    {
        Log::Comment(L"When created, it should be not be all.");
        til::bitmap bitmap{ til::size{ 2, 2 } };
        VERIFY_IS_FALSE(bitmap.all());

        Log::Comment(L"When a single point is set, it should not be all.");
        bitmap.set(til::point{ 1, 0 });
        VERIFY_IS_FALSE(bitmap.all());

        Log::Comment(L"Setting the same point again, should still not be all.");
        bitmap.set(til::point{ 1, 0 });
        VERIFY_IS_FALSE(bitmap.all());

        Log::Comment(L"Setting another point, it should still not be all.");
        bitmap.set(til::point{ 0, 0 });
        VERIFY_IS_FALSE(bitmap.all());

        Log::Comment(L"Clearing it, still not all.");
        bitmap.reset_all();
        VERIFY_IS_FALSE(bitmap.all());

        Log::Comment(L"Set one point, one again, not all.");
        bitmap.set(til::point{ 1, 0 });
        VERIFY_IS_FALSE(bitmap.all());

        Log::Comment(L"And setting all will finally be all.");
        bitmap.set_all();
        VERIFY_IS_TRUE(bitmap.all());

        Log::Comment(L"Clearing it, back to not all.");
        bitmap.reset_all();
        VERIFY_IS_FALSE(bitmap.all());
    }

    TEST_METHOD(Size)
    {
        til::size sz{ 5, 10 };
        til::bitmap map{ sz };

        VERIFY_ARE_EQUAL(sz, map.size());
    }

    TEST_METHOD(Runs)
    {
        // This map --> Those runs
        // 1 1 0 1      A A _ B
        // 1 0 1 1      C _ D D
        // 0 0 1 0      _ _ E _
        // 0 1 1 0      _ F F _
        Log::Comment(L"Set up a bitmap with some runs.");

        til::bitmap map{ til::size{ 4, 4 }, false };

        // 0 0 0 0     |1 1|0 0
        // 0 0 0 0      0 0 0 0
        // 0 0 0 0 -->  0 0 0 0
        // 0 0 0 0      0 0 0 0
        map.set(til::rect{ til::point{ 0, 0 }, til::size{ 2, 1 } });

        // 1 1 0 0     1 1 0 0
        // 0 0 0 0     0 0|1|0
        // 0 0 0 0 --> 0 0|1|0
        // 0 0 0 0     0 0|1|0
        map.set(til::rect{ til::point{ 2, 1 }, til::size{ 1, 3 } });

        // 1 1 0 0     1 1 0|1|
        // 0 0 1 0     0 0 1|1|
        // 0 0 1 0 --> 0 0 1 0
        // 0 0 1 0     0 0 1 0
        map.set(til::rect{ til::point{ 3, 0 }, til::size{ 1, 2 } });

        // 1 1 0 1     1 1 0 1
        // 0 0 1 1    |1|0 1 1
        // 0 0 1 0 --> 0 0 1 0
        // 0 0 1 0     0 0 1 0
        map.set(til::point{ 0, 1 });

        // 1 1 0 1     1 1 0 1
        // 1 0 1 1     1 0 1 1
        // 0 0 1 0 --> 0 0 1 0
        // 0 0 1 0     0|1|1 0
        map.set(til::point{ 1, 3 });

        Log::Comment(L"Building the expected run rectangles.");

        // Reminder, we're making 6 rectangle runs A-F like this:
        // A A _ B
        // C _ D D
        // _ _ E _
        // _ F F _
        til::some<til::rect, 6> expected;
        expected.push_back(til::rect{ til::point{ 0, 0 }, til::size{ 2, 1 } });
        expected.push_back(til::rect{ til::point{ 3, 0 }, til::size{ 1, 1 } });
        expected.push_back(til::rect{ til::point{ 0, 1 }, til::size{ 1, 1 } });
        expected.push_back(til::rect{ til::point{ 2, 1 }, til::size{ 2, 1 } });
        expected.push_back(til::rect{ til::point{ 2, 2 }, til::size{ 1, 1 } });
        expected.push_back(til::rect{ til::point{ 1, 3 }, til::size{ 2, 1 } });

        Log::Comment(L"Run the iterator and collect the runs.");
        til::some<til::rect, 6> actual;
        for (auto run : map.runs())
        {
            actual.push_back(run);
        }

        Log::Comment(L"Verify they match what we expected.");
        VERIFY_ARE_EQUAL(expected, actual);

        Log::Comment(L"Clear the map and iterate and make sure we get no results.");
        map.reset_all();

        expected.clear();
        actual.clear();
        for (auto run : map.runs())
        {
            actual.push_back(run);
        }

        Log::Comment(L"Verify they're empty.");
        VERIFY_ARE_EQUAL(expected, actual);

        Log::Comment(L"Set point and validate runs updated.");
        const til::point setPoint{ 2, 2 };
        expected.push_back(til::rect{ 2, 2, 3, 3 });
        map.set(setPoint);

        for (auto run : map.runs())
        {
            actual.push_back(run);
        }
        VERIFY_ARE_EQUAL(expected, actual);

        Log::Comment(L"Set rectangle and validate runs updated.");
        const til::rect setRect{ setPoint, til::size{ 2, 2 } };
        expected.clear();
        expected.push_back(til::rect{ til::point{ 2, 2 }, til::size{ 2, 1 } });
        expected.push_back(til::rect{ til::point{ 2, 3 }, til::size{ 2, 1 } });
        map.set(setRect);

        actual.clear();
        for (auto run : map.runs())
        {
            actual.push_back(run);
        }
        VERIFY_ARE_EQUAL(expected, actual);

        Log::Comment(L"Set all and validate runs updated.");
        expected.clear();
        expected.push_back(til::rect{ til::point{ 0, 0 }, til::size{ 4, 1 } });
        expected.push_back(til::rect{ til::point{ 0, 1 }, til::size{ 4, 1 } });
        expected.push_back(til::rect{ til::point{ 0, 2 }, til::size{ 4, 1 } });
        expected.push_back(til::rect{ til::point{ 0, 3 }, til::size{ 4, 1 } });
        map.set_all();

        actual.clear();
        for (auto run : map.runs())
        {
            actual.push_back(run);
        }
        VERIFY_ARE_EQUAL(expected, actual);

        Log::Comment(L"Resize and validate runs updated.");
        const til::size newSize{ 3, 3 };
        expected.clear();
        expected.push_back(til::rect{ til::point{ 0, 0 }, til::size{ 3, 1 } });
        expected.push_back(til::rect{ til::point{ 0, 1 }, til::size{ 3, 1 } });
        expected.push_back(til::rect{ til::point{ 0, 2 }, til::size{ 3, 1 } });
        map.resize(newSize);

        actual.clear();
        for (auto run : map.runs())
        {
            actual.push_back(run);
        }
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(RunsWithPmr)
    {
        // This is a copy of the above test, but with a pmr::bitmap.
        std::pmr::unsynchronized_pool_resource pool{ til::pmr::get_default_resource() };

        // This map --> Those runs
        // 1 1 0 1      A A _ B
        // 1 0 1 1      C _ D D
        // 0 0 1 0      _ _ E _
        // 0 1 1 0      _ F F _
        Log::Comment(L"Set up a PMR bitmap with some runs.");

        til::pmr::bitmap map{ til::size{ 4, 4 }, false, &pool };

        // 0 0 0 0     |1 1|0 0
        // 0 0 0 0      0 0 0 0
        // 0 0 0 0 -->  0 0 0 0
        // 0 0 0 0      0 0 0 0
        map.set(til::rect{ til::point{ 0, 0 }, til::size{ 2, 1 } });

        // 1 1 0 0     1 1 0 0
        // 0 0 0 0     0 0|1|0
        // 0 0 0 0 --> 0 0|1|0
        // 0 0 0 0     0 0|1|0
        map.set(til::rect{ til::point{ 2, 1 }, til::size{ 1, 3 } });

        // 1 1 0 0     1 1 0|1|
        // 0 0 1 0     0 0 1|1|
        // 0 0 1 0 --> 0 0 1 0
        // 0 0 1 0     0 0 1 0
        map.set(til::rect{ til::point{ 3, 0 }, til::size{ 1, 2 } });

        // 1 1 0 1     1 1 0 1
        // 0 0 1 1    |1|0 1 1
        // 0 0 1 0 --> 0 0 1 0
        // 0 0 1 0     0 0 1 0
        map.set(til::point{ 0, 1 });

        // 1 1 0 1     1 1 0 1
        // 1 0 1 1     1 0 1 1
        // 0 0 1 0 --> 0 0 1 0
        // 0 0 1 0     0|1|1 0
        map.set(til::point{ 1, 3 });

        Log::Comment(L"Building the expected run rectangles.");

        // Reminder, we're making 6 rectangle runs A-F like this:
        // A A _ B
        // C _ D D
        // _ _ E _
        // _ F F _
        til::some<til::rect, 6> expected;
        expected.push_back(til::rect{ til::point{ 0, 0 }, til::size{ 2, 1 } });
        expected.push_back(til::rect{ til::point{ 3, 0 }, til::size{ 1, 1 } });
        expected.push_back(til::rect{ til::point{ 0, 1 }, til::size{ 1, 1 } });
        expected.push_back(til::rect{ til::point{ 2, 1 }, til::size{ 2, 1 } });
        expected.push_back(til::rect{ til::point{ 2, 2 }, til::size{ 1, 1 } });
        expected.push_back(til::rect{ til::point{ 1, 3 }, til::size{ 2, 1 } });

        Log::Comment(L"Run the iterator and collect the runs.");
        til::some<til::rect, 6> actual;
        for (auto run : map.runs())
        {
            actual.push_back(run);
        }

        Log::Comment(L"Verify they match what we expected.");
        VERIFY_ARE_EQUAL(expected, actual);

        Log::Comment(L"Clear the map and iterate and make sure we get no results.");
        map.reset_all();

        expected.clear();
        actual.clear();
        for (auto run : map.runs())
        {
            actual.push_back(run);
        }

        Log::Comment(L"Verify they're empty.");
        VERIFY_ARE_EQUAL(expected, actual);

        Log::Comment(L"Set point and validate runs updated.");
        const til::point setPoint{ 2, 2 };
        expected.push_back(til::rect{ 2, 2, 3, 3 });
        map.set(setPoint);

        for (auto run : map.runs())
        {
            actual.push_back(run);
        }
        VERIFY_ARE_EQUAL(expected, actual);

        Log::Comment(L"Set rectangle and validate runs updated.");
        const til::rect setRect{ setPoint, til::size{ 2, 2 } };
        expected.clear();
        expected.push_back(til::rect{ til::point{ 2, 2 }, til::size{ 2, 1 } });
        expected.push_back(til::rect{ til::point{ 2, 3 }, til::size{ 2, 1 } });
        map.set(setRect);

        actual.clear();
        for (auto run : map.runs())
        {
            actual.push_back(run);
        }
        VERIFY_ARE_EQUAL(expected, actual);

        Log::Comment(L"Set all and validate runs updated.");
        expected.clear();
        expected.push_back(til::rect{ til::point{ 0, 0 }, til::size{ 4, 1 } });
        expected.push_back(til::rect{ til::point{ 0, 1 }, til::size{ 4, 1 } });
        expected.push_back(til::rect{ til::point{ 0, 2 }, til::size{ 4, 1 } });
        expected.push_back(til::rect{ til::point{ 0, 3 }, til::size{ 4, 1 } });
        map.set_all();

        actual.clear();
        for (auto run : map.runs())
        {
            actual.push_back(run);
        }
        VERIFY_ARE_EQUAL(expected, actual);

        Log::Comment(L"Resize and validate runs updated.");
        const til::size newSize{ 3, 3 };
        expected.clear();
        expected.push_back(til::rect{ til::point{ 0, 0 }, til::size{ 3, 1 } });
        expected.push_back(til::rect{ til::point{ 0, 1 }, til::size{ 3, 1 } });
        expected.push_back(til::rect{ til::point{ 0, 2 }, til::size{ 3, 1 } });
        map.resize(newSize);

        actual.clear();
        for (auto run : map.runs())
        {
            actual.push_back(run);
        }
        VERIFY_ARE_EQUAL(expected, actual);
    }
};
