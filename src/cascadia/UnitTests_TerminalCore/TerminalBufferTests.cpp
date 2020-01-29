// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include <WexTestClass.h>

#include "../renderer/inc/DummyRenderTarget.hpp"
#include "../cascadia/TerminalCore/Terminal.hpp"
#include "MockTermSettings.h"
#include "consoletaeftemplates.hpp"
#include "TestUtils.h"

using namespace winrt::Microsoft::Terminal::Settings;
using namespace Microsoft::Terminal::Core;

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace TerminalCoreUnitTests
{
    class TerminalBufferTests;
};
using namespace TerminalCoreUnitTests;

class TerminalCoreUnitTests::TerminalBufferTests final
{
    TEST_CLASS(TerminalBufferTests);

    TEST_METHOD(TestSimpleBufferWriting);

    TEST_METHOD_SETUP(MethodSetup)
    {
        // STEP 1: Set up the Terminal
        term = std::make_unique<Terminal>();
        term->Create({ 80, 32 }, 100, emptyRT);
        return true;
    }

    TEST_METHOD_CLEANUP(MethodCleanup)
    {
        term = nullptr;
        return true;
    }

private:
    DummyRenderTarget emptyRT;
    std::unique_ptr<Terminal> term;
};

void TerminalBufferTests::TestSimpleBufferWriting()
{
    auto& termTb = *term->_buffer;
    auto& termSm = *term->_stateMachine;
    const auto initialView = term->GetViewport();

    VERIFY_ARE_EQUAL(0, initialView.Top());
    VERIFY_ARE_EQUAL(32, initialView.BottomExclusive());

    termSm.ProcessString(L"Hello World");

    const auto secondView = term->GetViewport();

    VERIFY_ARE_EQUAL(0, secondView.Top());
    VERIFY_ARE_EQUAL(32, secondView.BottomExclusive());

    TestUtils::VerifyExpectedString(termTb, L"Hello World", { 0, 0 });
}
