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

using namespace winrt::Microsoft::Terminal::Core;
using namespace Microsoft::Terminal::Core;
using namespace Microsoft::Console::Render;
using namespace ::Microsoft::Console::Types;

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    class MockScrollRenderEngine final : public RenderEngineBase
    {
    public:
        std::optional<til::point> TriggerScrollDelta() const
        {
            return _triggerScrollDelta;
        }

        void Reset()
        {
            _triggerScrollDelta.reset();
        }

        HRESULT StartPaint() noexcept { return S_OK; }
        HRESULT EndPaint() noexcept { return S_OK; }
        HRESULT Present() noexcept { return S_OK; }
        HRESULT PrepareForTeardown(_Out_ bool* /*pForcePaint*/) noexcept { return S_OK; }
        HRESULT ScrollFrame() noexcept { return S_OK; }
        HRESULT Invalidate(const til::rect* /*psrRegion*/) noexcept { return S_OK; }
        HRESULT InvalidateCursor(const til::rect* /*psrRegion*/) noexcept { return S_OK; }
        HRESULT InvalidateSystem(const til::rect* /*prcDirtyClient*/) noexcept { return S_OK; }
        HRESULT InvalidateSelection(const std::vector<til::rect>& /*rectangles*/) noexcept { return S_OK; }
        HRESULT InvalidateScroll(const til::point* pcoordDelta) noexcept
        {
            _triggerScrollDelta = *pcoordDelta;
            return S_OK;
        }
        HRESULT InvalidateAll() noexcept { return S_OK; }
        HRESULT InvalidateCircling(_Out_ bool* /*pForcePaint*/) noexcept { return S_OK; }
        HRESULT PaintBackground() noexcept { return S_OK; }
        HRESULT PaintBufferLine(std::span<const Cluster> /*clusters*/, til::point /*coord*/, bool /*fTrimLeft*/, bool /*lineWrapped*/) noexcept { return S_OK; }
        HRESULT PaintBufferGridLines(GridLineSet /*lines*/, COLORREF /*color*/, size_t /*cchLine*/, til::point /*coordTarget*/) noexcept { return S_OK; }
        HRESULT PaintSelection(const til::rect& /*rect*/) noexcept { return S_OK; }
        HRESULT PaintCursor(const CursorOptions& /*options*/) noexcept { return S_OK; }
        HRESULT UpdateDrawingBrushes(const TextAttribute& /*textAttributes*/, const RenderSettings& /*renderSettings*/, gsl::not_null<IRenderData*> /*pData*/, bool /*usingSoftFont*/, bool /*isSettingDefaultBrushes*/) noexcept { return S_OK; }
        HRESULT UpdateFont(const FontInfoDesired& /*FontInfoDesired*/, _Out_ FontInfo& /*FontInfo*/) noexcept { return S_OK; }
        HRESULT UpdateDpi(int /*iDpi*/) noexcept { return S_OK; }
        HRESULT UpdateViewport(const til::inclusive_rect& /*srNewViewport*/) noexcept { return S_OK; }
        HRESULT GetProposedFont(const FontInfoDesired& /*FontInfoDesired*/, _Out_ FontInfo& /*FontInfo*/, int /*iDpi*/) noexcept { return S_OK; }
        HRESULT GetDirtyArea(std::span<const til::rect>& /*area*/) noexcept { return S_OK; }
        HRESULT GetFontSize(_Out_ til::size* /*pFontSize*/) noexcept { return S_OK; }
        HRESULT IsGlyphWideByFont(std::wstring_view /*glyph*/, _Out_ bool* /*pResult*/) noexcept { return S_OK; }

    protected:
        HRESULT _DoUpdateTitle(const std::wstring_view /*newTitle*/) noexcept { return S_OK; }

    private:
        std::optional<til::point> _triggerScrollDelta;
    };

    struct ScrollBarNotification
    {
        int ViewportTop;
        int ViewportHeight;
        int BufferHeight;
    };
}

namespace TerminalCoreUnitTests
{
    class ScrollTest;
};
using namespace TerminalCoreUnitTests;

class TerminalCoreUnitTests::ScrollTest final
{
    // !!! DANGER: Many tests in this class expect the Terminal buffer
    // to be 80x32. If you change these, you'll probably inadvertently break a
    // bunch of tests !!!
    static const til::CoordType TerminalViewWidth = 80;
    static const til::CoordType TerminalViewHeight = 32;
    // For TestNotifyScrolling, it's important that this value is ~=9000.
    // Something smaller like 100 won't cause the test to fail.
    static const til::CoordType TerminalHistoryLength = 9001;

    TEST_CLASS(ScrollTest);

    TEST_METHOD(TestNotifyScrolling);

    TEST_METHOD_SETUP(MethodSetup)
    {
        _term = std::make_unique<::Microsoft::Terminal::Core::Terminal>();

        _scrollBarNotification = std::make_shared<std::optional<ScrollBarNotification>>();
        _term->SetScrollPositionChangedCallback([scrollBarNotification = _scrollBarNotification](const int top, const int height, const int bottom) {
            ScrollBarNotification tmp;
            tmp.ViewportTop = top;
            tmp.ViewportHeight = height;
            tmp.BufferHeight = bottom;
            *scrollBarNotification = { tmp };
        });

        _renderEngine = std::make_unique<MockScrollRenderEngine>();
        _renderer = std::make_unique<DummyRenderer>(_term.get());
        _renderer->AddRenderEngine(_renderEngine.get());
        _term->Create({ TerminalViewWidth, TerminalViewHeight }, TerminalHistoryLength, *_renderer);
        return true;
    }

    TEST_METHOD_CLEANUP(MethodCleanup)
    {
        _term = nullptr;
        return true;
    }

private:
    std::unique_ptr<Terminal> _term;
    std::unique_ptr<MockScrollRenderEngine> _renderEngine;
    std::unique_ptr<DummyRenderer> _renderer;
    std::shared_ptr<std::optional<ScrollBarNotification>> _scrollBarNotification;
};

void ScrollTest::TestNotifyScrolling()
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

    auto& termTb = *_term->_mainBuffer;
    auto& termSm = *_term->_stateMachine;

    const auto totalBufferSize = termTb.GetSize().Height();

    auto currentRow = 0;

    // We're outputting like 18000 lines of text here, so emitting 18000*4 lines
    // of output to the console is actually quite unnecessary
    WEX::TestExecution::SetVerifyOutput settings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    // Emit a bunch of newlines to test scrolling.
    for (; currentRow < totalBufferSize * 2; currentRow++)
    {
        *_scrollBarNotification = std::nullopt;
        _renderEngine->Reset();

        termSm.ProcessString(L"X\r\n");

        // When we're on TerminalViewHeight-1, we'll emit the newline that
        // causes the first scroll event
        auto scrolled = currentRow >= TerminalViewHeight - 1;

        // When we circle the buffer, the scroll bar's position does not
        // change.
        auto circledBuffer = currentRow >= totalBufferSize - 1;
        auto expectScrollBarNotification = scrolled && !circledBuffer;

        if (expectScrollBarNotification)
        {
            VERIFY_IS_TRUE(_scrollBarNotification->has_value(),
                           fmt::format(L"Expected a 'scroll bar position changed' notification for row {}", currentRow).c_str());
        }
        else
        {
            VERIFY_IS_FALSE(_scrollBarNotification->has_value(),
                            fmt::format(L"Expected to not see a 'scroll bar position changed' notification for row {}", currentRow).c_str());
        }

        // If we scrolled but it circled the buffer, then the terminal will
        // call `TriggerScroll` with a delta to tell the renderer about it.
        if (scrolled && circledBuffer)
        {
            VERIFY_IS_TRUE(_renderEngine->TriggerScrollDelta().has_value(),
                           fmt::format(L"Expected a 'trigger scroll' notification in Render Engine for row {}", currentRow).c_str());

            til::point expectedDelta;
            expectedDelta.x = 0;
            expectedDelta.y = -1;
            VERIFY_ARE_EQUAL(expectedDelta, _renderEngine->TriggerScrollDelta().value(), fmt::format(L"Wrong value in 'trigger scroll' notification in Render Engine for row {}", currentRow).c_str());
        }
        else
        {
            VERIFY_IS_FALSE(_renderEngine->TriggerScrollDelta().has_value(),
                            fmt::format(L"Expected to not see a 'trigger scroll' notification in Render Engine for row {}", currentRow).c_str());
        }

        if (_scrollBarNotification->has_value())
        {
            const auto tmp = _scrollBarNotification->value();

            const auto expectedTop = std::clamp<int>(currentRow - TerminalViewHeight + 2,
                                                     0,
                                                     TerminalHistoryLength);
            const int expectedHeight = TerminalViewHeight;
            const auto expectedBottom = expectedTop + TerminalViewHeight;
            if ((tmp.ViewportTop != expectedTop) ||
                (tmp.ViewportHeight != expectedHeight) ||
                (tmp.BufferHeight != expectedBottom))
            {
                Log::Comment(NoThrowString().Format(L"Expected viewport values did not match on line %d", currentRow));
            }
            VERIFY_ARE_EQUAL(tmp.ViewportTop, expectedTop);
            VERIFY_ARE_EQUAL(tmp.ViewportHeight, expectedHeight);
            VERIFY_ARE_EQUAL(tmp.BufferHeight, expectedBottom);
        }
    }
}
