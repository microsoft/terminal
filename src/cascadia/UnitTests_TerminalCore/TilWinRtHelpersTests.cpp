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
    TEST_METHOD(TestEvent);
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

    Foo() += Bar(); // 48
    VERIFY_ARE_EQUAL(48, Foo());
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
