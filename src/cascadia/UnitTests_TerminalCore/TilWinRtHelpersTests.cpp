// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <WexTestClass.h>

#include <DefaultSettings.h>

#include "../renderer/inc/DummyRenderer.hpp"
#include "../renderer/base/Renderer.hpp"
#include "../renderer/dx/DxRenderer.hpp"

#include "../cascadia/TerminalCore/Terminal.hpp"
#include "MockTermSettings.h"
#include "consoletaeftemplates.hpp"
#include "../../inc/TestUtils.h"

#include <til/winrt.h>

using namespace winrt::Microsoft::Terminal::Core;
using namespace Microsoft::Terminal::Core;
using namespace Microsoft::Console::Render;
using namespace ::Microsoft::Console::Types;

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace TerminalCoreUnitTests
{
    class TilWinRtHelpersTests;
};
using namespace TerminalCoreUnitTests;

class TerminalCoreUnitTests::TilWinRtHelpersTests final
{
    TEST_CLASS(TilWinRtHelpersTests);
    TEST_METHOD(TestPropertySimple);
    TEST_METHOD(TestPropertyHString);
    TEST_METHOD(TestTruthiness);
    TEST_METHOD(TestSimpleConstProperties);
    TEST_METHOD(TestComposedConstProperties);

    TEST_METHOD(TestEvent);
    TEST_METHOD(TestForwardedEvent);

    TEST_METHOD(TestTypedEvent);
};

void TilWinRtHelpersTests::TestPropertySimple()
{
    til::property<int> Foo;
    til::property<int> Bar(11);

    VERIFY_ARE_EQUAL(11, Bar());

    Foo(42);
    VERIFY_ARE_EQUAL(42, Foo());

    Foo = Foo() - 5; // 37
    VERIFY_ARE_EQUAL(37, Foo());

    Foo = Foo() + Bar(); // 48
    VERIFY_ARE_EQUAL(48, Foo());
}

void TilWinRtHelpersTests::TestPropertyHString()
{
    til::property<winrt::hstring> Foo{ L"Foo" };

    VERIFY_ARE_EQUAL(L"Foo", Foo());

    Foo = L"bar";
    VERIFY_ARE_EQUAL(L"bar", Foo());
}

void TilWinRtHelpersTests::TestTruthiness()
{
    til::property<bool> Foo{ false };
    til::property<int> Bar(0);
    til::property<winrt::hstring> EmptyString;
    til::property<winrt::hstring> FullString{ L"Full" };

    VERIFY_IS_FALSE(Foo());
    VERIFY_IS_FALSE(Foo);

    VERIFY_IS_FALSE(Bar());
    VERIFY_IS_FALSE(Bar);

    VERIFY_IS_FALSE(EmptyString);
    VERIFY_IS_FALSE(!EmptyString().empty());

    Foo = true;
    VERIFY_IS_TRUE(Foo());
    VERIFY_IS_TRUE(Foo);

    Bar = 11;
    VERIFY_IS_TRUE(Bar());
    VERIFY_IS_TRUE(Bar);

    VERIFY_IS_TRUE(FullString);
    VERIFY_IS_TRUE(!FullString().empty());
}

void TilWinRtHelpersTests::TestSimpleConstProperties()
{
    struct InnerType
    {
        int first{ 1 };
        int second{ 2 };
    };

    struct Helper
    {
        til::property<int> Foo{ 0 };
        til::property<struct InnerType> Composed;
        til::property<winrt::hstring> MyString;
    };

    struct Helper changeMe;
    const struct Helper noTouching;

    VERIFY_ARE_EQUAL(0, changeMe.Foo());
    VERIFY_ARE_EQUAL(1, changeMe.Composed().first);
    VERIFY_ARE_EQUAL(2, changeMe.Composed().second);
    VERIFY_ARE_EQUAL(L"", changeMe.MyString());

    VERIFY_ARE_EQUAL(0, noTouching.Foo());
    VERIFY_ARE_EQUAL(1, noTouching.Composed().first);
    VERIFY_ARE_EQUAL(2, noTouching.Composed().second);
    VERIFY_ARE_EQUAL(L"", noTouching.MyString());

    changeMe.Foo = 42;
    VERIFY_ARE_EQUAL(42, changeMe.Foo());
    // noTouching.Foo = 123; // will not compile

    // None of this compiles.
    // Composed() doesn't return an l-value, it returns an _int_
    //
    // changeMe.Composed().first = 5;
    // VERIFY_ARE_EQUAL(5, changeMe.Composed().first);
    // noTouching.Composed().first = 0x0f; // will not compile

    changeMe.MyString = L"Foo";
    VERIFY_ARE_EQUAL(L"Foo", changeMe.MyString());
    // noTouching.MyString = L"Bar"; // will not compile
}
void TilWinRtHelpersTests::TestComposedConstProperties()
{
    struct InnerType
    {
        til::property<int> first{ 3 };
        til::property<int> second{ 2 };
    };

    struct Helper
    {
        til::property<int> Foo{ 0 };
        til::property<struct InnerType> Composed;
        til::property<winrt::hstring> MyString;
    };

    struct Helper changeMe;
    const struct Helper noTouching;

    VERIFY_ARE_EQUAL(0, changeMe.Foo());
    VERIFY_ARE_EQUAL(3, changeMe.Composed().first);
    VERIFY_ARE_EQUAL(2, changeMe.Composed().second);
    VERIFY_ARE_EQUAL(L"", changeMe.MyString());

    VERIFY_ARE_EQUAL(0, noTouching.Foo());
    VERIFY_ARE_EQUAL(3, noTouching.Composed().first);
    VERIFY_ARE_EQUAL(2, noTouching.Composed().second);
    VERIFY_ARE_EQUAL(L"", noTouching.MyString());

    changeMe.Foo = 42;
    VERIFY_ARE_EQUAL(42, changeMe.Foo());
    // noTouching.Foo = 123; // will not compile

    // THIS IS A HUGE FOOTGUN
    changeMe.Composed().first = 5;
    VERIFY_ARE_NOT_EQUAL(5, changeMe.Composed().first());
    // This roughly translates to:
    //     auto copy = changeMe.Composed();
    //     copy.first(5);
    //     VERIFY_ARE_EQUAL(5, changeMe.Composed().first);
    // (same with changeMe.Composed().first(5))

    // This is okay, because technically, Composed itself isn't const. Only noTouching is.
    noTouching.Composed().first = 0x0f;

    changeMe.MyString = L"Foo";
    VERIFY_ARE_EQUAL(L"Foo", changeMe.MyString());
    // noTouching.MyString = L"Bar"; // will not compile
}

void TilWinRtHelpersTests::TestEvent()
{
    bool handledOne = false;
    bool handledTwo = false;
    auto handler = [&](const int& v) -> void {
        VERIFY_ARE_EQUAL(42, v);
        handledOne = true;
    };

    til::event<winrt::delegate<void(int)>> MyEvent;
    MyEvent(handler);
    MyEvent([&](int) { handledTwo = true; });
    MyEvent.raise(42);
    VERIFY_ARE_EQUAL(true, handledOne);
    VERIFY_ARE_EQUAL(true, handledTwo);
}

void TilWinRtHelpersTests::TestForwardedEvent()
{
    using callback = winrt::delegate<void(int)>;

    struct Helper
    {
        til::event<callback> MyEvent;
    } helper;

    int handledOne = 0;
    int handledTwo = 0;

    auto handler = [&](const int& v) -> void {
        VERIFY_ARE_EQUAL(42, v);
        handledOne++;
    };

    helper.MyEvent(handler);

    til::forwarded_event<callback> ForwardedEvent{ helper.MyEvent };

    ForwardedEvent([&](int) { handledTwo++; });

    ForwardedEvent.raise(42);

    VERIFY_ARE_EQUAL(1, handledOne);
    VERIFY_ARE_EQUAL(1, handledTwo);

    helper.MyEvent.raise(42);

    VERIFY_ARE_EQUAL(2, handledOne);
    VERIFY_ARE_EQUAL(2, handledTwo);

    til::forwarded_event<callback> LayersOnLayers{ ForwardedEvent };
    LayersOnLayers.raise(42);

    VERIFY_ARE_EQUAL(3, handledOne);
    VERIFY_ARE_EQUAL(3, handledTwo);
}

void TilWinRtHelpersTests::TestTypedEvent()
{
    bool handledOne = false;
    bool handledTwo = false;

    auto handler = [&](const winrt::hstring sender, const int& v) -> void {
        VERIFY_ARE_EQUAL(L"sure", sender);
        VERIFY_ARE_EQUAL(42, v);
        handledOne = true;
    };

    til::typed_event<winrt::hstring, int> MyEvent;
    MyEvent(handler);
    MyEvent([&](winrt::hstring, int) { handledTwo = true; });
    MyEvent.raise(L"sure", 42);
    VERIFY_ARE_EQUAL(true, handledOne);
    VERIFY_ARE_EQUAL(true, handledTwo);
}
