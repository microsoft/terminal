// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "..\..\inc\consoletaeftemplates.hpp"

#include "CommonState.hpp"

#include "globals.h"
#include "screenInfo.hpp"

#include "input.h"
#include "getset.h"
#include "_stream.h" // For WriteCharsLegacy

#include "..\interactivity\inc\ServiceLocator.hpp"
#include "..\..\inc\conattrs.hpp"
#include "..\..\types\inc\Viewport.hpp"

#include <sstream>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Interactivity;
using namespace Microsoft::Console::VirtualTerminal;

class ScreenBufferTests
{
    CommonState* m_state;

    TEST_CLASS(ScreenBufferTests);

    TEST_CLASS_SETUP(ClassSetup)
    {
        m_state = new CommonState();

        m_state->InitEvents();
        m_state->PrepareGlobalFont();
        m_state->PrepareGlobalScreenBuffer();
        m_state->PrepareGlobalInputBuffer();

        return true;
    }

    TEST_CLASS_CLEANUP(ClassCleanup)
    {
        m_state->CleanupGlobalScreenBuffer();
        m_state->CleanupGlobalFont();
        m_state->CleanupGlobalInputBuffer();

        delete m_state;

        return true;
    }

    TEST_METHOD_SETUP(MethodSetup)
    {
        // Set up some sane defaults
        CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        gci.SetDefaultForegroundColor(INVALID_COLOR);
        gci.SetDefaultBackgroundColor(INVALID_COLOR);
        gci.SetFillAttribute(0x07); // DARK_WHITE on DARK_BLACK

        m_state->PrepareNewTextBufferInfo();
        auto& currentBuffer = gci.GetActiveOutputBuffer();
        // Make sure a test hasn't left us in the alt buffer on accident
        VERIFY_IS_FALSE(currentBuffer._IsAltBuffer());
        VERIFY_SUCCEEDED(currentBuffer.SetViewportOrigin(true, { 0, 0 }, true));
        VERIFY_ARE_EQUAL(COORD({ 0, 0 }), currentBuffer.GetTextBuffer().GetCursor().GetPosition());

        return true;
    }

    TEST_METHOD_CLEANUP(MethodCleanup)
    {
        m_state->CleanupNewTextBufferInfo();

        return true;
    }

    TEST_METHOD(SingleAlternateBufferCreationTest);

    TEST_METHOD(MultipleAlternateBufferCreationTest);

    TEST_METHOD(MultipleAlternateBuffersFromMainCreationTest);

    TEST_METHOD(TestReverseLineFeed);

    TEST_METHOD(TestAddTabStop);

    TEST_METHOD(TestClearTabStops);

    TEST_METHOD(TestClearTabStop);

    TEST_METHOD(TestGetForwardTab);

    TEST_METHOD(TestGetReverseTab);

    TEST_METHOD(TestAreTabsSet);

    TEST_METHOD(TestAltBufferDefaultTabStops);

    TEST_METHOD(EraseAllTests);

    TEST_METHOD(OutputNULTest);

    TEST_METHOD(VtResize);
    TEST_METHOD(VtResizeComprehensive);
    TEST_METHOD(VtResizeDECCOLM);

    TEST_METHOD(VtSoftResetCursorPosition);

    TEST_METHOD(VtScrollMarginsNewlineColor);

    TEST_METHOD(VtNewlinePastViewport);

    TEST_METHOD(VtSetColorTable);

    TEST_METHOD(ResizeTraditionalDoesntDoubleFreeAttrRows);

    TEST_METHOD(ResizeCursorUnchanged);

    TEST_METHOD(ResizeAltBuffer);

    TEST_METHOD(ResizeAltBufferGetScreenBufferInfo);

    TEST_METHOD(VtEraseAllPersistCursor);
    TEST_METHOD(VtEraseAllPersistCursorFillColor);

    TEST_METHOD(GetWordBoundary);
    void GetWordBoundaryTrimZeros(bool on);
    TEST_METHOD(GetWordBoundaryTrimZerosOn);
    TEST_METHOD(GetWordBoundaryTrimZerosOff);

    TEST_METHOD(TestAltBufferCursorState);

    TEST_METHOD(TestAltBufferVtDispatching);

    TEST_METHOD(SetDefaultsIndividuallyBothDefault);
    TEST_METHOD(SetDefaultsTogether);

    TEST_METHOD(ReverseResetWithDefaultBackground);

    TEST_METHOD(BackspaceDefaultAttrs);
    TEST_METHOD(BackspaceDefaultAttrsWriteCharsLegacy);

    TEST_METHOD(BackspaceDefaultAttrsInPrompt);

    TEST_METHOD(SetGlobalColorTable);

    TEST_METHOD(SetColorTableThreeDigits);

    TEST_METHOD(SetDefaultForegroundColor);

    TEST_METHOD(SetDefaultBackgroundColor);

    TEST_METHOD(DeleteCharsNearEndOfLine);
    TEST_METHOD(DeleteCharsNearEndOfLineSimpleFirstCase);
    TEST_METHOD(DeleteCharsNearEndOfLineSimpleSecondCase);

    TEST_METHOD(DontResetColorsAboveVirtualBottom);

    TEST_METHOD(ScrollOperations);
    TEST_METHOD(InsertChars);
    TEST_METHOD(DeleteChars);

    TEST_METHOD(ScrollUpInMargins);
    TEST_METHOD(ScrollDownInMargins);
    TEST_METHOD(InsertLinesInMargins);
    TEST_METHOD(DeleteLinesInMargins);
    TEST_METHOD(ReverseLineFeedInMargins);

    TEST_METHOD(ScrollLines256Colors);

    TEST_METHOD(SetOriginMode);

    TEST_METHOD(HardResetBuffer);

    TEST_METHOD(RestoreDownAltBufferWithTerminalScrolling);

    TEST_METHOD(SnapCursorWithTerminalScrolling);

    TEST_METHOD(ClearAlternateBuffer);

    TEST_METHOD(InitializeTabStopsInVTMode);

    TEST_METHOD(TestExtendedTextAttributes);
    TEST_METHOD(TestExtendedTextAttributesWithColors);

    TEST_METHOD(CursorUpDownAcrossMargins);
    TEST_METHOD(CursorUpDownOutsideMargins);
    TEST_METHOD(CursorUpDownExactlyAtMargins);

    TEST_METHOD(CursorSaveRestore);
};

void ScreenBufferTests::SingleAlternateBufferCreationTest()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.LockConsole(); // Lock must be taken to manipulate buffer.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    Log::Comment(L"Testing creating one alternate buffer, then returning to the main buffer.");
    SCREEN_INFORMATION* const psiOriginal = &gci.GetActiveOutputBuffer();
    VERIFY_IS_NULL(psiOriginal->_psiAlternateBuffer);
    VERIFY_IS_NULL(psiOriginal->_psiMainBuffer);

    NTSTATUS Status = psiOriginal->UseAlternateScreenBuffer();
    if (VERIFY_IS_TRUE(NT_SUCCESS(Status)))
    {
        Log::Comment(L"First alternate buffer successfully created");
        SCREEN_INFORMATION* const psiFirstAlternate = &gci.GetActiveOutputBuffer();
        VERIFY_ARE_NOT_EQUAL(psiOriginal, psiFirstAlternate);
        VERIFY_ARE_EQUAL(psiFirstAlternate, psiOriginal->_psiAlternateBuffer);
        VERIFY_ARE_EQUAL(psiOriginal, psiFirstAlternate->_psiMainBuffer);
        VERIFY_IS_NULL(psiOriginal->_psiMainBuffer);
        VERIFY_IS_NULL(psiFirstAlternate->_psiAlternateBuffer);

        psiFirstAlternate->UseMainScreenBuffer();
        Log::Comment(L"successfully swapped to the main buffer");
        SCREEN_INFORMATION* const psiFinal = &gci.GetActiveOutputBuffer();
        VERIFY_ARE_NOT_EQUAL(psiFinal, psiFirstAlternate);
        VERIFY_ARE_EQUAL(psiFinal, psiOriginal);
        VERIFY_IS_NULL(psiFinal->_psiMainBuffer);
        VERIFY_IS_NULL(psiFinal->_psiAlternateBuffer);
    }
}

void ScreenBufferTests::MultipleAlternateBufferCreationTest()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.LockConsole(); // Lock must be taken to manipulate buffer.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    Log::Comment(
        L"Testing creating one alternate buffer, then creating another "
        L"alternate from that first alternate, before returning to the "
        L"main buffer.");

    SCREEN_INFORMATION* const psiOriginal = &gci.GetActiveOutputBuffer();
    NTSTATUS Status = psiOriginal->UseAlternateScreenBuffer();
    if (VERIFY_IS_TRUE(NT_SUCCESS(Status)))
    {
        Log::Comment(L"First alternate buffer successfully created");
        SCREEN_INFORMATION* const psiFirstAlternate = &gci.GetActiveOutputBuffer();
        VERIFY_ARE_NOT_EQUAL(psiOriginal, psiFirstAlternate);
        VERIFY_ARE_EQUAL(psiFirstAlternate, psiOriginal->_psiAlternateBuffer);
        VERIFY_ARE_EQUAL(psiOriginal, psiFirstAlternate->_psiMainBuffer);
        VERIFY_IS_NULL(psiOriginal->_psiMainBuffer);
        VERIFY_IS_NULL(psiFirstAlternate->_psiAlternateBuffer);

        Status = psiFirstAlternate->UseAlternateScreenBuffer();
        if (VERIFY_IS_TRUE(NT_SUCCESS(Status)))
        {
            Log::Comment(L"Second alternate buffer successfully created");
            SCREEN_INFORMATION* psiSecondAlternate = &gci.GetActiveOutputBuffer();
            VERIFY_ARE_NOT_EQUAL(psiOriginal, psiSecondAlternate);
            VERIFY_ARE_NOT_EQUAL(psiSecondAlternate, psiFirstAlternate);
            VERIFY_ARE_EQUAL(psiSecondAlternate, psiOriginal->_psiAlternateBuffer);
            VERIFY_ARE_EQUAL(psiOriginal, psiSecondAlternate->_psiMainBuffer);
            VERIFY_IS_NULL(psiOriginal->_psiMainBuffer);
            VERIFY_IS_NULL(psiSecondAlternate->_psiAlternateBuffer);

            psiSecondAlternate->UseMainScreenBuffer();
            Log::Comment(L"successfully swapped to the main buffer");
            SCREEN_INFORMATION* const psiFinal = &gci.GetActiveOutputBuffer();
            VERIFY_ARE_NOT_EQUAL(psiFinal, psiFirstAlternate);
            VERIFY_ARE_NOT_EQUAL(psiFinal, psiSecondAlternate);
            VERIFY_ARE_EQUAL(psiFinal, psiOriginal);
            VERIFY_IS_NULL(psiFinal->_psiMainBuffer);
            VERIFY_IS_NULL(psiFinal->_psiAlternateBuffer);
        }
    }
}

void ScreenBufferTests::MultipleAlternateBuffersFromMainCreationTest()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.LockConsole(); // Lock must be taken to manipulate buffer.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    Log::Comment(
        L"Testing creating one alternate buffer, then creating another"
        L" alternate from the main, before returning to the main buffer.");
    SCREEN_INFORMATION* const psiOriginal = &gci.GetActiveOutputBuffer();
    NTSTATUS Status = psiOriginal->UseAlternateScreenBuffer();
    if (VERIFY_IS_TRUE(NT_SUCCESS(Status)))
    {
        Log::Comment(L"First alternate buffer successfully created");
        SCREEN_INFORMATION* const psiFirstAlternate = &gci.GetActiveOutputBuffer();
        VERIFY_ARE_NOT_EQUAL(psiOriginal, psiFirstAlternate);
        VERIFY_ARE_EQUAL(psiFirstAlternate, psiOriginal->_psiAlternateBuffer);
        VERIFY_ARE_EQUAL(psiOriginal, psiFirstAlternate->_psiMainBuffer);
        VERIFY_IS_NULL(psiOriginal->_psiMainBuffer);
        VERIFY_IS_NULL(psiFirstAlternate->_psiAlternateBuffer);

        Status = psiOriginal->UseAlternateScreenBuffer();
        if (VERIFY_IS_TRUE(NT_SUCCESS(Status)))
        {
            Log::Comment(L"Second alternate buffer successfully created");
            SCREEN_INFORMATION* const psiSecondAlternate = &gci.GetActiveOutputBuffer();
            VERIFY_ARE_NOT_EQUAL(psiOriginal, psiSecondAlternate);
            VERIFY_ARE_NOT_EQUAL(psiSecondAlternate, psiFirstAlternate);
            VERIFY_ARE_EQUAL(psiSecondAlternate, psiOriginal->_psiAlternateBuffer);
            VERIFY_ARE_EQUAL(psiOriginal, psiSecondAlternate->_psiMainBuffer);
            VERIFY_IS_NULL(psiOriginal->_psiMainBuffer);
            VERIFY_IS_NULL(psiSecondAlternate->_psiAlternateBuffer);

            psiSecondAlternate->UseMainScreenBuffer();
            Log::Comment(L"successfully swapped to the main buffer");
            SCREEN_INFORMATION* const psiFinal = &gci.GetActiveOutputBuffer();
            VERIFY_ARE_NOT_EQUAL(psiFinal, psiFirstAlternate);
            VERIFY_ARE_NOT_EQUAL(psiFinal, psiSecondAlternate);
            VERIFY_ARE_EQUAL(psiFinal, psiOriginal);
            VERIFY_IS_NULL(psiFinal->_psiMainBuffer);
            VERIFY_IS_NULL(psiFinal->_psiAlternateBuffer);
        }
    }
}

void ScreenBufferTests::TestReverseLineFeed()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& screenInfo = gci.GetActiveOutputBuffer();
    auto& stateMachine = screenInfo.GetStateMachine();
    auto& cursor = screenInfo._textBuffer->GetCursor();
    auto viewport = screenInfo.GetViewport();

    VERIFY_ARE_EQUAL(viewport.Top(), 0);

    ////////////////////////////////////////////////////////////////////////
    Log::Comment(L"Case 1: RI from below top of viewport");

    stateMachine.ProcessString(L"foo\nfoo", 7);
    VERIFY_ARE_EQUAL(cursor.GetPosition().X, 3);
    VERIFY_ARE_EQUAL(cursor.GetPosition().Y, 1);
    VERIFY_ARE_EQUAL(viewport.Top(), 0);

    VERIFY_SUCCEEDED(DoSrvPrivateReverseLineFeed(screenInfo));

    VERIFY_ARE_EQUAL(cursor.GetPosition().X, 3);
    VERIFY_ARE_EQUAL(cursor.GetPosition().Y, 0);
    viewport = screenInfo.GetViewport();
    VERIFY_ARE_EQUAL(viewport.Top(), 0);
    Log::Comment(NoThrowString().Format(
        L"viewport={L:%d,T:%d,R:%d,B:%d}",
        viewport.Left(),
        viewport.Top(),
        viewport.RightInclusive(),
        viewport.BottomInclusive()));

    ////////////////////////////////////////////////////////////////////////
    Log::Comment(L"Case 2: RI from top of viewport");
    cursor.SetPosition({ 0, 0 });
    stateMachine.ProcessString(L"123456789", 9);
    VERIFY_ARE_EQUAL(cursor.GetPosition().X, 9);
    VERIFY_ARE_EQUAL(cursor.GetPosition().Y, 0);
    VERIFY_ARE_EQUAL(screenInfo.GetViewport().Top(), 0);

    VERIFY_SUCCEEDED(DoSrvPrivateReverseLineFeed(screenInfo));

    VERIFY_ARE_EQUAL(cursor.GetPosition().X, 9);
    VERIFY_ARE_EQUAL(cursor.GetPosition().Y, 0);
    viewport = screenInfo.GetViewport();
    VERIFY_ARE_EQUAL(viewport.Top(), 0);
    Log::Comment(NoThrowString().Format(
        L"viewport={L:%d,T:%d,R:%d,B:%d}",
        viewport.Left(),
        viewport.Top(),
        viewport.RightInclusive(),
        viewport.BottomInclusive()));
    auto c = screenInfo._textBuffer->GetLastNonSpaceCharacter();
    VERIFY_ARE_EQUAL(c.Y, 2); // This is the coordinates of the second "foo" from before.

    ////////////////////////////////////////////////////////////////////////
    Log::Comment(L"Case 3: RI from top of viewport, when viewport is below top of buffer");

    cursor.SetPosition({ 0, 5 });
    VERIFY_SUCCEEDED(screenInfo.SetViewportOrigin(true, { 0, 5 }, true));
    stateMachine.ProcessString(L"ABCDEFGH");
    VERIFY_ARE_EQUAL(cursor.GetPosition().X, 8);
    VERIFY_ARE_EQUAL(cursor.GetPosition().Y, 5);
    VERIFY_ARE_EQUAL(screenInfo.GetViewport().Top(), 5);

    LOG_IF_FAILED(DoSrvPrivateReverseLineFeed(screenInfo));

    VERIFY_ARE_EQUAL(cursor.GetPosition().X, 8);
    VERIFY_ARE_EQUAL(cursor.GetPosition().Y, 5);
    viewport = screenInfo.GetViewport();
    VERIFY_ARE_EQUAL(viewport.Top(), 5);
    Log::Comment(NoThrowString().Format(
        L"viewport={L:%d,T:%d,R:%d,B:%d}",
        viewport.Left(),
        viewport.Top(),
        viewport.RightInclusive(),
        viewport.BottomInclusive()));
    c = screenInfo._textBuffer->GetLastNonSpaceCharacter();
    VERIFY_ARE_EQUAL(c.Y, 6);
}

void ScreenBufferTests::TestAddTabStop()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& screenInfo = gci.GetActiveOutputBuffer();
    screenInfo.ClearTabStops();
    auto scopeExit = wil::scope_exit([&]() { screenInfo.ClearTabStops(); });

    std::list<short> expectedStops{ 12 };
    Log::Comment(L"Add tab to empty list.");
    screenInfo.AddTabStop(12);
    VERIFY_ARE_EQUAL(expectedStops, screenInfo._tabStops);

    Log::Comment(L"Add tab to head of existing list.");
    screenInfo.AddTabStop(4);
    expectedStops.push_front(4);
    VERIFY_ARE_EQUAL(expectedStops, screenInfo._tabStops);

    Log::Comment(L"Add tab to tail of existing list.");
    screenInfo.AddTabStop(30);
    expectedStops.push_back(30);
    VERIFY_ARE_EQUAL(expectedStops, screenInfo._tabStops);

    Log::Comment(L"Add tab to middle of existing list.");
    screenInfo.AddTabStop(24);
    expectedStops.push_back(24);
    expectedStops.sort();
    VERIFY_ARE_EQUAL(expectedStops, screenInfo._tabStops);

    Log::Comment(L"Add tab that duplicates an item in the existing list.");
    screenInfo.AddTabStop(24);
    VERIFY_ARE_EQUAL(expectedStops, screenInfo._tabStops);
}

void ScreenBufferTests::TestClearTabStops()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& screenInfo = gci.GetActiveOutputBuffer();

    Log::Comment(L"Clear non-existant tab stops.");
    {
        screenInfo.ClearTabStops();
        VERIFY_IS_TRUE(screenInfo._tabStops.empty());
    }

    Log::Comment(L"Clear handful of tab stops.");
    {
        for (auto x : { 3, 6, 13, 2, 25 })
        {
            screenInfo.AddTabStop(gsl::narrow<short>(x));
        }
        VERIFY_IS_FALSE(screenInfo._tabStops.empty());
        screenInfo.ClearTabStops();
        VERIFY_IS_TRUE(screenInfo._tabStops.empty());
    }
}

void ScreenBufferTests::TestClearTabStop()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& screenInfo = gci.GetActiveOutputBuffer();

    Log::Comment(L"Try to clear nonexistant list.");
    {
        screenInfo.ClearTabStop(0);

        VERIFY_IS_TRUE(screenInfo._tabStops.empty(), L"List should remain empty");
    }

    Log::Comment(L"Allocate 1 list item and clear it.");
    {
        screenInfo._tabStops.push_back(0);
        screenInfo.ClearTabStop(0);

        VERIFY_IS_TRUE(screenInfo._tabStops.empty());
    }

    Log::Comment(L"Allocate 1 list item and clear non-existant.");
    {
        screenInfo._tabStops.push_back(0);

        Log::Comment(L"Free greater");
        screenInfo.ClearTabStop(1);
        VERIFY_IS_FALSE(screenInfo._tabStops.empty());

        Log::Comment(L"Free less than");
        screenInfo.ClearTabStop(-1);
        VERIFY_IS_FALSE(screenInfo._tabStops.empty());

        // clear all tab stops
        screenInfo._tabStops.clear();
    }

    Log::Comment(L"Allocate many (5) list items and clear head.");
    {
        std::list<short> inputData = { 3, 5, 6, 10, 15, 17 };
        screenInfo._tabStops = inputData;
        screenInfo.ClearTabStop(inputData.front());

        inputData.pop_front();
        VERIFY_ARE_EQUAL(inputData, screenInfo._tabStops);

        // clear all tab stops
        screenInfo._tabStops.clear();
    }

    Log::Comment(L"Allocate many (5) list items and clear middle.");
    {
        std::list<short> inputData = { 3, 5, 6, 10, 15, 17 };
        screenInfo._tabStops = inputData;
        screenInfo.ClearTabStop(*std::next(inputData.begin()));

        inputData.erase(std::next(inputData.begin()));
        VERIFY_ARE_EQUAL(inputData, screenInfo._tabStops);

        // clear all tab stops
        screenInfo._tabStops.clear();
    }

    Log::Comment(L"Allocate many (5) list items and clear tail.");
    {
        std::list<short> inputData = { 3, 5, 6, 10, 15, 17 };
        screenInfo._tabStops = inputData;
        screenInfo.ClearTabStop(inputData.back());

        inputData.pop_back();
        VERIFY_ARE_EQUAL(inputData, screenInfo._tabStops);

        // clear all tab stops
        screenInfo._tabStops.clear();
    }

    Log::Comment(L"Allocate many (5) list items and clear non-existant item.");
    {
        std::list<short> inputData = { 3, 5, 6, 10, 15, 17 };
        screenInfo._tabStops = inputData;
        screenInfo.ClearTabStop(9000);

        VERIFY_ARE_EQUAL(inputData, screenInfo._tabStops);

        // clear all tab stops
        screenInfo._tabStops.clear();
    }
}

void ScreenBufferTests::TestGetForwardTab()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer();

    std::list<short> inputData = { 3, 5, 6, 10, 15, 17 };
    si._tabStops = inputData;

    const COORD coordScreenBufferSize = si.GetBufferSize().Dimensions();
    COORD coordCursor;
    coordCursor.Y = coordScreenBufferSize.Y / 2; // in the middle of the buffer, it doesn't make a difference.

    Log::Comment(L"Find next tab from before front.");
    {
        coordCursor.X = 0;

        COORD coordCursorExpected;
        coordCursorExpected = coordCursor;
        coordCursorExpected.X = inputData.front();

        COORD const coordCursorResult = si.GetForwardTab(coordCursor);
        VERIFY_ARE_EQUAL(coordCursorExpected,
                         coordCursorResult,
                         L"Cursor advanced to first tab stop from sample list.");
    }

    Log::Comment(L"Find next tab from in the middle.");
    {
        coordCursor.X = 6;

        COORD coordCursorExpected;
        coordCursorExpected = coordCursor;
        coordCursorExpected.X = *std::next(inputData.begin(), 3);

        COORD const coordCursorResult = si.GetForwardTab(coordCursor);
        VERIFY_ARE_EQUAL(coordCursorExpected,
                         coordCursorResult,
                         L"Cursor advanced to middle tab stop from sample list.");
    }

    Log::Comment(L"Find next tab from end.");
    {
        coordCursor.X = 30;

        COORD coordCursorExpected;
        coordCursorExpected = coordCursor;
        coordCursorExpected.X = coordScreenBufferSize.X - 1;

        COORD const coordCursorResult = si.GetForwardTab(coordCursor);
        VERIFY_ARE_EQUAL(coordCursorExpected,
                         coordCursorResult,
                         L"Cursor advanced to end of screen buffer.");
    }

    si._tabStops.clear();
}

void ScreenBufferTests::TestGetReverseTab()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer();

    std::list<short> inputData = { 3, 5, 6, 10, 15, 17 };
    si._tabStops = inputData;

    COORD coordCursor;
    // in the middle of the buffer, it doesn't make a difference.
    coordCursor.Y = si.GetBufferSize().Height() / 2;

    Log::Comment(L"Find previous tab from before front.");
    {
        coordCursor.X = 1;

        COORD coordCursorExpected;
        coordCursorExpected = coordCursor;
        coordCursorExpected.X = 0;

        COORD const coordCursorResult = si.GetReverseTab(coordCursor);
        VERIFY_ARE_EQUAL(coordCursorExpected,
                         coordCursorResult,
                         L"Cursor adjusted to beginning of the buffer when it started before sample list.");
    }

    Log::Comment(L"Find previous tab from in the middle.");
    {
        coordCursor.X = 6;

        COORD coordCursorExpected;
        coordCursorExpected = coordCursor;
        coordCursorExpected.X = *std::next(inputData.begin());

        COORD const coordCursorResult = si.GetReverseTab(coordCursor);
        VERIFY_ARE_EQUAL(coordCursorExpected,
                         coordCursorResult,
                         L"Cursor adjusted back one tab spot from middle of sample list.");
    }

    Log::Comment(L"Find next tab from end.");
    {
        coordCursor.X = 30;

        COORD coordCursorExpected;
        coordCursorExpected = coordCursor;
        coordCursorExpected.X = inputData.back();

        COORD const coordCursorResult = si.GetReverseTab(coordCursor);
        VERIFY_ARE_EQUAL(coordCursorExpected,
                         coordCursorResult,
                         L"Cursor adjusted to last item in the sample list from position beyond end.");
    }

    si._tabStops.clear();
}

void ScreenBufferTests::TestAreTabsSet()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer();

    si._tabStops.clear();
    VERIFY_IS_FALSE(si.AreTabsSet());

    si.AddTabStop(1);
    VERIFY_IS_TRUE(si.AreTabsSet());
}

void ScreenBufferTests::TestAltBufferDefaultTabStops()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.LockConsole(); // Lock must be taken to swap buffers.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    SCREEN_INFORMATION& mainBuffer = gci.GetActiveOutputBuffer();
    // Make sure we're in VT mode
    WI_SetFlag(mainBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    VERIFY_IS_TRUE(WI_IsFlagSet(mainBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING));

    mainBuffer.SetDefaultVtTabStops();
    VERIFY_IS_TRUE(mainBuffer.AreTabsSet());

    VERIFY_SUCCEEDED(mainBuffer.UseAlternateScreenBuffer());
    SCREEN_INFORMATION& altBuffer = gci.GetActiveOutputBuffer();
    auto useMain = wil::scope_exit([&] { altBuffer.UseMainScreenBuffer(); });

    Log::Comment(NoThrowString().Format(
        L"Manually enable VT mode for the alt buffer - "
        L"usually the ctor will pick this up from GCI, but not in the tests."));
    WI_SetFlag(altBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    VERIFY_IS_TRUE(WI_IsFlagSet(altBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING));
    VERIFY_IS_TRUE(altBuffer.AreTabsSet());
    VERIFY_IS_TRUE(altBuffer._tabStops.size() > 3);

    const COORD origin{ 0, 0 };
    auto& cursor = altBuffer.GetTextBuffer().GetCursor();
    cursor.SetPosition(origin);
    auto& stateMachine = altBuffer.GetStateMachine();

    Log::Comment(NoThrowString().Format(
        L"Tab a few times - make sure the cursor is where we expect."));

    stateMachine.ProcessString(L"\t");
    COORD expected{ 8, 0 };
    VERIFY_ARE_EQUAL(expected, cursor.GetPosition());

    stateMachine.ProcessString(L"\t");
    expected = { 16, 0 };
    VERIFY_ARE_EQUAL(expected, cursor.GetPosition());

    stateMachine.ProcessString(L"\n");
    expected = { 0, 1 };
    VERIFY_ARE_EQUAL(expected, cursor.GetPosition());

    altBuffer.ClearTabStops();
    VERIFY_IS_FALSE(altBuffer.AreTabsSet());
    stateMachine.ProcessString(L"\t");
    expected = { altBuffer.GetBufferSize().Width() - 1, 1 };

    VERIFY_ARE_EQUAL(expected, cursor.GetPosition());

    useMain.release();
    altBuffer.UseMainScreenBuffer();
    VERIFY_IS_TRUE(mainBuffer.AreTabsSet());
}

void ScreenBufferTests::EraseAllTests()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si._textBuffer->GetCursor();

    VERIFY_ARE_EQUAL(si.GetViewport().Top(), 0);

    ////////////////////////////////////////////////////////////////////////
    Log::Comment(L"Case 1: Erase a single line of text in the buffer\n");

    stateMachine.ProcessString(L"foo", 3);
    COORD originalRelativePosition = { 3, 0 };
    VERIFY_ARE_EQUAL(si.GetViewport().Top(), 0);
    VERIFY_ARE_EQUAL(cursor.GetPosition(), originalRelativePosition);

    VERIFY_SUCCEEDED(si.VtEraseAll());

    auto viewport = si._viewport;
    VERIFY_ARE_EQUAL(viewport.Top(), 1);
    COORD newRelativePos = originalRelativePosition;
    viewport.ConvertFromOrigin(&newRelativePos);
    VERIFY_ARE_EQUAL(cursor.GetPosition(), newRelativePos);
    Log::Comment(NoThrowString().Format(
        L"viewport={L:%d,T:%d,R:%d,B:%d}",
        viewport.Left(),
        viewport.Top(),
        viewport.RightInclusive(),
        viewport.BottomInclusive()));

    ////////////////////////////////////////////////////////////////////////
    Log::Comment(L"Case 2: Erase multiple lines, below the top of the buffer\n");

    stateMachine.ProcessString(L"bar\nbar\nbar", 11);
    viewport = si._viewport;
    originalRelativePosition = cursor.GetPosition();
    viewport.ConvertToOrigin(&originalRelativePosition);
    VERIFY_ARE_EQUAL(viewport.Top(), 1);
    Log::Comment(NoThrowString().Format(
        L"viewport={L:%d,T:%d,R:%d,B:%d}",
        viewport.Left(),
        viewport.Top(),
        viewport.RightInclusive(),
        viewport.BottomInclusive()));

    VERIFY_SUCCEEDED(si.VtEraseAll());
    viewport = si._viewport;
    VERIFY_ARE_EQUAL(viewport.Top(), 4);
    newRelativePos = originalRelativePosition;
    viewport.ConvertFromOrigin(&newRelativePos);
    VERIFY_ARE_EQUAL(cursor.GetPosition(), newRelativePos);
    Log::Comment(NoThrowString().Format(
        L"viewport={L:%d,T:%d,R:%d,B:%d}",
        viewport.Left(),
        viewport.Top(),
        viewport.RightInclusive(),
        viewport.BottomInclusive()));

    ////////////////////////////////////////////////////////////////////////
    Log::Comment(L"Case 3: multiple lines at the bottom of the buffer\n");

    cursor.SetPosition({ 0, 275 });
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, { 0, 220 }, true));
    stateMachine.ProcessString(L"bar\nbar\nbar", 11);
    viewport = si._viewport;
    VERIFY_ARE_EQUAL(cursor.GetPosition().X, 3);
    VERIFY_ARE_EQUAL(cursor.GetPosition().Y, 277);
    originalRelativePosition = cursor.GetPosition();
    viewport.ConvertToOrigin(&originalRelativePosition);

    Log::Comment(NoThrowString().Format(
        L"viewport={L:%d,T:%d,R:%d,B:%d}",
        viewport.Left(),
        viewport.Top(),
        viewport.RightInclusive(),
        viewport.BottomInclusive()));
    VERIFY_SUCCEEDED(si.VtEraseAll());

    viewport = si._viewport;
    auto heightFromBottom = si.GetBufferSize().Height() - (viewport.Height());
    VERIFY_ARE_EQUAL(viewport.Top(), heightFromBottom);
    newRelativePos = originalRelativePosition;
    viewport.ConvertFromOrigin(&newRelativePos);
    VERIFY_ARE_EQUAL(cursor.GetPosition(), newRelativePos);
    Log::Comment(NoThrowString().Format(
        L"viewport={L:%d,T:%d,R:%d,B:%d}",
        viewport.Left(),
        viewport.Top(),
        viewport.RightInclusive(),
        viewport.BottomInclusive()));
}

void ScreenBufferTests::OutputNULTest()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si._textBuffer->GetCursor();

    VERIFY_ARE_EQUAL(0, cursor.GetPosition().X);
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().Y);

    Log::Comment(NoThrowString().Format(
        L"Writing a single NUL"));
    stateMachine.ProcessString(L"\0", 1);
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().X);
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().Y);

    Log::Comment(NoThrowString().Format(
        L"Writing many NULs"));
    stateMachine.ProcessString(L"\0\0\0\0\0\0\0\0", 8);
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().X);
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().Y);

    Log::Comment(NoThrowString().Format(
        L"Testing a single NUL followed by real text"));
    stateMachine.ProcessString(L"\0foo", 4);
    VERIFY_ARE_EQUAL(3, cursor.GetPosition().X);
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().Y);

    stateMachine.ProcessString(L"\n");
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().X);
    VERIFY_ARE_EQUAL(1, cursor.GetPosition().Y);

    Log::Comment(NoThrowString().Format(
        L"Writing NULs in between other strings"));
    stateMachine.ProcessString(L"\0foo\0bar\0", 9);
    VERIFY_ARE_EQUAL(6, cursor.GetPosition().X);
    VERIFY_ARE_EQUAL(1, cursor.GetPosition().Y);
}

void ScreenBufferTests::VtResize()
{
    // Run this test in isolation - for one reason or another, this breaks other tests.
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
    END_TEST_METHOD_PROPERTIES()

    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    TextBuffer& tbi = si.GetTextBuffer();
    StateMachine& stateMachine = si.GetStateMachine();
    Cursor& cursor = tbi.GetCursor();
    WI_SetFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    cursor.SetXPosition(0);
    cursor.SetYPosition(0);

    auto initialSbHeight = si.GetBufferSize().Height();
    auto initialSbWidth = si.GetBufferSize().Width();
    auto initialViewHeight = si.GetViewport().Height();
    auto initialViewWidth = si.GetViewport().Width();

    Log::Comment(NoThrowString().Format(
        L"Write '\x1b[8;30;80t'"
        L" The Screen buffer height should remain unchanged, but the width should be 80 columns"
        L" The viewport should be w,h=80,30"));

    std::wstring sequence = L"\x1b[8;30;80t";
    stateMachine.ProcessString(&sequence[0], sequence.length());

    auto newSbHeight = si.GetBufferSize().Height();
    auto newSbWidth = si.GetBufferSize().Width();
    auto newViewHeight = si.GetViewport().Height();
    auto newViewWidth = si.GetViewport().Width();

    VERIFY_ARE_EQUAL(initialSbHeight, newSbHeight);
    VERIFY_ARE_EQUAL(80, newSbWidth);
    VERIFY_ARE_EQUAL(30, newViewHeight);
    VERIFY_ARE_EQUAL(80, newViewWidth);

    initialSbHeight = newSbHeight;
    initialSbWidth = newSbWidth;
    initialViewHeight = newViewHeight;
    initialViewWidth = newViewWidth;

    Log::Comment(NoThrowString().Format(
        L"Write '\x1b[8;40;80t'"
        L" The Screen buffer height should remain unchanged, but the width should be 80 columns"
        L" The viewport should be w,h=80,40"));

    sequence = L"\x1b[8;40;80t";
    stateMachine.ProcessString(&sequence[0], sequence.length());

    newSbHeight = si.GetBufferSize().Height();
    newSbWidth = si.GetBufferSize().Width();
    newViewHeight = si.GetViewport().Height();
    newViewWidth = si.GetViewport().Width();

    VERIFY_ARE_EQUAL(initialSbHeight, newSbHeight);
    VERIFY_ARE_EQUAL(80, newSbWidth);
    VERIFY_ARE_EQUAL(40, newViewHeight);
    VERIFY_ARE_EQUAL(80, newViewWidth);

    initialSbHeight = newSbHeight;
    initialSbWidth = newSbWidth;
    initialViewHeight = newViewHeight;
    initialViewWidth = newViewWidth;

    Log::Comment(NoThrowString().Format(
        L"Write '\x1b[8;40;90t'"
        L" The Screen buffer height should remain unchanged, but the width should be 90 columns"
        L" The viewport should be w,h=90,40"));

    sequence = L"\x1b[8;40;90t";
    stateMachine.ProcessString(&sequence[0], sequence.length());

    newSbHeight = si.GetBufferSize().Height();
    newSbWidth = si.GetBufferSize().Width();
    newViewHeight = si.GetViewport().Height();
    newViewWidth = si.GetViewport().Width();

    VERIFY_ARE_EQUAL(initialSbHeight, newSbHeight);
    VERIFY_ARE_EQUAL(90, newSbWidth);
    VERIFY_ARE_EQUAL(40, newViewHeight);
    VERIFY_ARE_EQUAL(90, newViewWidth);

    initialSbHeight = newSbHeight;
    initialSbWidth = newSbWidth;
    initialViewHeight = newViewHeight;
    initialViewWidth = newViewWidth;

    Log::Comment(NoThrowString().Format(
        L"Write '\x1b[8;12;12t'"
        L" The Screen buffer height should remain unchanged, but the width should be 12 columns"
        L" The viewport should be w,h=12,12"));

    sequence = L"\x1b[8;12;12t";
    stateMachine.ProcessString(&sequence[0], sequence.length());

    newSbHeight = si.GetBufferSize().Height();
    newSbWidth = si.GetBufferSize().Width();
    newViewHeight = si.GetViewport().Height();
    newViewWidth = si.GetViewport().Width();

    VERIFY_ARE_EQUAL(initialSbHeight, newSbHeight);
    VERIFY_ARE_EQUAL(12, newSbWidth);
    VERIFY_ARE_EQUAL(12, newViewHeight);
    VERIFY_ARE_EQUAL(12, newViewWidth);

    initialSbHeight = newSbHeight;
    initialSbWidth = newSbWidth;
    initialViewHeight = newViewHeight;
    initialViewWidth = newViewWidth;

    Log::Comment(NoThrowString().Format(
        L"Write '\x1b[8;0;0t'"
        L" Nothing should change"));

    sequence = L"\x1b[8;0;0t";
    stateMachine.ProcessString(&sequence[0], sequence.length());

    newSbHeight = si.GetBufferSize().Height();
    newSbWidth = si.GetBufferSize().Width();
    newViewHeight = si.GetViewport().Height();
    newViewWidth = si.GetViewport().Width();

    VERIFY_ARE_EQUAL(initialSbHeight, newSbHeight);
    VERIFY_ARE_EQUAL(initialSbWidth, newSbWidth);
    VERIFY_ARE_EQUAL(initialViewHeight, newViewHeight);
    VERIFY_ARE_EQUAL(initialViewWidth, newViewWidth);
}

void ScreenBufferTests::VtResizeComprehensive()
{
    // Run this test in isolation - for one reason or another, this breaks other tests.
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
        TEST_METHOD_PROPERTY(L"Data:dx", L"{-10, -1, 0, 1, 10}")
        TEST_METHOD_PROPERTY(L"Data:dy", L"{-10, -1, 0, 1, 10}")
    END_TEST_METHOD_PROPERTIES()

    int dx, dy;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"dx", dx), L"change in width of buffer");
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"dy", dy), L"change in height of buffer");

    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    TextBuffer& tbi = si.GetTextBuffer();
    StateMachine& stateMachine = si.GetStateMachine();
    Cursor& cursor = tbi.GetCursor();
    WI_SetFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    cursor.SetXPosition(0);
    cursor.SetYPosition(0);

    auto initialViewHeight = si.GetViewport().Height();
    auto initialViewWidth = si.GetViewport().Width();

    auto expectedViewWidth = initialViewWidth + dx;
    auto expectedViewHeight = initialViewHeight + dy;

    std::wstringstream ss;
    ss << L"\x1b[8;" << expectedViewHeight << L";" << expectedViewWidth << L"t";

    Log::Comment(NoThrowString().Format(
        L"Write '\\x1b[8;%d;%dt'"
        L" The viewport should be w,h=%d,%d",
        expectedViewHeight,
        expectedViewWidth,
        expectedViewWidth,
        expectedViewHeight));

    std::wstring sequence = ss.str();
    stateMachine.ProcessString(sequence);

    auto newViewHeight = si.GetViewport().Height();
    auto newViewWidth = si.GetViewport().Width();

    VERIFY_ARE_EQUAL(expectedViewWidth, newViewWidth);
    VERIFY_ARE_EQUAL(expectedViewHeight, newViewHeight);
}

void ScreenBufferTests::VtResizeDECCOLM()
{
    // Run this test in isolation - for one reason or another, this breaks other tests.
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
    END_TEST_METHOD_PROPERTIES()

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    auto& stateMachine = si.GetStateMachine();
    WI_SetFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    const auto setInitialMargins = L"\x1b[5;15r";
    const auto setInitialCursor = L"\x1b[10;40HABCDEF";
    const auto allowDECCOLM = L"\x1b[?40h";
    const auto disallowDECCOLM = L"\x1b[?40l";
    const auto setDECCOLM = L"\x1b[?3h";
    const auto resetDECCOLM = L"\x1b[?3l";

    auto getRelativeCursorPosition = [&]() {
        return si.GetTextBuffer().GetCursor().GetPosition() - si.GetViewport().Origin();
    };

    stateMachine.ProcessString(setInitialMargins);
    stateMachine.ProcessString(setInitialCursor);
    auto initialMargins = si.GetRelativeScrollMargins();
    auto initialCursorPosition = getRelativeCursorPosition();

    auto initialSbHeight = si.GetBufferSize().Height();
    auto initialSbWidth = si.GetBufferSize().Width();
    auto initialViewHeight = si.GetViewport().Height();
    auto initialViewWidth = si.GetViewport().Width();

    Log::Comment(L"By default, setting DECCOLM should have no effect");
    stateMachine.ProcessString(setDECCOLM);

    auto newSbHeight = si.GetBufferSize().Height();
    auto newSbWidth = si.GetBufferSize().Width();
    auto newViewHeight = si.GetViewport().Height();
    auto newViewWidth = si.GetViewport().Width();

    VERIFY_IS_TRUE(si.AreMarginsSet());
    VERIFY_ARE_EQUAL(initialMargins, si.GetRelativeScrollMargins());
    VERIFY_ARE_EQUAL(initialCursorPosition, getRelativeCursorPosition());
    VERIFY_ARE_EQUAL(initialSbHeight, newSbHeight);
    VERIFY_ARE_EQUAL(initialViewHeight, newViewHeight);
    VERIFY_ARE_EQUAL(initialSbWidth, newSbWidth);
    VERIFY_ARE_EQUAL(initialViewWidth, newViewWidth);

    stateMachine.ProcessString(setInitialMargins);
    stateMachine.ProcessString(setInitialCursor);

    initialSbHeight = newSbHeight;
    initialSbWidth = newSbWidth;
    initialViewHeight = newViewHeight;
    initialViewWidth = newViewWidth;

    Log::Comment(
        L"Once DECCOLM is allowed, setting it "
        L"should change the width to 132 columns "
        L"and reset the margins and cursor position");
    stateMachine.ProcessString(allowDECCOLM);
    stateMachine.ProcessString(setDECCOLM);

    newSbHeight = si.GetBufferSize().Height();
    newSbWidth = si.GetBufferSize().Width();
    newViewHeight = si.GetViewport().Height();
    newViewWidth = si.GetViewport().Width();

    VERIFY_IS_FALSE(si.AreMarginsSet());
    VERIFY_ARE_EQUAL(COORD({ 0, 0 }), getRelativeCursorPosition());
    VERIFY_ARE_EQUAL(initialSbHeight, newSbHeight);
    VERIFY_ARE_EQUAL(initialViewHeight, newViewHeight);
    VERIFY_ARE_EQUAL(132, newSbWidth);
    VERIFY_ARE_EQUAL(132, newViewWidth);

    stateMachine.ProcessString(setInitialMargins);
    stateMachine.ProcessString(setInitialCursor);
    initialMargins = si.GetRelativeScrollMargins();
    initialCursorPosition = getRelativeCursorPosition();

    initialSbHeight = newSbHeight;
    initialSbWidth = newSbWidth;
    initialViewHeight = newViewHeight;
    initialViewWidth = newViewWidth;

    Log::Comment(L"If DECCOLM is disallowed, resetting it should have no effect");
    stateMachine.ProcessString(disallowDECCOLM);
    stateMachine.ProcessString(resetDECCOLM);

    newSbHeight = si.GetBufferSize().Height();
    newSbWidth = si.GetBufferSize().Width();
    newViewHeight = si.GetViewport().Height();
    newViewWidth = si.GetViewport().Width();

    VERIFY_IS_TRUE(si.AreMarginsSet());
    VERIFY_ARE_EQUAL(initialMargins, si.GetRelativeScrollMargins());
    VERIFY_ARE_EQUAL(initialCursorPosition, getRelativeCursorPosition());
    VERIFY_ARE_EQUAL(initialSbHeight, newSbHeight);
    VERIFY_ARE_EQUAL(initialViewHeight, newViewHeight);
    VERIFY_ARE_EQUAL(initialSbWidth, newSbWidth);
    VERIFY_ARE_EQUAL(initialViewWidth, newViewWidth);

    stateMachine.ProcessString(setInitialMargins);
    stateMachine.ProcessString(setInitialCursor);

    initialSbHeight = newSbHeight;
    initialSbWidth = newSbWidth;
    initialViewHeight = newViewHeight;
    initialViewWidth = newViewWidth;

    Log::Comment(
        L"Once DECCOLM is allowed again, resetting it "
        L"should change the width to 80 columns "
        L"and reset the margins and cursor position");
    stateMachine.ProcessString(allowDECCOLM);
    stateMachine.ProcessString(resetDECCOLM);

    newSbHeight = si.GetBufferSize().Height();
    newSbWidth = si.GetBufferSize().Width();
    newViewHeight = si.GetViewport().Height();
    newViewWidth = si.GetViewport().Width();

    VERIFY_IS_FALSE(si.AreMarginsSet());
    VERIFY_ARE_EQUAL(COORD({ 0, 0 }), getRelativeCursorPosition());
    VERIFY_ARE_EQUAL(initialSbHeight, newSbHeight);
    VERIFY_ARE_EQUAL(initialViewHeight, newViewHeight);
    VERIFY_ARE_EQUAL(80, newSbWidth);
    VERIFY_ARE_EQUAL(80, newViewWidth);
}

void ScreenBufferTests::VtSoftResetCursorPosition()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const TextBuffer& tbi = si.GetTextBuffer();
    StateMachine& stateMachine = si.GetStateMachine();
    const Cursor& cursor = tbi.GetCursor();

    Log::Comment(NoThrowString().Format(L"Make sure the viewport is at 0,0"));
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, COORD({ 0, 0 }), true));

    Log::Comment(NoThrowString().Format(
        L"Move the cursor to 2,2, then execute a soft reset.\n"
        L"The cursor should not move."));

    std::wstring seq = L"\x1b[2;2H";
    stateMachine.ProcessString(&seq[0], seq.length());
    VERIFY_ARE_EQUAL(COORD({ 1, 1 }), cursor.GetPosition());

    seq = L"\x1b[!p";
    stateMachine.ProcessString(&seq[0], seq.length());
    VERIFY_ARE_EQUAL(COORD({ 1, 1 }), cursor.GetPosition());

    Log::Comment(NoThrowString().Format(
        L"Set some margins. The cursor should move home."));

    seq = L"\x1b[2;10r";
    stateMachine.ProcessString(&seq[0], seq.length());
    VERIFY_ARE_EQUAL(COORD({ 0, 0 }), cursor.GetPosition());

    Log::Comment(NoThrowString().Format(
        L"Move the cursor to 2,2, then execute a soft reset.\n"
        L"The cursor should not move, even though there are margins."));
    seq = L"\x1b[2;2H";
    stateMachine.ProcessString(&seq[0], seq.length());
    VERIFY_ARE_EQUAL(COORD({ 1, 1 }), cursor.GetPosition());
    seq = L"\x1b[!p";
    stateMachine.ProcessString(&seq[0], seq.length());
    VERIFY_ARE_EQUAL(COORD({ 1, 1 }), cursor.GetPosition());

    Log::Comment(
        L"Set the origin mode, some margins, and move the cursor to 2,2.\n"
        L"The position should be relative to the top-left of the margin area.");
    stateMachine.ProcessString(L"\x1b[?6h");
    stateMachine.ProcessString(L"\x1b[5;10r");
    stateMachine.ProcessString(L"\x1b[2;2H");
    VERIFY_ARE_EQUAL(COORD({ 1, 5 }), cursor.GetPosition());

    Log::Comment(
        L"Execute a soft reset, reapply the margins, and move the cursor to 2,2.\n"
        L"The position should now be relative to the top-left of the screen.");
    stateMachine.ProcessString(L"\x1b[!p");
    stateMachine.ProcessString(L"\x1b[5;10r");
    stateMachine.ProcessString(L"\x1b[2;2H");
    VERIFY_ARE_EQUAL(COORD({ 1, 1 }), cursor.GetPosition());
}

void ScreenBufferTests::VtScrollMarginsNewlineColor()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const TextBuffer& tbi = si.GetTextBuffer();
    StateMachine& stateMachine = si.GetStateMachine();
    Cursor& cursor = si.GetTextBuffer().GetCursor();

    Log::Comment(NoThrowString().Format(L"Make sure the viewport is at 0,0"));
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, COORD({ 0, 0 }), true));
    cursor.SetPosition(COORD({ 0, 0 }));

    const COLORREF yellow = RGB(255, 255, 0);
    const COLORREF magenta = RGB(255, 0, 255);
    gci.SetDefaultForegroundColor(yellow);
    gci.SetDefaultBackgroundColor(magenta);
    const TextAttribute defaultAttrs = gci.GetDefaultAttributes();
    si.SetAttributes(defaultAttrs);

    Log::Comment(NoThrowString().Format(L"Begin by clearing the screen."));

    std::wstring seq = L"\x1b[2J";
    stateMachine.ProcessString(seq);
    seq = L"\x1b[m";
    stateMachine.ProcessString(seq);

    Log::Comment(NoThrowString().Format(
        L"Set the margins to 2, 5, then emit 10 'X\\n' strings. "
        L"Each time, check that rows 0-10 have default attributes in their entire row."));
    seq = L"\x1b[2;5r";
    stateMachine.ProcessString(seq);
    // Make sure we clear the margins to not screw up another test.
    auto clearMargins = wil::scope_exit([&] { stateMachine.ProcessString(L"\x1b[r"); });

    for (int iteration = 0; iteration < 10; iteration++)
    {
        Log::Comment(NoThrowString().Format(
            L"Iteration:%d", iteration));
        seq = L"X";
        stateMachine.ProcessString(seq);
        seq = L"\n";
        stateMachine.ProcessString(seq);

        const COORD cursorPos = cursor.GetPosition();

        Log::Comment(NoThrowString().Format(
            L"Cursor=%s",
            VerifyOutputTraits<COORD>::ToString(cursorPos).GetBuffer()));
        const auto viewport = si.GetViewport();
        Log::Comment(NoThrowString().Format(
            L"Viewport=%s",
            VerifyOutputTraits<SMALL_RECT>::ToString(viewport.ToInclusive()).GetBuffer()));
        const auto viewTop = viewport.Top();
        for (int y = viewTop; y < viewTop + 10; y++)
        {
            SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);
            const ROW& row = tbi.GetRowByOffset(y);
            const auto attrRow = &row.GetAttrRow();
            const std::vector<TextAttribute> attrs{ attrRow->begin(), attrRow->end() };
            for (int x = 0; x < viewport.RightInclusive(); x++)
            {
                const auto& attr = attrs[x];
                VERIFY_ARE_EQUAL(false, attr.IsLegacy());
                VERIFY_ARE_EQUAL(defaultAttrs, attr);
                VERIFY_ARE_EQUAL(yellow, gci.LookupForegroundColor(attr));
                VERIFY_ARE_EQUAL(magenta, gci.LookupBackgroundColor(attr));
            }
        }
    }
}

void ScreenBufferTests::VtNewlinePastViewport()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const TextBuffer& tbi = si.GetTextBuffer();
    StateMachine& stateMachine = si.GetStateMachine();
    Cursor& cursor = si.GetTextBuffer().GetCursor();

    // Make sure we're in VT mode
    WI_SetFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    VERIFY_IS_TRUE(WI_IsFlagSet(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING));

    Log::Comment(NoThrowString().Format(L"Make sure the viewport is at 0,0"));
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, COORD({ 0, 0 }), true));
    cursor.SetPosition(COORD({ 0, 0 }));

    std::wstring seq = L"\x1b[m";
    stateMachine.ProcessString(seq);
    seq = L"\x1b[2J";
    stateMachine.ProcessString(seq);

    const TextAttribute defaultAttrs{};
    const TextAttribute expectedTwo{ FOREGROUND_GREEN | FOREGROUND_INTENSITY | BACKGROUND_BLUE };

    Log::Comment(NoThrowString().Format(
        L"Move the cursor to the bottom of the viewport"));

    const auto initialViewport = si.GetViewport();
    Log::Comment(NoThrowString().Format(
        L"initialViewport=%s",
        VerifyOutputTraits<SMALL_RECT>::ToString(initialViewport.ToInclusive()).GetBuffer()));

    cursor.SetPosition(COORD({ 0, initialViewport.BottomInclusive() }));

    seq = L"\x1b[92;44m"; // bright-green on dark-blue
    stateMachine.ProcessString(seq);
    seq = L"\n";
    stateMachine.ProcessString(seq);

    const auto viewport = si.GetViewport();
    Log::Comment(NoThrowString().Format(
        L"viewport=%s",
        VerifyOutputTraits<SMALL_RECT>::ToString(viewport.ToInclusive()).GetBuffer()));

    VERIFY_ARE_EQUAL(viewport.BottomInclusive(), cursor.GetPosition().Y);
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().X);

    for (int y = viewport.Top(); y < viewport.BottomInclusive(); y++)
    {
        SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);
        const ROW& row = tbi.GetRowByOffset(y);
        const auto attrRow = &row.GetAttrRow();
        const std::vector<TextAttribute> attrs{ attrRow->begin(), attrRow->end() };
        for (int x = 0; x < viewport.RightInclusive(); x++)
        {
            const auto& attr = attrs[x];
            VERIFY_ARE_EQUAL(defaultAttrs, attr);
        }
    }

    const ROW& row = tbi.GetRowByOffset(viewport.BottomInclusive());
    const auto attrRow = &row.GetAttrRow();
    const std::vector<TextAttribute> attrs{ attrRow->begin(), attrRow->end() };
    for (int x = 0; x < viewport.RightInclusive(); x++)
    {
        const auto& attr = attrs[x];
        VERIFY_ARE_EQUAL(expectedTwo, attr);
    }
}

void ScreenBufferTests::VtSetColorTable()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    StateMachine& stateMachine = si.GetStateMachine();

    // Start with a known value
    gci.SetColorTableEntry(0, RGB(0, 0, 0));

    Log::Comment(NoThrowString().Format(
        L"Process some valid sequences for setting the table"));

    std::wstring seq = L"\x1b]4;0;rgb:1/1/1\x7";
    stateMachine.ProcessString(&seq[0], seq.length());
    VERIFY_ARE_EQUAL(RGB(1, 1, 1), gci.GetColorTableEntry(::XtermToWindowsIndex(0)));

    seq = L"\x1b]4;1;rgb:1/23/1\x7";
    stateMachine.ProcessString(&seq[0], seq.length());
    VERIFY_ARE_EQUAL(RGB(1, 0x23, 1), gci.GetColorTableEntry(::XtermToWindowsIndex(1)));

    seq = L"\x1b]4;2;rgb:1/23/12\x7";
    stateMachine.ProcessString(&seq[0], seq.length());
    VERIFY_ARE_EQUAL(RGB(1, 0x23, 0x12), gci.GetColorTableEntry(::XtermToWindowsIndex(2)));

    seq = L"\x1b]4;3;rgb:12/23/12\x7";
    stateMachine.ProcessString(&seq[0], seq.length());
    VERIFY_ARE_EQUAL(RGB(0x12, 0x23, 0x12), gci.GetColorTableEntry(::XtermToWindowsIndex(3)));

    seq = L"\x1b]4;4;rgb:ff/a1/1b\x7";
    stateMachine.ProcessString(&seq[0], seq.length());
    VERIFY_ARE_EQUAL(RGB(0xff, 0xa1, 0x1b), gci.GetColorTableEntry(::XtermToWindowsIndex(4)));

    seq = L"\x1b]4;5;rgb:ff/a1/1b\x1b\\";
    stateMachine.ProcessString(&seq[0], seq.length());
    VERIFY_ARE_EQUAL(RGB(0xff, 0xa1, 0x1b), gci.GetColorTableEntry(::XtermToWindowsIndex(5)));

    Log::Comment(NoThrowString().Format(
        L"Try a bunch of invalid sequences."));
    Log::Comment(NoThrowString().Format(
        L"First start by setting an entry to a known value to compare to."));
    seq = L"\x1b]4;5;rgb:9/9/9\x1b\\";
    stateMachine.ProcessString(&seq[0], seq.length());
    VERIFY_ARE_EQUAL(RGB(9, 9, 9), gci.GetColorTableEntry(::XtermToWindowsIndex(5)));

    Log::Comment(NoThrowString().Format(
        L"invalid: Missing the first component"));
    seq = L"\x1b]4;5;rgb:/1/1\x1b\\";
    stateMachine.ProcessString(&seq[0], seq.length());
    VERIFY_ARE_EQUAL(RGB(9, 9, 9), gci.GetColorTableEntry(::XtermToWindowsIndex(5)));

    Log::Comment(NoThrowString().Format(
        L"invalid: too many characters in a component"));
    seq = L"\x1b]4;5;rgb:111/1/1\x1b\\";
    stateMachine.ProcessString(&seq[0], seq.length());
    VERIFY_ARE_EQUAL(RGB(9, 9, 9), gci.GetColorTableEntry(::XtermToWindowsIndex(5)));

    Log::Comment(NoThrowString().Format(
        L"invalid: too many componenets"));
    seq = L"\x1b]4;5;rgb:1/1/1/1\x1b\\";
    stateMachine.ProcessString(&seq[0], seq.length());
    VERIFY_ARE_EQUAL(RGB(9, 9, 9), gci.GetColorTableEntry(::XtermToWindowsIndex(5)));

    Log::Comment(NoThrowString().Format(
        L"invalid: no second component"));
    seq = L"\x1b]4;5;rgb:1//1\x1b\\";
    stateMachine.ProcessString(&seq[0], seq.length());
    VERIFY_ARE_EQUAL(RGB(9, 9, 9), gci.GetColorTableEntry(::XtermToWindowsIndex(5)));

    Log::Comment(NoThrowString().Format(
        L"invalid: no components"));
    seq = L"\x1b]4;5;rgb://\x1b\\";
    stateMachine.ProcessString(&seq[0], seq.length());
    VERIFY_ARE_EQUAL(RGB(9, 9, 9), gci.GetColorTableEntry(::XtermToWindowsIndex(5)));

    Log::Comment(NoThrowString().Format(
        L"invalid: no third component"));
    seq = L"\x1b]4;5;rgb:1/11/\x1b\\";
    stateMachine.ProcessString(&seq[0], seq.length());
    VERIFY_ARE_EQUAL(RGB(9, 9, 9), gci.GetColorTableEntry(::XtermToWindowsIndex(5)));

    Log::Comment(NoThrowString().Format(
        L"invalid: rgbi is not a supported color space"));
    seq = L"\x1b]4;5;rgbi:1/1/1\x1b\\";
    stateMachine.ProcessString(&seq[0], seq.length());
    VERIFY_ARE_EQUAL(RGB(9, 9, 9), gci.GetColorTableEntry(::XtermToWindowsIndex(5)));

    Log::Comment(NoThrowString().Format(
        L"invalid: cmyk is not a supported color space"));
    seq = L"\x1b]4;5;cmyk:1/1/1\x1b\\";
    stateMachine.ProcessString(&seq[0], seq.length());
    VERIFY_ARE_EQUAL(RGB(9, 9, 9), gci.GetColorTableEntry(::XtermToWindowsIndex(5)));

    Log::Comment(NoThrowString().Format(
        L"invalid: no table index should do nothing"));
    seq = L"\x1b]4;;rgb:1/1/1\x1b\\";
    stateMachine.ProcessString(&seq[0], seq.length());
    VERIFY_ARE_EQUAL(RGB(9, 9, 9), gci.GetColorTableEntry(::XtermToWindowsIndex(5)));

    Log::Comment(NoThrowString().Format(
        L"invalid: need to specify a color space"));
    seq = L"\x1b]4;5;1/1/1\x1b\\";
    stateMachine.ProcessString(&seq[0], seq.length());
    VERIFY_ARE_EQUAL(RGB(9, 9, 9), gci.GetColorTableEntry(::XtermToWindowsIndex(5)));
}

void ScreenBufferTests::ResizeTraditionalDoesntDoubleFreeAttrRows()
{
    // there is not much to verify here, this test passes if the console doesn't crash.
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();

    gci.SetWrapText(false);
    COORD newBufferSize = si.GetBufferSize().Dimensions();
    newBufferSize.Y--;

    VERIFY_SUCCEEDED(si.ResizeTraditional(newBufferSize));
}

void ScreenBufferTests::ResizeCursorUnchanged()
{
    // Created for MSFT:19863799. Make sure whewn we resize the buffer, the
    //      cursor looks the same as it did before.

    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:useResizeWithReflow", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:dx", L"{-10, -1, 0, 1, 10}")
        TEST_METHOD_PROPERTY(L"Data:dy", L"{-10, -1, 0, 1, 10}")
    END_TEST_METHOD_PROPERTIES();
    bool useResizeWithReflow;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"useResizeWithReflow", useResizeWithReflow), L"Use ResizeWithReflow or not");

    int dx, dy;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"dx", dx), L"change in width of buffer");
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"dy", dy), L"change in height of buffer");

    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const auto& initialCursor = si.GetTextBuffer().GetCursor();

    // Get initial cursor values
    const CursorType initialType = initialCursor.GetType();
    const auto initialSize = initialCursor.GetSize();
    const COLORREF initialColor = initialCursor.GetColor();

    // set our wrap mode accordingly - ResizeScreenBuffer will be smart enough
    //  to call the appropriate implementation
    gci.SetWrapText(useResizeWithReflow);

    COORD newBufferSize = si.GetBufferSize().Dimensions();
    newBufferSize.X += static_cast<short>(dx);
    newBufferSize.Y += static_cast<short>(dy);

    VERIFY_SUCCEEDED(si.ResizeScreenBuffer(newBufferSize, false));

    const auto& finalCursor = si.GetTextBuffer().GetCursor();
    const CursorType finalType = finalCursor.GetType();
    const auto finalSize = finalCursor.GetSize();
    const COLORREF finalColor = finalCursor.GetColor();

    VERIFY_ARE_EQUAL(initialType, finalType);
    VERIFY_ARE_EQUAL(initialColor, finalColor);
    VERIFY_ARE_EQUAL(initialSize, finalSize);
}

void ScreenBufferTests::ResizeAltBuffer()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.LockConsole(); // Lock must be taken to manipulate buffer.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    StateMachine& stateMachine = si.GetStateMachine();

    Log::Comment(NoThrowString().Format(
        L"Try resizing the alt buffer. Make sure the call doesn't stack overflow."));

    VERIFY_IS_FALSE(si._IsAltBuffer());
    const Viewport originalMainSize = Viewport(si._viewport);

    Log::Comment(NoThrowString().Format(
        L"Switch to alt buffer"));
    std::wstring seq = L"\x1b[?1049h";
    stateMachine.ProcessString(&seq[0], seq.length());

    VERIFY_IS_FALSE(si._IsAltBuffer());
    VERIFY_IS_NOT_NULL(si._psiAlternateBuffer);
    SCREEN_INFORMATION* const psiAlt = si._psiAlternateBuffer;

    COORD newSize = originalMainSize.Dimensions();
    newSize.X += 2;
    newSize.Y += 2;

    Log::Comment(NoThrowString().Format(
        L"MSFT:15917333 This call shouldn't stack overflow"));
    psiAlt->SetViewportSize(&newSize);
    VERIFY_IS_TRUE(true);

    Log::Comment(NoThrowString().Format(
        L"Switch back from buffer"));
    seq = L"\x1b[?1049l";
    stateMachine.ProcessString(&seq[0], seq.length());
    VERIFY_IS_FALSE(si._IsAltBuffer());
    VERIFY_IS_NULL(si._psiAlternateBuffer);
}

void ScreenBufferTests::ResizeAltBufferGetScreenBufferInfo()
{
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:dx", L"{-10, -1, 1, 10}")
        TEST_METHOD_PROPERTY(L"Data:dy", L"{-10, -1, 1, 10}")
    END_TEST_METHOD_PROPERTIES();

    int dx, dy;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"dx", dx), L"change in width of buffer");
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"dy", dy), L"change in height of buffer");

    // Tests MSFT:19918103
    Log::Comment(NoThrowString().Format(
        L"Switch to the alt buffer, then resize the buffer. "
        L"GetConsoleScreenBufferInfoEx(mainBuffer) should return the alt "
        L"buffer's size, not the main buffer's size."));

    auto& g = ServiceLocator::LocateGlobals();
    CONSOLE_INFORMATION& gci = g.getConsoleInformation();
    gci.LockConsole(); // Lock must be taken to manipulate buffer.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    SCREEN_INFORMATION& mainBuffer = gci.GetActiveOutputBuffer().GetActiveBuffer();
    StateMachine& stateMachine = mainBuffer.GetStateMachine();

    VERIFY_IS_FALSE(mainBuffer._IsAltBuffer());
    const Viewport originalMainSize = Viewport(mainBuffer._viewport);

    Log::Comment(NoThrowString().Format(
        L"Switch to alt buffer"));
    std::wstring seq = L"\x1b[?1049h";
    stateMachine.ProcessString(seq);

    VERIFY_IS_FALSE(mainBuffer._IsAltBuffer());
    VERIFY_IS_NOT_NULL(mainBuffer._psiAlternateBuffer);

    auto& altBuffer = *(mainBuffer._psiAlternateBuffer);
    auto useMain = wil::scope_exit([&] { altBuffer.UseMainScreenBuffer(); });

    COORD newBufferSize = originalMainSize.Dimensions();
    newBufferSize.X += static_cast<short>(dx);
    newBufferSize.Y += static_cast<short>(dy);

    const Viewport originalAltSize = Viewport(altBuffer._viewport);

    VERIFY_ARE_EQUAL(originalMainSize.Width(), originalAltSize.Width());
    VERIFY_ARE_EQUAL(originalMainSize.Height(), originalAltSize.Height());

    altBuffer.SetViewportSize(&newBufferSize);

    CONSOLE_SCREEN_BUFFER_INFOEX csbiex{ 0 };
    g.api.GetConsoleScreenBufferInfoExImpl(mainBuffer, csbiex);
    const auto newActualMainView = mainBuffer.GetViewport();
    const auto newActualAltView = altBuffer.GetViewport();

    const auto newApiViewport = Viewport::FromExclusive(csbiex.srWindow);

    VERIFY_ARE_NOT_EQUAL(originalAltSize.Width(), newActualAltView.Width());
    VERIFY_ARE_NOT_EQUAL(originalAltSize.Height(), newActualAltView.Height());

    VERIFY_ARE_NOT_EQUAL(originalMainSize.Width(), newActualAltView.Width());
    VERIFY_ARE_NOT_EQUAL(originalMainSize.Height(), newActualAltView.Height());

    VERIFY_ARE_EQUAL(newActualAltView.Width(), newApiViewport.Width());
    VERIFY_ARE_EQUAL(newActualAltView.Height(), newApiViewport.Height());
}

void ScreenBufferTests::VtEraseAllPersistCursor()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const TextBuffer& tbi = si.GetTextBuffer();
    StateMachine& stateMachine = si.GetStateMachine();
    const Cursor& cursor = tbi.GetCursor();

    Log::Comment(NoThrowString().Format(
        L"Make sure the viewport is at 0,0"));
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, COORD({ 0, 0 }), true));

    Log::Comment(NoThrowString().Format(
        L"Move the cursor to 2,2, then execute a Erase All.\n"
        L"The cursor should not move relative to the viewport."));

    std::wstring seq = L"\x1b[2;2H";
    stateMachine.ProcessString(&seq[0], seq.length());
    VERIFY_ARE_EQUAL(COORD({ 1, 1 }), cursor.GetPosition());

    seq = L"\x1b[2J";
    stateMachine.ProcessString(&seq[0], seq.length());

    auto newViewport = si._viewport;
    COORD expectedCursor = { 1, 1 };
    newViewport.ConvertFromOrigin(&expectedCursor);

    VERIFY_ARE_EQUAL(expectedCursor, cursor.GetPosition());
}

void ScreenBufferTests::VtEraseAllPersistCursorFillColor()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const TextBuffer& tbi = si.GetTextBuffer();
    StateMachine& stateMachine = si.GetStateMachine();

    Log::Comment(NoThrowString().Format(
        L"Make sure the viewport is at 0,0"));
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, COORD({ 0, 0 }), true));

    Log::Comment(NoThrowString().Format(
        L"Change the colors to dark_red on bright_blue, then execute a Erase All.\n"
        L"The viewport should be full of dark_red on bright_blue"));

    auto expectedAttr = TextAttribute(XtermToLegacy(1, 12));
    std::wstring seq = L"\x1b[31;104m";
    stateMachine.ProcessString(&seq[0], seq.length());

    VERIFY_ARE_EQUAL(expectedAttr, si.GetAttributes());

    seq = L"\x1b[2J";
    stateMachine.ProcessString(&seq[0], seq.length());

    VERIFY_ARE_EQUAL(expectedAttr, si.GetAttributes());

    auto newViewport = si._viewport;
    Log::Comment(NoThrowString().Format(
        L"new Viewport: %s",
        VerifyOutputTraits<SMALL_RECT>::ToString(newViewport.ToInclusive()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"Buffer Size: %s",
        VerifyOutputTraits<SMALL_RECT>::ToString(si.GetBufferSize().ToInclusive()).GetBuffer()));

    auto iter = tbi.GetCellDataAt(newViewport.Origin());
    auto height = newViewport.Height();
    auto width = newViewport.Width();
    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
        {
            VERIFY_ARE_EQUAL(expectedAttr, iter->TextAttr());
            iter++;
        }
    }
}

void ScreenBufferTests::GetWordBoundary()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();

    const auto text = L"This is some test text for word boundaries.";
    const auto length = wcslen(text);

    // Make the buffer as big as our test text.
    const COORD newBufferSize = { gsl::narrow<SHORT>(length), 10 };
    VERIFY_SUCCEEDED(si.GetTextBuffer().ResizeTraditional(newBufferSize));

    const OutputCellIterator it(text, si.GetAttributes());
    si.Write(it, { 0, 0 });

    // Now find some words in it.
    Log::Comment(L"Find first word from its front.");
    COORD expectedFirst = { 0, 0 };
    COORD expectedSecond = { 4, 0 };

    auto boundary = si.GetWordBoundary({ 0, 0 });
    VERIFY_ARE_EQUAL(expectedFirst, boundary.first);
    VERIFY_ARE_EQUAL(expectedSecond, boundary.second);

    Log::Comment(L"Find first word from its middle.");
    boundary = si.GetWordBoundary({ 1, 0 });
    VERIFY_ARE_EQUAL(expectedFirst, boundary.first);
    VERIFY_ARE_EQUAL(expectedSecond, boundary.second);

    Log::Comment(L"Find first word from its end.");
    boundary = si.GetWordBoundary({ 3, 0 });
    VERIFY_ARE_EQUAL(expectedFirst, boundary.first);
    VERIFY_ARE_EQUAL(expectedSecond, boundary.second);

    Log::Comment(L"Find middle word from its front.");
    expectedFirst = { 13, 0 };
    expectedSecond = { 17, 0 };
    boundary = si.GetWordBoundary({ 13, 0 });
    VERIFY_ARE_EQUAL(expectedFirst, boundary.first);
    VERIFY_ARE_EQUAL(expectedSecond, boundary.second);

    Log::Comment(L"Find middle word from its middle.");
    boundary = si.GetWordBoundary({ 15, 0 });
    VERIFY_ARE_EQUAL(expectedFirst, boundary.first);
    VERIFY_ARE_EQUAL(expectedSecond, boundary.second);

    Log::Comment(L"Find middle word from its end.");
    boundary = si.GetWordBoundary({ 16, 0 });
    VERIFY_ARE_EQUAL(expectedFirst, boundary.first);
    VERIFY_ARE_EQUAL(expectedSecond, boundary.second);

    Log::Comment(L"Find end word from its front.");
    expectedFirst = { 32, 0 };
    expectedSecond = { 43, 0 };
    boundary = si.GetWordBoundary({ 32, 0 });
    VERIFY_ARE_EQUAL(expectedFirst, boundary.first);
    VERIFY_ARE_EQUAL(expectedSecond, boundary.second);

    Log::Comment(L"Find end word from its middle.");
    boundary = si.GetWordBoundary({ 39, 0 });
    VERIFY_ARE_EQUAL(expectedFirst, boundary.first);
    VERIFY_ARE_EQUAL(expectedSecond, boundary.second);

    Log::Comment(L"Find end word from its end.");
    boundary = si.GetWordBoundary({ 43, 0 });
    VERIFY_ARE_EQUAL(expectedFirst, boundary.first);
    VERIFY_ARE_EQUAL(expectedSecond, boundary.second);

    Log::Comment(L"Find a word starting from a boundary character.");
    expectedFirst = { 8, 0 };
    expectedSecond = { 12, 0 };
    boundary = si.GetWordBoundary({ 12, 0 });
    VERIFY_ARE_EQUAL(expectedFirst, boundary.first);
    VERIFY_ARE_EQUAL(expectedSecond, boundary.second);
}

void ScreenBufferTests::GetWordBoundaryTrimZeros(const bool on)
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();

    const auto text = L"000fe12 0xfe12 0Xfe12 0nfe12 0Nfe12";
    const auto length = wcslen(text);

    // Make the buffer as big as our test text.
    const COORD newBufferSize = { gsl::narrow<SHORT>(length), 10 };
    VERIFY_SUCCEEDED(si.GetTextBuffer().ResizeTraditional(newBufferSize));

    const OutputCellIterator it(text, si.GetAttributes());
    si.Write(it, { 0, 0 });

    gci.SetTrimLeadingZeros(on);

    COORD expectedFirst;
    COORD expectedSecond;
    std::pair<COORD, COORD> boundary;

    Log::Comment(L"Find lead with 000");
    expectedFirst = on ? COORD{ 3, 0 } : COORD{ 0, 0 };
    expectedSecond = COORD{ 7, 0 };
    boundary = si.GetWordBoundary({ 0, 0 });
    VERIFY_ARE_EQUAL(expectedFirst, boundary.first);
    VERIFY_ARE_EQUAL(expectedSecond, boundary.second);

    Log::Comment(L"Find lead with 0x");
    expectedFirst = COORD{ 8, 0 };
    expectedSecond = COORD{ 14, 0 };
    boundary = si.GetWordBoundary({ 8, 0 });
    VERIFY_ARE_EQUAL(expectedFirst, boundary.first);
    VERIFY_ARE_EQUAL(expectedSecond, boundary.second);

    Log::Comment(L"Find lead with 0X");
    expectedFirst = COORD{ 15, 0 };
    expectedSecond = COORD{ 21, 0 };
    boundary = si.GetWordBoundary({ 15, 0 });
    VERIFY_ARE_EQUAL(expectedFirst, boundary.first);
    VERIFY_ARE_EQUAL(expectedSecond, boundary.second);

    Log::Comment(L"Find lead with 0n");
    expectedFirst = COORD{ 22, 0 };
    expectedSecond = COORD{ 28, 0 };
    boundary = si.GetWordBoundary({ 22, 0 });
    VERIFY_ARE_EQUAL(expectedFirst, boundary.first);
    VERIFY_ARE_EQUAL(expectedSecond, boundary.second);

    Log::Comment(L"Find lead with 0N");
    expectedFirst = on ? COORD{ 30, 0 } : COORD{ 29, 0 };
    expectedSecond = COORD{ 35, 0 };
    boundary = si.GetWordBoundary({ 29, 0 });
    VERIFY_ARE_EQUAL(expectedFirst, boundary.first);
    VERIFY_ARE_EQUAL(expectedSecond, boundary.second);
}

void ScreenBufferTests::GetWordBoundaryTrimZerosOn()
{
    GetWordBoundaryTrimZeros(true);
}

void ScreenBufferTests::GetWordBoundaryTrimZerosOff()
{
    GetWordBoundaryTrimZeros(false);
}

void ScreenBufferTests::TestAltBufferCursorState()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.LockConsole(); // Lock must be taken to manipulate buffer.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    Log::Comment(L"Creating one alternate buffer");
    auto& original = gci.GetActiveOutputBuffer();
    VERIFY_IS_NULL(original._psiAlternateBuffer);
    VERIFY_IS_NULL(original._psiMainBuffer);

    NTSTATUS Status = original.UseAlternateScreenBuffer();
    if (VERIFY_IS_TRUE(NT_SUCCESS(Status)))
    {
        Log::Comment(L"Alternate buffer successfully created");
        auto& alternate = gci.GetActiveOutputBuffer();
        // Make sure that when the test is done, we switch back to the main buffer.
        // Otherwise, one test could pollute another.
        auto useMain = wil::scope_exit([&] { alternate.UseMainScreenBuffer(); });

        const auto* pMain = &original;
        const auto* pAlt = &alternate;
        // Validate that the pointers were mapped appropriately to link
        //      alternate and main buffers
        VERIFY_ARE_NOT_EQUAL(pMain, pAlt);
        VERIFY_ARE_EQUAL(pAlt, original._psiAlternateBuffer);
        VERIFY_ARE_EQUAL(pMain, alternate._psiMainBuffer);
        VERIFY_IS_NULL(original._psiMainBuffer);
        VERIFY_IS_NULL(alternate._psiAlternateBuffer);

        auto& mainCursor = original.GetTextBuffer().GetCursor();
        auto& altCursor = alternate.GetTextBuffer().GetCursor();

        // Validate that the cursor state was copied appropriately into the
        //      alternate buffer
        VERIFY_ARE_EQUAL(mainCursor.GetSize(), altCursor.GetSize());
        VERIFY_ARE_EQUAL(mainCursor.GetColor(), altCursor.GetColor());
        VERIFY_ARE_EQUAL(mainCursor.GetType(), altCursor.GetType());
    }
}

void ScreenBufferTests::TestAltBufferVtDispatching()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.LockConsole(); // Lock must be taken to manipulate buffer.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });
    Log::Comment(L"Creating one alternate buffer");
    auto& mainBuffer = gci.GetActiveOutputBuffer();
    // Make sure we're in VT mode
    WI_SetFlag(mainBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    // Make sure we're suing the default attributes at the start of the test,
    // Otherwise they could be polluted from a previous test.
    mainBuffer.SetAttributes(gci.GetDefaultAttributes());

    VERIFY_IS_NULL(mainBuffer._psiAlternateBuffer);
    VERIFY_IS_NULL(mainBuffer._psiMainBuffer);

    NTSTATUS Status = mainBuffer.UseAlternateScreenBuffer();
    if (VERIFY_IS_TRUE(NT_SUCCESS(Status)))
    {
        Log::Comment(L"Alternate buffer successfully created");
        auto& alternate = gci.GetActiveOutputBuffer();
        // Make sure that when the test is done, we switch back to the main buffer.
        // Otherwise, one test could pollute another.
        auto useMain = wil::scope_exit([&] { alternate.UseMainScreenBuffer(); });
        // Manually turn on VT mode - usually gci enables this for you.
        WI_SetFlag(alternate.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);

        const auto* pMain = &mainBuffer;
        const auto* pAlt = &alternate;
        // Validate that the pointers were mapped appropriately to link
        //      alternate and main buffers
        VERIFY_ARE_NOT_EQUAL(pMain, pAlt);
        VERIFY_ARE_EQUAL(pAlt, mainBuffer._psiAlternateBuffer);
        VERIFY_ARE_EQUAL(pMain, alternate._psiMainBuffer);
        VERIFY_IS_NULL(mainBuffer._psiMainBuffer);
        VERIFY_IS_NULL(alternate._psiAlternateBuffer);

        auto& mainCursor = mainBuffer.GetTextBuffer().GetCursor();
        auto& altCursor = alternate.GetTextBuffer().GetCursor();

        const COORD origin = { 0, 0 };
        mainCursor.SetPosition(origin);
        altCursor.SetPosition(origin);
        Log::Comment(NoThrowString().Format(L"Make sure the viewport is at 0,0"));
        VERIFY_SUCCEEDED(mainBuffer.SetViewportOrigin(true, origin, true));
        VERIFY_SUCCEEDED(alternate.SetViewportOrigin(true, origin, true));
        VERIFY_ARE_EQUAL(origin, mainCursor.GetPosition());
        VERIFY_ARE_EQUAL(origin, altCursor.GetPosition());

        // We're going to write some data to either the main buffer or the alt
        //  buffer, as if we were using the API.

        std::unique_ptr<WriteData> waiter;
        std::wstring seq = L"\x1b[5;6H";
        size_t seqCb = 2 * seq.size();
        VERIFY_SUCCEEDED(DoWriteConsole(&seq[0], &seqCb, mainBuffer, waiter));

        VERIFY_ARE_EQUAL(COORD({ 0, 0 }), mainCursor.GetPosition());
        // recall: vt coordinates are (row, column), 1-indexed
        VERIFY_ARE_EQUAL(COORD({ 5, 4 }), altCursor.GetPosition());

        const TextAttribute expectedDefaults = gci.GetDefaultAttributes();
        TextAttribute expectedRgb = expectedDefaults;
        expectedRgb.SetBackground(RGB(255, 0, 255));

        VERIFY_ARE_EQUAL(expectedDefaults, mainBuffer.GetAttributes());
        VERIFY_ARE_EQUAL(expectedDefaults, alternate.GetAttributes());

        seq = L"\x1b[48;2;255;0;255m";
        seqCb = 2 * seq.size();
        VERIFY_SUCCEEDED(DoWriteConsole(&seq[0], &seqCb, mainBuffer, waiter));

        VERIFY_ARE_EQUAL(expectedDefaults, mainBuffer.GetAttributes());
        VERIFY_ARE_EQUAL(expectedRgb, alternate.GetAttributes());

        seq = L"X";
        seqCb = 2 * seq.size();
        VERIFY_SUCCEEDED(DoWriteConsole(&seq[0], &seqCb, mainBuffer, waiter));

        VERIFY_ARE_EQUAL(COORD({ 0, 0 }), mainCursor.GetPosition());
        VERIFY_ARE_EQUAL(COORD({ 6, 4 }), altCursor.GetPosition());

        // Recall we didn't print an 'X' to the main buffer, so there's no
        //      char to inspect the attributes of.
        const ROW& altRow = alternate.GetTextBuffer().GetRowByOffset(altCursor.GetPosition().Y);
        const auto altAttrRow = &altRow.GetAttrRow();
        const std::vector<TextAttribute> altAttrs{ altAttrRow->begin(), altAttrRow->end() };
        const auto altAttrA = altAttrs[altCursor.GetPosition().X - 1];
        VERIFY_ARE_EQUAL(expectedRgb, altAttrA);
    }
}

void ScreenBufferTests::SetDefaultsIndividuallyBothDefault()
{
    // Tests MSFT:19828103

    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const TextBuffer& tbi = si.GetTextBuffer();
    StateMachine& stateMachine = si.GetStateMachine();
    Cursor& cursor = si.GetTextBuffer().GetCursor();

    Log::Comment(NoThrowString().Format(L"Make sure the viewport is at 0,0"));
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, COORD({ 0, 0 }), true));
    cursor.SetPosition({ 0, 0 });

    COLORREF magenta = RGB(255, 0, 255);
    COLORREF yellow = RGB(255, 255, 0);
    COLORREF brightGreen = gci.GetColorTableEntry(::XtermToWindowsIndex(10));
    COLORREF darkBlue = gci.GetColorTableEntry(::XtermToWindowsIndex(4));

    gci.SetDefaultForegroundColor(yellow);
    gci.SetDefaultBackgroundColor(magenta);
    si.SetDefaultAttributes(gci.GetDefaultAttributes(), { gci.GetPopupFillAttribute() });

    Log::Comment(NoThrowString().Format(L"Write 6 X's:"));
    Log::Comment(NoThrowString().Format(L"  The first in default-fg on default-bg (yellow on magenta)"));
    Log::Comment(NoThrowString().Format(L"  The second with bright-green on dark-blue"));
    Log::Comment(NoThrowString().Format(L"  The third with default-fg on dark-blue"));
    Log::Comment(NoThrowString().Format(L"  The fourth in default-fg on default-bg (yellow on magenta)"));
    Log::Comment(NoThrowString().Format(L"  The fifth with bright-green on dark-blue"));
    Log::Comment(NoThrowString().Format(L"  The sixth with bright-green on default-bg"));

    std::wstring seq = L"\x1b[m"; // Reset to defaults
    stateMachine.ProcessString(seq);
    seq = L"X";
    stateMachine.ProcessString(seq);

    seq = L"\x1b[92;44m"; // bright-green on dark-blue
    stateMachine.ProcessString(seq);
    seq = L"X";
    stateMachine.ProcessString(seq);

    seq = L"\x1b[39m"; // reset fg
    stateMachine.ProcessString(seq);
    seq = L"X";
    stateMachine.ProcessString(seq);

    seq = L"\x1b[49m"; // reset bg
    stateMachine.ProcessString(seq);
    seq = L"X";
    stateMachine.ProcessString(seq);

    seq = L"\x1b[92;44m"; // bright-green on dark-blue
    stateMachine.ProcessString(seq);
    seq = L"X";
    stateMachine.ProcessString(seq);

    seq = L"\x1b[49m"; // reset bg
    stateMachine.ProcessString(seq);
    seq = L"X";
    stateMachine.ProcessString(seq);

    // See the log comment above for description of these values.
    TextAttribute expectedDefaults{};
    TextAttribute expectedTwo{ FOREGROUND_GREEN | FOREGROUND_INTENSITY | BACKGROUND_BLUE };
    TextAttribute expectedThree{ FOREGROUND_GREEN | FOREGROUND_INTENSITY | BACKGROUND_BLUE };
    expectedThree.SetDefaultForeground();
    // Four is the same as Defaults
    // Five is the same as two
    TextAttribute expectedSix{ FOREGROUND_GREEN | FOREGROUND_INTENSITY | BACKGROUND_BLUE };
    expectedSix.SetDefaultBackground();

    COORD expectedCursor{ 6, 0 };
    VERIFY_ARE_EQUAL(expectedCursor, cursor.GetPosition());

    const ROW& row = tbi.GetRowByOffset(0);
    const auto attrRow = &row.GetAttrRow();
    const std::vector<TextAttribute> attrs{ attrRow->begin(), attrRow->end() };
    const auto attrA = attrs[0];
    const auto attrB = attrs[1];
    const auto attrC = attrs[2];
    const auto attrD = attrs[3];
    const auto attrE = attrs[4];
    const auto attrF = attrs[5];

    LOG_ATTR(attrA);
    LOG_ATTR(attrB);
    LOG_ATTR(attrC);
    LOG_ATTR(attrD);
    LOG_ATTR(attrE);
    LOG_ATTR(attrF);

    VERIFY_ARE_EQUAL(false, attrA.IsLegacy());
    VERIFY_ARE_EQUAL(true, attrB.IsLegacy());
    VERIFY_ARE_EQUAL(false, attrC.IsLegacy());
    VERIFY_ARE_EQUAL(false, attrD.IsLegacy());
    VERIFY_ARE_EQUAL(true, attrE.IsLegacy());
    VERIFY_ARE_EQUAL(false, attrF.IsLegacy());

    VERIFY_ARE_EQUAL(expectedDefaults, attrA);
    VERIFY_ARE_EQUAL(expectedTwo, attrB);
    VERIFY_ARE_EQUAL(expectedThree, attrC);
    VERIFY_ARE_EQUAL(expectedDefaults, attrD);
    VERIFY_ARE_EQUAL(expectedTwo, attrE);
    VERIFY_ARE_EQUAL(expectedSix, attrF);

    VERIFY_ARE_EQUAL(yellow, gci.LookupForegroundColor(attrA));
    VERIFY_ARE_EQUAL(brightGreen, gci.LookupForegroundColor(attrB));
    VERIFY_ARE_EQUAL(yellow, gci.LookupForegroundColor(attrC));
    VERIFY_ARE_EQUAL(yellow, gci.LookupForegroundColor(attrD));
    VERIFY_ARE_EQUAL(brightGreen, gci.LookupForegroundColor(attrE));
    VERIFY_ARE_EQUAL(brightGreen, gci.LookupForegroundColor(attrF));

    VERIFY_ARE_EQUAL(magenta, gci.LookupBackgroundColor(attrA));
    VERIFY_ARE_EQUAL(darkBlue, gci.LookupBackgroundColor(attrB));
    VERIFY_ARE_EQUAL(darkBlue, gci.LookupBackgroundColor(attrC));
    VERIFY_ARE_EQUAL(magenta, gci.LookupBackgroundColor(attrD));
    VERIFY_ARE_EQUAL(darkBlue, gci.LookupBackgroundColor(attrE));
    VERIFY_ARE_EQUAL(magenta, gci.LookupBackgroundColor(attrF));
}

void ScreenBufferTests::SetDefaultsTogether()
{
    // Tests MSFT:19828103

    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const TextBuffer& tbi = si.GetTextBuffer();
    StateMachine& stateMachine = si.GetStateMachine();
    Cursor& cursor = si.GetTextBuffer().GetCursor();

    Log::Comment(NoThrowString().Format(L"Make sure the viewport is at 0,0"));
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, COORD({ 0, 0 }), true));
    cursor.SetPosition({ 0, 0 });

    COLORREF magenta = RGB(255, 0, 255);
    COLORREF yellow = RGB(255, 255, 0);
    COLORREF color250 = gci.GetColorTableEntry(250);

    gci.SetDefaultForegroundColor(yellow);
    gci.SetDefaultBackgroundColor(magenta);
    si.SetDefaultAttributes(gci.GetDefaultAttributes(), { gci.GetPopupFillAttribute() });

    Log::Comment(NoThrowString().Format(L"Write 6 X's:"));
    Log::Comment(NoThrowString().Format(L"  The first in default-fg on default-bg (yellow on magenta)"));
    Log::Comment(NoThrowString().Format(L"  The second with default-fg on xterm(250)"));
    Log::Comment(NoThrowString().Format(L"  The third with defaults again"));

    std::wstring seq = L"\x1b[m"; // Reset to defaults
    stateMachine.ProcessString(seq);
    seq = L"X";
    stateMachine.ProcessString(seq);

    seq = L"\x1b[48;5;250m"; // bright-green on dark-blue
    stateMachine.ProcessString(seq);
    seq = L"X";
    stateMachine.ProcessString(seq);

    seq = L"\x1b[39;49m"; // reset fg
    stateMachine.ProcessString(seq);
    seq = L"X";
    stateMachine.ProcessString(seq);

    // See the log comment above for description of these values.
    TextAttribute expectedDefaults{};
    TextAttribute expectedTwo{};
    expectedTwo.SetBackground(color250);

    COORD expectedCursor{ 3, 0 };
    VERIFY_ARE_EQUAL(expectedCursor, cursor.GetPosition());

    const ROW& row = tbi.GetRowByOffset(0);
    const auto attrRow = &row.GetAttrRow();
    const std::vector<TextAttribute> attrs{ attrRow->begin(), attrRow->end() };
    const auto attrA = attrs[0];
    const auto attrB = attrs[1];
    const auto attrC = attrs[2];

    LOG_ATTR(attrA);
    LOG_ATTR(attrB);
    LOG_ATTR(attrC);

    VERIFY_ARE_EQUAL(false, attrA.IsLegacy());
    VERIFY_ARE_EQUAL(false, attrB.IsLegacy());
    VERIFY_ARE_EQUAL(false, attrC.IsLegacy());

    VERIFY_ARE_EQUAL(expectedDefaults, attrA);
    VERIFY_ARE_EQUAL(expectedTwo, attrB);
    VERIFY_ARE_EQUAL(expectedDefaults, attrC);

    VERIFY_ARE_EQUAL(yellow, gci.LookupForegroundColor(attrA));
    VERIFY_ARE_EQUAL(yellow, gci.LookupForegroundColor(attrB));
    VERIFY_ARE_EQUAL(yellow, gci.LookupForegroundColor(attrC));

    VERIFY_ARE_EQUAL(magenta, gci.LookupBackgroundColor(attrA));
    VERIFY_ARE_EQUAL(color250, gci.LookupBackgroundColor(attrB));
    VERIFY_ARE_EQUAL(magenta, gci.LookupBackgroundColor(attrC));
}

void ScreenBufferTests::ReverseResetWithDefaultBackground()
{
    // Tests MSFT:19694089
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const TextBuffer& tbi = si.GetTextBuffer();
    StateMachine& stateMachine = si.GetStateMachine();
    Cursor& cursor = si.GetTextBuffer().GetCursor();

    Log::Comment(NoThrowString().Format(L"Make sure the viewport is at 0,0"));
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, COORD({ 0, 0 }), true));
    cursor.SetPosition({ 0, 0 });

    COLORREF magenta = RGB(255, 0, 255);

    gci.SetDefaultForegroundColor(INVALID_COLOR);
    gci.SetDefaultBackgroundColor(magenta);
    si.SetDefaultAttributes(gci.GetDefaultAttributes(), { gci.GetPopupFillAttribute() });

    Log::Comment(NoThrowString().Format(L"Write 3 X's:"));
    Log::Comment(NoThrowString().Format(L"  The first in default-attr on default color (magenta)"));
    Log::Comment(NoThrowString().Format(L"  The second with reversed attrs"));
    Log::Comment(NoThrowString().Format(L"  The third after resetting the attrs back"));

    std::wstring seq = L"X";
    stateMachine.ProcessString(seq);
    seq = L"\x1b[7m";
    stateMachine.ProcessString(seq);
    seq = L"X";
    stateMachine.ProcessString(seq);
    seq = L"\x1b[27m";
    stateMachine.ProcessString(seq);
    seq = L"X";
    stateMachine.ProcessString(seq);

    TextAttribute expectedDefaults{ gci.GetFillAttribute() };
    expectedDefaults.SetDefaultBackground();
    TextAttribute expectedReversed = expectedDefaults;
    expectedReversed.Invert();

    COORD expectedCursor{ 3, 0 };
    VERIFY_ARE_EQUAL(expectedCursor, cursor.GetPosition());

    const ROW& row = tbi.GetRowByOffset(0);
    const auto attrRow = &row.GetAttrRow();
    const std::vector<TextAttribute> attrs{ attrRow->begin(), attrRow->end() };
    const auto attrA = attrs[0];
    const auto attrB = attrs[1];
    const auto attrC = attrs[2];

    LOG_ATTR(attrA);
    LOG_ATTR(attrB);
    LOG_ATTR(attrC);

    VERIFY_ARE_EQUAL(false, attrA.IsLegacy());
    VERIFY_ARE_EQUAL(false, attrB.IsLegacy());
    VERIFY_ARE_EQUAL(false, attrC.IsLegacy());

    VERIFY_ARE_EQUAL(false, WI_IsFlagSet(attrA.GetMetaAttributes(), COMMON_LVB_REVERSE_VIDEO));
    VERIFY_ARE_EQUAL(true, WI_IsFlagSet(attrB.GetMetaAttributes(), COMMON_LVB_REVERSE_VIDEO));
    VERIFY_ARE_EQUAL(false, WI_IsFlagSet(attrC.GetMetaAttributes(), COMMON_LVB_REVERSE_VIDEO));

    VERIFY_ARE_EQUAL(expectedDefaults, attrA);
    VERIFY_ARE_EQUAL(expectedReversed, attrB);
    VERIFY_ARE_EQUAL(expectedDefaults, attrC);

    VERIFY_ARE_EQUAL(magenta, gci.LookupBackgroundColor(attrA));
    VERIFY_ARE_EQUAL(magenta, gci.LookupForegroundColor(attrB));
    VERIFY_ARE_EQUAL(magenta, gci.LookupBackgroundColor(attrC));
}

void ScreenBufferTests::BackspaceDefaultAttrs()
{
    // Created for MSFT:19735050, but doesn't actually test that.
    // That bug actually involves the input line, and that needs to use
    //      TextAttributes instead of WORDs

    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const TextBuffer& tbi = si.GetTextBuffer();
    StateMachine& stateMachine = si.GetStateMachine();
    Cursor& cursor = si.GetTextBuffer().GetCursor();

    Log::Comment(NoThrowString().Format(L"Make sure the viewport is at 0,0"));
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, COORD({ 0, 0 }), true));
    cursor.SetPosition({ 0, 0 });

    COLORREF magenta = RGB(255, 0, 255);

    gci.SetDefaultBackgroundColor(magenta);
    si.SetDefaultAttributes(gci.GetDefaultAttributes(), { gci.GetPopupFillAttribute() });

    Log::Comment(NoThrowString().Format(L"Write 2 X's, then backspace one."));

    std::wstring seq = L"\x1b[m";
    stateMachine.ProcessString(seq);
    seq = L"XX";
    stateMachine.ProcessString(seq);

    seq = UNICODE_BACKSPACE;
    stateMachine.ProcessString(seq);

    TextAttribute expectedDefaults{};
    expectedDefaults.SetDefaultBackground();

    COORD expectedCursor{ 1, 0 };
    VERIFY_ARE_EQUAL(expectedCursor, cursor.GetPosition());

    const ROW& row = tbi.GetRowByOffset(0);
    const auto attrRow = &row.GetAttrRow();
    const std::vector<TextAttribute> attrs{ attrRow->begin(), attrRow->end() };
    const auto attrA = attrs[0];
    const auto attrB = attrs[1];

    LOG_ATTR(attrA);
    LOG_ATTR(attrB);

    VERIFY_ARE_EQUAL(false, attrA.IsLegacy());
    VERIFY_ARE_EQUAL(false, attrB.IsLegacy());

    VERIFY_ARE_EQUAL(expectedDefaults, attrA);
    VERIFY_ARE_EQUAL(expectedDefaults, attrB);

    VERIFY_ARE_EQUAL(magenta, gci.LookupBackgroundColor(attrA));
    VERIFY_ARE_EQUAL(magenta, gci.LookupBackgroundColor(attrB));
}

void ScreenBufferTests::BackspaceDefaultAttrsWriteCharsLegacy()
{
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:writeSingly", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:writeCharsLegacyMode", L"{0, 1, 2, 3, 4, 5, 6, 7}")
    END_TEST_METHOD_PROPERTIES();

    bool writeSingly;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"writeSingly", writeSingly), L"Write one at a time = true, all at the same time = false");

    DWORD writeCharsLegacyMode;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"writeCharsLegacyMode", writeCharsLegacyMode), L"");

    // Created for MSFT:19735050.
    // Kinda the same as above, but with WriteCharsLegacy instead.
    // The variable that really breaks this scenario

    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const TextBuffer& tbi = si.GetTextBuffer();
    StateMachine& stateMachine = si.GetStateMachine();
    Cursor& cursor = si.GetTextBuffer().GetCursor();

    Log::Comment(NoThrowString().Format(L"Make sure the viewport is at 0,0"));
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, COORD({ 0, 0 }), true));
    cursor.SetPosition({ 0, 0 });

    COLORREF magenta = RGB(255, 0, 255);

    gci.SetDefaultBackgroundColor(magenta);
    si.SetDefaultAttributes(gci.GetDefaultAttributes(), { gci.GetPopupFillAttribute() });

    Log::Comment(NoThrowString().Format(L"Write 2 X's, then backspace one."));

    std::wstring seq = L"\x1b[m";
    stateMachine.ProcessString(seq);

    if (writeSingly)
    {
        wchar_t* str = L"X";
        size_t seqCb = 2;
        VERIFY_SUCCESS_NTSTATUS(WriteCharsLegacy(si, str, str, str, &seqCb, nullptr, cursor.GetPosition().X, writeCharsLegacyMode, nullptr));
        VERIFY_SUCCESS_NTSTATUS(WriteCharsLegacy(si, str, str, str, &seqCb, nullptr, cursor.GetPosition().X, writeCharsLegacyMode, nullptr));
        str = L"\x08";
        VERIFY_SUCCESS_NTSTATUS(WriteCharsLegacy(si, str, str, str, &seqCb, nullptr, cursor.GetPosition().X, writeCharsLegacyMode, nullptr));
    }
    else
    {
        wchar_t* str = L"XX\x08";
        size_t seqCb = 6;
        VERIFY_SUCCESS_NTSTATUS(WriteCharsLegacy(si, str, str, str, &seqCb, nullptr, cursor.GetPosition().X, writeCharsLegacyMode, nullptr));
    }

    TextAttribute expectedDefaults{};
    expectedDefaults.SetDefaultBackground();

    COORD expectedCursor{ 1, 0 };
    VERIFY_ARE_EQUAL(expectedCursor, cursor.GetPosition());

    const ROW& row = tbi.GetRowByOffset(0);
    const auto attrRow = &row.GetAttrRow();
    const std::vector<TextAttribute> attrs{ attrRow->begin(), attrRow->end() };
    const auto attrA = attrs[0];
    const auto attrB = attrs[1];

    LOG_ATTR(attrA);
    LOG_ATTR(attrB);

    VERIFY_ARE_EQUAL(false, attrA.IsLegacy());
    VERIFY_ARE_EQUAL(false, attrB.IsLegacy());

    VERIFY_ARE_EQUAL(expectedDefaults, attrA);
    VERIFY_ARE_EQUAL(expectedDefaults, attrB);

    VERIFY_ARE_EQUAL(magenta, gci.LookupBackgroundColor(attrA));
    VERIFY_ARE_EQUAL(magenta, gci.LookupBackgroundColor(attrB));
}

void ScreenBufferTests::BackspaceDefaultAttrsInPrompt()
{
    // Tests MSFT:19853701 - when you edit the prompt line at a bash prompt,
    //  make sure that the end of the line isn't filled with default/garbage attributes.

    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const TextBuffer& tbi = si.GetTextBuffer();
    StateMachine& stateMachine = si.GetStateMachine();
    Cursor& cursor = si.GetTextBuffer().GetCursor();
    // Make sure we're in VT mode
    WI_SetFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    VERIFY_IS_TRUE(WI_IsFlagSet(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING));

    Log::Comment(NoThrowString().Format(L"Make sure the viewport is at 0,0"));
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, COORD({ 0, 0 }), true));
    cursor.SetPosition({ 0, 0 });

    COLORREF magenta = RGB(255, 0, 255);

    gci.SetDefaultBackgroundColor(magenta);
    si.SetDefaultAttributes(gci.GetDefaultAttributes(), { gci.GetPopupFillAttribute() });
    TextAttribute expectedDefaults{};

    Log::Comment(NoThrowString().Format(L"Write 3 X's, move to the left, then delete-char the second."));
    Log::Comment(NoThrowString().Format(L"This emulates editing the prompt line on bash"));

    std::wstring seq = L"\x1b[m";
    stateMachine.ProcessString(seq);
    Log::Comment(NoThrowString().Format(
        L"Clear the screen - make sure the line is filled with the current attributes."));
    seq = L"\x1b[2J";
    stateMachine.ProcessString(seq);

    const auto viewport = si.GetViewport();
    const ROW& row = tbi.GetRowByOffset(cursor.GetPosition().Y);
    const auto attrRow = &row.GetAttrRow();

    {
        SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);
        Log::Comment(NoThrowString().Format(
            L"Make sure the row contains what we're expecting before we start."
            L"It should entirely be filled with defaults"));

        const std::vector<TextAttribute> initialAttrs{ attrRow->begin(), attrRow->end() };
        for (int x = 0; x <= viewport.RightInclusive(); x++)
        {
            const auto& attr = initialAttrs[x];
            VERIFY_ARE_EQUAL(expectedDefaults, attr);
        }
    }
    Log::Comment(NoThrowString().Format(
        L"Print 'XXX', move the cursor left 2, delete a character."));

    seq = L"XXX";
    stateMachine.ProcessString(seq);
    seq = L"\x1b[2D";
    stateMachine.ProcessString(seq);
    seq = L"\x1b[P";
    stateMachine.ProcessString(seq);

    COORD expectedCursor{ 1, 1 }; // We're expecting y=1, because the 2J above
        // should have moved the viewport down a line.
    VERIFY_ARE_EQUAL(expectedCursor, cursor.GetPosition());

    const std::vector<TextAttribute> attrs{ attrRow->begin(), attrRow->end() };
    for (int x = 0; x <= viewport.RightInclusive(); x++)
    {
        const auto& attr = attrs[x];
        VERIFY_ARE_EQUAL(expectedDefaults, attr);
    }
}

void ScreenBufferTests::SetGlobalColorTable()
{
    // Created for MSFT:19723934.
    // Changing the value of the color table should apply to the attributes in
    //  both the alt AND main buffer. While many other properties should be
    //      reset upon returning to the main buffer, the color table is a
    //      global property. This behavior is consistent with other terminals
    //      tested.

    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.LockConsole(); // Lock must be taken to swap buffers.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    SCREEN_INFORMATION& mainBuffer = gci.GetActiveOutputBuffer();
    VERIFY_IS_FALSE(mainBuffer._IsAltBuffer());
    WI_SetFlag(mainBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    VERIFY_IS_TRUE(WI_IsFlagSet(mainBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING));

    StateMachine& stateMachine = mainBuffer.GetStateMachine();
    Cursor& mainCursor = mainBuffer.GetTextBuffer().GetCursor();

    Log::Comment(NoThrowString().Format(L"Make sure the viewport is at 0,0"));
    VERIFY_SUCCEEDED(mainBuffer.SetViewportOrigin(true, COORD({ 0, 0 }), true));
    mainCursor.SetPosition({ 0, 0 });

    const COLORREF originalRed = gci.GetColorTableEntry(4);
    const COLORREF testColor = RGB(0x11, 0x22, 0x33);
    VERIFY_ARE_NOT_EQUAL(originalRed, testColor);

    std::wstring seq = L"\x1b[41m";
    stateMachine.ProcessString(seq);
    seq = L"X";
    stateMachine.ProcessString(seq);
    COORD expectedCursor{ 1, 0 };
    VERIFY_ARE_EQUAL(expectedCursor, mainCursor.GetPosition());
    {
        const ROW& row = mainBuffer.GetTextBuffer().GetRowByOffset(mainCursor.GetPosition().Y);
        const auto attrRow = &row.GetAttrRow();
        const std::vector<TextAttribute> attrs{ attrRow->begin(), attrRow->end() };
        const auto attrA = attrs[0];
        LOG_ATTR(attrA);
        VERIFY_ARE_EQUAL(originalRed, gci.LookupBackgroundColor(attrA));
    }

    Log::Comment(NoThrowString().Format(L"Create an alt buffer"));

    VERIFY_SUCCEEDED(mainBuffer.UseAlternateScreenBuffer());
    SCREEN_INFORMATION& altBuffer = gci.GetActiveOutputBuffer();
    auto useMain = wil::scope_exit([&] { altBuffer.UseMainScreenBuffer(); });

    WI_SetFlag(altBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    VERIFY_IS_TRUE(WI_IsFlagSet(altBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING));

    Cursor& altCursor = altBuffer.GetTextBuffer().GetCursor();
    altCursor.SetPosition({ 0, 0 });

    Log::Comment(NoThrowString().Format(
        L"Print one X in red, should be the original red color"));
    seq = L"\x1b[41m";
    stateMachine.ProcessString(seq);
    seq = L"X";
    stateMachine.ProcessString(seq);
    VERIFY_ARE_EQUAL(expectedCursor, altCursor.GetPosition());
    {
        const ROW& row = altBuffer.GetTextBuffer().GetRowByOffset(altCursor.GetPosition().Y);
        const auto attrRow = &row.GetAttrRow();
        const std::vector<TextAttribute> attrs{ attrRow->begin(), attrRow->end() };
        const auto attrA = attrs[0];
        LOG_ATTR(attrA);
        VERIFY_ARE_EQUAL(originalRed, gci.LookupBackgroundColor(attrA));
    }

    Log::Comment(NoThrowString().Format(L"Change the value of red to RGB(0x11, 0x22, 0x33)"));
    seq = L"\x1b]4;1;rgb:11/22/33\x07";
    stateMachine.ProcessString(seq);
    Log::Comment(NoThrowString().Format(
        L"Print another X, both should be the new \"red\" color"));
    seq = L"X";
    stateMachine.ProcessString(seq);
    VERIFY_ARE_EQUAL(COORD({ 2, 0 }), altCursor.GetPosition());
    {
        const ROW& row = altBuffer.GetTextBuffer().GetRowByOffset(altCursor.GetPosition().Y);
        const auto attrRow = &row.GetAttrRow();
        const std::vector<TextAttribute> attrs{ attrRow->begin(), attrRow->end() };
        const auto attrA = attrs[0];
        const auto attrB = attrs[1];
        LOG_ATTR(attrA);
        LOG_ATTR(attrB);
        VERIFY_ARE_EQUAL(testColor, gci.LookupBackgroundColor(attrA));
        VERIFY_ARE_EQUAL(testColor, gci.LookupBackgroundColor(attrB));
    }

    Log::Comment(NoThrowString().Format(L"Switch back to the main buffer"));
    useMain.release();
    altBuffer.UseMainScreenBuffer();

    const auto& mainBufferPostSwitch = gci.GetActiveOutputBuffer();
    VERIFY_ARE_EQUAL(&mainBufferPostSwitch, &mainBuffer);

    Log::Comment(NoThrowString().Format(
        L"Print another X, both should be the new \"red\" color"));
    seq = L"X";
    stateMachine.ProcessString(seq);
    VERIFY_ARE_EQUAL(COORD({ 2, 0 }), mainCursor.GetPosition());
    {
        const ROW& row = mainBuffer.GetTextBuffer().GetRowByOffset(mainCursor.GetPosition().Y);
        const auto attrRow = &row.GetAttrRow();
        const std::vector<TextAttribute> attrs{ attrRow->begin(), attrRow->end() };
        const auto attrA = attrs[0];
        const auto attrB = attrs[1];
        LOG_ATTR(attrA);
        LOG_ATTR(attrB);
        VERIFY_ARE_EQUAL(testColor, gci.LookupBackgroundColor(attrA));
        VERIFY_ARE_EQUAL(testColor, gci.LookupBackgroundColor(attrB));
    }
}

void ScreenBufferTests::SetColorTableThreeDigits()
{
    // Created for MSFT:19723934.
    // Changing the value of the color table above index 99 should work

    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.LockConsole(); // Lock must be taken to swap buffers.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    SCREEN_INFORMATION& mainBuffer = gci.GetActiveOutputBuffer();
    VERIFY_IS_FALSE(mainBuffer._IsAltBuffer());
    WI_SetFlag(mainBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    VERIFY_IS_TRUE(WI_IsFlagSet(mainBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING));

    StateMachine& stateMachine = mainBuffer.GetStateMachine();
    Cursor& mainCursor = mainBuffer.GetTextBuffer().GetCursor();

    Log::Comment(NoThrowString().Format(L"Make sure the viewport is at 0,0"));
    VERIFY_SUCCEEDED(mainBuffer.SetViewportOrigin(true, COORD({ 0, 0 }), true));
    mainCursor.SetPosition({ 0, 0 });

    const COLORREF originalRed = gci.GetColorTableEntry(123);
    const COLORREF testColor = RGB(0x11, 0x22, 0x33);
    VERIFY_ARE_NOT_EQUAL(originalRed, testColor);

    std::wstring seq = L"\x1b[48;5;123m";
    stateMachine.ProcessString(seq);
    seq = L"X";
    stateMachine.ProcessString(seq);
    COORD expectedCursor{ 1, 0 };
    VERIFY_ARE_EQUAL(expectedCursor, mainCursor.GetPosition());
    {
        const ROW& row = mainBuffer.GetTextBuffer().GetRowByOffset(mainCursor.GetPosition().Y);
        const auto attrRow = &row.GetAttrRow();
        const std::vector<TextAttribute> attrs{ attrRow->begin(), attrRow->end() };
        const auto attrA = attrs[0];
        LOG_ATTR(attrA);
        VERIFY_ARE_EQUAL(originalRed, gci.LookupBackgroundColor(attrA));
    }

    Log::Comment(NoThrowString().Format(L"Create an alt buffer"));

    VERIFY_SUCCEEDED(mainBuffer.UseAlternateScreenBuffer());
    SCREEN_INFORMATION& altBuffer = gci.GetActiveOutputBuffer();
    auto useMain = wil::scope_exit([&] { altBuffer.UseMainScreenBuffer(); });

    WI_SetFlag(altBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    VERIFY_IS_TRUE(WI_IsFlagSet(altBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING));

    Cursor& altCursor = altBuffer.GetTextBuffer().GetCursor();
    altCursor.SetPosition({ 0, 0 });

    Log::Comment(NoThrowString().Format(
        L"Print one X in red, should be the original red color"));
    seq = L"\x1b[48;5;123m";
    stateMachine.ProcessString(seq);
    seq = L"X";
    stateMachine.ProcessString(seq);
    VERIFY_ARE_EQUAL(expectedCursor, altCursor.GetPosition());
    {
        const ROW& row = altBuffer.GetTextBuffer().GetRowByOffset(altCursor.GetPosition().Y);
        const auto attrRow = &row.GetAttrRow();
        const std::vector<TextAttribute> attrs{ attrRow->begin(), attrRow->end() };
        const auto attrA = attrs[0];
        LOG_ATTR(attrA);
        VERIFY_ARE_EQUAL(originalRed, gci.LookupBackgroundColor(attrA));
    }

    Log::Comment(NoThrowString().Format(L"Change the value of red to RGB(0x11, 0x22, 0x33)"));
    seq = L"\x1b]4;123;rgb:11/22/33\x07";
    stateMachine.ProcessString(seq);
    Log::Comment(NoThrowString().Format(
        L"Print another X, it should be the new \"red\" color"));
    // TODO MSFT:20105972 -
    // You shouldn't need to manually update the attributes again.
    seq = L"\x1b[48;5;123m";
    stateMachine.ProcessString(seq);
    seq = L"X";
    stateMachine.ProcessString(seq);
    VERIFY_ARE_EQUAL(COORD({ 2, 0 }), altCursor.GetPosition());
    {
        const ROW& row = altBuffer.GetTextBuffer().GetRowByOffset(altCursor.GetPosition().Y);
        const auto attrRow = &row.GetAttrRow();
        const std::vector<TextAttribute> attrs{ attrRow->begin(), attrRow->end() };
        const auto attrB = attrs[1];
        // TODO MSFT:20105972 - attrA and attrB should both be the same color now
        LOG_ATTR(attrB);
        VERIFY_ARE_EQUAL(testColor, gci.LookupBackgroundColor(attrB));
    }
}

void ScreenBufferTests::SetDefaultForegroundColor()
{
    // Setting the default foreground color should work

    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.LockConsole(); // Lock must be taken to swap buffers.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    SCREEN_INFORMATION& mainBuffer = gci.GetActiveOutputBuffer();
    VERIFY_IS_FALSE(mainBuffer._IsAltBuffer());
    WI_SetFlag(mainBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    VERIFY_IS_TRUE(WI_IsFlagSet(mainBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING));

    StateMachine& stateMachine = mainBuffer.GetStateMachine();

    COLORREF originalColor = gci.GetDefaultForegroundColor();
    COLORREF newColor = gci.GetDefaultForegroundColor();
    COLORREF testColor = RGB(0x33, 0x66, 0x99);
    VERIFY_ARE_NOT_EQUAL(originalColor, testColor);

    Log::Comment(L"Valid Hexadecimal Notation");
    std::wstring seq = L"\x1b]10;rgb:33/66/99\x1b\\";
    stateMachine.ProcessString(seq);

    newColor = gci.GetDefaultForegroundColor();
    VERIFY_ARE_EQUAL(testColor, newColor);

    Log::Comment(L"Valid Hexadecimal Notation");
    originalColor = newColor;
    testColor = RGB(0xff, 0xff, 0xff);
    seq = L"\x1b]10;rgb:ff/ff/ff\x1b\\";
    stateMachine.ProcessString(seq);

    newColor = gci.GetDefaultForegroundColor();
    VERIFY_ARE_EQUAL(testColor, newColor);

    Log::Comment(L"Invalid Decimal Notation");
    originalColor = newColor;
    testColor = RGB(153, 102, 51);
    seq = L"\x1b]10;rgb:153/102/51\x1b\\";
    stateMachine.ProcessString(seq);

    newColor = gci.GetDefaultForegroundColor();
    VERIFY_ARE_NOT_EQUAL(testColor, newColor);
    // it will, in fact leave the color the way it was
    VERIFY_ARE_EQUAL(originalColor, newColor);

    Log::Comment(L"Invalid syntax");
    testColor = RGB(153, 102, 51);
    seq = L"\x1b]10;99/66/33\x1b\\";
    stateMachine.ProcessString(seq);

    newColor = gci.GetDefaultForegroundColor();
    VERIFY_ARE_NOT_EQUAL(testColor, newColor);
    // it will, in fact leave the color the way it was
    VERIFY_ARE_EQUAL(originalColor, newColor);
}

void ScreenBufferTests::SetDefaultBackgroundColor()
{
    // Setting the default Background color should work

    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.LockConsole(); // Lock must be taken to swap buffers.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    SCREEN_INFORMATION& mainBuffer = gci.GetActiveOutputBuffer();
    VERIFY_IS_FALSE(mainBuffer._IsAltBuffer());
    WI_SetFlag(mainBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    VERIFY_IS_TRUE(WI_IsFlagSet(mainBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING));

    StateMachine& stateMachine = mainBuffer.GetStateMachine();

    COLORREF originalColor = gci.GetDefaultBackgroundColor();
    COLORREF newColor = gci.GetDefaultBackgroundColor();
    COLORREF testColor = RGB(0x33, 0x66, 0x99);
    VERIFY_ARE_NOT_EQUAL(originalColor, testColor);

    Log::Comment(L"Valid Hexadecimal Notation");
    std::wstring seq = L"\x1b]11;rgb:33/66/99\x1b\\";
    stateMachine.ProcessString(seq);

    newColor = gci.GetDefaultBackgroundColor();
    VERIFY_ARE_EQUAL(testColor, newColor);

    Log::Comment(L"Valid Hexadecimal Notation");
    originalColor = newColor;
    testColor = RGB(0xff, 0xff, 0xff);
    seq = L"\x1b]11;rgb:ff/ff/ff\x1b\\";
    stateMachine.ProcessString(seq);

    newColor = gci.GetDefaultBackgroundColor();
    VERIFY_ARE_EQUAL(testColor, newColor);

    Log::Comment(L"Invalid Decimal Notation");
    originalColor = newColor;
    testColor = RGB(153, 102, 51);
    seq = L"\x1b]11;rgb:153/102/51\x1b\\";
    stateMachine.ProcessString(seq);

    newColor = gci.GetDefaultBackgroundColor();
    VERIFY_ARE_NOT_EQUAL(testColor, newColor);
    // it will, in fact leave the color the way it was
    VERIFY_ARE_EQUAL(originalColor, newColor);

    Log::Comment(L"Invalid Syntax");
    testColor = RGB(153, 102, 51);
    seq = L"\x1b]11;99/66/33\x1b\\";
    stateMachine.ProcessString(seq);

    newColor = gci.GetDefaultBackgroundColor();
    VERIFY_ARE_NOT_EQUAL(testColor, newColor);
    // it will, in fact leave the color the way it was
    VERIFY_ARE_EQUAL(originalColor, newColor);
}

void ScreenBufferTests::DeleteCharsNearEndOfLine()
{
    // Created for MSFT:19888564.
    // There are some cases when you DCH N chars, where there are artifacts left
    //       from the previous contents of the row after the DCH finishes.
    // If you are deleting N chars,
    // and there are N+X chars left in the row after the cursor, such that X<N,
    // We'll move the X chars to the left, and delete X chars both at the cursor
    //       pos and at cursor.X+N, but the region of characters at
    //      [cursor.X+X, cursor.X+N] is left untouched.
    //
    // Which is the case:
    // `(d - 1 > v_w - 1 - c_x - d) && (v_w - 1 - c_x - d >= 0)`
    // where:
    // - `d`: num chars to delete
    // - `v_w`: viewport.Width()
    // - `c_x`: cursor.X
    //
    // Example: (this is tested by DeleteCharsNearEndOfLineSimpleFirstCase)
    // start with the following buffer contents, and the cursor on the "D"
    // [ABCDEFG ]
    //     ^
    // When you DCH(3) here, we are trying to delete the D, E and F.
    // We do that by shifting the contents of the line after the deleted
    // characters to the left. HOWEVER, there are only 2 chars left to move.
    // So (before the fix) the buffer end up like this:
    // [ABCG F  ]
    //     ^
    // The G and " " have moved, but the F did not get overwritten.

    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:dx", L"{1, 2, 3, 5, 8, 13, 21, 34}")
        TEST_METHOD_PROPERTY(L"Data:numCharsToDelete", L"{1, 2, 3, 5, 8, 13, 21, 34}")
    END_TEST_METHOD_PROPERTIES();

    int dx;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"dx", dx), L"Distance to move the cursor back into the line");

    int numCharsToDelete;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"numCharsToDelete", numCharsToDelete), L"Number of characters to delete");

    // let W = viewport.Width
    // Print W 'X' chars
    // Move to (0, W-dx)
    // DCH(numCharsToDelete)
    // There should be N 'X' chars, and then numSpaces spaces
    // where
    //      numSpaces = min(dx, numCharsToDelete)
    //      N = W - numSpaces

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& mainBuffer = gci.GetActiveOutputBuffer();
    auto& tbi = mainBuffer.GetTextBuffer();
    auto& stateMachine = mainBuffer.GetStateMachine();
    auto& mainCursor = tbi.GetCursor();
    auto& mainView = mainBuffer.GetViewport();

    VERIFY_ARE_EQUAL(COORD({ 0, 0 }), mainCursor.GetPosition());
    VERIFY_ARE_EQUAL(mainBuffer.GetBufferSize().Width(), mainView.Width());
    VERIFY_IS_GREATER_THAN(mainView.Width(), (dx + numCharsToDelete));

    std::wstring seq = L"X";
    for (int x = 0; x < mainView.Width(); x++)
    {
        stateMachine.ProcessString(seq);
    }

    VERIFY_ARE_EQUAL(COORD({ mainView.Width() - 1, 0 }), mainCursor.GetPosition());

    Log::Comment(NoThrowString().Format(
        L"row_i=[%s]",
        tbi.GetRowByOffset(0).GetText().c_str()));

    mainCursor.SetPosition({ mainView.Width() - static_cast<short>(dx), 0 });
    std::wstringstream ss;
    ss << L"\x1b[" << numCharsToDelete << L"P";
    seq = ss.str(); // Delete N chars
    stateMachine.ProcessString(seq);

    Log::Comment(NoThrowString().Format(
        L"row_f=[%s]",
        tbi.GetRowByOffset(0).GetText().c_str()));
    VERIFY_ARE_EQUAL(COORD({ mainView.Width() - static_cast<short>(dx), 0 }), mainCursor.GetPosition());
    auto iter = tbi.GetCellDataAt({ 0, 0 });
    auto expectedNumSpaces = std::min(dx, numCharsToDelete);
    for (int x = 0; x < mainView.Width() - expectedNumSpaces; x++)
    {
        SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);
        if (iter->Chars() != L"X")
        {
            Log::Comment(NoThrowString().Format(L"character [%d] was mismatched", x));
        }
        VERIFY_ARE_EQUAL(L"X", iter->Chars());
        iter++;
    }
    for (int x = mainView.Width() - expectedNumSpaces; x < mainView.Width(); x++)
    {
        if (iter->Chars() != L"\x20")
        {
            Log::Comment(NoThrowString().Format(L"character [%d] was mismatched", x));
        }
        VERIFY_ARE_EQUAL(L"\x20", iter->Chars());
        iter++;
    }
}

void ScreenBufferTests::DeleteCharsNearEndOfLineSimpleFirstCase()
{
    // Created for MSFT:19888564.
    // This is a single case that I'm absolutely sure will repro this bug -
    // DeleteCharsNearEndOfLine is the more comprehensive version of this test.
    // Write a string, move the cursor into it, then delete some chars.
    // There should be no artifacts left behind.

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& stateMachine = si.GetStateMachine();
    const auto newBufferWidth = 8;

    VERIFY_SUCCEEDED(si.ResizeScreenBuffer({ newBufferWidth, si.GetBufferSize().Height() }, false));
    auto& mainBuffer = gci.GetActiveOutputBuffer();

    const COORD newViewSize{ newBufferWidth, mainBuffer.GetViewport().Height() };
    mainBuffer.SetViewportSize(&newViewSize);
    auto& tbi = mainBuffer.GetTextBuffer();
    auto& mainView = mainBuffer.GetViewport();
    auto& mainCursor = tbi.GetCursor();

    VERIFY_ARE_EQUAL(COORD({ 0, 0 }), mainCursor.GetPosition());
    VERIFY_ARE_EQUAL(newBufferWidth, mainView.Width());
    VERIFY_ARE_EQUAL(mainBuffer.GetBufferSize().Width(), mainView.Width());

    std::wstring seq = L"ABCDEFG";
    stateMachine.ProcessString(seq);

    VERIFY_ARE_EQUAL(COORD({ 7, 0 }), mainCursor.GetPosition());
    // Place the cursor on the 'D'
    mainCursor.SetPosition({ 3, 0 });

    Log::Comment(NoThrowString().Format(L"before=[%s]", tbi.GetRowByOffset(0).GetText().c_str()));
    // Delete 3 chars - [D, E, F]
    std::wstringstream ss;
    ss << L"\x1b[" << 3 << L"P";
    seq = ss.str();
    stateMachine.ProcessString(seq);

    Log::Comment(NoThrowString().Format(L"after =[%s]", tbi.GetRowByOffset(0).GetText().c_str()));

    // Cursor shouldn't have moved
    VERIFY_ARE_EQUAL(COORD({ 3, 0 }), mainCursor.GetPosition());

    auto iter = tbi.GetCellDataAt({ 0, 0 });
    VERIFY_ARE_EQUAL(L"A", iter->Chars());
    iter++;
    VERIFY_ARE_EQUAL(L"B", iter->Chars());
    iter++;
    VERIFY_ARE_EQUAL(L"C", iter->Chars());
    iter++;
    VERIFY_ARE_EQUAL(L"G", iter->Chars());
    iter++;
    VERIFY_ARE_EQUAL(L"\x20", iter->Chars());
    iter++;
    VERIFY_ARE_EQUAL(L"\x20", iter->Chars());
    iter++;
    VERIFY_ARE_EQUAL(L"\x20", iter->Chars());
    iter++;
}

void ScreenBufferTests::DeleteCharsNearEndOfLineSimpleSecondCase()
{
    // Created for MSFT:19888564.
    // This is another single case that I'm absolutely sure will repro this bug
    // DeleteCharsNearEndOfLine is the more comprehensive version of this test.
    // Write a string, move the cursor into it, then delete some chars.
    // There should be no artifacts left behind.

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& stateMachine = si.GetStateMachine();

    const auto newBufferWidth = 8;
    VERIFY_SUCCEEDED(si.ResizeScreenBuffer({ newBufferWidth, si.GetBufferSize().Height() }, false));
    auto& mainBuffer = gci.GetActiveOutputBuffer();

    const COORD newViewSize{ newBufferWidth, mainBuffer.GetViewport().Height() };
    mainBuffer.SetViewportSize(&newViewSize);
    auto& tbi = mainBuffer.GetTextBuffer();
    auto& mainView = mainBuffer.GetViewport();
    auto& mainCursor = tbi.GetCursor();

    VERIFY_ARE_EQUAL(COORD({ 0, 0 }), mainCursor.GetPosition());
    VERIFY_ARE_EQUAL(newBufferWidth, mainView.Width());
    VERIFY_ARE_EQUAL(mainBuffer.GetBufferSize().Width(), mainView.Width());

    std::wstring seq = L"ABCDEFG";
    stateMachine.ProcessString(seq);

    VERIFY_ARE_EQUAL(COORD({ 7, 0 }), mainCursor.GetPosition());

    // Place the cursor on the 'C'
    mainCursor.SetPosition({ 2, 0 });

    Log::Comment(NoThrowString().Format(L"before=[%s]", tbi.GetRowByOffset(0).GetText().c_str()));

    // Delete 4 chars - [C, D, E, F]
    std::wstringstream ss;
    ss << L"\x1b[" << 4 << L"P";
    seq = ss.str();
    stateMachine.ProcessString(seq);

    Log::Comment(NoThrowString().Format(L"after =[%s]", tbi.GetRowByOffset(0).GetText().c_str()));

    VERIFY_ARE_EQUAL(COORD({ 2, 0 }), mainCursor.GetPosition());

    auto iter = tbi.GetCellDataAt({ 0, 0 });
    VERIFY_ARE_EQUAL(L"A", iter->Chars());
    iter++;
    VERIFY_ARE_EQUAL(L"B", iter->Chars());
    iter++;
    VERIFY_ARE_EQUAL(L"G", iter->Chars());
    iter++;
    VERIFY_ARE_EQUAL(L"\x20", iter->Chars());
    iter++;
    VERIFY_ARE_EQUAL(L"\x20", iter->Chars());
    iter++;
    VERIFY_ARE_EQUAL(L"\x20", iter->Chars());
    iter++;
    VERIFY_ARE_EQUAL(L"\x20", iter->Chars());
    iter++;
}

void ScreenBufferTests::DontResetColorsAboveVirtualBottom()
{
    // Created for MSFT:19989333.
    // Print some colored text, then scroll the viewport up, so the colored text
    //  is below the visible viewport. Change the colors, then write a character.
    // Both the old chars and the new char should have different colors, the
    //  first character should not have been reset to the new colors.

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si.GetTextBuffer().GetCursor();

    VERIFY_SUCCESS_NTSTATUS(si.SetViewportOrigin(true, { 0, 1 }, true));
    cursor.SetPosition({ 0, si.GetViewport().BottomInclusive() });
    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<COORD>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"viewport=%s", VerifyOutputTraits<SMALL_RECT>::ToString(si.GetViewport().ToInclusive()).GetBuffer()));
    const auto darkRed = gci.GetColorTableEntry(::XtermToWindowsIndex(1));
    const auto darkBlue = gci.GetColorTableEntry(::XtermToWindowsIndex(4));
    const auto darkBlack = gci.GetColorTableEntry(::XtermToWindowsIndex(0));
    const auto darkWhite = gci.GetColorTableEntry(::XtermToWindowsIndex(7));
    stateMachine.ProcessString(L"\x1b[31;44m");
    stateMachine.ProcessString(L"X");
    stateMachine.ProcessString(L"\x1b[m");
    stateMachine.ProcessString(L"X");

    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<COORD>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"viewport=%s", VerifyOutputTraits<SMALL_RECT>::ToString(si.GetViewport().ToInclusive()).GetBuffer()));
    VERIFY_ARE_EQUAL(2, cursor.GetPosition().X);
    {
        const ROW& row = tbi.GetRowByOffset(cursor.GetPosition().Y);
        const auto attrRow = &row.GetAttrRow();
        const std::vector<TextAttribute> attrs{ attrRow->begin(), attrRow->end() };
        const auto attrA = attrs[0];
        const auto attrB = attrs[1];
        LOG_ATTR(attrA);
        LOG_ATTR(attrB);
        VERIFY_ARE_EQUAL(darkRed, gci.LookupForegroundColor(attrA));
        VERIFY_ARE_EQUAL(darkBlue, gci.LookupBackgroundColor(attrA));

        VERIFY_ARE_EQUAL(darkWhite, gci.LookupForegroundColor(attrB));
        VERIFY_ARE_EQUAL(darkBlack, gci.LookupBackgroundColor(attrB));
    }

    Log::Comment(NoThrowString().Format(L"Emulate scrolling up with the mouse"));
    VERIFY_SUCCESS_NTSTATUS(si.SetViewportOrigin(true, { 0, 0 }, false));

    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<COORD>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"viewport=%s", VerifyOutputTraits<SMALL_RECT>::ToString(si.GetViewport().ToInclusive()).GetBuffer()));

    VERIFY_IS_GREATER_THAN(cursor.GetPosition().Y, si.GetViewport().BottomInclusive());

    stateMachine.ProcessString(L"X");

    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<COORD>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"viewport=%s", VerifyOutputTraits<SMALL_RECT>::ToString(si.GetViewport().ToInclusive()).GetBuffer()));

    VERIFY_ARE_EQUAL(3, cursor.GetPosition().X);
    {
        const ROW& row = tbi.GetRowByOffset(cursor.GetPosition().Y);
        const auto attrRow = &row.GetAttrRow();
        const std::vector<TextAttribute> attrs{ attrRow->begin(), attrRow->end() };
        const auto attrA = attrs[0];
        const auto attrB = attrs[1];
        const auto attrC = attrs[1];
        LOG_ATTR(attrA);
        LOG_ATTR(attrB);
        LOG_ATTR(attrC);
        VERIFY_ARE_EQUAL(darkRed, gci.LookupForegroundColor(attrA));
        VERIFY_ARE_EQUAL(darkBlue, gci.LookupBackgroundColor(attrA));

        VERIFY_ARE_EQUAL(darkWhite, gci.LookupForegroundColor(attrB));
        VERIFY_ARE_EQUAL(darkBlack, gci.LookupBackgroundColor(attrB));

        VERIFY_ARE_EQUAL(darkWhite, gci.LookupForegroundColor(attrC));
        VERIFY_ARE_EQUAL(darkBlack, gci.LookupBackgroundColor(attrC));
    }
}

template<class T>
void _FillLine(COORD position, T fillContent, TextAttribute fillAttr)
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    auto& row = si.GetTextBuffer().GetRowByOffset(position.Y);
    row.WriteCells({ fillContent, fillAttr }, position.X, false);
}

template<class T>
void _FillLine(int line, T fillContent, TextAttribute fillAttr)
{
    _FillLine({ 0, gsl::narrow<SHORT>(line) }, fillContent, fillAttr);
}

template<class T>
void _FillLines(int startLine, int endLine, T fillContent, TextAttribute fillAttr)
{
    for (auto line = startLine; line < endLine; ++line)
    {
        _FillLine(line, fillContent, fillAttr);
    }
}

template<class T>
bool _ValidateLineContains(COORD position, T expectedContent, TextAttribute expectedAttr)
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    auto actual = si.GetCellLineDataAt(position);
    auto expected = OutputCellIterator{ expectedContent, expectedAttr };
    while (actual && expected)
    {
        if (actual->Chars() != expected->Chars() || actual->TextAttr() != expected->TextAttr())
        {
            return false;
        }
        ++actual;
        ++expected;
    }
    return true;
};

template<class T>
bool _ValidateLineContains(int line, T expectedContent, TextAttribute expectedAttr)
{
    return _ValidateLineContains({ 0, gsl::narrow<SHORT>(line) }, expectedContent, expectedAttr);
}

template<class T>
auto _ValidateLinesContain(int startLine, int endLine, T expectedContent, TextAttribute expectedAttr)
{
    for (auto line = startLine; line < endLine; ++line)
    {
        if (!_ValidateLineContains(line, expectedContent, expectedAttr))
        {
            return false;
        }
    }
    return true;
};

void ScreenBufferTests::ScrollOperations()
{
    enum ScrollType : int
    {
        ScrollUp,
        ScrollDown,
        InsertLine,
        DeleteLine,
        ReverseIndex
    };
    enum ScrollDirection : int
    {
        Up,
        Down
    };

    ScrollType scrollType;
    ScrollDirection scrollDirection;
    int scrollMagnitude;

    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:scrollType", L"{0, 1, 2, 3, 4}")
        TEST_METHOD_PROPERTY(L"Data:scrollMagnitude", L"{1, 2, 5}")
    END_TEST_METHOD_PROPERTIES()

    VERIFY_SUCCEEDED(TestData::TryGetValue(L"scrollType", (int&)scrollType));
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"scrollMagnitude", scrollMagnitude));

    std::wstringstream escapeSequence;
    switch (scrollType)
    {
    case ScrollUp:
        Log::Comment(L"Testing scroll up (SU).");
        escapeSequence << "\x1b[" << scrollMagnitude << "S";
        scrollDirection = Up;
        break;
    case ScrollDown:
        Log::Comment(L"Testing scroll down (SD).");
        escapeSequence << "\x1b[" << scrollMagnitude << "T";
        scrollDirection = Down;
        break;
    case InsertLine:
        Log::Comment(L"Testing insert line (IL).");
        escapeSequence << "\x1b[" << scrollMagnitude << "L";
        scrollDirection = Down;
        break;
    case DeleteLine:
        Log::Comment(L"Testing delete line (DL).");
        escapeSequence << "\x1b[" << scrollMagnitude << "M";
        scrollDirection = Up;
        break;
    case ReverseIndex:
        Log::Comment(L"Testing reverse index (RI).");
        for (auto i = 0; i < scrollMagnitude; ++i)
        {
            escapeSequence << "\x1bM";
        }
        scrollDirection = Down;
        break;
    default:
        VERIFY_FAIL();
        return;
    }

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    auto& stateMachine = si.GetStateMachine();
    const auto& cursor = si.GetTextBuffer().GetCursor();
    WI_SetFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    const auto bufferWidth = si.GetBufferSize().Width();
    const auto bufferHeight = si.GetBufferSize().Height();

    // Move the viewport down a few lines, and only cover part of the buffer width.
    si.SetViewport(Viewport::FromDimensions({ 5, 10 }, { bufferWidth - 10, 10 }), true);
    const auto viewportStart = si.GetViewport().Top();
    const auto viewportEnd = si.GetViewport().BottomExclusive();

    // Fill the entire buffer with Zs. Blue on Green.
    const auto bufferChar = L'Z';
    const auto bufferAttr = TextAttribute{ FOREGROUND_BLUE | BACKGROUND_GREEN };
    _FillLines(0, bufferHeight, bufferChar, bufferAttr);

    // Fill the viewport with a range of letters to see if they move. Red on Blue.
    const auto viewportAttr = TextAttribute{ FOREGROUND_RED | BACKGROUND_BLUE };
    auto viewportChar = L'A';
    auto viewportLine = viewportStart;
    while (viewportLine < viewportEnd)
    {
        _FillLine(viewportLine++, viewportChar++, viewportAttr);
    }

    // Set the background color so that it will be used to fill the revealed area.
    si.SetAttributes({ BACKGROUND_RED });

    // Place the cursor in the center.
    auto cursorPos = COORD{ bufferWidth / 2, (viewportStart + viewportEnd) / 2 };
    // Unless this is reverse index, which has to be be at the top of the viewport.
    if (scrollType == ReverseIndex)
    {
        cursorPos.Y = viewportStart;
    }

    Log::Comment(L"Set the cursor position and perform the operation.");
    VERIFY_SUCCEEDED(si.SetCursorPosition(cursorPos, true));
    stateMachine.ProcessString(escapeSequence.str());

    // The cursor shouldn't move.
    auto expectedCursorPos = cursorPos;
    // Unless this is an IL or DL control, which moves the cursor to the left margin.
    if (scrollType == InsertLine || scrollType == DeleteLine)
    {
        expectedCursorPos.X = 0;
    }

    Log::Comment(L"Verify expected cursor position.");
    VERIFY_ARE_EQUAL(expectedCursorPos, cursor.GetPosition());

    Log::Comment(L"Field of Zs outside viewport should remain unchanged.");
    VERIFY_IS_TRUE(_ValidateLinesContain(0, viewportStart, bufferChar, bufferAttr));
    VERIFY_IS_TRUE(_ValidateLinesContain(viewportEnd, bufferHeight, bufferChar, bufferAttr));

    // Depending on the direction of scrolling, lines are either deleted or inserted.
    const auto deletedLines = scrollDirection == Up ? scrollMagnitude : 0;
    const auto insertedLines = scrollDirection == Down ? scrollMagnitude : 0;

    // Insert and delete operations only scroll the viewport below the cursor position.
    const auto scrollStart = (scrollType == InsertLine || scrollType == DeleteLine) ? cursorPos.Y : viewportStart;

    // Reset the viewport character and line number for the verification loop.
    viewportChar = L'A';
    viewportLine = viewportStart;

    Log::Comment(L"Lines above the scrolled area should remain unchanged.");
    while (viewportLine < scrollStart)
    {
        VERIFY_IS_TRUE(_ValidateLineContains(viewportLine++, viewportChar++, viewportAttr));
    }

    Log::Comment(L"Scrolled area should have moved up/down by given magnitude.");
    viewportChar += gsl::narrow<wchar_t>(deletedLines); // Characters dropped when deleting
    viewportLine += gsl::narrow<SHORT>(insertedLines); // Lines skipped when inserting
    while (viewportLine < viewportEnd - deletedLines)
    {
        VERIFY_IS_TRUE(_ValidateLineContains(viewportLine++, viewportChar++, viewportAttr));
    }

    Log::Comment(L"The revealed area should now be blank, with default buffer attributes.");
    const auto revealedStart = scrollDirection == Up ? viewportEnd - deletedLines : scrollStart;
    const auto revealedEnd = revealedStart + scrollMagnitude;
    VERIFY_IS_TRUE(_ValidateLinesContain(revealedStart, revealedEnd, L' ', si.GetAttributes()));
}

void ScreenBufferTests::InsertChars()
{
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:setMargins", L"{false, true}")
    END_TEST_METHOD_PROPERTIES();

    bool setMargins;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"setMargins", setMargins));

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    auto& stateMachine = si.GetStateMachine();
    WI_SetFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    // Set the buffer width to 40, with a centered viewport of 20.
    const auto bufferWidth = 40;
    const auto bufferHeight = si.GetBufferSize().Height();
    const auto viewportStart = 10;
    const auto viewportEnd = viewportStart + 20;
    VERIFY_SUCCEEDED(si.ResizeScreenBuffer({ bufferWidth, bufferHeight }, false));
    si.SetViewport(Viewport::FromExclusive({ viewportStart, 0, viewportEnd, 25 }), true);

    // Tests are run both with and without the DECSTBM margins set. This should not alter
    // the results, since the ICH operation is not affected by vertical margins.
    stateMachine.ProcessString(setMargins ? L"\x1b[15;20r" : L"\x1b[r");
    // Make sure we clear the margins on exit so they can't break other tests.
    auto clearMargins = wil::scope_exit([&] { stateMachine.ProcessString(L"\x1b[r"); });

    Log::Comment(
        L"Test 1: Fill the line with Qs. Write some text within the viewport boundaries. "
        L"Then insert 5 spaces at the cursor. Watch spaces get inserted, text slides right "
        L"out of the viewport, pushing some of the Qs out of the buffer.");

    const auto insertLine = SHORT{ 10 };
    auto insertPos = SHORT{ 20 };

    // Place the cursor in the center of the line.
    VERIFY_SUCCEEDED(si.SetCursorPosition({ insertPos, insertLine }, true));

    // Save the cursor position. It shouldn't move for the rest of the test.
    const auto& cursor = si.GetTextBuffer().GetCursor();
    auto expectedCursor = cursor.GetPosition();

    // Fill the entire line with Qs. Blue on Green.
    const auto bufferChar = L'Q';
    const auto bufferAttr = TextAttribute{ FOREGROUND_BLUE | BACKGROUND_GREEN };
    _FillLine(insertLine, bufferChar, bufferAttr);

    // Fill the viewport range with text. Red on Blue.
    const auto textChars = L"ABCDEFGHIJKLMNOPQRST";
    const auto textAttr = TextAttribute{ FOREGROUND_RED | BACKGROUND_BLUE };
    _FillLine({ viewportStart, insertLine }, textChars, textAttr);

    // Set the background color so that it will be used to fill the revealed area.
    si.SetAttributes({ BACKGROUND_RED });

    // Insert 5 spaces at the cursor position.
    // Before: QQQQQQQQQQABCDEFGHIJKLMNOPQRSTQQQQQQQQQQ
    //  After: QQQQQQQQQQABCDEFGHIJ     KLMNOPQRSTQQQQQ
    Log::Comment(L"Inserting 5 spaces in the middle of the line.");
    auto before = si.GetTextBuffer().GetRowByOffset(insertLine).GetText();
    stateMachine.ProcessString(L"\x1b[5@");
    auto after = si.GetTextBuffer().GetRowByOffset(insertLine).GetText();
    Log::Comment(before.c_str(), L"Before");
    Log::Comment(after.c_str(), L" After");

    // Verify cursor didn't move.
    VERIFY_ARE_EQUAL(expectedCursor, cursor.GetPosition(), L"Verify cursor didn't move from insert operation.");

    // Verify the updated structure of the line.
    VERIFY_IS_TRUE(_ValidateLineContains({ 0, insertLine }, L"QQQQQQQQQQ", bufferAttr),
                   L"Field of Qs left of the viewport should remain unchanged.");
    VERIFY_IS_TRUE(_ValidateLineContains({ viewportStart, insertLine }, L"ABCDEFGHIJ", textAttr),
                   L"First half of the alphabet should remain unchanged.");
    VERIFY_IS_TRUE(_ValidateLineContains({ insertPos, insertLine }, L"     ", si.GetAttributes()),
                   L"Spaces should be inserted with the current attributes at the cursor position.");
    VERIFY_IS_TRUE(_ValidateLineContains({ insertPos + 5, insertLine }, L"KLMNOPQRST", textAttr),
                   L"Second half of the alphabet should have moved to the right by the number of spaces inserted.");
    VERIFY_IS_TRUE(_ValidateLineContains({ viewportEnd + 5, insertLine }, L"QQQQQ", bufferAttr),
                   L"Field of Qs right of the viewport should be moved right, half pushed outside the buffer.");

    Log::Comment(
        L"Test 2: Inserting at the exact end of the line. Same line structure. "
        L"Move cursor to right edge of window and insert > 1 space. "
        L"Only 1 should be inserted, everything else unchanged.");

    // Move cursor to right edge.
    insertPos = bufferWidth - 1;
    VERIFY_SUCCEEDED(si.SetCursorPosition({ insertPos, insertLine }, true));
    expectedCursor = cursor.GetPosition();

    // Fill the entire line with Qs. Blue on Green.
    _FillLine(insertLine, bufferChar, bufferAttr);

    // Fill the viewport range with text. Red on Blue.
    _FillLine({ viewportStart, insertLine }, textChars, textAttr);

    // Set the background color so that it will be used to fill the revealed area.
    si.SetAttributes({ BACKGROUND_RED });

    // Insert 5 spaces at the right edge. Only 1 should be inserted.
    // Before: QQQQQQQQQQABCDEFGHIJKLMNOPQRSTQQQQQQQQQQ
    //  After: QQQQQQQQQQABCDEFGHIJKLMNOPQRSTQQQQQQQQQ
    Log::Comment(L"Inserting 5 spaces at the right edge of the buffer.");
    before = si.GetTextBuffer().GetRowByOffset(insertLine).GetText();
    stateMachine.ProcessString(L"\x1b[5@");
    after = si.GetTextBuffer().GetRowByOffset(insertLine).GetText();
    Log::Comment(before.c_str(), L"Before");
    Log::Comment(after.c_str(), L" After");

    // Verify cursor didn't move.
    VERIFY_ARE_EQUAL(expectedCursor, cursor.GetPosition(), L"Verify cursor didn't move from insert operation.");

    // Verify the updated structure of the line.
    VERIFY_IS_TRUE(_ValidateLineContains({ 0, insertLine }, L"QQQQQQQQQQ", bufferAttr),
                   L"Field of Qs left of the viewport should remain unchanged.");
    VERIFY_IS_TRUE(_ValidateLineContains({ viewportStart, insertLine }, L"ABCDEFGHIJKLMNOPQRST", textAttr),
                   L"Entire viewport range should remain unchanged.");
    VERIFY_IS_TRUE(_ValidateLineContains({ viewportEnd, insertLine }, L"QQQQQQQQQ", bufferAttr),
                   L"Field of Qs right of the viewport should remain unchanged except for the last spot.");
    VERIFY_IS_TRUE(_ValidateLineContains({ insertPos, insertLine }, L" ", si.GetAttributes()),
                   L"One space should be inserted with the current attributes at the cursor postion.");

    Log::Comment(
        L"Test 3: Inserting at the exact beginning of the line. Same line structure. "
        L"Move cursor to left edge of buffer and insert > buffer width of space. "
        L"The whole row should be replaced with spaces.");

    // Move cursor to left edge.
    VERIFY_SUCCEEDED(si.SetCursorPosition({ 0, insertLine }, true));
    expectedCursor = cursor.GetPosition();

    // Fill the entire line with Qs. Blue on Green.
    _FillLine(insertLine, bufferChar, bufferAttr);

    // Fill the viewport range with text. Red on Blue.
    _FillLine({ viewportStart, insertLine }, textChars, textAttr);

    // Insert greater than the buffer width at the left edge. The entire line should be erased.
    // Before: QQQQQQQQQQABCDEFGHIJKLMNOPQRSTQQQQQQQQQQ
    //  After:
    Log::Comment(L"Inserting 100 spaces at the left edge of the buffer.");
    before = si.GetTextBuffer().GetRowByOffset(insertLine).GetText();
    stateMachine.ProcessString(L"\x1b[100@");
    after = si.GetTextBuffer().GetRowByOffset(insertLine).GetText();
    Log::Comment(before.c_str(), L"Before");
    Log::Comment(after.c_str(), L" After");

    // Verify cursor didn't move.
    VERIFY_ARE_EQUAL(expectedCursor, cursor.GetPosition(), L"Verify cursor didn't move from insert operation.");

    // Verify the updated structure of the line.
    VERIFY_IS_TRUE(_ValidateLineContains(insertLine, L' ', si.GetAttributes()),
                   L"A whole line of spaces was inserted at the start, erasing the line.");
}

void ScreenBufferTests::DeleteChars()
{
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:setMargins", L"{false, true}")
    END_TEST_METHOD_PROPERTIES();

    bool setMargins;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"setMargins", setMargins));

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    auto& stateMachine = si.GetStateMachine();
    WI_SetFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    // Set the buffer width to 40, with a centered viewport of 20.
    const auto bufferWidth = 40;
    const auto bufferHeight = si.GetBufferSize().Height();
    const auto viewportStart = 10;
    const auto viewportEnd = viewportStart + 20;
    VERIFY_SUCCEEDED(si.ResizeScreenBuffer({ bufferWidth, bufferHeight }, false));
    si.SetViewport(Viewport::FromExclusive({ viewportStart, 0, viewportEnd, 25 }), true);

    // Tests are run both with and without the DECSTBM margins set. This should not alter
    // the results, since the DCH operation is not affected by vertical margins.
    stateMachine.ProcessString(setMargins ? L"\x1b[15;20r" : L"\x1b[r");
    // Make sure we clear the margins on exit so they can't break other tests.
    auto clearMargins = wil::scope_exit([&] { stateMachine.ProcessString(L"\x1b[r"); });

    Log::Comment(
        L"Test 1: Fill the line with Qs. Write some text within the viewport boundaries. "
        L"Then delete 5 characters at the cursor. Watch the rest of the line slide left, "
        L"replacing the deleted characters, with spaces inserted at the end of the line.");

    const auto deleteLine = SHORT{ 10 };
    auto deletePos = SHORT{ 20 };

    // Place the cursor in the center of the line.
    VERIFY_SUCCEEDED(si.SetCursorPosition({ deletePos, deleteLine }, true));

    // Save the cursor position. It shouldn't move for the rest of the test.
    const auto& cursor = si.GetTextBuffer().GetCursor();
    auto expectedCursor = cursor.GetPosition();

    // Fill the entire line with Qs. Blue on Green.
    const auto bufferChar = L'Q';
    const auto bufferAttr = TextAttribute{ FOREGROUND_BLUE | BACKGROUND_GREEN };
    _FillLine(deleteLine, bufferChar, bufferAttr);

    // Fill the viewport range with text. Red on Blue.
    const auto textChars = L"ABCDEFGHIJKLMNOPQRST";
    const auto textAttr = TextAttribute{ FOREGROUND_RED | BACKGROUND_BLUE };
    _FillLine({ viewportStart, deleteLine }, textChars, textAttr);

    // Set the background color so that it will be used to fill the revealed area.
    si.SetAttributes({ BACKGROUND_RED });

    // Delete 5 characters at the cursor position.
    // Before: QQQQQQQQQQABCDEFGHIJKLMNOPQRSTQQQQQQQQQQ
    //  After: QQQQQQQQQQABCDEFGHIJPQRSTQQQQQQQQQQ
    Log::Comment(L"Deleting 5 characters in the middle of the line.");
    auto before = si.GetTextBuffer().GetRowByOffset(deleteLine).GetText();
    stateMachine.ProcessString(L"\x1b[5P");
    auto after = si.GetTextBuffer().GetRowByOffset(deleteLine).GetText();
    Log::Comment(before.c_str(), L"Before");
    Log::Comment(after.c_str(), L" After");

    // Verify cursor didn't move.
    VERIFY_ARE_EQUAL(expectedCursor, cursor.GetPosition(), L"Verify cursor didn't move from delete operation.");

    // Verify the updated structure of the line.
    VERIFY_IS_TRUE(_ValidateLineContains({ 0, deleteLine }, L"QQQQQQQQQQ", bufferAttr),
                   L"Field of Qs left of the viewport should remain unchanged.");
    VERIFY_IS_TRUE(_ValidateLineContains({ viewportStart, deleteLine }, L"ABCDEFGHIJ", textAttr),
                   L"First half of the alphabet should remain unchanged.");
    VERIFY_IS_TRUE(_ValidateLineContains({ deletePos, deleteLine }, L"PQRST", textAttr),
                   L"Only half of the second part of the alphabet remains.");
    VERIFY_IS_TRUE(_ValidateLineContains({ viewportEnd - 5, deleteLine }, L"QQQQQQQQQQ", bufferAttr),
                   L"Field of Qs right of the viewport should be moved left.");
    VERIFY_IS_TRUE(_ValidateLineContains({ bufferWidth - 5, deleteLine }, L"     ", si.GetAttributes()),
                   L"The rest of the line should be replaced with spaces with the current attributes.");

    Log::Comment(
        L"Test 2: Deleting at the exact end of the line. Same line structure. "
        L"Move cursor to right edge of window and delete > 1 character. "
        L"Only 1 should be deleted, everything else unchanged.");

    // Move cursor to right edge.
    deletePos = bufferWidth - 1;
    VERIFY_SUCCEEDED(si.SetCursorPosition({ deletePos, deleteLine }, true));
    expectedCursor = cursor.GetPosition();

    // Fill the entire line with Qs. Blue on Green.
    _FillLine(deleteLine, bufferChar, bufferAttr);

    // Fill the viewport range with text. Red on Blue.
    _FillLine({ viewportStart, deleteLine }, textChars, textAttr);

    // Set the background color so that it will be used to fill the revealed area.
    si.SetAttributes({ BACKGROUND_RED });

    // Delete 5 characters at the right edge. Only 1 should be deleted.
    // Before: QQQQQQQQQQABCDEFGHIJKLMNOPQRSTQQQQQQQQQQ
    //  After: QQQQQQQQQQABCDEFGHIJKLMNOPQRSTQQQQQQQQQ
    Log::Comment(L"Deleting 5 characters at the right edge of the buffer.");
    before = si.GetTextBuffer().GetRowByOffset(deleteLine).GetText();
    stateMachine.ProcessString(L"\x1b[5P");
    after = si.GetTextBuffer().GetRowByOffset(deleteLine).GetText();
    Log::Comment(before.c_str(), L"Before");
    Log::Comment(after.c_str(), L" After");

    // Verify cursor didn't move.
    VERIFY_ARE_EQUAL(expectedCursor, cursor.GetPosition(), L"Verify cursor didn't move from delete operation.");

    // Verify the updated structure of the line.
    VERIFY_IS_TRUE(_ValidateLineContains({ 0, deleteLine }, L"QQQQQQQQQQ", bufferAttr),
                   L"Field of Qs left of the viewport should remain unchanged.");
    VERIFY_IS_TRUE(_ValidateLineContains({ viewportStart, deleteLine }, L"ABCDEFGHIJKLMNOPQRST", textAttr),
                   L"Entire viewport range should remain unchanged.");
    VERIFY_IS_TRUE(_ValidateLineContains({ viewportEnd, deleteLine }, L"QQQQQQQQQ", bufferAttr),
                   L"Field of Qs right of the viewport should remain unchanged except for the last spot.");
    VERIFY_IS_TRUE(_ValidateLineContains({ deletePos, deleteLine }, L" ", si.GetAttributes()),
                   L"One character should be erased with the current attributes at the cursor postion.");

    Log::Comment(
        L"Test 3: Deleting at the exact beginning of the line. Same line structure. "
        L"Move cursor to left edge of buffer and delete > buffer width of characters. "
        L"The whole row should be replaced with spaces.");

    // Move cursor to left edge.
    VERIFY_SUCCEEDED(si.SetCursorPosition({ 0, deleteLine }, true));
    expectedCursor = cursor.GetPosition();

    // Fill the entire line with Qs. Blue on Green.
    _FillLine(deleteLine, bufferChar, bufferAttr);

    // Fill the viewport range with text. Red on Blue.
    _FillLine({ viewportStart, deleteLine }, textChars, textAttr);

    // Delete greater than the buffer width at the left edge. The entire line should be erased.
    // Before: QQQQQQQQQQABCDEFGHIJKLMNOPQRSTQQQQQQQQQQ
    //  After:
    Log::Comment(L"Deleting 100 characters at the left edge of the buffer.");
    before = si.GetTextBuffer().GetRowByOffset(deleteLine).GetText();
    stateMachine.ProcessString(L"\x1b[100P");
    after = si.GetTextBuffer().GetRowByOffset(deleteLine).GetText();
    Log::Comment(before.c_str(), L"Before");
    Log::Comment(after.c_str(), L" After");

    // Verify cursor didn't move.
    VERIFY_ARE_EQUAL(expectedCursor, cursor.GetPosition(), L"Verify cursor didn't move from delete operation.");

    // Verify the updated structure of the line.
    VERIFY_IS_TRUE(_ValidateLineContains(deleteLine, L' ', si.GetAttributes()),
                   L"A whole line of spaces was inserted from the right, erasing the line.");
}

void _CommonScrollingSetup()
{
    // Used for testing MSFT:20204600
    // Place an A on the first line, and a B on the 6th line (index 5).
    // Set the scrolling region in between those lines (so scrolling won't affect them.)
    // First write "1\n2\n3\n4", to put 1-4 on the lines in between the A and B.
    // the viewport will look like:
    // A
    // 1
    // 2
    // 3
    // 4
    // B
    // then write "\n5\n6\n7\n", which will cycle around the scroll region a bit.
    // the viewport will look like:
    // A
    // 5
    // 6
    // 7
    //
    // B

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si.GetTextBuffer().GetCursor();
    const auto oldView = si.GetViewport();
    const auto view = Viewport::FromDimensions({ 0, 0 }, { oldView.Width(), 6 });
    si.SetViewport(view, true);
    cursor.SetPosition({ 0, 0 });
    std::wstring seq = L"A";
    stateMachine.ProcessString(seq);
    cursor.SetPosition({ 0, 5 });
    seq = L"B";
    stateMachine.ProcessString(seq);
    seq = L"\x1b[2;5r";
    stateMachine.ProcessString(seq);
    seq = L"\x1b[2;1H";
    stateMachine.ProcessString(seq);
    seq = L"1\n2\n3\n4";
    stateMachine.ProcessString(seq);

    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<COORD>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"viewport=%s", VerifyOutputTraits<SMALL_RECT>::ToString(si.GetViewport().ToInclusive()).GetBuffer()));

    VERIFY_ARE_EQUAL(1, cursor.GetPosition().X);
    VERIFY_ARE_EQUAL(4, cursor.GetPosition().Y);
    {
        auto iter0 = tbi.GetCellDataAt({ 0, 0 });
        auto iter1 = tbi.GetCellDataAt({ 0, 1 });
        auto iter2 = tbi.GetCellDataAt({ 0, 2 });
        auto iter3 = tbi.GetCellDataAt({ 0, 3 });
        auto iter4 = tbi.GetCellDataAt({ 0, 4 });
        auto iter5 = tbi.GetCellDataAt({ 0, 5 });
        VERIFY_ARE_EQUAL(L"A", iter0->Chars());
        VERIFY_ARE_EQUAL(L"1", iter1->Chars());
        VERIFY_ARE_EQUAL(L"2", iter2->Chars());
        VERIFY_ARE_EQUAL(L"3", iter3->Chars());
        VERIFY_ARE_EQUAL(L"4", iter4->Chars());
        VERIFY_ARE_EQUAL(L"B", iter5->Chars());
    }

    seq = L"\n5\n6\n7\n";
    stateMachine.ProcessString(seq);

    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<COORD>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"viewport=%s", VerifyOutputTraits<SMALL_RECT>::ToString(si.GetViewport().ToInclusive()).GetBuffer()));

    VERIFY_ARE_EQUAL(0, cursor.GetPosition().X);
    VERIFY_ARE_EQUAL(4, cursor.GetPosition().Y);
    {
        auto iter0 = tbi.GetCellDataAt({ 0, 0 });
        auto iter1 = tbi.GetCellDataAt({ 0, 1 });
        auto iter2 = tbi.GetCellDataAt({ 0, 2 });
        auto iter3 = tbi.GetCellDataAt({ 0, 3 });
        auto iter4 = tbi.GetCellDataAt({ 0, 4 });
        auto iter5 = tbi.GetCellDataAt({ 0, 5 });
        VERIFY_ARE_EQUAL(L"A", iter0->Chars());
        VERIFY_ARE_EQUAL(L"5", iter1->Chars());
        VERIFY_ARE_EQUAL(L"6", iter2->Chars());
        VERIFY_ARE_EQUAL(L"7", iter3->Chars());
        // Chars() will return a single space for an empty row.
        VERIFY_ARE_EQUAL(L"\x20", iter4->Chars());
        VERIFY_ARE_EQUAL(L"B", iter5->Chars());
    }
}

void ScreenBufferTests::ScrollUpInMargins()
{
    // Tests MSFT:20204600
    // Do the common scrolling setup, then executes a Scroll Up, and verifies
    //      the rows have what we'd expect.

    _CommonScrollingSetup();
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si.GetTextBuffer().GetCursor();

    // Execute a Scroll Up command
    std::wstring seq = L"\x1b[S";
    stateMachine.ProcessString(seq);

    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<COORD>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"viewport=%s", VerifyOutputTraits<SMALL_RECT>::ToString(si.GetViewport().ToInclusive()).GetBuffer()));

    VERIFY_ARE_EQUAL(0, cursor.GetPosition().X);
    VERIFY_ARE_EQUAL(4, cursor.GetPosition().Y);
    {
        auto iter0 = tbi.GetCellDataAt({ 0, 0 });
        auto iter1 = tbi.GetCellDataAt({ 0, 1 });
        auto iter2 = tbi.GetCellDataAt({ 0, 2 });
        auto iter3 = tbi.GetCellDataAt({ 0, 3 });
        auto iter4 = tbi.GetCellDataAt({ 0, 4 });
        auto iter5 = tbi.GetCellDataAt({ 0, 5 });
        VERIFY_ARE_EQUAL(L"A", iter0->Chars());
        VERIFY_ARE_EQUAL(L"6", iter1->Chars());
        VERIFY_ARE_EQUAL(L"7", iter2->Chars());
        VERIFY_ARE_EQUAL(L"\x20", iter3->Chars());
        VERIFY_ARE_EQUAL(L"\x20", iter4->Chars());
        VERIFY_ARE_EQUAL(L"B", iter5->Chars());
    }
}

void ScreenBufferTests::ScrollDownInMargins()
{
    // Tests MSFT:20204600
    // Do the common scrolling setup, then executes a Scroll Down, and verifies
    //      the rows have what we'd expect.

    _CommonScrollingSetup();
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si.GetTextBuffer().GetCursor();

    // Execute a Scroll Down command
    std::wstring seq = L"\x1b[T";
    stateMachine.ProcessString(seq);

    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<COORD>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"viewport=%s", VerifyOutputTraits<SMALL_RECT>::ToString(si.GetViewport().ToInclusive()).GetBuffer()));

    VERIFY_ARE_EQUAL(0, cursor.GetPosition().X);
    VERIFY_ARE_EQUAL(4, cursor.GetPosition().Y);
    {
        auto iter0 = tbi.GetCellDataAt({ 0, 0 });
        auto iter1 = tbi.GetCellDataAt({ 0, 1 });
        auto iter2 = tbi.GetCellDataAt({ 0, 2 });
        auto iter3 = tbi.GetCellDataAt({ 0, 3 });
        auto iter4 = tbi.GetCellDataAt({ 0, 4 });
        auto iter5 = tbi.GetCellDataAt({ 0, 5 });
        VERIFY_ARE_EQUAL(L"A", iter0->Chars());
        VERIFY_ARE_EQUAL(L"\x20", iter1->Chars());
        VERIFY_ARE_EQUAL(L"5", iter2->Chars());
        VERIFY_ARE_EQUAL(L"6", iter3->Chars());
        VERIFY_ARE_EQUAL(L"7", iter4->Chars());
        VERIFY_ARE_EQUAL(L"B", iter5->Chars());
    }
}

void ScreenBufferTests::InsertLinesInMargins()
{
    Log::Comment(
        L"Does the common scrolling setup, then inserts two lines inside the "
        L"margin boundaries, and verifies the rows have what we'd expect.");

    _CommonScrollingSetup();
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si.GetTextBuffer().GetCursor();

    // Move to column 5 of line 3
    stateMachine.ProcessString(L"\x1b[3;5H");
    // Insert 2 lines
    stateMachine.ProcessString(L"\x1b[2L");

    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<COORD>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"viewport=%s", VerifyOutputTraits<SMALL_RECT>::ToString(si.GetViewport().ToInclusive()).GetBuffer()));

    // Verify cursor moved to left margin.
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().X);
    VERIFY_ARE_EQUAL(2, cursor.GetPosition().Y);
    {
        auto iter0 = tbi.GetCellDataAt({ 0, 0 });
        auto iter1 = tbi.GetCellDataAt({ 0, 1 });
        auto iter2 = tbi.GetCellDataAt({ 0, 2 });
        auto iter3 = tbi.GetCellDataAt({ 0, 3 });
        auto iter4 = tbi.GetCellDataAt({ 0, 4 });
        auto iter5 = tbi.GetCellDataAt({ 0, 5 });
        VERIFY_ARE_EQUAL(L"A", iter0->Chars());
        VERIFY_ARE_EQUAL(L"5", iter1->Chars());
        VERIFY_ARE_EQUAL(L"\x20", iter2->Chars());
        VERIFY_ARE_EQUAL(L"\x20", iter3->Chars());
        VERIFY_ARE_EQUAL(L"6", iter4->Chars());
        VERIFY_ARE_EQUAL(L"B", iter5->Chars());
    }

    Log::Comment(
        L"Does the common scrolling setup, then inserts one line with no "
        L"margins set, and verifies the rows have what we'd expect.");

    _CommonScrollingSetup();
    // Clear the scroll margins
    stateMachine.ProcessString(L"\x1b[r");
    // Move to column 5 of line 2
    stateMachine.ProcessString(L"\x1b[2;5H");
    // Insert 1 line
    stateMachine.ProcessString(L"\x1b[L");

    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<COORD>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"viewport=%s", VerifyOutputTraits<SMALL_RECT>::ToString(si.GetViewport().ToInclusive()).GetBuffer()));

    // Verify cursor moved to left margin.
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().X);
    VERIFY_ARE_EQUAL(1, cursor.GetPosition().Y);
    {
        auto iter0 = tbi.GetCellDataAt({ 0, 0 });
        auto iter1 = tbi.GetCellDataAt({ 0, 1 });
        auto iter2 = tbi.GetCellDataAt({ 0, 2 });
        auto iter3 = tbi.GetCellDataAt({ 0, 3 });
        auto iter4 = tbi.GetCellDataAt({ 0, 4 });
        auto iter5 = tbi.GetCellDataAt({ 0, 5 });
        VERIFY_ARE_EQUAL(L"A", iter0->Chars());
        VERIFY_ARE_EQUAL(L"\x20", iter1->Chars());
        VERIFY_ARE_EQUAL(L"5", iter2->Chars());
        VERIFY_ARE_EQUAL(L"6", iter3->Chars());
        VERIFY_ARE_EQUAL(L"7", iter4->Chars());
        VERIFY_ARE_EQUAL(L"\x20", iter5->Chars());
    }
}

void ScreenBufferTests::DeleteLinesInMargins()
{
    Log::Comment(
        L"Does the common scrolling setup, then deletes two lines inside the "
        L"margin boundaries, and verifies the rows have what we'd expect.");

    _CommonScrollingSetup();
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si.GetTextBuffer().GetCursor();

    // Move to column 5 of line 3
    stateMachine.ProcessString(L"\x1b[3;5H");
    // Delete 2 lines
    stateMachine.ProcessString(L"\x1b[2M");

    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<COORD>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"viewport=%s", VerifyOutputTraits<SMALL_RECT>::ToString(si.GetViewport().ToInclusive()).GetBuffer()));

    // Verify cursor moved to left margin.
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().X);
    VERIFY_ARE_EQUAL(2, cursor.GetPosition().Y);
    {
        auto iter0 = tbi.GetCellDataAt({ 0, 0 });
        auto iter1 = tbi.GetCellDataAt({ 0, 1 });
        auto iter2 = tbi.GetCellDataAt({ 0, 2 });
        auto iter3 = tbi.GetCellDataAt({ 0, 3 });
        auto iter4 = tbi.GetCellDataAt({ 0, 4 });
        auto iter5 = tbi.GetCellDataAt({ 0, 5 });
        VERIFY_ARE_EQUAL(L"A", iter0->Chars());
        VERIFY_ARE_EQUAL(L"5", iter1->Chars());
        VERIFY_ARE_EQUAL(L"\x20", iter2->Chars());
        VERIFY_ARE_EQUAL(L"\x20", iter3->Chars());
        VERIFY_ARE_EQUAL(L"\x20", iter4->Chars());
        VERIFY_ARE_EQUAL(L"B", iter5->Chars());
    }

    Log::Comment(
        L"Does the common scrolling setup, then deletes one line with no "
        L"margins set, and verifies the rows have what we'd expect.");

    _CommonScrollingSetup();
    // Clear the scroll margins
    stateMachine.ProcessString(L"\x1b[r");
    // Move to column 5 of line 2
    stateMachine.ProcessString(L"\x1b[2;5H");
    // Delete 1 line
    stateMachine.ProcessString(L"\x1b[M");

    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<COORD>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"viewport=%s", VerifyOutputTraits<SMALL_RECT>::ToString(si.GetViewport().ToInclusive()).GetBuffer()));

    // Verify cursor moved to left margin.
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().X);
    VERIFY_ARE_EQUAL(1, cursor.GetPosition().Y);
    {
        auto iter0 = tbi.GetCellDataAt({ 0, 0 });
        auto iter1 = tbi.GetCellDataAt({ 0, 1 });
        auto iter2 = tbi.GetCellDataAt({ 0, 2 });
        auto iter3 = tbi.GetCellDataAt({ 0, 3 });
        auto iter4 = tbi.GetCellDataAt({ 0, 4 });
        auto iter5 = tbi.GetCellDataAt({ 0, 5 });
        VERIFY_ARE_EQUAL(L"A", iter0->Chars());
        VERIFY_ARE_EQUAL(L"6", iter1->Chars());
        VERIFY_ARE_EQUAL(L"7", iter2->Chars());
        VERIFY_ARE_EQUAL(L"\x20", iter3->Chars());
        VERIFY_ARE_EQUAL(L"B", iter4->Chars());
        VERIFY_ARE_EQUAL(L"\x20", iter5->Chars());
    }
}

void ScreenBufferTests::ReverseLineFeedInMargins()
{
    Log::Comment(
        L"Does the common scrolling setup, then executes a reverse line feed "
        L"below the top margin, and verifies the rows have what we'd expect.");

    _CommonScrollingSetup();
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si.GetTextBuffer().GetCursor();

    // Move to column 5 of line 2, the top margin
    stateMachine.ProcessString(L"\x1b[2;5H");
    // Execute a reverse line feed (RI)
    stateMachine.ProcessString(L"\x1bM");

    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<COORD>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"viewport=%s", VerifyOutputTraits<SMALL_RECT>::ToString(si.GetViewport().ToInclusive()).GetBuffer()));

    VERIFY_ARE_EQUAL(4, cursor.GetPosition().X);
    VERIFY_ARE_EQUAL(1, cursor.GetPosition().Y);
    {
        auto iter0 = tbi.GetCellDataAt({ 0, 0 });
        auto iter1 = tbi.GetCellDataAt({ 0, 1 });
        auto iter2 = tbi.GetCellDataAt({ 0, 2 });
        auto iter3 = tbi.GetCellDataAt({ 0, 3 });
        auto iter4 = tbi.GetCellDataAt({ 0, 4 });
        auto iter5 = tbi.GetCellDataAt({ 0, 5 });
        VERIFY_ARE_EQUAL(L"A", iter0->Chars());
        VERIFY_ARE_EQUAL(L"\x20", iter1->Chars());
        VERIFY_ARE_EQUAL(L"5", iter2->Chars());
        VERIFY_ARE_EQUAL(L"6", iter3->Chars());
        VERIFY_ARE_EQUAL(L"7", iter4->Chars());
        VERIFY_ARE_EQUAL(L"B", iter5->Chars());
    }

    Log::Comment(
        L"Does the common scrolling setup, then executes a reverse line feed "
        L"with the top margin at the top of the screen, and verifies the rows "
        L"have what we'd expect.");

    _CommonScrollingSetup();
    // Set the top scroll margin to the top of the screen
    stateMachine.ProcessString(L"\x1b[1;5r");
    // Move to column 5 of line 1, the top of the screen
    stateMachine.ProcessString(L"\x1b[1;5H");
    // Execute a reverse line feed (RI)
    stateMachine.ProcessString(L"\x1bM");

    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<COORD>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"viewport=%s", VerifyOutputTraits<SMALL_RECT>::ToString(si.GetViewport().ToInclusive()).GetBuffer()));

    VERIFY_ARE_EQUAL(4, cursor.GetPosition().X);
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().Y);
    {
        auto iter0 = tbi.GetCellDataAt({ 0, 0 });
        auto iter1 = tbi.GetCellDataAt({ 0, 1 });
        auto iter2 = tbi.GetCellDataAt({ 0, 2 });
        auto iter3 = tbi.GetCellDataAt({ 0, 3 });
        auto iter4 = tbi.GetCellDataAt({ 0, 4 });
        auto iter5 = tbi.GetCellDataAt({ 0, 5 });
        VERIFY_ARE_EQUAL(L"\x20", iter0->Chars());
        VERIFY_ARE_EQUAL(L"A", iter1->Chars());
        VERIFY_ARE_EQUAL(L"5", iter2->Chars());
        VERIFY_ARE_EQUAL(L"6", iter3->Chars());
        VERIFY_ARE_EQUAL(L"7", iter4->Chars());
        VERIFY_ARE_EQUAL(L"B", iter5->Chars());
    }
}

void ScreenBufferTests::ScrollLines256Colors()
{
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:scrollType", L"{0, 1, 2}")
        TEST_METHOD_PROPERTY(L"Data:colorStyle", L"{0, 1, 2}")
    END_TEST_METHOD_PROPERTIES();

    // colorStyle will be used to control whether we use a color from the 16
    // color table, a color from the 256 color table, or a pure RGB color.
    const int Use16Color = 0;
    const int Use256Color = 1;
    const int UseRGBColor = 2;

    // scrollType will be used to control whether we use InsertLines,
    // DeleteLines, or ReverseIndex to scroll the contents of the buffer.
    const int InsertLines = 0;
    const int DeleteLines = 1;
    const int ReverseLineFeed = 2;

    int scrollType;
    int colorStyle;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"scrollType", scrollType), L"controls whether to use InsertLines, DeleteLines ot ReverseLineFeed");
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"colorStyle", colorStyle), L"controls whether to use the 16 color table, 256 table, or RGB colors");

    // This test is largely taken from repro code from
    // https://github.com/microsoft/terminal/issues/832#issuecomment-507447272
    Log::Comment(
        L"Sets the attributes to a 256/RGB color, then scrolls some lines with"
        L" IL/DL/RI. Verifies the rows are cleared with the attributes we'd expect.");

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si.GetTextBuffer().GetCursor();

    TextAttribute expectedAttr{ si.GetAttributes() };
    std::wstring sgrSeq = L"\x1b[48;5;2m";
    if (colorStyle == Use16Color)
    {
        expectedAttr.SetBackground(gci.GetColorTableEntry(2));
    }
    else if (colorStyle == Use256Color)
    {
        expectedAttr.SetBackground(gci.GetColorTableEntry(20));
        sgrSeq = L"\x1b[48;5;20m";
    }
    else if (colorStyle == UseRGBColor)
    {
        expectedAttr.SetBackground(RGB(1, 2, 3));
        sgrSeq = L"\x1b[48;2;1;2;3m";
    }

    // Set some scrolling margins
    stateMachine.ProcessString(L"\x1b[1;3r");

    // Set the BG color to the table index 2, as a 256-color sequence
    stateMachine.ProcessString(sgrSeq);

    VERIFY_ARE_EQUAL(expectedAttr, si.GetAttributes());

    // Move to home
    stateMachine.ProcessString(L"\x1b[H");

    // Insert/Delete/Reverse Index 10 lines
    std::wstring scrollSeq = L"";
    if (scrollType == InsertLines)
    {
        scrollSeq = L"\x1b[10L";
    }
    if (scrollType == DeleteLines)
    {
        scrollSeq = L"\x1b[10M";
    }
    if (scrollType == ReverseLineFeed)
    {
        // This is 10 "Reverse Index" commands, which don't accept a parameter.
        scrollSeq = L"\x1bM\x1bM\x1bM\x1bM\x1bM\x1bM\x1bM\x1bM\x1bM\x1bM";
    }
    stateMachine.ProcessString(scrollSeq);

    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<COORD>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"viewport=%s", VerifyOutputTraits<SMALL_RECT>::ToString(si.GetViewport().ToInclusive()).GetBuffer()));

    VERIFY_ARE_EQUAL(0, cursor.GetPosition().X);
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().Y);

    stateMachine.ProcessString(L"foo");
    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<COORD>::ToString(cursor.GetPosition()).GetBuffer()));
    VERIFY_ARE_EQUAL(3, cursor.GetPosition().X);
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().Y);
    {
        auto iter00 = tbi.GetCellDataAt({ 0, 0 });
        auto iter10 = tbi.GetCellDataAt({ 1, 0 });
        auto iter20 = tbi.GetCellDataAt({ 2, 0 });
        auto iter30 = tbi.GetCellDataAt({ 3, 0 });
        auto iter01 = tbi.GetCellDataAt({ 0, 1 });
        auto iter02 = tbi.GetCellDataAt({ 0, 2 });
        VERIFY_ARE_EQUAL(L"f", iter00->Chars());
        VERIFY_ARE_EQUAL(L"o", iter10->Chars());
        VERIFY_ARE_EQUAL(L"o", iter20->Chars());
        VERIFY_ARE_EQUAL(L"\x20", iter30->Chars());
        VERIFY_ARE_EQUAL(L"\x20", iter01->Chars());
        VERIFY_ARE_EQUAL(L"\x20", iter02->Chars());

        VERIFY_ARE_EQUAL(expectedAttr, iter00->TextAttr());
        VERIFY_ARE_EQUAL(expectedAttr, iter10->TextAttr());
        VERIFY_ARE_EQUAL(expectedAttr, iter20->TextAttr());
        VERIFY_ARE_EQUAL(expectedAttr, iter30->TextAttr());
        VERIFY_ARE_EQUAL(expectedAttr, iter01->TextAttr());
        VERIFY_ARE_EQUAL(expectedAttr, iter02->TextAttr());
    }
}

void ScreenBufferTests::SetOriginMode()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si.GetTextBuffer().GetCursor();

    const auto view = Viewport::FromDimensions({ 0, 0 }, { 80, 25 });
    si.SetViewport(view, true);

    // Testing the default state (absolute cursor addressing)
    Log::Comment(L"By default, setting a margin moves the cursor to the top-left of the screen.");
    cursor.SetPosition({ 40, 12 });
    stateMachine.ProcessString(L"\x1B[6;20r");
    VERIFY_ARE_EQUAL(COORD({ 0, 0 }), cursor.GetPosition());
    Log::Comment(L"Cursor addressing is relative to the top-left of the screen.");
    stateMachine.ProcessString(L"\x1B[13;41H");
    VERIFY_ARE_EQUAL(COORD({ 40, 12 }), cursor.GetPosition());
    Log::Comment(L"The cursor can be moved below the bottom margin.");
    stateMachine.ProcessString(L"\x1B[23;41H");
    VERIFY_ARE_EQUAL(COORD({ 40, 22 }), cursor.GetPosition());

    // Testing the effects of DECOM being set (relative cursor addressing)
    Log::Comment(L"Setting DECOM moves the cursor to the top-left of the margin area.");
    cursor.SetPosition({ 40, 12 });
    stateMachine.ProcessString(L"\x1B[?6h");
    VERIFY_ARE_EQUAL(COORD({ 0, 5 }), cursor.GetPosition());
    Log::Comment(L"Setting a margin moves the cursor to the top-left of the margin area.");
    cursor.SetPosition({ 40, 12 });
    stateMachine.ProcessString(L"\x1B[6;20r");
    VERIFY_ARE_EQUAL(COORD({ 0, 5 }), cursor.GetPosition());
    Log::Comment(L"Cursor addressing is relative to the top-left of the margin area.");
    stateMachine.ProcessString(L"\x1B[8;41H");
    VERIFY_ARE_EQUAL(COORD({ 40, 12 }), cursor.GetPosition());
    Log::Comment(L"The cursor cannot be moved below the bottom margin.");
    stateMachine.ProcessString(L"\x1B[100;41H");
    VERIFY_ARE_EQUAL(COORD({ 40, 19 }), cursor.GetPosition());

    // Testing the effects of DECOM being reset (absolute cursor addressing)
    Log::Comment(L"Resetting DECOM moves the cursor to the top-left of the screen.");
    cursor.SetPosition({ 40, 12 });
    stateMachine.ProcessString(L"\x1B[?6l");
    VERIFY_ARE_EQUAL(COORD({ 0, 0 }), cursor.GetPosition());
    Log::Comment(L"Setting a margin moves the cursor to the top-left of the screen.");
    cursor.SetPosition({ 40, 12 });
    stateMachine.ProcessString(L"\x1B[6;20r");
    VERIFY_ARE_EQUAL(COORD({ 0, 0 }), cursor.GetPosition());
    Log::Comment(L"Cursor addressing is relative to the top-left of the screen.");
    stateMachine.ProcessString(L"\x1B[13;41H");
    VERIFY_ARE_EQUAL(COORD({ 40, 12 }), cursor.GetPosition());
    Log::Comment(L"The cursor can be moved below the bottom margin.");
    stateMachine.ProcessString(L"\x1B[23;41H");
    VERIFY_ARE_EQUAL(COORD({ 40, 22 }), cursor.GetPosition());

    // Testing the effects of DECOM being set with no margins
    Log::Comment(L"With no margins, setting DECOM moves the cursor to the top-left of the screen.");
    stateMachine.ProcessString(L"\x1B[r");
    cursor.SetPosition({ 40, 12 });
    stateMachine.ProcessString(L"\x1B[?6h");
    VERIFY_ARE_EQUAL(COORD({ 0, 0 }), cursor.GetPosition());
    Log::Comment(L"Cursor addressing is still relative to the top-left of the screen.");
    stateMachine.ProcessString(L"\x1B[13;41H");
    VERIFY_ARE_EQUAL(COORD({ 40, 12 }), cursor.GetPosition());

    // Reset DECOM so we don't affect future tests
    stateMachine.ProcessString(L"\x1B[?6l");
}

void ScreenBufferTests::HardResetBuffer()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    auto& stateMachine = si.GetStateMachine();
    const auto& viewport = si.GetViewport();
    const auto& cursor = si.GetTextBuffer().GetCursor();
    WI_SetFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    auto isBufferClear = [&]() {
        auto offset = 0;
        auto width = si.GetBufferSize().Width();
        for (auto iter = si.GetCellDataAt({}); iter; ++iter, ++offset)
        {
            if (iter->Chars() != L" " || iter->TextAttr() != TextAttribute{})
            {
                Log::Comment(NoThrowString().Format(
                    L"Buffer not clear at (X:%d, Y:%d)",
                    offset % width,
                    offset / width));
                return false;
            }
        }
        return true;
    };

    const auto resetToInitialState = L"\033c";

    Log::Comment(L"Start with a clear buffer, viewport and cursor at 0,0");
    si.SetAttributes(TextAttribute());
    si.ClearTextData();
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, { 0, 0 }, true));
    VERIFY_SUCCEEDED(si.SetCursorPosition({ 0, 0 }, true));
    VERIFY_IS_TRUE(isBufferClear());

    Log::Comment(L"Write a single line of text to the buffer");
    stateMachine.ProcessString(L"Hello World!\n");
    VERIFY_IS_FALSE(isBufferClear());
    VERIFY_ARE_EQUAL(COORD({ 0, 1 }), cursor.GetPosition());

    Log::Comment(L"After a reset, buffer should be clear, with cursor at 0,0");
    stateMachine.ProcessString(resetToInitialState);
    VERIFY_IS_TRUE(isBufferClear());
    VERIFY_ARE_EQUAL(COORD({ 0, 0 }), cursor.GetPosition());

    Log::Comment(L"Set the background color to red");
    stateMachine.ProcessString(L"\x1b[41m");
    Log::Comment(L"Write multiple pages of text to the buffer");
    for (auto i = 0; i < viewport.Height() * 2; i++)
    {
        stateMachine.ProcessString(L"Hello World!\n");
    }
    VERIFY_IS_FALSE(isBufferClear());
    VERIFY_IS_GREATER_THAN(viewport.Top(), viewport.Height());
    VERIFY_IS_GREATER_THAN(cursor.GetPosition().Y, viewport.Height());

    Log::Comment(L"After a reset, buffer should be clear, with viewport and cursor at 0,0");
    stateMachine.ProcessString(resetToInitialState);
    VERIFY_IS_TRUE(isBufferClear());
    VERIFY_ARE_EQUAL(COORD({ 0, 0 }), viewport.Origin());
    VERIFY_ARE_EQUAL(COORD({ 0, 0 }), cursor.GetPosition());
}

void ScreenBufferTests::RestoreDownAltBufferWithTerminalScrolling()
{
    // This is a test for microsoft/terminal#1206. Refer to that issue for more
    // context

    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.SetTerminalScrolling(true);
    gci.LockConsole(); // Lock must be taken to manipulate buffer.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    auto& siMain = gci.GetActiveOutputBuffer();
    COORD const coordFontSize = siMain.GetScreenFontSize();
    siMain._virtualBottom = siMain._viewport.BottomInclusive();

    auto originalView = siMain._viewport;

    VERIFY_IS_NULL(siMain._psiMainBuffer);
    VERIFY_IS_NULL(siMain._psiAlternateBuffer);

    Log::Comment(L"Create an alternate buffer");
    if (VERIFY_IS_TRUE(NT_SUCCESS(siMain.UseAlternateScreenBuffer())))
    {
        VERIFY_IS_NOT_NULL(siMain._psiAlternateBuffer);
        auto& altBuffer = *siMain._psiAlternateBuffer;
        VERIFY_ARE_EQUAL(0, altBuffer._viewport.Top());
        VERIFY_ARE_EQUAL(altBuffer._viewport.BottomInclusive(), altBuffer._virtualBottom);

        auto useMain = wil::scope_exit([&] { altBuffer.UseMainScreenBuffer(); });

        const COORD originalSize = originalView.Dimensions();
        const COORD doubledSize = { originalSize.X * 2, originalSize.Y * 2 };

        // Create some RECTs, which are dimensions in pixels, because
        // ProcessResizeWindow needs to work on rects in screen _pixel_
        // dimensions, not character sizes.
        RECT originalClientRect{ 0 }, maximizedClientRect{ 0 };

        originalClientRect.right = originalSize.X * coordFontSize.X;
        originalClientRect.bottom = originalSize.Y * coordFontSize.Y;

        maximizedClientRect.right = doubledSize.X * coordFontSize.X;
        maximizedClientRect.bottom = doubledSize.Y * coordFontSize.Y;

        Log::Comment(NoThrowString().Format(
            L"Emulate a maximize"));
        // Note that just calling _InternalSetViewportSize does not hit the
        // exceptional case here. There's other logic farther down the stack
        // that triggers it.
        altBuffer.ProcessResizeWindow(&maximizedClientRect, &originalClientRect);

        VERIFY_ARE_EQUAL(0, altBuffer._viewport.Top());
        VERIFY_ARE_EQUAL(altBuffer._viewport.BottomInclusive(), altBuffer._virtualBottom);

        Log::Comment(NoThrowString().Format(
            L"Emulate a restore down"));

        altBuffer.ProcessResizeWindow(&originalClientRect, &maximizedClientRect);

        // Before the bugfix, this would fail, with the top being roughly 80,
        // halfway into the buffer, with the bottom being anchored to the old
        // size.
        VERIFY_ARE_EQUAL(0, altBuffer._viewport.Top());
        VERIFY_ARE_EQUAL(altBuffer._viewport.BottomInclusive(), altBuffer._virtualBottom);
    }
}

void ScreenBufferTests::SnapCursorWithTerminalScrolling()
{
    // This is a test for microsoft/terminal#1222. Refer to that issue for more
    // context

    auto& g = ServiceLocator::LocateGlobals();
    CONSOLE_INFORMATION& gci = g.getConsoleInformation();
    gci.SetTerminalScrolling(true);
    gci.LockConsole(); // Lock must be taken to manipulate buffer.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    auto& si = gci.GetActiveOutputBuffer();
    auto& cursor = si.GetTextBuffer().GetCursor();
    const auto originalView = si._viewport;
    si._virtualBottom = originalView.BottomInclusive();

    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<COORD>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"originalView=%s", VerifyOutputTraits<SMALL_RECT>::ToString(originalView.ToInclusive()).GetBuffer()));

    Log::Comment(NoThrowString().Format(
        L"First set the viewport somewhere lower in the buffer, as if the text "
        L"was output there. Manually move the cursor there as well, so the "
        L"cursor is within that viewport."));
    const COORD secondWindowOrigin{ 0, 10 };
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, secondWindowOrigin, true));
    si.GetTextBuffer().GetCursor().SetPosition(secondWindowOrigin);

    const auto secondView = si._viewport;
    const auto secondVirtualBottom = si._virtualBottom;
    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<COORD>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"secondView=%s", VerifyOutputTraits<SMALL_RECT>::ToString(secondView.ToInclusive()).GetBuffer()));

    VERIFY_ARE_EQUAL(10, secondView.Top());
    VERIFY_ARE_EQUAL(originalView.Height() + 10, secondView.BottomExclusive());
    VERIFY_ARE_EQUAL(originalView.Height() + 10 - 1, secondVirtualBottom);

    Log::Comment(NoThrowString().Format(
        L"Emulate scrolling upwards with the mouse (not moving the virtual view)"));

    const COORD thirdWindowOrigin{ 0, 2 };
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, thirdWindowOrigin, false));

    const auto thirdView = si._viewport;
    const auto thirdVirtualBottom = si._virtualBottom;

    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<COORD>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"thirdView=%s", VerifyOutputTraits<SMALL_RECT>::ToString(thirdView.ToInclusive()).GetBuffer()));

    VERIFY_ARE_EQUAL(2, thirdView.Top());
    VERIFY_ARE_EQUAL(originalView.Height() + 2, thirdView.BottomExclusive());
    VERIFY_ARE_EQUAL(secondVirtualBottom, thirdVirtualBottom);

    Log::Comment(NoThrowString().Format(
        L"Call SetConsoleCursorPosition to snap to the cursor"));
    VERIFY_SUCCEEDED(g.api.SetConsoleCursorPositionImpl(si, secondWindowOrigin));

    const auto fourthView = si._viewport;
    const auto fourthVirtualBottom = si._virtualBottom;

    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<COORD>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"thirdView=%s", VerifyOutputTraits<SMALL_RECT>::ToString(fourthView.ToInclusive()).GetBuffer()));

    VERIFY_ARE_EQUAL(10, fourthView.Top());
    VERIFY_ARE_EQUAL(originalView.Height() + 10, fourthView.BottomExclusive());
    VERIFY_ARE_EQUAL(secondVirtualBottom, fourthVirtualBottom);
}

void ScreenBufferTests::ClearAlternateBuffer()
{
    // This is a test for microsoft/terminal#1189. Refer to that issue for more
    // context

    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& g = ServiceLocator::LocateGlobals();
    gci.LockConsole(); // Lock must be taken to manipulate buffer.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    auto& siMain = gci.GetActiveOutputBuffer();
    auto WriteText = [&](TextBuffer& tbi) {
        // Write text to buffer
        auto& stateMachine = siMain.GetStateMachine();
        auto& cursor = tbi.GetCursor();
        stateMachine.ProcessString(L"foo\nfoo");
        VERIFY_ARE_EQUAL(cursor.GetPosition().X, 3);
        VERIFY_ARE_EQUAL(cursor.GetPosition().Y, 1);
    };

    auto VerifyText = [&](TextBuffer& tbi) {
        // Verify written text in buffer
        {
            auto iter00 = tbi.GetCellDataAt({ 0, 0 });
            auto iter10 = tbi.GetCellDataAt({ 1, 0 });
            auto iter20 = tbi.GetCellDataAt({ 2, 0 });
            auto iter30 = tbi.GetCellDataAt({ 3, 0 });
            auto iter01 = tbi.GetCellDataAt({ 0, 1 });
            auto iter02 = tbi.GetCellDataAt({ 1, 1 });
            auto iter03 = tbi.GetCellDataAt({ 2, 1 });
            VERIFY_ARE_EQUAL(L"f", iter00->Chars());
            VERIFY_ARE_EQUAL(L"o", iter10->Chars());
            VERIFY_ARE_EQUAL(L"o", iter20->Chars());
            VERIFY_ARE_EQUAL(L"\x20", iter30->Chars());
            VERIFY_ARE_EQUAL(L"f", iter01->Chars());
            VERIFY_ARE_EQUAL(L"o", iter02->Chars());
            VERIFY_ARE_EQUAL(L"o", iter03->Chars());
        }
    };

    WriteText(siMain.GetTextBuffer());
    VerifyText(siMain.GetTextBuffer());

    Log::Comment(L"Create an alternate buffer");
    if (VERIFY_IS_TRUE(NT_SUCCESS(siMain.UseAlternateScreenBuffer())))
    {
        VERIFY_IS_NOT_NULL(siMain._psiAlternateBuffer);
        auto& altBuffer = *siMain._psiAlternateBuffer;
        VERIFY_ARE_EQUAL(0, altBuffer._viewport.Top());
        VERIFY_ARE_EQUAL(altBuffer._viewport.BottomInclusive(), altBuffer._virtualBottom);

        auto useMain = wil::scope_exit([&] { altBuffer.UseMainScreenBuffer(); });

        WriteText(altBuffer.GetTextBuffer());
        VerifyText(altBuffer.GetTextBuffer());

#pragma region Test ScrollConsoleScreenBufferWImpl()
        // Clear text of alt buffer (same params as in CMD)
        VERIFY_SUCCEEDED(g.api.ScrollConsoleScreenBufferWImpl(siMain,
                                                              { 0, 0, 120, 9001 },
                                                              { 0, -9001 },
                                                              std::nullopt,
                                                              L' ',
                                                              7));

        // Verify text is now gone
        VERIFY_ARE_EQUAL(L" ", altBuffer.GetTextBuffer().GetCellDataAt({ 0, 0 })->Chars());
#pragma endregion

#pragma region Test SetConsoleCursorPositionImpl()
        // Reset cursor position as we do with CLS command (same params as in CMD)
        VERIFY_SUCCEEDED(g.api.SetConsoleCursorPositionImpl(siMain, { 0 }));

        // Verify state of alt buffer
        auto& altBufferCursor = altBuffer.GetTextBuffer().GetCursor();
        VERIFY_ARE_EQUAL(altBufferCursor.GetPosition().X, 0);
        VERIFY_ARE_EQUAL(altBufferCursor.GetPosition().Y, 0);
#pragma endregion
    }

    // Verify state of main buffer is untouched
    auto& cursor = siMain.GetTextBuffer().GetCursor();
    VERIFY_ARE_EQUAL(cursor.GetPosition().X, 3);
    VERIFY_ARE_EQUAL(cursor.GetPosition().Y, 1);

    VerifyText(siMain.GetTextBuffer());
}

void ScreenBufferTests::InitializeTabStopsInVTMode()
{
    // This is a test for microsoft/terminal#411. Refer to that issue for more
    // context.

    // Run this test in isolation - Let's not pollute the VT level for other
    // tests, or go blowing away other test's buffers
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
    END_TEST_METHOD_PROPERTIES()

    auto& g = ServiceLocator::LocateGlobals();
    auto& gci = g.getConsoleInformation();

    VERIFY_IS_FALSE(gci.GetActiveOutputBuffer().AreTabsSet());

    // Enable VT mode before we construct the buffer. This emulates setting the
    // VirtualTerminalLevel reg key before launching the console.
    gci.SetVirtTermLevel(1);

    // Clean up the old buffer, and re-create it. This new buffer will be
    // created as if the VT mode was always on.
    m_state->CleanupGlobalScreenBuffer();
    m_state->PrepareGlobalScreenBuffer();

    VERIFY_IS_TRUE(gci.GetActiveOutputBuffer().AreTabsSet());
}

void ScreenBufferTests::TestExtendedTextAttributes()
{
    // This is a test for microsoft/terminal#2554. Refer to that issue for more
    // context.

    // We're going to set every possible combination of extended attributes via
    // VT, then disable them, and make sure that they are all always represented
    // internally correctly.

    // Run this test for each and every possible combination of states.
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:bold", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:italics", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:blink", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:invisible", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:crossedOut", L"{false, true}")
    END_TEST_METHOD_PROPERTIES()

    bool bold, italics, blink, invisible, crossedOut;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"bold", bold));
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"italics", italics));
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"blink", blink));
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"invisible", invisible));
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"crossedOut", crossedOut));

    auto& g = ServiceLocator::LocateGlobals();
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = tbi.GetCursor();

    ExtendedAttributes expectedAttrs{ ExtendedAttributes::Normal };
    std::wstring vtSeq = L"";

    // Collect up a VT sequence to set the state given the method properties
    if (bold)
    {
        WI_SetFlag(expectedAttrs, ExtendedAttributes::Bold);
        vtSeq += L"\x1b[1m";
    }
    if (italics)
    {
        WI_SetFlag(expectedAttrs, ExtendedAttributes::Italics);
        vtSeq += L"\x1b[3m";
    }
    if (blink)
    {
        WI_SetFlag(expectedAttrs, ExtendedAttributes::Blinking);
        vtSeq += L"\x1b[5m";
    }
    if (invisible)
    {
        WI_SetFlag(expectedAttrs, ExtendedAttributes::Invisible);
        vtSeq += L"\x1b[8m";
    }
    if (crossedOut)
    {
        WI_SetFlag(expectedAttrs, ExtendedAttributes::CrossedOut);
        vtSeq += L"\x1b[9m";
    }

    // Helper lambda to write a VT sequence, then an "X", then check that the
    // attributes of the "X" match what we think they should be.
    auto validate = [&](const ExtendedAttributes expectedAttrs,
                        const std::wstring& vtSequence) {
        auto cursorPos = cursor.GetPosition();

        // Convert the vtSequence to something printable. Lets not set these
        // attrs on the test console
        std::wstring debugString = vtSequence;
        {
            size_t start_pos = 0;
            while ((start_pos = debugString.find(L"\x1b", start_pos)) != std::string::npos)
            {
                debugString.replace(start_pos, 1, L"\\x1b");
                start_pos += 4;
            }
        }

        Log::Comment(NoThrowString().Format(
            L"Testing string:\"%s\"", debugString.c_str()));
        Log::Comment(NoThrowString().Format(
            L"Expecting attrs:0x%02x", expectedAttrs));

        stateMachine.ProcessString(vtSequence);
        stateMachine.ProcessString(L"X");

        auto iter = tbi.GetCellDataAt(cursorPos);
        auto currentExtendedAttrs = iter->TextAttr().GetExtendedAttributes();
        VERIFY_ARE_EQUAL(expectedAttrs, currentExtendedAttrs);
    };

    // Check setting all the states collected above
    validate(expectedAttrs, vtSeq);

    // One-by-one, turn off each of these states with VT, then check that the
    // state matched.
    if (bold)
    {
        WI_ClearFlag(expectedAttrs, ExtendedAttributes::Bold);
        vtSeq = L"\x1b[22m";
        validate(expectedAttrs, vtSeq);
    }
    if (italics)
    {
        WI_ClearFlag(expectedAttrs, ExtendedAttributes::Italics);
        vtSeq = L"\x1b[23m";
        validate(expectedAttrs, vtSeq);
    }
    if (blink)
    {
        WI_ClearFlag(expectedAttrs, ExtendedAttributes::Blinking);
        vtSeq = L"\x1b[25m";
        validate(expectedAttrs, vtSeq);
    }
    if (invisible)
    {
        WI_ClearFlag(expectedAttrs, ExtendedAttributes::Invisible);
        vtSeq = L"\x1b[28m";
        validate(expectedAttrs, vtSeq);
    }
    if (crossedOut)
    {
        WI_ClearFlag(expectedAttrs, ExtendedAttributes::CrossedOut);
        vtSeq = L"\x1b[29m";
        validate(expectedAttrs, vtSeq);
    }

    stateMachine.ProcessString(L"\x1b[0m");
}

void ScreenBufferTests::TestExtendedTextAttributesWithColors()
{
    // This is a test for microsoft/terminal#2554. Refer to that issue for more
    // context.

    // We're going to set every possible combination of extended attributes via
    // VT, then set assorted colors, then disable extended attrs, then reset
    // colors, in various ways, and make sure that they are all always
    // represented internally correctly.

    // Run this test for each and every possible combination of states.
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:bold", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:italics", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:blink", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:invisible", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:crossedOut", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:setForegroundType", L"{0, 1, 2, 3}")
        TEST_METHOD_PROPERTY(L"Data:setBackgroundType", L"{0, 1, 2, 3}")
    END_TEST_METHOD_PROPERTIES()

    // colorStyle will be used to control whether we use a color from the 16
    // color table, a color from the 256 color table, or a pure RGB color.
    const int UseDefault = 0;
    const int Use16Color = 1;
    const int Use256Color = 2;
    const int UseRGBColor = 3;

    bool bold, italics, blink, invisible, crossedOut;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"bold", bold));
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"italics", italics));
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"blink", blink));
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"invisible", invisible));
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"crossedOut", crossedOut));

    int setForegroundType, setBackgroundType;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"setForegroundType", setForegroundType), L"controls whether to use the 16 color table, 256 table, or RGB colors");
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"setBackgroundType", setBackgroundType), L"controls whether to use the 16 color table, 256 table, or RGB colors");

    auto& g = ServiceLocator::LocateGlobals();
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = tbi.GetCursor();

    TextAttribute expectedAttr{ si.GetAttributes() };
    ExtendedAttributes expectedExtendedAttrs{ ExtendedAttributes::Normal };
    std::wstring vtSeq = L"";

    // Collect up a VT sequence to set the state given the method properties
    if (bold)
    {
        WI_SetFlag(expectedExtendedAttrs, ExtendedAttributes::Bold);
        vtSeq += L"\x1b[1m";
    }
    if (italics)
    {
        WI_SetFlag(expectedExtendedAttrs, ExtendedAttributes::Italics);
        vtSeq += L"\x1b[3m";
    }
    if (blink)
    {
        WI_SetFlag(expectedExtendedAttrs, ExtendedAttributes::Blinking);
        vtSeq += L"\x1b[5m";
    }
    if (invisible)
    {
        WI_SetFlag(expectedExtendedAttrs, ExtendedAttributes::Invisible);
        vtSeq += L"\x1b[8m";
    }
    if (crossedOut)
    {
        WI_SetFlag(expectedExtendedAttrs, ExtendedAttributes::CrossedOut);
        vtSeq += L"\x1b[9m";
    }

    // Prepare the foreground attributes
    if (setForegroundType == UseDefault)
    {
        expectedAttr.SetDefaultForeground();
        vtSeq += L"\x1b[39m";
    }
    else if (setForegroundType == Use16Color)
    {
        expectedAttr.SetIndexedAttributes({ static_cast<BYTE>(2) }, std::nullopt);
        vtSeq += L"\x1b[32m";
    }
    else if (setForegroundType == Use256Color)
    {
        expectedAttr.SetForeground(gci.GetColorTableEntry(20));
        vtSeq += L"\x1b[38;5;20m";
    }
    else if (setForegroundType == UseRGBColor)
    {
        expectedAttr.SetForeground(RGB(1, 2, 3));
        vtSeq += L"\x1b[38;2;1;2;3m";
    }

    // Prepare the background attributes
    if (setBackgroundType == UseDefault)
    {
        expectedAttr.SetDefaultBackground();
        vtSeq += L"\x1b[49m";
    }
    else if (setBackgroundType == Use16Color)
    {
        expectedAttr.SetIndexedAttributes(std::nullopt, { static_cast<BYTE>(2) });
        vtSeq += L"\x1b[42m";
    }
    else if (setBackgroundType == Use256Color)
    {
        expectedAttr.SetBackground(gci.GetColorTableEntry(20));
        vtSeq += L"\x1b[48;5;20m";
    }
    else if (setBackgroundType == UseRGBColor)
    {
        expectedAttr.SetBackground(RGB(1, 2, 3));
        vtSeq += L"\x1b[48;2;1;2;3m";
    }

    expectedAttr.SetExtendedAttributes(expectedExtendedAttrs);

    // Helper lambda to write a VT sequence, then an "X", then check that the
    // attributes of the "X" match what we think they should be.
    auto validate = [&](const TextAttribute attr,
                        const std::wstring& vtSequence) {
        auto cursorPos = cursor.GetPosition();

        // Convert the vtSequence to something printable. Lets not set these
        // attrs on the test console
        std::wstring debugString = vtSequence;
        {
            size_t start_pos = 0;
            while ((start_pos = debugString.find(L"\x1b", start_pos)) != std::string::npos)
            {
                debugString.replace(start_pos, 1, L"\\x1b");
                start_pos += 4;
            }
        }

        Log::Comment(NoThrowString().Format(
            L"Testing string:\"%s\"", debugString.c_str()));
        Log::Comment(NoThrowString().Format(
            L"Expecting attrs:0x%02x", VerifyOutputTraits<TextAttribute>::ToString(attr).GetBuffer()));

        stateMachine.ProcessString(vtSequence);
        stateMachine.ProcessString(L"X");

        auto iter = tbi.GetCellDataAt(cursorPos);
        const TextAttribute currentAttrs = iter->TextAttr();
        VERIFY_ARE_EQUAL(attr, currentAttrs);
    };

    // Check setting all the states collected above
    validate(expectedAttr, vtSeq);

    // One-by-one, turn off each of these states with VT, then check that the
    // state matched.
    if (bold)
    {
        WI_ClearFlag(expectedExtendedAttrs, ExtendedAttributes::Bold);
        expectedAttr.SetExtendedAttributes(expectedExtendedAttrs);
        vtSeq = L"\x1b[22m";
        validate(expectedAttr, vtSeq);
    }
    if (italics)
    {
        WI_ClearFlag(expectedExtendedAttrs, ExtendedAttributes::Italics);
        expectedAttr.SetExtendedAttributes(expectedExtendedAttrs);
        vtSeq = L"\x1b[23m";
        validate(expectedAttr, vtSeq);
    }
    if (blink)
    {
        WI_ClearFlag(expectedExtendedAttrs, ExtendedAttributes::Blinking);
        expectedAttr.SetExtendedAttributes(expectedExtendedAttrs);
        vtSeq = L"\x1b[25m";
        validate(expectedAttr, vtSeq);
    }
    if (invisible)
    {
        WI_ClearFlag(expectedExtendedAttrs, ExtendedAttributes::Invisible);
        expectedAttr.SetExtendedAttributes(expectedExtendedAttrs);
        vtSeq = L"\x1b[28m";
        validate(expectedAttr, vtSeq);
    }
    if (crossedOut)
    {
        WI_ClearFlag(expectedExtendedAttrs, ExtendedAttributes::CrossedOut);
        expectedAttr.SetExtendedAttributes(expectedExtendedAttrs);
        vtSeq = L"\x1b[29m";
        validate(expectedAttr, vtSeq);
    }

    stateMachine.ProcessString(L"\x1b[0m");
}

void ScreenBufferTests::CursorUpDownAcrossMargins()
{
    // Test inspired by: https://github.com/microsoft/terminal/issues/2929
    // echo -e "\e[6;19r\e[24H\e[99AX\e[1H\e[99BY\e[r"
    // This does the following:
    // * sets the top and bottom DECSTBM margins to 6 and 19
    // * moves to line 24 (i.e. below the bottom margin)
    // * executes the CUU sequence with a count of 99, to move up 99 lines
    // * writes out X
    // * moves to line 1 (i.e. above the top margin)
    // * executes the CUD sequence with a count of 99, to move down 99 lines
    // * writes out Y

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si.GetTextBuffer().GetCursor();

    VERIFY_IS_TRUE(si.GetViewport().BottomInclusive() > 24);

    // Set some scrolling margins
    stateMachine.ProcessString(L"\x1b[6;19r");
    stateMachine.ProcessString(L"\x1b[24H");
    VERIFY_ARE_EQUAL(23, cursor.GetPosition().Y);

    stateMachine.ProcessString(L"\x1b[99A");
    VERIFY_ARE_EQUAL(5, cursor.GetPosition().Y);
    stateMachine.ProcessString(L"X");
    {
        auto iter = tbi.GetCellDataAt({ 0, 5 });
        VERIFY_ARE_EQUAL(L"X", iter->Chars());
    }
    stateMachine.ProcessString(L"\x1b[1H");
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().Y);

    stateMachine.ProcessString(L"\x1b[99B");
    VERIFY_ARE_EQUAL(18, cursor.GetPosition().Y);
    stateMachine.ProcessString(L"Y");
    {
        auto iter = tbi.GetCellDataAt({ 0, 18 });
        VERIFY_ARE_EQUAL(L"Y", iter->Chars());
    }
    stateMachine.ProcessString(L"\x1b[r");
}

void ScreenBufferTests::CursorUpDownOutsideMargins()
{
    // Test inspired by the CursorUpDownAcrossMargins test.
    // echo -e "\e[6;19r\e[24H\e[1AX\e[1H\e[1BY\e[r"
    // This does the following:
    // * sets the top and bottom DECSTBM margins to 6 and 19
    // * moves to line 24 (i.e. below the bottom margin)
    // * executes the CUU sequence with a count of 1, to move up 1 lines (still below margins)
    // * writes out X
    // * moves to line 1 (i.e. above the top margin)
    // * executes the CUD sequence with a count of 1, to move down 1 lines (still above margins)
    // * writes out Y

    // This test is different becasue the end location of the vertical movement
    // should not be within the margins at all. We should not clamp this
    // movement to be within the margins.

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si.GetTextBuffer().GetCursor();

    VERIFY_IS_TRUE(si.GetViewport().BottomInclusive() > 24);

    // Set some scrolling margins
    stateMachine.ProcessString(L"\x1b[6;19r");
    stateMachine.ProcessString(L"\x1b[24H");
    VERIFY_ARE_EQUAL(23, cursor.GetPosition().Y);

    stateMachine.ProcessString(L"\x1b[1A");
    VERIFY_ARE_EQUAL(22, cursor.GetPosition().Y);
    stateMachine.ProcessString(L"X");
    {
        auto iter = tbi.GetCellDataAt({ 0, 22 });
        VERIFY_ARE_EQUAL(L"X", iter->Chars());
    }
    stateMachine.ProcessString(L"\x1b[1H");
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().Y);

    stateMachine.ProcessString(L"\x1b[1B");
    VERIFY_ARE_EQUAL(1, cursor.GetPosition().Y);
    stateMachine.ProcessString(L"Y");
    {
        auto iter = tbi.GetCellDataAt({ 0, 1 });
        VERIFY_ARE_EQUAL(L"Y", iter->Chars());
    }
    stateMachine.ProcessString(L"\x1b[r");
}

void ScreenBufferTests::CursorUpDownExactlyAtMargins()
{
    // Test inspired by the CursorUpDownAcrossMargins test.
    // echo -e "\e[6;19r\e[19H\e[1B1\e[1A2\e[6H\e[1A3\e[1B4\e[r"
    // This does the following:
    // * sets the top and bottom DECSTBM margins to 6 and 19
    // * moves to line 19 (i.e. on the bottom margin)
    // * executes the CUD sequence with a count of 1, to move down 1 lines (still on the margin)
    // * writes out 1
    // * executes the CUU sequence with a count of 1, to move up 1 lines (now inside margins)
    // * writes out 2
    // * moves to line 6 (i.e. on the top margin)
    // * executes the CUU sequence with a count of 1, to move up 1 lines (still on the margin)
    // * writes out 3
    // * executes the CUD sequence with a count of 1, to move down 1 lines (still above margins)
    // * writes out 4

    // This test is different becasue the starting location for these scroll
    // operations is _exactly_ on the margins

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si.GetTextBuffer().GetCursor();

    VERIFY_IS_TRUE(si.GetViewport().BottomInclusive() > 24);

    // Set some scrolling margins
    stateMachine.ProcessString(L"\x1b[6;19r");

    stateMachine.ProcessString(L"\x1b[19;1H");
    VERIFY_ARE_EQUAL(18, cursor.GetPosition().Y);
    stateMachine.ProcessString(L"\x1b[1B");
    VERIFY_ARE_EQUAL(18, cursor.GetPosition().Y);
    stateMachine.ProcessString(L"1");
    {
        auto iter = tbi.GetCellDataAt({ 0, 18 });
        VERIFY_ARE_EQUAL(L"1", iter->Chars());
    }

    stateMachine.ProcessString(L"\x1b[1A");
    VERIFY_ARE_EQUAL(17, cursor.GetPosition().Y);
    stateMachine.ProcessString(L"2");
    {
        auto iter = tbi.GetCellDataAt({ 1, 17 });
        VERIFY_ARE_EQUAL(L"2", iter->Chars());
    }

    stateMachine.ProcessString(L"\x1b[6;1H");
    VERIFY_ARE_EQUAL(5, cursor.GetPosition().Y);

    stateMachine.ProcessString(L"\x1b[1A");
    VERIFY_ARE_EQUAL(5, cursor.GetPosition().Y);
    stateMachine.ProcessString(L"3");
    {
        auto iter = tbi.GetCellDataAt({ 0, 5 });
        VERIFY_ARE_EQUAL(L"3", iter->Chars());
    }

    stateMachine.ProcessString(L"\x1b[1B");
    VERIFY_ARE_EQUAL(6, cursor.GetPosition().Y);
    stateMachine.ProcessString(L"4");
    {
        auto iter = tbi.GetCellDataAt({ 1, 6 });
        VERIFY_ARE_EQUAL(L"4", iter->Chars());
    }

    stateMachine.ProcessString(L"\x1b[r");
}

void ScreenBufferTests::CursorSaveRestore()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si.GetTextBuffer().GetCursor();

    const auto defaultAttrs = TextAttribute{};
    const auto colorAttrs = TextAttribute{ RGB(12, 34, 56), RGB(78, 90, 12) };

    const auto asciiText = L"lwkmvj";
    const auto graphicText = L"┌┬┐└┴┘";

    const auto selectAsciiChars = L"\x1b(B";
    const auto selectGraphicsChars = L"\x1b(0";
    const auto saveCursor = L"\x1b[s";
    const auto restoreCursor = L"\x1b[u";
    const auto setDECOM = L"\x1b[?6h";
    const auto resetDECOM = L"\x1b[?6l";

    Log::Comment(L"Make sure the viewport is at 0,0");
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, COORD({ 0, 0 }), true));

    Log::Comment(L"Restore after save.");
    // Set the cursor position, attributes, and character set.
    cursor.SetPosition(COORD{ 20, 10 });
    si.SetAttributes(colorAttrs);
    stateMachine.ProcessString(selectGraphicsChars);
    // Save state.
    stateMachine.ProcessString(saveCursor);
    // Reset the cursor position, attributes, and character set.
    cursor.SetPosition(COORD{ 0, 0 });
    si.SetAttributes(defaultAttrs);
    stateMachine.ProcessString(selectAsciiChars);
    // Restore state.
    stateMachine.ProcessString(restoreCursor);
    // Verify initial position, colors, and graphic character set.
    VERIFY_ARE_EQUAL(COORD({ 20, 10 }), cursor.GetPosition());
    VERIFY_ARE_EQUAL(colorAttrs, si.GetAttributes());
    stateMachine.ProcessString(asciiText);
    VERIFY_IS_TRUE(_ValidateLineContains(COORD({ 20, 10 }), graphicText, colorAttrs));

    Log::Comment(L"Restore again without save.");
    // Reset the cursor position, attributes, and character set.
    cursor.SetPosition(COORD{ 0, 0 });
    si.SetAttributes(defaultAttrs);
    stateMachine.ProcessString(selectAsciiChars);
    // Restore state.
    stateMachine.ProcessString(restoreCursor);
    // Verify initial saved position, colors, and graphic character set.
    VERIFY_ARE_EQUAL(COORD({ 20, 10 }), cursor.GetPosition());
    VERIFY_ARE_EQUAL(colorAttrs, si.GetAttributes());
    stateMachine.ProcessString(asciiText);
    VERIFY_IS_TRUE(_ValidateLineContains(COORD({ 20, 10 }), graphicText, colorAttrs));

    Log::Comment(L"Restore after reset.");
    // Soft reset.
    stateMachine.ProcessString(L"\x1b[!p");
    // Set the cursor position, attributes, and character set.
    cursor.SetPosition(COORD{ 20, 10 });
    si.SetAttributes(colorAttrs);
    stateMachine.ProcessString(selectGraphicsChars);
    // Restore state.
    stateMachine.ProcessString(restoreCursor);
    // Verify home position, default attributes, and ascii character set.
    VERIFY_ARE_EQUAL(COORD({ 0, 0 }), cursor.GetPosition());
    VERIFY_ARE_EQUAL(defaultAttrs, si.GetAttributes());
    stateMachine.ProcessString(asciiText);
    VERIFY_IS_TRUE(_ValidateLineContains(COORD({ 0, 0 }), asciiText, defaultAttrs));

    Log::Comment(L"Restore origin mode.");
    // Set margins and origin mode to relative.
    stateMachine.ProcessString(L"\x1b[10;20r");
    stateMachine.ProcessString(setDECOM);
    // Verify home position inside margins.
    VERIFY_ARE_EQUAL(COORD({ 0, 9 }), cursor.GetPosition());
    // Save state and reset origin mode to absolute.
    stateMachine.ProcessString(saveCursor);
    stateMachine.ProcessString(resetDECOM);
    // Verify home position at origin.
    VERIFY_ARE_EQUAL(COORD({ 0, 0 }), cursor.GetPosition());
    // Restore state and move to home position.
    stateMachine.ProcessString(restoreCursor);
    stateMachine.ProcessString(L"\x1b[H");
    // Verify home position inside margins, i.e. relative origin mode restored.
    VERIFY_ARE_EQUAL(COORD({ 0, 9 }), cursor.GetPosition());

    Log::Comment(L"Clamp inside top margin.");
    // Reset margins, with absolute origin, and set cursor position.
    stateMachine.ProcessString(L"\x1b[r");
    stateMachine.ProcessString(setDECOM);
    cursor.SetPosition(COORD{ 5, 15 });
    // Save state.
    stateMachine.ProcessString(saveCursor);
    // Set margins and restore state.
    stateMachine.ProcessString(L"\x1b[20;25r");
    stateMachine.ProcessString(restoreCursor);
    // Verfify Y position is clamped inside the top margin
    VERIFY_ARE_EQUAL(COORD({ 5, 19 }), cursor.GetPosition());

    Log::Comment(L"Clamp inside bottom margin.");
    // Reset margins, with absolute origin, and set cursor position.
    stateMachine.ProcessString(L"\x1b[r");
    stateMachine.ProcessString(setDECOM);
    cursor.SetPosition(COORD{ 5, 15 });
    // Save state.
    stateMachine.ProcessString(saveCursor);
    // Set margins and restore state.
    stateMachine.ProcessString(L"\x1b[1;10r");
    stateMachine.ProcessString(restoreCursor);
    // Verfify Y position is clamped inside the top margin
    VERIFY_ARE_EQUAL(COORD({ 5, 9 }), cursor.GetPosition());

    // Reset origin mode and margins.
    stateMachine.ProcessString(resetDECOM);
    stateMachine.ProcessString(L"\x1b[r");
}
