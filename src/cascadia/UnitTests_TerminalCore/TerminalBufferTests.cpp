// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include <WexTestClass.h>

#include "../renderer/inc/DummyRenderTarget.hpp"
#include "../cascadia/TerminalCore/Terminal.hpp"
#include "MockTermSettings.h"
#include "consoletaeftemplates.hpp"

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

    TEST_METHOD(TestResizeHeight);

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

void TerminalBufferTests::TestResizeHeight()
{
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
        TEST_METHOD_PROPERTY(L"Data:dy", L"{-10, -1, 0, 1, 10}")
    END_TEST_METHOD_PROPERTIES()
    int dy;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"dy", dy), L"change in height of buffer");

    auto& termTb = *term->_buffer;
    auto& termSm = *term->_stateMachine;
    const auto initialView = term->GetViewport();

    VERIFY_ARE_EQUAL(0, initialView.Top());
    VERIFY_ARE_EQUAL(32, initialView.BottomExclusive());

    Log::Comment(NoThrowString().Format(
        L"Print 50 lines of output, which will scroll the viewport"));

    for (auto i = 0; i < 50; i++)
    {
        auto wstr = std::wstring(1, static_cast<wchar_t>(L'0' + i));
        termSm.ProcessString(wstr);
        termSm.ProcessString(L"\r\n");
    }

    const auto secondView = term->GetViewport();

    VERIFY_ARE_EQUAL(50 - initialView.Height() + 1, secondView.Top());
    VERIFY_ARE_EQUAL(50, secondView.BottomInclusive());

    auto verifyBufferContents = [&termTb]() {
        for (short row = 0; row < 50; row++)
        {
            SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);
            auto iter = termTb.GetCellDataAt({ 0, row });
            auto expectedString = std::wstring(1, static_cast<wchar_t>(L'0' + row));

            if (iter->Chars() != expectedString)
            {
                Log::Comment(NoThrowString().Format(L"row [%d] was mismatched", row));
            }
            VERIFY_ARE_EQUAL(expectedString, (iter++)->Chars());
            VERIFY_ARE_EQUAL(L" ", (iter)->Chars());
        }
    };
    verifyBufferContents();

    auto resizeResult = term->UserResize({ 80, gsl::narrow_cast<short>(32 + dy) });
    VERIFY_SUCCEEDED(resizeResult);

    const auto thirdView = term->GetViewport();

    if (dy > 0)
    {
        VERIFY_ARE_EQUAL(50 + dy - thirdView.Height() + 1, thirdView.Top());
        VERIFY_ARE_EQUAL(50 + dy, thirdView.BottomInclusive());
    }
    else if (dy < 0)
    {
        VERIFY_ARE_EQUAL(50 - thirdView.Height() + 1, thirdView.Top());
        VERIFY_ARE_EQUAL(50, thirdView.BottomInclusive());
    }

    verifyBufferContents();
}
