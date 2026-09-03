// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <WexTestClass.h>

#include "../cascadia/TerminalCore/Terminal.hpp"
#include "MockTermSettings.h"
#include "../renderer/inc/DummyRenderer.hpp"
#include "consoletaeftemplates.hpp"

using namespace winrt::Microsoft::Terminal::Core;
using namespace Microsoft::Terminal::Core;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

namespace TerminalCoreUnitTests
{
#define WCS(x) WCSHELPER(x)
#define WCSHELPER(x) L## #x

    class ScreenSizeLimitsTest
    {
        TEST_CLASS(ScreenSizeLimitsTest);

        TEST_METHOD(ScreenWidthAndHeightAreClampedToBounds);
        TEST_METHOD(ScrollbackHistorySizeIsClampedToBounds);
        TEST_METHOD(InfiniteHistoryGrowsBeyondLegacyLimit);
        TEST_METHOD(InfiniteHistoryDoesNotGrowAlternateBuffer);
        TEST_METHOD(InfiniteHistoryPreservesScrolledViewport);
        TEST_METHOD(InfiniteHistoryClearShrinksAndRegrows);

        TEST_METHOD(ResizeIsClampedToBounds);
        TEST_METHOD(InfiniteHistorySurvivesReflow);
    };
}

using namespace TerminalCoreUnitTests;

void ScreenSizeLimitsTest::ScreenWidthAndHeightAreClampedToBounds()
{
    // Negative values for initial visible row count or column count
    // are clamped to 1. Too-large positive values are clamped to SHRT_MAX.
    auto negativeColumnsSettings = winrt::make<MockTermSettings>(10000, 9999999, -1234);
    Terminal negativeColumnsTerminal{ Terminal::TestDummyMarker{} };
    DummyRenderer renderer{ &negativeColumnsTerminal };
    negativeColumnsTerminal.CreateFromSettings(negativeColumnsSettings, renderer);
    auto actualDimensions = negativeColumnsTerminal.GetViewport().Dimensions();
    VERIFY_ARE_EQUAL(actualDimensions.height, SHRT_MAX, L"Row count clamped to SHRT_MAX == " WCS(SHRT_MAX));
    VERIFY_ARE_EQUAL(actualDimensions.width, 1, L"Column count clamped to 1");

    // Zero values are clamped to 1 as well.
    auto zeroRowsSettings = winrt::make<MockTermSettings>(10000, 0, 9999999);
    Terminal zeroRowsTerminal{ Terminal::TestDummyMarker{} };
    zeroRowsTerminal.CreateFromSettings(zeroRowsSettings, renderer);
    actualDimensions = zeroRowsTerminal.GetViewport().Dimensions();
    VERIFY_ARE_EQUAL(actualDimensions.height, 1, L"Row count clamped to 1");
    VERIFY_ARE_EQUAL(actualDimensions.width, SHRT_MAX, L"Column count clamped to SHRT_MAX == " WCS(SHRT_MAX));
}

void ScreenSizeLimitsTest::ScrollbackHistorySizeIsClampedToBounds()
{
    // What is actually clamped is the number of rows in the internal history buffer,
    // which is the *sum* of the history size plus the number of rows
    // actually visible on screen at the moment.

    static constexpr til::CoordType visibleRowCount = 100;

    // Zero history size is acceptable.
    auto noHistorySettings = winrt::make<MockTermSettings>(0, visibleRowCount, 100);
    Terminal noHistoryTerminal{ Terminal::TestDummyMarker{} };
    DummyRenderer renderer{ &noHistoryTerminal };
    noHistoryTerminal.CreateFromSettings(noHistorySettings, renderer);
    VERIFY_ARE_EQUAL(noHistoryTerminal.GetTextBuffer().TotalRowCount(), visibleRowCount, L"History size of 0 is accepted");

    // Negative history sizes other than the -1 sentinel are clamped to zero.
    auto negativeHistorySizeSettings = winrt::make<MockTermSettings>(-100, visibleRowCount, 100);
    Terminal negativeHistorySizeTerminal{ Terminal::TestDummyMarker{} };
    negativeHistorySizeTerminal.CreateFromSettings(negativeHistorySizeSettings, renderer);
    VERIFY_ARE_EQUAL(negativeHistorySizeTerminal.GetTextBuffer().TotalRowCount(), visibleRowCount, L"Negative history size is clamped to 0");

    // History size + initial visible rows == SHRT_MAX is acceptable.
    auto maxHistorySizeSettings = winrt::make<MockTermSettings>(SHRT_MAX - visibleRowCount, visibleRowCount, 100);
    Terminal maxHistorySizeTerminal{ Terminal::TestDummyMarker{} };
    maxHistorySizeTerminal.CreateFromSettings(maxHistorySizeSettings, renderer);
    VERIFY_ARE_EQUAL(maxHistorySizeTerminal.GetTextBuffer().TotalRowCount(), SHRT_MAX, L"History size == SHRT_MAX - initial row count is accepted");

    // History size + initial visible rows == SHRT_MAX + 1 will be clamped slightly.
    auto justTooBigHistorySizeSettings = winrt::make<MockTermSettings>(SHRT_MAX - visibleRowCount + 1, visibleRowCount, 100);
    Terminal justTooBigHistorySizeTerminal{ Terminal::TestDummyMarker{} };
    justTooBigHistorySizeTerminal.CreateFromSettings(justTooBigHistorySizeSettings, renderer);
    VERIFY_ARE_EQUAL(justTooBigHistorySizeTerminal.GetTextBuffer().TotalRowCount(), SHRT_MAX, L"History size == 1 + SHRT_MAX - initial row count is clamped to SHRT_MAX - initial row count");

    // Ridiculously large history sizes are also clamped.
    auto farTooBigHistorySizeSettings = winrt::make<MockTermSettings>(99999999, visibleRowCount, 100);
    Terminal farTooBigHistorySizeTerminal{ Terminal::TestDummyMarker{} };
    farTooBigHistorySizeTerminal.CreateFromSettings(farTooBigHistorySizeSettings, renderer);
    VERIFY_ARE_EQUAL(farTooBigHistorySizeTerminal.GetTextBuffer().TotalRowCount(), SHRT_MAX, L"History size that is far too large is clamped to SHRT_MAX - initial row count");
}

void ScreenSizeLimitsTest::InfiniteHistoryGrowsBeyondLegacyLimit()
{
    static constexpr til::CoordType visibleRowCount = 4;
    auto settings = winrt::make<MockTermSettings>(-1, visibleRowCount, 8);
    Terminal terminal{ Terminal::TestDummyMarker{} };
    DummyRenderer renderer{ &terminal };
    terminal.CreateFromSettings(settings, renderer);

    std::wstring output{ L"first\r\n" };
    for (til::CoordType i = 0; i < SHRT_MAX + visibleRowCount; ++i)
    {
        output.append(L"x\r\n");
    }
    terminal.Write(output);

    const auto& textBuffer = terminal.GetTextBuffer();
    VERIFY_IS_TRUE(textBuffer.TotalRowCount() > SHRT_MAX);
    VERIFY_ARE_EQUAL(std::wstring_view{ L"first" }, textBuffer.GetRowByOffset(0).GetText().substr(0, 5));
    VERIFY_ARE_EQUAL(0, textBuffer.GetFirstRowIndex(), L"Unlimited history must append instead of rotating");
}

void ScreenSizeLimitsTest::InfiniteHistoryDoesNotGrowAlternateBuffer()
{
    auto settings = winrt::make<MockTermSettings>(-1, 4, 8);
    Terminal terminal{ Terminal::TestDummyMarker{} };
    DummyRenderer renderer{ &terminal };
    terminal.CreateFromSettings(settings, renderer);

    terminal.Write(L"main\r\nmain\r\nmain\r\nmain\r\nmain\r\n");
    const auto mainBufferHeight = terminal.GetTextBuffer().TotalRowCount();
    VERIFY_IS_TRUE(mainBufferHeight > 4);

    terminal.Write(L"\x1b[?1049h");
    VERIFY_IS_FALSE(terminal.GetTextBuffer().IsGrowable());
    for (auto i = 0; i < 100; ++i)
    {
        terminal.Write(L"alt\r\n");
    }
    VERIFY_ARE_EQUAL(4, terminal.GetTextBuffer().TotalRowCount());

    terminal.Write(L"\x1b[?1049l");
    VERIFY_IS_TRUE(terminal.GetTextBuffer().IsGrowable());
    VERIFY_ARE_EQUAL(mainBufferHeight, terminal.GetTextBuffer().TotalRowCount());
    VERIFY_ARE_EQUAL(std::wstring_view{ L"main" }, terminal.GetTextBuffer().GetRowByOffset(0).GetText().substr(0, 4));
}

void ScreenSizeLimitsTest::InfiniteHistoryPreservesScrolledViewport()
{
    auto settings = winrt::make<MockTermSettings>(-1, 4, 8);
    Terminal terminal{ Terminal::TestDummyMarker{} };
    DummyRenderer renderer{ &terminal };
    terminal.CreateFromSettings(settings, renderer);

    terminal.Write(L"anchor\r\n1\r\n2\r\n3\r\n4\r\n5\r\n6\r\n7\r\n");
    terminal.UserScrollViewport(0);
    VERIFY_ARE_EQUAL(0, terminal.GetViewport().Top());

    terminal.Write(L"8\r\n9\r\n10\r\n11\r\n12\r\n");

    VERIFY_ARE_EQUAL(0, terminal.GetViewport().Top());
    VERIFY_ARE_EQUAL(std::wstring_view{ L"anchor" }, terminal.GetTextBuffer().GetRowByOffset(0).GetText().substr(0, 6));
}

void ScreenSizeLimitsTest::InfiniteHistoryClearShrinksAndRegrows()
{
    auto settings = winrt::make<MockTermSettings>(-1, 4, 8);
    Terminal terminal{ Terminal::TestDummyMarker{} };
    DummyRenderer renderer{ &terminal };
    terminal.CreateFromSettings(settings, renderer);

    terminal.Write(L"0\r\n1\r\n2\r\n3\r\n4\r\n5\r\n6\r\n7\r\n");
    VERIFY_IS_TRUE(terminal.GetTextBuffer().TotalRowCount() > 4);

    terminal.Write(L"\x1b[3J");
    VERIFY_ARE_EQUAL(4, terminal.GetTextBuffer().TotalRowCount());
    VERIFY_ARE_EQUAL(0, terminal.GetTextBuffer().GetFirstRowIndex());
    VERIFY_ARE_EQUAL(0, terminal.GetViewport().Top());

    terminal.Write(L"8\r\n9\r\n10\r\n11\r\n12\r\n");
    VERIFY_IS_TRUE(terminal.GetTextBuffer().TotalRowCount() > 4);
}

void ScreenSizeLimitsTest::ResizeIsClampedToBounds()
{
    // What is actually clamped is the number of rows in the internal history buffer,
    // which is the *sum* of the history size plus the number of rows
    // actually visible on screen at the moment.
    //
    // This is a test for GH#2630, GH#2815.

    static constexpr til::CoordType initialVisibleColCount = 50;
    static constexpr til::CoordType initialVisibleRowCount = 50;
    const auto historySize = SHRT_MAX - (initialVisibleRowCount * 2);

    Log::Comment(L"Watch out - this test takes a while on debug, because "
                 L"ResizeWithReflow takes a while on debug. This is expected.");

    auto settings = winrt::make<MockTermSettings>(historySize, initialVisibleRowCount, initialVisibleColCount);
    Log::Comment(L"First create a terminal with fewer than SHRT_MAX lines");
    Terminal terminal{ Terminal::TestDummyMarker{} };
    DummyRenderer renderer{ &terminal };
    terminal.CreateFromSettings(settings, renderer);
    VERIFY_ARE_EQUAL(terminal.GetTextBuffer().TotalRowCount(), historySize + initialVisibleRowCount);

    Log::Comment(L"Resize the terminal to have exactly SHRT_MAX lines");
    VERIFY_SUCCEEDED(terminal.UserResize({ initialVisibleColCount, initialVisibleRowCount * 2 }));

    VERIFY_ARE_EQUAL(terminal.GetTextBuffer().TotalRowCount(), SHRT_MAX);

    Log::Comment(L"Resize the terminal to have MORE than SHRT_MAX lines - we should clamp to SHRT_MAX");
    VERIFY_SUCCEEDED(terminal.UserResize({ initialVisibleColCount, initialVisibleRowCount * 3 }));
    VERIFY_ARE_EQUAL(terminal.GetTextBuffer().TotalRowCount(), SHRT_MAX);

    Log::Comment(L"Resize back down to the original size");
    VERIFY_SUCCEEDED(terminal.UserResize({ initialVisibleColCount, initialVisibleRowCount }));
    VERIFY_ARE_EQUAL(terminal.GetTextBuffer().TotalRowCount(), historySize + initialVisibleRowCount);
}

void ScreenSizeLimitsTest::InfiniteHistorySurvivesReflow()
{
    auto settings = winrt::make<MockTermSettings>(-1, 4, 16);
    Terminal terminal{ Terminal::TestDummyMarker{} };
    DummyRenderer renderer{ &terminal };
    terminal.CreateFromSettings(settings, renderer);

    terminal.Write(L"first-line\r\n");
    for (auto i = 0; i < 100; ++i)
    {
        terminal.Write(L"history-line\r\n");
    }

    const auto heightBeforeResize = terminal.GetTextBuffer().TotalRowCount();
    VERIFY_SUCCEEDED(terminal.UserResize({ 8, 4 }));

    const auto& textBuffer = terminal.GetTextBuffer();
    VERIFY_IS_TRUE(textBuffer.TotalRowCount() > heightBeforeResize);
    VERIFY_ARE_EQUAL(std::wstring_view{ L"first-li" }, textBuffer.GetRowByOffset(0).GetText().substr(0, 8));
    VERIFY_ARE_EQUAL(0, textBuffer.GetFirstRowIndex());

    const auto heightAfterResize = textBuffer.TotalRowCount();
    terminal.Write(L"after-resize\r\n");
    VERIFY_IS_TRUE(terminal.GetTextBuffer().TotalRowCount() > heightAfterResize);
}
