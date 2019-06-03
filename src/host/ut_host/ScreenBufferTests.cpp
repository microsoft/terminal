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

    TEST_METHOD(VtResize);
    TEST_METHOD(VtResizeComprehensive);

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

    TEST_METHOD(ScrollUpInMargins);
    TEST_METHOD(ScrollDownInMargins);
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
    stateMachine.ProcessString(L"ABCDEFGH", 9);
    VERIFY_ARE_EQUAL(cursor.GetPosition().X, 9);
    VERIFY_ARE_EQUAL(cursor.GetPosition().Y, 5);
    VERIFY_ARE_EQUAL(screenInfo.GetViewport().Top(), 5);

    LOG_IF_FAILED(DoSrvPrivateReverseLineFeed(screenInfo));

    VERIFY_ARE_EQUAL(cursor.GetPosition().X, 9);
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
