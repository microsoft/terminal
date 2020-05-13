// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// This test class is useful for cases where we need a Terminal, a Renderer, and
// a DxEngine. Some bugs won't repro without all three actually being hooked up.
// Note however, that the DxEngine is not wired up to actually be PaintFrame'd
// in this test - it pretty heavily depends on being able to actually get a
// render target, and as we're running in a unit test, we don't have one of
// those. However, this class is good for testing how invalidation works across
// all three.

#include "precomp.h"
#include <WexTestClass.h>

#include <argb.h>
#include <DefaultSettings.h>

#include "../renderer/inc/DummyRenderTarget.hpp"
#include "../renderer/base/Renderer.hpp"
#include "../renderer/dx/DxRenderer.hpp"

#include "../cascadia/TerminalCore/Terminal.hpp"
#include "MockTermSettings.h"
#include "consoletaeftemplates.hpp"
#include "TestUtils.h"

using namespace winrt::Microsoft::Terminal::Settings;
using namespace Microsoft::Terminal::Core;
using namespace ::Microsoft::Console::Types;

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace TerminalCoreUnitTests
{
    class TerminalAndRendererTests;
};
using namespace TerminalCoreUnitTests;

class TerminalCoreUnitTests::TerminalAndRendererTests final
{
    // !!! DANGER: Many tests in this class expect the Terminal buffer
    // to be 80x32. If you change these, you'll probably inadvertently break a
    // bunch of tests !!!
    static const SHORT TerminalViewWidth = 80;
    static const SHORT TerminalViewHeight = 32;
    // For TestNotifyScrolling, it's important that this value is ~=9000.
    // Something smaller like 100 won't cause the test to fail.
    static const SHORT TerminalHistoryLength = 9001;

    TEST_CLASS(TerminalAndRendererTests);

    TEST_METHOD(TestNotifyScrolling);

    TEST_METHOD_SETUP(MethodSetup)
    {
        _term = std::make_unique<::Microsoft::Terminal::Core::Terminal>();

        // Create the renderer
        _renderer = std::make_unique<::Microsoft::Console::Render::Renderer>(_term.get(), nullptr, 0, nullptr);
        ::Microsoft::Console::Render::IRenderTarget& renderTarget = *_renderer;

        // Create the engine
        _dxEngine = std::make_unique<::Microsoft::Console::Render::DxEngine>();
        _renderer->AddRenderEngine(_dxEngine.get());

        // Initialize the renderer, engine for a default font size
        _renderer->TriggerFontChange(USER_DEFAULT_SCREEN_DPI, _desiredFont, _actualFont);
        const til::size viewportSize{ TerminalViewWidth, TerminalViewHeight };
        const til::size fontSize = _actualFont.GetSize();
        const til::size windowSize = viewportSize * fontSize;
        VERIFY_SUCCEEDED(_dxEngine->SetWindowSize(windowSize));
        const auto vp = _dxEngine->GetViewportInCharacters(Viewport::FromDimensions({ 0, 0 }, windowSize));
        VERIFY_ARE_EQUAL(viewportSize, til::size{ vp.Dimensions() });

        // Set up the Terminal, using the Renderer (which has the engine in it)
        _term->Create({ TerminalViewWidth, TerminalViewHeight }, TerminalHistoryLength, renderTarget);
        return true;
    }

    TEST_METHOD_CLEANUP(MethodCleanup)
    {
        _term = nullptr;
        return true;
    }

private:
    std::unique_ptr<Terminal> _term;
    std::unique_ptr<::Microsoft::Console::Render::Renderer> _renderer;
    std::unique_ptr<::Microsoft::Console::Render::DxEngine> _dxEngine;

    FontInfoDesired _desiredFont{ DEFAULT_FONT_FACE, 0, 10, { 0, DEFAULT_FONT_SIZE }, CP_UTF8 };
    FontInfo _actualFont{ DEFAULT_FONT_FACE, 0, 10, { 0, DEFAULT_FONT_SIZE }, CP_UTF8, false };
};

void TerminalAndRendererTests::TestNotifyScrolling()
{
    // See https://github.com/microsoft/terminal/pull/5630
    //
    // This is a test for GH#5540, in the most bizarre way. The origin of that
    // bug was that as newlines were emitted, we'd accumulate an enormous scroll
    // delta into a selection region, to the point of overflowing a SHORT. When
    // the overflow occurred, the Terminal would fail to send a NotifyScroll() to
    // the TermControl hosting it.
    //
    // For this bug to repro, we need to:
    // - Have a sufficiently large buffer, because each newline we'll accumulate
    //   a delta of (0, ~bufferHeight), so (bufferHeight^2 + bufferHeight) >
    //   SHRT_MAX
    // - Have a selection

    Log::Comment(L"Watch out - this test takes a while to run, and won't "
                 L"output anything unless in encounters an error. This is expected.");

    auto& termTb = *_term->_buffer;
    auto& termSm = *_term->_stateMachine;

    const auto totalBufferSize = termTb.GetSize().Height();

    auto currentRow = 0;
    bool gotScrollingNotification = false;

    // We're outputting like 18000 lines of text here, so emitting 18000*4 lines
    // of output to the console is actually quite unnecessary
    WEX::TestExecution::SetVerifyOutput settings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    auto verifyScrolling = [&](const int top, const int height, const int bottom) {
        const int expectedTop = std::clamp<int>(currentRow - TerminalViewHeight + 2,
                                                0,
                                                TerminalHistoryLength);

        const int expectedHeight = TerminalViewHeight;
        const int expectedBottom = expectedTop + TerminalViewHeight;
        if ((expectedTop != top) ||
            (expectedHeight != height) ||
            (expectedBottom != bottom))
        {
            Log::Comment(NoThrowString().Format(L"Expected values did not match on line %d", currentRow));
        }
        VERIFY_ARE_EQUAL(expectedTop, top);
        VERIFY_ARE_EQUAL(expectedHeight, height);
        VERIFY_ARE_EQUAL(expectedBottom, bottom);

        gotScrollingNotification = true;
    };

    // Hook up the scrolling callback
    _term->SetScrollPositionChangedCallback(verifyScrolling);

    // Create a selection - the actual bounds don't matter, we just need to have one.
    _term->SetSelectionAnchor(COORD{ 0, 0 });
    _term->SetSelectionEnd(COORD{ TerminalViewWidth - 1, 0 });
    _renderer->TriggerSelection();

    // Emit a bunch of newlines. Eventually, the accumulated scroll delta will
    // cause an overflow, and cause us to miss a NotifyScroll.
    for (; currentRow < totalBufferSize * 2; currentRow++)
    {
        gotScrollingNotification = false;

        termSm.ProcessString(L"X\r\n");

        // When we're on TerminalViewHeight-1, we'll emit the newline that
        // causes the first scroll event
        if (currentRow >= TerminalViewHeight - 1)
        {
            VERIFY_IS_TRUE(gotScrollingNotification,
                           fmt::format(L"Expected a scrolling notification for row {}", currentRow).c_str());
        }
        else
        {
            VERIFY_IS_FALSE(gotScrollingNotification,
                            fmt::format(L"Expected to not see scrolling notification for row {}", currentRow).c_str());
        }
    }
}
