// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "til/rle.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

class RunLengthEncodingTests
{
    TEST_CLASS(RunLengthEncodingTests);

    TEST_METHOD(ConstructDefaultLength)
    {
        til::rle<unsigned int> rle(86, 9);
        auto foo = rle.begin();
    }

    TEST_METHOD(ConstructVerySmall)
    {
        const til::rle<unsigned short, unsigned char> rle(12, 37);
    }

    TEST_METHOD(Size)
    {
        const til::rle<unsigned short> rle(19, 12);
        VERIFY_ARE_EQUAL(19, rle.size());
    }

    TEST_METHOD(AtPos)
    {
        til::rle<int> rle(10, 10);
        rle.insert(3, 0, 4);
        rle.insert(7, 4, 2);
        rle.insert(11, 6, 3);
        rle.insert(4, 9, 1);

        VERIFY_ARE_EQUAL(3, rle.at(0));
        VERIFY_ARE_EQUAL(3, rle.at(1));
        VERIFY_ARE_EQUAL(3, rle.at(2));
        VERIFY_ARE_EQUAL(3, rle.at(3));
        VERIFY_ARE_EQUAL(7, rle.at(4));
        VERIFY_ARE_EQUAL(7, rle.at(5));
        VERIFY_ARE_EQUAL(11, rle.at(6));
        VERIFY_ARE_EQUAL(11, rle.at(7));
        VERIFY_ARE_EQUAL(11, rle.at(8));
        VERIFY_ARE_EQUAL(4, rle.at(9));
    }

    TEST_METHOD(AtPosApplies)
    {
        til::rle<int> rle(10, 10);
        rle.insert(3, 0, 4);
        rle.insert(7, 4, 2);
        rle.insert(11, 6, 3);
        rle.insert(4, 9, 1);

        size_t appliesExpected = 4;
        size_t applies = 0;
        VERIFY_ARE_EQUAL(3, rle.at(0, applies));
        VERIFY_ARE_EQUAL(appliesExpected, applies);
        --appliesExpected;
        VERIFY_ARE_EQUAL(3, rle.at(1, applies));
        VERIFY_ARE_EQUAL(appliesExpected, applies);
        --appliesExpected;
        VERIFY_ARE_EQUAL(3, rle.at(2, applies));
        VERIFY_ARE_EQUAL(appliesExpected, applies);
        --appliesExpected;
        VERIFY_ARE_EQUAL(3, rle.at(3, applies));
        VERIFY_ARE_EQUAL(appliesExpected, applies);
        appliesExpected = 2;
        VERIFY_ARE_EQUAL(7, rle.at(4, applies));
        VERIFY_ARE_EQUAL(appliesExpected, applies);
        --appliesExpected;
        VERIFY_ARE_EQUAL(7, rle.at(5, applies));
        VERIFY_ARE_EQUAL(appliesExpected, applies);
        appliesExpected = 3;
        VERIFY_ARE_EQUAL(11, rle.at(6, applies));
        VERIFY_ARE_EQUAL(appliesExpected, applies);
        --appliesExpected;
        VERIFY_ARE_EQUAL(11, rle.at(7, applies));
        VERIFY_ARE_EQUAL(appliesExpected, applies);
        --appliesExpected;
        VERIFY_ARE_EQUAL(11, rle.at(8, applies));
        VERIFY_ARE_EQUAL(appliesExpected, applies);
        appliesExpected = 1;
        VERIFY_ARE_EQUAL(4, rle.at(9, applies));
        VERIFY_ARE_EQUAL(appliesExpected, applies);
    }

    TEST_METHOD(Substr)
    {
        til::rle<int> rle(10, 10);
        rle.insert(3, 0, 4);
        rle.insert(7, 4, 2);
        rle.insert(11, 6, 3);
        rle.insert(4, 9, 1);

        // 3 3 3 3 7 7 11 11 11 4

        Log::Comment(L"1.) Nothing substring should match original.");
        {
            til::rle<int> expected(10, 10);
            expected = rle;
            // 3 3 3 3 7 7 11 11 11 4

            const auto actual = rle.substr();
            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"2.) Offset substring to implicit end.");
        {
            til::rle<int> expected(7, 10);
            expected.insert(3, 0, 1);
            expected.insert(7, 1, 2);
            expected.insert(11, 3, 3);
            expected.insert(4, 6, 1);

            // 3 7 7 11 11 11 4

            const auto actual = rle.substr(3);
            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"3.) Substring cutting out middle bit.");
        {
            til::rle<int> expected(4, 4);
            expected.insert(7, 0, 1);
            expected.insert(11, 1, 3);

            // 7 11 11 11

            const auto actual = rle.substr(5, 4);
            VERIFY_ARE_EQUAL(expected, actual);
        }
    }

    TEST_METHOD(Replace)
    {
        til::rle<int> actual(20, 10);
        actual.insert(3, 0, 4);
        actual.insert(7, 4, 2);
        actual.insert(11, 6, 3);
        actual.insert(4, 9, 1);
        actual.insert(7, 10, 5);
        actual.insert(11, 15, 2);
        actual.insert(9, 17, 3);

        actual.replace(7, 49);
        actual.replace(9, 81);
        actual.replace(3, 9);

        til::rle<int> expected(20, 10);
        expected.insert(9, 0, 4);
        expected.insert(49, 4, 2);
        expected.insert(11, 6, 3);
        expected.insert(4, 9, 1);
        expected.insert(49, 10, 5);
        expected.insert(11, 15, 2);
        expected.insert(81, 17, 3);
    }

    TEST_METHOD(ResizeShrink)
    {
        til::rle<int> actual(10, 10);
        actual.insert(3, 0, 4);
        actual.insert(7, 4, 2);
        actual.insert(11, 6, 3);
        actual.insert(4, 9, 1);

        // 3 3 3 3 7 7 11 11 11 4
        // 3 for 4, 7 for 2, 11 for 3, 4 for 1.

        til::rle<int> expected(7, 10);
        expected.insert(3, 0, 4);
        expected.insert(7, 4, 2);
        expected.insert(11, 6, 1);

        // 3 3 3 3 7 7 11
        // 3 for 4, 7 for 2, 11 for 1

        actual.resize(7);

        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(ResizeGrow)
    {
        til::rle<int> actual(10, 10);
        actual.insert(3, 0, 4);
        actual.insert(7, 4, 2);
        actual.insert(11, 6, 3);
        actual.insert(4, 9, 1);

        // 3 3 3 3 7 7 11 11 11 4
        // 3 for 4, 7 for 2, 11 for 3, 4 for 1.

        til::rle<int> expected(13, 10);
        expected.insert(3, 0, 4);
        expected.insert(7, 4, 2);
        expected.insert(11, 6, 3);
        expected.insert(4, 9, 4);

        // 3 3 3 3 7 7 11 11 11 4 4 4 4
        // 3 for 4, 7 for 2, 11 for 3, 4 for 4.

        actual.resize(13);

        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(FillAll)
    {
        til::rle<int> actual(10, 10);
        actual.insert(3, 0, 4);
        actual.insert(7, 4, 2);
        actual.insert(11, 6, 3);
        actual.insert(4, 9, 1);
        actual.fill(20);

        til::rle<int> expected(10, 20);

        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(FillFrom)
    {
        til::rle<int> actual(10, 10);
        actual.insert(3, 0, 4);
        actual.insert(7, 4, 2);
        actual.insert(11, 6, 3);
        actual.insert(4, 9, 1);
        actual.fill(20, 2);

        til::rle<int> expected(10, 20);
        expected.insert(3, 0, 2);

        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(Insert)
    {
        til::rle<int> actual(10, 10);
        actual.insert(4, 9); // insert single, implicit length

        til::rle<int> expected(10, 4);
        expected.insert(10, 0, 9); // insert multiple, say length

        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(Assign)
    {
        til::rle<int> actual(10, 10);

        // Prep initial buffer.
        // 10 10 10 10 10 10 10 10 10 10

        // Make something that can hold a span of pairs to assign in bulk.
        std::vector<std::pair<int, size_t>> items;
        items.push_back({ 400, 2 });
        items.push_back({ 20, 3 });

        // 400 400 20 20 20

        // If we assign this to the front, we expect
        // 400 400 20 20 20 10 10 10 10 10
        til::rle<int> expected(10, 10);
        expected.insert(400, 0, 2);
        expected.insert(20, 2, 3);

        actual.assign(items.cbegin(), items.cend());
        VERIFY_ARE_EQUAL(expected, actual);

        // Now try assigning it again part way in.
        // Our new expectation building on the last one if we assign at
        // index 3 would be
        // 400 400 20 400 400 20 20 20 10 10
        expected.insert(400, 3, 2);
        expected.insert(20, 5, 3);

        actual.assign(items.cbegin(), items.cend(), 3);
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(Equal)
    {
        til::rle<int> actual(10, 10);
        til::rle<int> expected(10, 10);

        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(NotEqualValue)
    {
        til::rle<int> actual(10, 9);
        til::rle<int> expected(10, 10);

        VERIFY_ARE_NOT_EQUAL(expected, actual);
    }

    TEST_METHOD(NotEqualLength)
    {
        til::rle<int> actual(5, 10);
        til::rle<int> expected(10, 10);

        VERIFY_ARE_NOT_EQUAL(expected, actual);
    }

    TEST_METHOD(IteratorIncDecOnes)
    {
        til::rle<int> rle(10, 1);
        rle.insert(2, 0, 2);
        rle.insert(3, 2, 3);
        rle.insert(4, 5, 4);

        // test array should be 2 2 3 3 3 4 4 4 4 1
        // or 2 for 2, 3 for 3, 4 for 4, 1 for 1.

        Log::Comment(L"Increment by 1s.");

        auto it = rle.begin();
        for (auto i = 0; i < 2; ++i)
        {
            VERIFY_ARE_EQUAL(2, *it);
            ++it;
        }

        for (auto i = 0; i < 3; ++i)
        {
            VERIFY_ARE_EQUAL(3, *it);
            ++it;
        }

        for (auto i = 0; i < 4; ++i)
        {
            VERIFY_ARE_EQUAL(4, *it);
            ++it;
        }

        for (auto i = 0; i < 1; ++i)
        {
            VERIFY_ARE_EQUAL(1, *it);
            ++it;
        }

        VERIFY_ARE_EQUAL(rle.end(), it);

        Log::Comment(L"Decrement by 1s.");

        for (auto i = 0; i < 1; ++i)
        {
            --it;
            VERIFY_ARE_EQUAL(1, *it);
        }

        for (auto i = 0; i < 4; ++i)
        {
            --it;
            VERIFY_ARE_EQUAL(4, *it);
        }

        for (auto i = 0; i < 3; ++i)
        {
            --it;
            VERIFY_ARE_EQUAL(3, *it);
        }

        for (auto i = 0; i < 2; ++i)
        {
            --it;
            VERIFY_ARE_EQUAL(2, *it);
        }

        VERIFY_ARE_EQUAL(rle.begin(), it);
    }

    struct TestStruct
    {
        int a;
        int b;

        [[nodiscard]] bool operator==(const TestStruct& right) const noexcept
        {
            return a == right.a && b == right.b;
        }
    };

    TEST_METHOD(ConstIteratorReference)
    {
        const TestStruct expected{ 3, 2 };

        til::rle<TestStruct> rle(5, expected);

        const TestStruct actual = *rle.cbegin();
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(ConstIteratorPointer)
    {
        const TestStruct expected{ 3, 2 };

        til::rle<TestStruct> rle(5, expected);

        const auto it = rle.cbegin();

        const TestStruct actual{ it->a, it->b };
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(ConstIteratorIncPrefix)
    {
        til::rle<int> rle(5, 2);
        rle.insert(7, 1, 1);

        // 2 7 2 2 2

        auto it = rle.cbegin();
        ++it;
        VERIFY_ARE_EQUAL(7, *it);
    }

    TEST_METHOD(ConstIteratorIncPostfix)
    {
        til::rle<int> rle(5, 2);
        rle.insert(7, 1, 1);

        // 2 7 2 2 2

        auto it = rle.cbegin();
        auto prevIt = it++;
        VERIFY_ARE_EQUAL(7, *it);
        VERIFY_ARE_EQUAL(2, *prevIt);
        VERIFY_ARE_NOT_EQUAL(it, prevIt);
    }

    TEST_METHOD(ConstIteratorDecPrefix)
    {
        til::rle<int> rle(5, 2);
        rle.insert(7, 4, 1);

        // 2 2 2 2 7

        auto it = rle.cend();
        --it;
        VERIFY_ARE_EQUAL(7, *it);
    }

    TEST_METHOD(ConstIteratorDecPostfix)
    {
        til::rle<int> rle(5, 2);
        rle.insert(7, 4, 1);

        // 2 2 2 2 7

        auto it = rle.cend();
        auto prevIt = it--;
        VERIFY_ARE_EQUAL(7, *it);
        VERIFY_ARE_EQUAL(rle.cend(), prevIt);
        VERIFY_ARE_NOT_EQUAL(it, prevIt);
    }

    TEST_METHOD(ConstIteratorPlusEquals)
    {
        til::rle<int> rle(5, 2);
        rle.insert(7, 2, 1);

        // 2 2 7 2 2

        auto it = rle.cbegin();
        it += 2;
        VERIFY_ARE_EQUAL(7, *it);
    }

    TEST_METHOD(ConstIteratorPlusOffset)
    {
        til::rle<int> rle(5, 2);
        rle.insert(7, 2, 1);

        // 2 2 7 2 2

        auto it = rle.cbegin();
        auto itAfter = it + 2;
        VERIFY_ARE_EQUAL(7, *itAfter);
        VERIFY_ARE_NOT_EQUAL(it, itAfter);
    }

    TEST_METHOD(ConstIteratorMinusEquals)
    {
        til::rle<int> rle(5, 2);
        rle.insert(7, 3, 1);

        // 2 2 2 7 2

        auto it = rle.cend();
        it -= 2;
        VERIFY_ARE_EQUAL(7, *it);
    }

    TEST_METHOD(ConstIteratorMinusOffset)
    {
        til::rle<int> rle(5, 2);
        rle.insert(7, 3, 1);
        auto it = rle.cend();
        auto itAfter = it - 2;
        VERIFY_ARE_EQUAL(7, *itAfter);
        VERIFY_ARE_NOT_EQUAL(it, itAfter);
    }

    TEST_METHOD(ConstIteratorDifference)
    {
        til::rle<int> rle(5, 2);

        const ptrdiff_t expected = 5;
        VERIFY_ARE_EQUAL(expected, rle.cend() - rle.cbegin());
        VERIFY_ARE_EQUAL(-expected, rle.cbegin() - rle.cend());
    }

    TEST_METHOD(ConstIteratorArrayOffset)
    {
        til::rle<int> rle(5, 2);
        rle.insert(7, 2, 1);
        const auto it = rle.cbegin();
        VERIFY_ARE_EQUAL(2, it[0]);
        VERIFY_ARE_EQUAL(2, it[1]);
        VERIFY_ARE_EQUAL(7, it[2]);
        VERIFY_ARE_EQUAL(2, it[3]);
        VERIFY_ARE_EQUAL(2, it[4]);
    }

    TEST_METHOD(ConstIteratorEquality)
    {
        til::rle<int> rle(5, 2);

        auto begin = rle.cbegin();
        auto end = rle.cend();
        end -= 5;
        VERIFY_IS_TRUE(begin == end);
    }

    TEST_METHOD(ConstIteratorInequality)
    {
        til::rle<int> rle(5, 2);

        auto begin = rle.cbegin();
        auto end = rle.cend();
        VERIFY_IS_TRUE(begin != end);
    }

    TEST_METHOD(ConstIteratorLessThan)
    {
        til::rle<int> rle(5, 2);

        auto begin = rle.cbegin();
        auto end = rle.cend();
        auto begin2 = end - 5;
        VERIFY_IS_TRUE(begin < end);
        VERIFY_IS_FALSE(end < begin);
        VERIFY_IS_FALSE(begin < begin2);
    }

    TEST_METHOD(ConstIteratorGreaterThan)
    {
        til::rle<int> rle(5, 2);

        auto begin = rle.cbegin();
        auto end = rle.cend();
        auto begin2 = end - 5;
        VERIFY_IS_FALSE(begin > end);
        VERIFY_IS_TRUE(end > begin);
        VERIFY_IS_FALSE(begin > begin2);
    }

    TEST_METHOD(ConstIteratorLessThanEqual)
    {
        til::rle<int> rle(5, 2);

        auto begin = rle.cbegin();
        auto end = rle.cend();
        auto begin2 = end - 5;
        VERIFY_IS_TRUE(begin <= end);
        VERIFY_IS_FALSE(end <= begin);
        VERIFY_IS_TRUE(begin <= begin2);
    }

    TEST_METHOD(ConstIteratorGreaterThanEqual)
    {
        til::rle<int> rle(5, 2);

        auto begin = rle.cbegin();
        auto end = rle.cend();
        auto begin2 = end - 5;
        VERIFY_IS_FALSE(begin >= end);
        VERIFY_IS_TRUE(end >= begin);
        VERIFY_IS_TRUE(begin >= begin2);
    }

    TEST_METHOD(ConstReverseIterate)
    {
        til::rle<int> rle(5, 5);
        rle.insert(1, 0, 1);
        rle.insert(2, 1, 1);
        rle.insert(3, 2, 1);
        rle.insert(4, 3, 1);

        // 1 2 3 4 5

        auto rit = rle.crbegin();
        for (int i = 5; i > 0; i--)
        {
            VERIFY_ARE_EQUAL(i, *rit);
            rit++;
        }

        VERIFY_ARE_EQUAL(rle.crend(), rit);
    }

    //TEST_METHOD(NonConstIterators)
    //{
    //    til::rle<int> rle(5, 5);

    //    auto iter = rle.begin();
    //    *iter++ = 1;
    //    *iter++ = 2;
    //    *iter++ = 3;
    //    *iter++ = 4;

    //    VERIFY_ARE_EQUAL(1, rle.at(0));
    //    VERIFY_ARE_EQUAL(2, rle.at(1));
    //    VERIFY_ARE_EQUAL(3, rle.at(2));
    //    VERIFY_ARE_EQUAL(4, rle.at(3));
    //    VERIFY_ARE_EQUAL(5, rle.at(4));

    //    VERIFY_ARE_EQUAL(rle.end(), iter);

    //    auto reverseIter = rle.crbegin();
    //    VERIFY_ARE_EQUAL(5, *reverseIter++);
    //    VERIFY_ARE_EQUAL(4, *reverseIter++);
    //    VERIFY_ARE_EQUAL(3, *reverseIter++);
    //    VERIFY_ARE_EQUAL(2, *reverseIter++);
    //    VERIFY_ARE_EQUAL(1, *reverseIter++);

    //    VERIFY_ARE_EQUAL(rle.crend(), reverseIter);
    //}

    TEST_METHOD(IteratorIncDecMultiple)
    {
        til::rle<int> rle(10, 1);
        rle.insert(2, 0, 2);
        rle.insert(3, 2, 3);
        rle.insert(4, 5, 4);

        // test array should be 2 2 3 3 3 4 4 4 4 1
        // or 2 for 2, 3 for 3, 4 for 4, 1 for 1.

        auto it = rle.begin();

        // 2 2 3 3 3 4 4 4 4 1
        // ^
        VERIFY_ARE_EQUAL(2, *it, L"Check we're sitting on the first of the first run.");

        // 2 2 3 3 3 4 4 4 4 1
        // ^->
        ++it;

        // 2 2 3 3 3 4 4 4 4 1
        //   ^
        VERIFY_ARE_EQUAL(2, *it, L"Move a spot into the run and ensure we're still on the same one.");

        // 2 2 3 3 3 4 4 4 4 1
        //   ^----->
        it += 3;

        // 2 2 3 3 3 4 4 4 4 1
        //         ^
        VERIFY_ARE_EQUAL(3, *it, L"Jump forward by 3 and we should still be on the second run of 3s.");

        // 2 2 3 3 3 4 4 4 4 1
        //         ^------->
        it += 4;

        // 2 2 3 3 3 4 4 4 4 1
        //                 ^
        VERIFY_ARE_EQUAL(4, *it, L"Jump forward by 4 and we should still be on the third run of 4s.");

        // 2 2 3 3 3 4 4 4 4 1
        //                 ^--->
        it += 2;

        // 2 2 3 3 3 4 4 4 4 1
        //                     ^
        VERIFY_ARE_EQUAL(rle.end(), it, L"Jump past the last run of 1 for 1 to what should be the end.");

        // 2 2 3 3 3 4 4 4 4 1
        //               <-----^
        it -= 3;

        // 2 2 3 3 3 4 4 4 4 1
        //               ^
        VERIFY_ARE_EQUAL(4, *it, L"Jump back by 3 and we should be in the middle of the 4s run.");

        // 2 2 3 3 3 4 4 4 4 1
        //       <-------^
        it -= 4;

        // 2 2 3 3 3 4 4 4 4 1
        //       ^
        VERIFY_ARE_EQUAL(3, *it, L"Jump back by 4 and we should be in the middle of the 3s run.");

        // 2 2 3 3 3 4 4 4 4 1
        // <-----^
        it -= 3;

        // 2 2 3 3 3 4 4 4 4 1
        // ^
        VERIFY_ARE_EQUAL(2, *it, L"Jump back by 3 and we should be at the beginning of the 2s run.");
        VERIFY_ARE_EQUAL(rle.begin(), it, L"And it should equal 'begin'");
    }
};
