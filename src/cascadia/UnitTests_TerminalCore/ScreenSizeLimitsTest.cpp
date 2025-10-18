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

        TEST_METHOD(ResizeIsClampedToBounds);
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
