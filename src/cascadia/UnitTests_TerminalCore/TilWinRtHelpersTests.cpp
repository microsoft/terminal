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

    TEST_METHOD(TestTypedEvent);
};

void TilWinRtHelpersTests::TestPropertySimple()
{
    til::property<int> Foo;
    til::property<int> Bar(11);

    VERIFY_ARE_EQUAL(11, Bar());

    Foo(42);
    VERIFY_ARE_EQUAL(42, Foo());

    Foo(Foo() - 5); // 37
    VERIFY_ARE_EQUAL(37, Foo());

    Foo(Foo() + Bar()); // 48
    VERIFY_ARE_EQUAL(48, Foo());
}

void TilWinRtHelpersTests::TestPropertyHString()
{
    til::property<winrt::hstring> Foo{ L"Foo" };

    VERIFY_ARE_EQUAL(L"Foo", Foo());

    Foo(L"bar");
    VERIFY_ARE_EQUAL(L"bar", Foo());
}

void TilWinRtHelpersTests::TestTruthiness()
{
    til::property<bool> Foo{ false };
    til::property<int> Bar(0);
    til::property<winrt::hstring> EmptyString;
    til::property<winrt::hstring> FullString{ L"Full" };

    VERIFY_IS_FALSE(Foo());
    VERIFY_IS_FALSE((bool)Foo);

    VERIFY_IS_FALSE(Bar());
    VERIFY_IS_FALSE((bool)Bar);

    VERIFY_IS_FALSE((bool)EmptyString);
    VERIFY_IS_FALSE(!EmptyString().empty());

    Foo(true);
    VERIFY_IS_TRUE(Foo());
    VERIFY_IS_TRUE((bool)Foo);

    Bar(11);
    VERIFY_IS_TRUE(Bar());
    VERIFY_IS_TRUE((bool)Bar);

    VERIFY_IS_TRUE((bool)FullString);
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

    changeMe.Foo(42);
    VERIFY_ARE_EQUAL(42, changeMe.Foo());
    // noTouching.Foo = 123; // will not compile

    // None of this compiles.
    // Composed() doesn't return an l-value, it returns an _int_
    //
    // changeMe.Composed().first = 5;
    // VERIFY_ARE_EQUAL(5, changeMe.Composed().first);
    // noTouching.Composed().first = 0x0f; // will not compile

    changeMe.MyString(L"Foo");
    VERIFY_ARE_EQUAL(L"Foo", changeMe.MyString());
    // noTouching.MyString = L"Bar"; // will not compile
}
void TilWinRtHelpersTests::TestComposedConstProperties()
{
    // This is an intentionally obtuse test, to show a weird edge case you
    // should avoid.
    //
    // In this sample, `Helper` has a `property` of a raw struct
    // `InnerType`, which itself is composed of two `property`s. This is not
    // something that will actually occur in practice. In practice, the things
    // inside the `property` will be WinRT types (or primitive types), and
    // things that contain properties will THEMSELVES be WinRT types.
    //
    // But if you do it like this, you can't call
    //
    //    changeMe.Composed().first(5);
    //
    // Or any variation of that, without ~ unexpected ~ behavior. This demonstrates that.
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
    VERIFY_ARE_EQUAL(3, changeMe.Composed().first());
    VERIFY_ARE_EQUAL(2, changeMe.Composed().second());
    VERIFY_ARE_EQUAL(L"", changeMe.MyString());

    VERIFY_ARE_EQUAL(0, noTouching.Foo());
    VERIFY_ARE_EQUAL(3, noTouching.Composed().first());
    VERIFY_ARE_EQUAL(2, noTouching.Composed().second());
    VERIFY_ARE_EQUAL(L"", noTouching.MyString());

    changeMe.Foo(42);
    VERIFY_ARE_EQUAL(42, changeMe.Foo());
    // noTouching.Foo = 123; // will not compile

    // This test was authored to work through a potential foot gun.
    // If you have property::operator() return `T`, then
    //     changeMe.Composed().first = 5;
    //
    // Roughly translates to:
    //     auto copy = changeMe.Composed();
    //     copy.first(5);
    //
    // Which rather seems like a foot gun.
    changeMe.Composed().first(5);
    VERIFY_ARE_EQUAL(3, changeMe.Composed().first());

    // IN PRACTICE, this shouldn't ever occur. Composed would be a WinRT type,
    // and you'd get a ref to it, rather than a copy.

    changeMe.MyString(L"Foo");
    VERIFY_ARE_EQUAL(L"Foo", changeMe.MyString());
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
