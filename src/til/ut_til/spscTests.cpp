// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"

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

class spscTests
{
    BEGIN_TEST_CLASS(spscTests)
        TEST_CLASS_PROPERTY(L"TestTimeout", L"0:0:10") // 10s timeout
    END_TEST_CLASS()

    TEST_METHOD(DropEmptyTest);
    TEST_METHOD(DropSameRevolutionTest);
    TEST_METHOD(DropDifferentRevolutionTest);
    TEST_METHOD(IntegrationTest);
};

void spscTests::DropEmptyTest()
{
    auto [tx, rx] = til::spsc::channel<drop_indicator>(5);
    int counter = 0;

    for (int i = 0; i < 5; ++i)
    {
        tx.emplace(counter);
    }
    VERIFY_ARE_EQUAL(counter, 0);

    for (int i = 0; i < 5; ++i)
    {
        rx.pop();
    }
    VERIFY_ARE_EQUAL(counter, 5);

    for (int i = 0; i < 3; ++i)
    {
        tx.emplace(counter);
    }
    VERIFY_ARE_EQUAL(counter, 5);

    drop(tx);
    VERIFY_ARE_EQUAL(counter, 5);

    for (int i = 0; i < 3; ++i)
    {
        rx.pop();
    }
    VERIFY_ARE_EQUAL(counter, 8);

    drop(rx);
    VERIFY_ARE_EQUAL(counter, 8);
}

void spscTests::DropSameRevolutionTest()
{
    auto [tx, rx] = til::spsc::channel<drop_indicator>(5);
    int counter = 0;

    for (int i = 0; i < 5; ++i)
    {
        tx.emplace(counter);
    }
    VERIFY_ARE_EQUAL(counter, 0);

    drop(tx);
    VERIFY_ARE_EQUAL(counter, 0);

    for (int i = 0; i < 3; ++i)
    {
        rx.pop();
    }
    VERIFY_ARE_EQUAL(counter, 3);

    drop(rx);
    VERIFY_ARE_EQUAL(counter, 5);
}

void spscTests::DropDifferentRevolutionTest()
{
    auto [tx, rx] = til::spsc::channel<drop_indicator>(5);
    int counter = 0;

    for (int i = 0; i < 4; ++i)
    {
        tx.emplace(counter);
    }
    VERIFY_ARE_EQUAL(counter, 0);

    for (int i = 0; i < 3; ++i)
    {
        rx.pop();
    }
    VERIFY_ARE_EQUAL(counter, 3);

    for (int i = 0; i < 4; ++i)
    {
        tx.emplace(counter);
    }
    VERIFY_ARE_EQUAL(counter, 3);

    // At this point we emplace()d 8 items and pop()ed 3 in a channel with a capacity of 5.
    // Both producer and consumer positions will be 3 and only differ in their revolution flag.
    // This ensures that the arc<T> destructor works even if

    drop(tx);
    VERIFY_ARE_EQUAL(counter, 3);

    drop(rx);
    VERIFY_ARE_EQUAL(counter, 8);
}

void spscTests::IntegrationTest()
{
    auto [tx, rx] = til::spsc::channel<int>(7);

    std::thread t([tx = std::move(tx)]() {
        std::array<int, 11> buffer{};
        std::generate(buffer.begin(), buffer.end(), [v = 0]() mutable { return v++; });

        for (int i = 0; i < 37; ++i)
        {
            tx.emplace(i);
        }
        for (int i = 0; i < 3; ++i)
        {
            tx.push(buffer.begin(), buffer.end());
        }
    });

    std::array<int, 11> buffer{};

    for (int i = 0; i < 3; ++i)
    {
        rx.pop_n(buffer.data(), buffer.size());
        for (int j = 0; j < 11; ++j)
        {
            VERIFY_ARE_EQUAL(i * 11 + j, buffer[j]);
        }
    }
    for (int i = 33; i < 37; ++i)
    {
        VERIFY_ARE_EQUAL(i, rx.pop());
    }
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 11; ++j)
        {
            VERIFY_ARE_EQUAL(j, rx.pop());
        }
    }

    t.join();
}
