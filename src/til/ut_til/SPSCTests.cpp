// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"

#include <til/spsc.h>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

struct drop_indicator
{
    explicit drop_indicator(int& counter) noexcept :
        _counter(&counter) {}

    drop_indicator(const drop_indicator&) = delete;
    drop_indicator& operator=(const drop_indicator&) = delete;

    drop_indicator(drop_indicator&& other) noexcept
    {
        _counter = std::exchange(other._counter, nullptr);
    }

    drop_indicator& operator=(drop_indicator&& other) noexcept
    {
        _counter = std::exchange(other._counter, nullptr);
    }

    ~drop_indicator()
    {
        if (_counter)
        {
            ++*_counter;
        }
    }

private:
    int* _counter = nullptr;
};

template<typename T>
void drop(T&& val)
{
    auto _ = std::move(val);
}

class SPSCTests
{
    BEGIN_TEST_CLASS(SPSCTests)
        TEST_CLASS_PROPERTY(L"TestTimeout", L"0:0:10") // 10s timeout
    END_TEST_CLASS()

    TEST_METHOD(SmokeTest);
    TEST_METHOD(DropEmptyTest);
    TEST_METHOD(DropSameRevolutionTest);
    TEST_METHOD(DropDifferentRevolutionTest);
    TEST_METHOD(IntegrationTest);
};

void SPSCTests::SmokeTest()
{
    // This test mostly ensures that the API wasn't broken.

    // construction
    auto [tx, rx] = til::spsc::channel<int>(32);
    std::array<int, 3> data{};

    // move constructor
    auto tx2(std::move(tx));
    auto rx2(std::move(rx));

    // move assignment operator
    tx = std::move(tx2);
    rx = std::move(rx2);

    // push
    tx.emplace(0);
    tx.push(data.begin(), data.end());
    tx.push(til::spsc::block_initially, data.begin(), data.end());
    tx.push(til::spsc::block_forever, data.begin(), data.end());
    tx.push_n(data.begin(), data.size());
    tx.push_n(til::spsc::block_initially, data.begin(), data.size());
    tx.push_n(til::spsc::block_forever, data.begin(), data.size());

    // pop
    auto x = rx.pop();
    rx.pop_n(til::spsc::block_initially, data.begin(), data.size());
    rx.pop_n(til::spsc::block_forever, data.begin(), data.size());
}

void SPSCTests::DropEmptyTest()
{
    auto [tx, rx] = til::spsc::channel<drop_indicator>(5);
    auto counter = 0;

    for (auto i = 0; i < 5; ++i)
    {
        tx.emplace(counter);
    }
    VERIFY_ARE_EQUAL(counter, 0);

    for (auto i = 0; i < 5; ++i)
    {
        rx.pop();
    }
    VERIFY_ARE_EQUAL(counter, 5);

    for (auto i = 0; i < 3; ++i)
    {
        tx.emplace(counter);
    }
    VERIFY_ARE_EQUAL(counter, 5);

    drop(tx);
    VERIFY_ARE_EQUAL(counter, 5);

    for (auto i = 0; i < 3; ++i)
    {
        rx.pop();
    }
    VERIFY_ARE_EQUAL(counter, 8);

    drop(rx);
    VERIFY_ARE_EQUAL(counter, 8);
}

void SPSCTests::DropSameRevolutionTest()
{
    auto [tx, rx] = til::spsc::channel<drop_indicator>(5);
    auto counter = 0;

    for (auto i = 0; i < 5; ++i)
    {
        tx.emplace(counter);
    }
    VERIFY_ARE_EQUAL(counter, 0);

    drop(tx);
    VERIFY_ARE_EQUAL(counter, 0);

    for (auto i = 0; i < 3; ++i)
    {
        rx.pop();
    }
    VERIFY_ARE_EQUAL(counter, 3);

    drop(rx);
    VERIFY_ARE_EQUAL(counter, 5);
}

void SPSCTests::DropDifferentRevolutionTest()
{
    auto [tx, rx] = til::spsc::channel<drop_indicator>(5);
    auto counter = 0;

    for (auto i = 0; i < 4; ++i)
    {
        tx.emplace(counter);
    }
    VERIFY_ARE_EQUAL(counter, 0);

    for (auto i = 0; i < 3; ++i)
    {
        rx.pop();
    }
    VERIFY_ARE_EQUAL(counter, 3);

    for (auto i = 0; i < 4; ++i)
    {
        tx.emplace(counter);
    }
    VERIFY_ARE_EQUAL(counter, 3);

    // At this point we emplace()d 8 items and pop()ed 3 in a channel with a capacity of 5.
    // Both producer and consumer positions will be 3 and only differ in their revolution flag.
    // This ensures that the arc<T> destructor works even if the
    // two positions within the circular buffer are identical (modulo the capacity).

    drop(tx);
    VERIFY_ARE_EQUAL(counter, 3);

    drop(rx);
    VERIFY_ARE_EQUAL(counter, 8);
}

void SPSCTests::IntegrationTest()
{
    auto [tx, rx] = til::spsc::channel<int>(7);

    std::thread t([tx = std::move(tx)]() {
        std::array<int, 11> buffer{};
        std::ranges::generate(buffer, [v = 0]() mutable { return v++; });

        for (auto i = 0; i < 37; ++i)
        {
            tx.emplace(i);
        }
        for (auto i = 0; i < 3; ++i)
        {
            tx.push(buffer.begin(), buffer.end());
        }
    });

    std::array<int, 11> buffer{};

    for (auto i = 0; i < 3; ++i)
    {
        rx.pop_n(buffer.data(), buffer.size());
        for (auto j = 0; j < 11; ++j)
        {
            VERIFY_ARE_EQUAL(i * 11 + j, buffer[j]);
        }
    }
    for (auto i = 33; i < 37; ++i)
    {
        auto actual = rx.pop();
        VERIFY_ARE_EQUAL(i, actual);
    }
    for (auto i = 0; i < 33; ++i)
    {
        auto expected = i % 11;
        auto actual = rx.pop();
        VERIFY_ARE_EQUAL(expected, actual);
    }

    t.join();
}
