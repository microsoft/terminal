// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "../../inc/consoletaeftemplates.hpp"

#include "CommonState.hpp"

#include "globals.h"
#include "screenInfo.hpp"
#include "input.h"
#include "getset.h"
#include "_stream.h" // For WriteCharsLegacy
#include "output.h" // For ScrollRegion

#include "../interactivity/inc/ServiceLocator.hpp"
#include "../../inc/conattrs.hpp"
#include "../../types/inc/Viewport.hpp"
#include "../../renderer/vt/Xterm256Engine.hpp"

#include "../../inc/TestUtils.h"

#include <sstream>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Interactivity;
using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::VirtualTerminal;
using namespace TerminalCoreUnitTests;

class ScreenBufferTests
{
    CommonState* m_state;

    TEST_CLASS(ScreenBufferTests);

    TEST_CLASS_SETUP(ClassSetup)
    {
        m_state = new CommonState();

        m_state->InitEvents();
        m_state->PrepareGlobalFont({ 1, 1 });
        m_state->PrepareGlobalRenderer();
        m_state->PrepareGlobalInputBuffer();
        m_state->PrepareGlobalScreenBuffer();

        return true;
    }

    TEST_CLASS_CLEANUP(ClassCleanup)
    {
        m_state->CleanupGlobalScreenBuffer();
        m_state->CleanupGlobalRenderer();
        m_state->CleanupGlobalFont();
        m_state->CleanupGlobalInputBuffer();

        delete m_state;

        return true;
    }

    TEST_METHOD_SETUP(MethodSetup)
    {
        // Set up some sane defaults
        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        gci.SetColorTableEntry(TextColor::DEFAULT_FOREGROUND, INVALID_COLOR);
        gci.SetColorTableEntry(TextColor::DEFAULT_BACKGROUND, INVALID_COLOR);
        gci.SetFillAttribute(0x07); // DARK_WHITE on DARK_BLACK
        gci.CalculateDefaultColorIndices();

        m_state->PrepareNewTextBufferInfo();
        auto& currentBuffer = gci.GetActiveOutputBuffer();
        // Make sure a test hasn't left us in the alt buffer on accident
        VERIFY_IS_FALSE(currentBuffer._IsAltBuffer());
        VERIFY_SUCCEEDED(currentBuffer.SetViewportOrigin(true, { 0, 0 }, true));
        // Make sure the viewport always starts off at the default size.
        auto defaultSize = til::size{ CommonState::s_csWindowWidth, CommonState::s_csWindowHeight };
        currentBuffer.SetViewport(Viewport::FromDimensions(defaultSize), true);
        VERIFY_ARE_EQUAL(til::point(0, 0), currentBuffer.GetTextBuffer().GetCursor().GetPosition());
        // Make sure the virtual bottom is correctly positioned.
        currentBuffer.UpdateBottom();

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

    TEST_METHOD(AlternateBufferCursorInheritanceTest);

    TEST_METHOD(TestReverseLineFeed);

    TEST_METHOD(TestResetClearTabStops);

    TEST_METHOD(TestAddTabStop);

    TEST_METHOD(TestClearTabStop);

    TEST_METHOD(TestGetForwardTab);

    TEST_METHOD(TestGetReverseTab);

    TEST_METHOD(TestAltBufferTabStops);

    TEST_METHOD(EraseAllTests);

    TEST_METHOD(InactiveControlCharactersTest);

    TEST_METHOD(VtResize);
    TEST_METHOD(VtResizeComprehensive);
    TEST_METHOD(VtResizeDECCOLM);
    TEST_METHOD(VtResizePreservingAttributes);

    TEST_METHOD(VtSoftResetCursorPosition);

    TEST_METHOD(VtScrollMarginsNewlineColor);

    TEST_METHOD(VtNewlinePastViewport);
    TEST_METHOD(VtNewlinePastEndOfBuffer);
    TEST_METHOD(VtNewlineOutsideMargins);

    TEST_METHOD(VtSetColorTable);
    TEST_METHOD(VtRestoreColorTableReport);

    TEST_METHOD(ResizeTraditionalDoesNotDoubleFreeAttrRows);

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
    TEST_METHOD(TestAltBufferRIS);

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

    TEST_METHOD(AssignColorAliases);

    TEST_METHOD(DeleteCharsNearEndOfLine);
    TEST_METHOD(DeleteCharsNearEndOfLineSimpleFirstCase);
    TEST_METHOD(DeleteCharsNearEndOfLineSimpleSecondCase);

    TEST_METHOD(DontResetColorsAboveVirtualBottom);

    TEST_METHOD(ScrollOperations);
    TEST_METHOD(InsertReplaceMode);
    TEST_METHOD(InsertChars);
    TEST_METHOD(DeleteChars);
    TEST_METHOD(HorizontalScrollOperations);
    TEST_METHOD(ScrollingWideCharsHorizontally);

    TEST_METHOD(EraseScrollbackTests);
    TEST_METHOD(EraseTests);
    TEST_METHOD(ProtectedAttributeTests);

    TEST_METHOD(ScrollUpInMargins);
    TEST_METHOD(ScrollDownInMargins);
    TEST_METHOD(InsertLinesInMargins);
    TEST_METHOD(DeleteLinesInMargins);
    TEST_METHOD(ReverseLineFeedInMargins);

    TEST_METHOD(LineFeedEscapeSequences);

    TEST_METHOD(ScrollLines256Colors);

    TEST_METHOD(SetLineFeedMode);
    TEST_METHOD(SetScreenMode);
    TEST_METHOD(SetOriginMode);
    TEST_METHOD(SetAutoWrapMode);

    TEST_METHOD(HardResetBuffer);

    TEST_METHOD(RestoreDownAltBufferWithTerminalScrolling);

    TEST_METHOD(SnapCursorWithTerminalScrolling);

    TEST_METHOD(ClearAlternateBuffer);

    TEST_METHOD(TestExtendedTextAttributes);
    TEST_METHOD(TestExtendedTextAttributesWithColors);

    TEST_METHOD(CursorUpDownAcrossMargins);
    TEST_METHOD(CursorUpDownOutsideMargins);
    TEST_METHOD(CursorUpDownExactlyAtMargins);

    TEST_METHOD(CursorLeftRightAcrossMargins);
    TEST_METHOD(CursorLeftRightOutsideMargins);
    TEST_METHOD(CursorLeftRightExactlyAtMargins);

    TEST_METHOD(CursorNextPreviousLine);
    TEST_METHOD(CursorPositionRelative);

    TEST_METHOD(CursorSaveRestore);

    TEST_METHOD(ScreenAlignmentPattern);

    TEST_METHOD(TestCursorIsOn);

    TEST_METHOD(TestAddHyperlink);
    TEST_METHOD(TestAddHyperlinkCustomId);
    TEST_METHOD(TestAddHyperlinkCustomIdDifferentUri);

    TEST_METHOD(UpdateVirtualBottomWhenCursorMovesBelowIt);
    TEST_METHOD(UpdateVirtualBottomWithSetConsoleCursorPosition);
    TEST_METHOD(UpdateVirtualBottomAfterInternalSetViewportSize);
    TEST_METHOD(UpdateVirtualBottomAfterResizeWithReflow);
    TEST_METHOD(DontShrinkVirtualBottomDuringResizeWithReflowAtTop);
    TEST_METHOD(DontChangeVirtualBottomWithOffscreenLinefeed);
    TEST_METHOD(DontChangeVirtualBottomAfterResizeWindow);
    TEST_METHOD(DontChangeVirtualBottomWithMakeCursorVisible);
    TEST_METHOD(RetainHorizontalOffsetWhenMovingToBottom);

    TEST_METHOD(TestWriteConsoleVTQuirkMode);

    TEST_METHOD(TestReflowEndOfLineColor);
    TEST_METHOD(TestReflowSmallerLongLineWithColor);
    TEST_METHOD(TestReflowBiggerLongLineWithColor);

    TEST_METHOD(TestDeferredMainBufferResize);

    TEST_METHOD(RectangularAreaOperations);
    TEST_METHOD(CopyDoubleWidthRectangularArea);

    TEST_METHOD(DelayedWrapReset);

    TEST_METHOD(EraseColorMode);
};

void ScreenBufferTests::SingleAlternateBufferCreationTest()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.LockConsole(); // Lock must be taken to manipulate buffer.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    Log::Comment(L"Testing creating one alternate buffer, then returning to the main buffer.");
    const auto psiOriginal = &gci.GetActiveOutputBuffer();
    VERIFY_IS_NULL(psiOriginal->_psiAlternateBuffer);
    VERIFY_IS_NULL(psiOriginal->_psiMainBuffer);

    auto Status = psiOriginal->UseAlternateScreenBuffer({});
    if (VERIFY_NT_SUCCESS(Status))
    {
        Log::Comment(L"First alternate buffer successfully created");
        const auto psiFirstAlternate = &gci.GetActiveOutputBuffer();
        VERIFY_ARE_NOT_EQUAL(psiOriginal, psiFirstAlternate);
        VERIFY_ARE_EQUAL(psiFirstAlternate, psiOriginal->_psiAlternateBuffer);
        VERIFY_ARE_EQUAL(psiOriginal, psiFirstAlternate->_psiMainBuffer);
        VERIFY_IS_NULL(psiOriginal->_psiMainBuffer);
        VERIFY_IS_NULL(psiFirstAlternate->_psiAlternateBuffer);

        psiFirstAlternate->UseMainScreenBuffer();
        Log::Comment(L"successfully swapped to the main buffer");
        const auto psiFinal = &gci.GetActiveOutputBuffer();
        VERIFY_ARE_NOT_EQUAL(psiFinal, psiFirstAlternate);
        VERIFY_ARE_EQUAL(psiFinal, psiOriginal);
        VERIFY_IS_NULL(psiFinal->_psiMainBuffer);
        VERIFY_IS_NULL(psiFinal->_psiAlternateBuffer);
    }
}

void ScreenBufferTests::MultipleAlternateBufferCreationTest()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.LockConsole(); // Lock must be taken to manipulate buffer.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    Log::Comment(
        L"Testing creating one alternate buffer, then creating another "
        L"alternate from that first alternate, before returning to the "
        L"main buffer.");

    const auto psiOriginal = &gci.GetActiveOutputBuffer();
    auto Status = psiOriginal->UseAlternateScreenBuffer({});
    if (VERIFY_NT_SUCCESS(Status))
    {
        Log::Comment(L"First alternate buffer successfully created");
        const auto psiFirstAlternate = &gci.GetActiveOutputBuffer();
        VERIFY_ARE_NOT_EQUAL(psiOriginal, psiFirstAlternate);
        VERIFY_ARE_EQUAL(psiFirstAlternate, psiOriginal->_psiAlternateBuffer);
        VERIFY_ARE_EQUAL(psiOriginal, psiFirstAlternate->_psiMainBuffer);
        VERIFY_IS_NULL(psiOriginal->_psiMainBuffer);
        VERIFY_IS_NULL(psiFirstAlternate->_psiAlternateBuffer);

        Status = psiFirstAlternate->UseAlternateScreenBuffer({});
        if (VERIFY_NT_SUCCESS(Status))
        {
            Log::Comment(L"Second alternate buffer successfully created");
            auto psiSecondAlternate = &gci.GetActiveOutputBuffer();
            VERIFY_ARE_NOT_EQUAL(psiOriginal, psiSecondAlternate);
            VERIFY_ARE_NOT_EQUAL(psiSecondAlternate, psiFirstAlternate);
            VERIFY_ARE_EQUAL(psiSecondAlternate, psiOriginal->_psiAlternateBuffer);
            VERIFY_ARE_EQUAL(psiOriginal, psiSecondAlternate->_psiMainBuffer);
            VERIFY_IS_NULL(psiOriginal->_psiMainBuffer);
            VERIFY_IS_NULL(psiSecondAlternate->_psiAlternateBuffer);

            psiSecondAlternate->UseMainScreenBuffer();
            Log::Comment(L"successfully swapped to the main buffer");
            const auto psiFinal = &gci.GetActiveOutputBuffer();
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
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.LockConsole(); // Lock must be taken to manipulate buffer.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    Log::Comment(
        L"Testing creating one alternate buffer, then creating another"
        L" alternate from the main, before returning to the main buffer.");
    const auto psiOriginal = &gci.GetActiveOutputBuffer();
    auto Status = psiOriginal->UseAlternateScreenBuffer({});
    if (VERIFY_NT_SUCCESS(Status))
    {
        Log::Comment(L"First alternate buffer successfully created");
        const auto psiFirstAlternate = &gci.GetActiveOutputBuffer();
        VERIFY_ARE_NOT_EQUAL(psiOriginal, psiFirstAlternate);
        VERIFY_ARE_EQUAL(psiFirstAlternate, psiOriginal->_psiAlternateBuffer);
        VERIFY_ARE_EQUAL(psiOriginal, psiFirstAlternate->_psiMainBuffer);
        VERIFY_IS_NULL(psiOriginal->_psiMainBuffer);
        VERIFY_IS_NULL(psiFirstAlternate->_psiAlternateBuffer);

        Status = psiOriginal->UseAlternateScreenBuffer({});
        if (VERIFY_NT_SUCCESS(Status))
        {
            Log::Comment(L"Second alternate buffer successfully created");
            const auto psiSecondAlternate = &gci.GetActiveOutputBuffer();
            VERIFY_ARE_NOT_EQUAL(psiOriginal, psiSecondAlternate);
            VERIFY_ARE_NOT_EQUAL(psiSecondAlternate, psiFirstAlternate);
            VERIFY_ARE_EQUAL(psiSecondAlternate, psiOriginal->_psiAlternateBuffer);
            VERIFY_ARE_EQUAL(psiOriginal, psiSecondAlternate->_psiMainBuffer);
            VERIFY_IS_NULL(psiOriginal->_psiMainBuffer);
            VERIFY_IS_NULL(psiSecondAlternate->_psiAlternateBuffer);

            psiSecondAlternate->UseMainScreenBuffer();
            Log::Comment(L"successfully swapped to the main buffer");
            const auto psiFinal = &gci.GetActiveOutputBuffer();
            VERIFY_ARE_NOT_EQUAL(psiFinal, psiFirstAlternate);
            VERIFY_ARE_NOT_EQUAL(psiFinal, psiSecondAlternate);
            VERIFY_ARE_EQUAL(psiFinal, psiOriginal);
            VERIFY_IS_NULL(psiFinal->_psiMainBuffer);
            VERIFY_IS_NULL(psiFinal->_psiAlternateBuffer);
        }
    }
}

void ScreenBufferTests::AlternateBufferCursorInheritanceTest()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.LockConsole(); // Lock must be taken to manipulate buffer.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    auto& mainBuffer = gci.GetActiveOutputBuffer();
    auto& mainCursor = mainBuffer.GetTextBuffer().GetCursor();

    Log::Comment(L"Set the cursor attributes in the main buffer.");
    auto mainCursorPos = til::point{ 3, 5 };
    auto mainCursorVisible = false;
    auto mainCursorSize = 33u;
    auto mainCursorType = CursorType::DoubleUnderscore;
    auto mainCursorBlinking = false;
    mainCursor.SetPosition(mainCursorPos);
    mainCursor.SetIsVisible(mainCursorVisible);
    mainCursor.SetStyle(mainCursorSize, mainCursorType);
    mainCursor.SetBlinkingAllowed(mainCursorBlinking);

    Log::Comment(L"Switch to the alternate buffer.");
    VERIFY_SUCCEEDED(mainBuffer.UseAlternateScreenBuffer({}));
    auto& altBuffer = gci.GetActiveOutputBuffer();
    auto& altCursor = altBuffer.GetTextBuffer().GetCursor();
    auto useMain = wil::scope_exit([&] { altBuffer.UseMainScreenBuffer(); });

    Log::Comment(L"Confirm the cursor position is inherited from the main buffer.");
    VERIFY_ARE_EQUAL(mainCursorPos, altCursor.GetPosition());
    Log::Comment(L"Confirm the cursor visibility is inherited from the main buffer.");
    VERIFY_ARE_EQUAL(mainCursorVisible, altCursor.IsVisible());
    Log::Comment(L"Confirm the cursor style is inherited from the main buffer.");
    VERIFY_ARE_EQUAL(mainCursorSize, altCursor.GetSize());
    VERIFY_ARE_EQUAL(mainCursorType, altCursor.GetType());
    VERIFY_ARE_EQUAL(mainCursorBlinking, altCursor.IsBlinkingAllowed());

    Log::Comment(L"Set the cursor attributes in the alt buffer.");
    auto altCursorPos = til::point{ 5, 3 };
    auto altCursorVisible = true;
    auto altCursorSize = 66u;
    auto altCursorType = CursorType::EmptyBox;
    auto altCursorBlinking = true;
    altCursor.SetPosition(altCursorPos);
    altCursor.SetIsVisible(altCursorVisible);
    altCursor.SetStyle(altCursorSize, altCursorType);
    altCursor.SetBlinkingAllowed(altCursorBlinking);

    Log::Comment(L"Switch back to the main buffer.");
    useMain.release();
    altBuffer.UseMainScreenBuffer();
    VERIFY_ARE_EQUAL(&mainBuffer, &gci.GetActiveOutputBuffer());

    Log::Comment(L"Confirm the cursor position is restored to what it was.");
    VERIFY_ARE_EQUAL(mainCursorPos, mainCursor.GetPosition());
    Log::Comment(L"Confirm the cursor visibility is inherited from the alt buffer.");
    VERIFY_ARE_EQUAL(altCursorVisible, mainCursor.IsVisible());
    Log::Comment(L"Confirm the cursor style is inherited from the alt buffer.");
    VERIFY_ARE_EQUAL(altCursorSize, mainCursor.GetSize());
    VERIFY_ARE_EQUAL(altCursorType, mainCursor.GetType());
    VERIFY_ARE_EQUAL(altCursorBlinking, mainCursor.IsBlinkingAllowed());
}

void ScreenBufferTests::TestReverseLineFeed()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& screenInfo = gci.GetActiveOutputBuffer();
    auto& stateMachine = screenInfo.GetStateMachine();
    auto& cursor = screenInfo._textBuffer->GetCursor();
    auto viewport = screenInfo.GetViewport();
    const auto reverseLineFeed = L"\033M";

    VERIFY_ARE_EQUAL(viewport.Top(), 0);

    ////////////////////////////////////////////////////////////////////////
    Log::Comment(L"Case 1: RI from below top of viewport");

    stateMachine.ProcessString(L"foo\nfoo");
    VERIFY_ARE_EQUAL(cursor.GetPosition().x, 3);
    VERIFY_ARE_EQUAL(cursor.GetPosition().y, 1);
    VERIFY_ARE_EQUAL(viewport.Top(), 0);

    stateMachine.ProcessString(reverseLineFeed);

    VERIFY_ARE_EQUAL(cursor.GetPosition().x, 3);
    VERIFY_ARE_EQUAL(cursor.GetPosition().y, 0);
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
    stateMachine.ProcessString(L"123456789");
    VERIFY_ARE_EQUAL(cursor.GetPosition().x, 9);
    VERIFY_ARE_EQUAL(cursor.GetPosition().y, 0);
    VERIFY_ARE_EQUAL(screenInfo.GetViewport().Top(), 0);

    stateMachine.ProcessString(reverseLineFeed);

    VERIFY_ARE_EQUAL(cursor.GetPosition().x, 9);
    VERIFY_ARE_EQUAL(cursor.GetPosition().y, 0);
    viewport = screenInfo.GetViewport();
    VERIFY_ARE_EQUAL(viewport.Top(), 0);
    Log::Comment(NoThrowString().Format(
        L"viewport={L:%d,T:%d,R:%d,B:%d}",
        viewport.Left(),
        viewport.Top(),
        viewport.RightInclusive(),
        viewport.BottomInclusive()));
    auto c = screenInfo._textBuffer->GetLastNonSpaceCharacter();
    VERIFY_ARE_EQUAL(c.y, 2); // This is the coordinates of the second "foo" from before.

    ////////////////////////////////////////////////////////////////////////
    Log::Comment(L"Case 3: RI from top of viewport, when viewport is below top of buffer");

    cursor.SetPosition({ 0, 5 });
    VERIFY_SUCCEEDED(screenInfo.SetViewportOrigin(true, { 0, 5 }, true));
    stateMachine.ProcessString(L"ABCDEFGH");
    VERIFY_ARE_EQUAL(cursor.GetPosition().x, 8);
    VERIFY_ARE_EQUAL(cursor.GetPosition().y, 5);
    VERIFY_ARE_EQUAL(screenInfo.GetViewport().Top(), 5);

    stateMachine.ProcessString(reverseLineFeed);

    VERIFY_ARE_EQUAL(cursor.GetPosition().x, 8);
    VERIFY_ARE_EQUAL(cursor.GetPosition().y, 5);
    viewport = screenInfo.GetViewport();
    VERIFY_ARE_EQUAL(viewport.Top(), 5);
    Log::Comment(NoThrowString().Format(
        L"viewport={L:%d,T:%d,R:%d,B:%d}",
        viewport.Left(),
        viewport.Top(),
        viewport.RightInclusive(),
        viewport.BottomInclusive()));
    c = screenInfo._textBuffer->GetLastNonSpaceCharacter();
    VERIFY_ARE_EQUAL(c.y, 6);
}

void _SetTabStops(SCREEN_INFORMATION& screenInfo, std::list<til::CoordType> columns, bool replace)
{
    auto& stateMachine = screenInfo.GetStateMachine();
    auto& cursor = screenInfo.GetTextBuffer().GetCursor();
    const auto clearTabStops = L"\033[3g";
    const auto addTabStop = L"\033H";

    if (replace)
    {
        stateMachine.ProcessString(clearTabStops);
    }

    for (auto column : columns)
    {
        cursor.SetXPosition(column);
        stateMachine.ProcessString(addTabStop);
    }
}

std::list<til::CoordType> _GetTabStops(SCREEN_INFORMATION& screenInfo)
{
    std::list<til::CoordType> columns;

    const auto lastColumn = screenInfo.GetBufferSize().RightInclusive();
    auto& stateMachine = screenInfo.GetStateMachine();
    auto& cursor = screenInfo.GetTextBuffer().GetCursor();

    cursor.SetPosition({ 0, 0 });
    for (;;)
    {
        stateMachine.ProcessCharacter(L'\t');
        auto column = cursor.GetPosition().x;
        if (column >= lastColumn)
        {
            break;
        }
        columns.push_back(column);
    }

    return columns;
}

void ScreenBufferTests::TestResetClearTabStops()
{
    // Reset the screen buffer to test the defaults.
    m_state->CleanupNewTextBufferInfo();
    m_state->CleanupGlobalScreenBuffer();
    m_state->CleanupGlobalRenderer();
    m_state->PrepareGlobalRenderer();
    m_state->PrepareGlobalScreenBuffer();
    m_state->PrepareNewTextBufferInfo();

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& screenInfo = gci.GetActiveOutputBuffer();
    auto& stateMachine = screenInfo.GetStateMachine();

    const auto clearTabStops = L"\033[3g";
    const auto resetToInitialState = L"\033c";

    Log::Comment(L"Default tabs every 8 columns.");
    std::list<til::CoordType> expectedStops{ 8, 16, 24, 32, 40, 48, 56, 64, 72 };
    VERIFY_ARE_EQUAL(expectedStops, _GetTabStops(screenInfo));

    Log::Comment(L"Clear all tabs.");
    stateMachine.ProcessString(clearTabStops);
    expectedStops = {};
    VERIFY_ARE_EQUAL(expectedStops, _GetTabStops(screenInfo));

    Log::Comment(L"RIS resets tabs to defaults.");
    stateMachine.ProcessString(resetToInitialState);
    expectedStops = { 8, 16, 24, 32, 40, 48, 56, 64, 72 };
    VERIFY_ARE_EQUAL(expectedStops, _GetTabStops(screenInfo));
}

void ScreenBufferTests::TestAddTabStop()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& screenInfo = gci.GetActiveOutputBuffer();
    auto& stateMachine = screenInfo.GetStateMachine();
    auto& cursor = screenInfo.GetTextBuffer().GetCursor();

    const auto clearTabStops = L"\033[3g";
    const auto addTabStop = L"\033H";

    Log::Comment(L"Clear all tabs.");
    stateMachine.ProcessString(clearTabStops);
    std::list<til::CoordType> expectedStops{};
    VERIFY_ARE_EQUAL(expectedStops, _GetTabStops(screenInfo));

    Log::Comment(L"Add tab to empty list.");
    cursor.SetXPosition(12);
    stateMachine.ProcessString(addTabStop);
    expectedStops.push_back(12);
    VERIFY_ARE_EQUAL(expectedStops, _GetTabStops(screenInfo));

    Log::Comment(L"Add tab to head of existing list.");
    cursor.SetXPosition(4);
    stateMachine.ProcessString(addTabStop);
    expectedStops.push_front(4);
    VERIFY_ARE_EQUAL(expectedStops, _GetTabStops(screenInfo));

    Log::Comment(L"Add tab to tail of existing list.");
    cursor.SetXPosition(30);
    stateMachine.ProcessString(addTabStop);
    expectedStops.push_back(30);
    VERIFY_ARE_EQUAL(expectedStops, _GetTabStops(screenInfo));

    Log::Comment(L"Add tab to middle of existing list.");
    cursor.SetXPosition(24);
    stateMachine.ProcessString(addTabStop);
    expectedStops.push_back(24);
    expectedStops.sort();
    VERIFY_ARE_EQUAL(expectedStops, _GetTabStops(screenInfo));

    Log::Comment(L"Add tab that duplicates an item in the existing list.");
    cursor.SetXPosition(24);
    stateMachine.ProcessString(addTabStop);
    VERIFY_ARE_EQUAL(expectedStops, _GetTabStops(screenInfo));
}

void ScreenBufferTests::TestClearTabStop()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& screenInfo = gci.GetActiveOutputBuffer();
    auto& stateMachine = screenInfo.GetStateMachine();
    auto& cursor = screenInfo.GetTextBuffer().GetCursor();

    const auto clearTabStops = L"\033[3g";
    const auto clearTabStop = L"\033[0g";
    const auto addTabStop = L"\033H";

    Log::Comment(L"Start with all tabs cleared.");
    {
        stateMachine.ProcessString(clearTabStops);

        VERIFY_IS_TRUE(_GetTabStops(screenInfo).empty());
    }

    Log::Comment(L"Try to clear nonexistent list.");
    {
        cursor.SetXPosition(0);
        stateMachine.ProcessString(clearTabStop);

        VERIFY_IS_TRUE(_GetTabStops(screenInfo).empty(), L"List should remain empty");
    }

    Log::Comment(L"Allocate 1 list item and clear it.");
    {
        cursor.SetXPosition(0);
        stateMachine.ProcessString(addTabStop);
        stateMachine.ProcessString(clearTabStop);

        VERIFY_IS_TRUE(_GetTabStops(screenInfo).empty());
    }

    Log::Comment(L"Allocate 1 list item and clear nonexistent.");
    {
        cursor.SetXPosition(1);
        stateMachine.ProcessString(addTabStop);

        Log::Comment(L"Free greater");
        cursor.SetXPosition(2);
        stateMachine.ProcessString(clearTabStop);
        VERIFY_IS_FALSE(_GetTabStops(screenInfo).empty());

        Log::Comment(L"Free less than");
        cursor.SetXPosition(0);
        stateMachine.ProcessString(clearTabStop);
        VERIFY_IS_FALSE(_GetTabStops(screenInfo).empty());

        // clear all tab stops
        stateMachine.ProcessString(clearTabStops);
    }

    Log::Comment(L"Allocate many (5) list items and clear head.");
    {
        std::list<til::CoordType> inputData = { 3, 5, 6, 10, 15, 17 };
        _SetTabStops(screenInfo, inputData, false);
        cursor.SetXPosition(inputData.front());
        stateMachine.ProcessString(clearTabStop);

        inputData.pop_front();
        VERIFY_ARE_EQUAL(inputData, _GetTabStops(screenInfo));

        // clear all tab stops
        stateMachine.ProcessString(clearTabStops);
    }

    Log::Comment(L"Allocate many (5) list items and clear middle.");
    {
        std::list<til::CoordType> inputData = { 3, 5, 6, 10, 15, 17 };
        _SetTabStops(screenInfo, inputData, false);
        cursor.SetXPosition(*std::next(inputData.begin()));
        stateMachine.ProcessString(clearTabStop);

        inputData.erase(std::next(inputData.begin()));
        VERIFY_ARE_EQUAL(inputData, _GetTabStops(screenInfo));

        // clear all tab stops
        stateMachine.ProcessString(clearTabStops);
    }

    Log::Comment(L"Allocate many (5) list items and clear tail.");
    {
        std::list<til::CoordType> inputData = { 3, 5, 6, 10, 15, 17 };
        _SetTabStops(screenInfo, inputData, false);
        cursor.SetXPosition(inputData.back());
        stateMachine.ProcessString(clearTabStop);

        inputData.pop_back();
        VERIFY_ARE_EQUAL(inputData, _GetTabStops(screenInfo));

        // clear all tab stops
        stateMachine.ProcessString(clearTabStops);
    }

    Log::Comment(L"Allocate many (5) list items and clear nonexistent item.");
    {
        std::list<til::CoordType> inputData = { 3, 5, 6, 10, 15, 17 };
        _SetTabStops(screenInfo, inputData, false);
        cursor.SetXPosition(0);
        stateMachine.ProcessString(clearTabStop);

        VERIFY_ARE_EQUAL(inputData, _GetTabStops(screenInfo));

        // clear all tab stops
        stateMachine.ProcessString(clearTabStops);
    }
}

void ScreenBufferTests::TestGetForwardTab()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si.GetTextBuffer().GetCursor();

    const auto nextForwardTab = L"\033[I";

    std::list<til::CoordType> inputData = { 3, 5, 6, 10, 15, 17 };
    _SetTabStops(si, inputData, true);

    const auto coordScreenBufferSize = si.GetBufferSize().Dimensions();

    Log::Comment(L"Find next tab from before front.");
    {
        cursor.SetXPosition(0);

        auto coordCursorExpected = cursor.GetPosition();
        coordCursorExpected.x = inputData.front();

        stateMachine.ProcessString(nextForwardTab);
        const auto coordCursorResult = cursor.GetPosition();
        VERIFY_ARE_EQUAL(coordCursorExpected,
                         coordCursorResult,
                         L"Cursor advanced to first tab stop from sample list.");
    }

    Log::Comment(L"Find next tab from in the middle.");
    {
        cursor.SetXPosition(6);

        auto coordCursorExpected = cursor.GetPosition();
        coordCursorExpected.x = *std::next(inputData.begin(), 3);

        stateMachine.ProcessString(nextForwardTab);
        const auto coordCursorResult = cursor.GetPosition();
        VERIFY_ARE_EQUAL(coordCursorExpected,
                         coordCursorResult,
                         L"Cursor advanced to middle tab stop from sample list.");
    }

    Log::Comment(L"Find next tab from end.");
    {
        cursor.SetXPosition(30);

        auto coordCursorExpected = cursor.GetPosition();
        coordCursorExpected.x = coordScreenBufferSize.width - 1;

        stateMachine.ProcessString(nextForwardTab);
        const auto coordCursorResult = cursor.GetPosition();
        VERIFY_ARE_EQUAL(coordCursorExpected,
                         coordCursorResult,
                         L"Cursor advanced to end of screen buffer.");
    }

    Log::Comment(L"Find next tab from rightmost column.");
    {
        cursor.SetXPosition(coordScreenBufferSize.width - 1);

        auto coordCursorExpected = cursor.GetPosition();

        stateMachine.ProcessString(nextForwardTab);
        const auto coordCursorResult = cursor.GetPosition();
        VERIFY_ARE_EQUAL(coordCursorExpected,
                         coordCursorResult,
                         L"Cursor remains in rightmost column.");
    }
}

void ScreenBufferTests::TestGetReverseTab()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si.GetTextBuffer().GetCursor();

    const auto nextReverseTab = L"\033[Z";

    std::list<til::CoordType> inputData = { 3, 5, 6, 10, 15, 17 };
    _SetTabStops(si, inputData, true);

    Log::Comment(L"Find previous tab from before front.");
    {
        cursor.SetXPosition(1);

        auto coordCursorExpected = cursor.GetPosition();
        coordCursorExpected.x = 0;

        stateMachine.ProcessString(nextReverseTab);
        const auto coordCursorResult = cursor.GetPosition();
        VERIFY_ARE_EQUAL(coordCursorExpected,
                         coordCursorResult,
                         L"Cursor adjusted to beginning of the buffer when it started before sample list.");
    }

    Log::Comment(L"Find previous tab from in the middle.");
    {
        cursor.SetXPosition(6);

        auto coordCursorExpected = cursor.GetPosition();
        coordCursorExpected.x = *std::next(inputData.begin());

        stateMachine.ProcessString(nextReverseTab);
        const auto coordCursorResult = cursor.GetPosition();
        VERIFY_ARE_EQUAL(coordCursorExpected,
                         coordCursorResult,
                         L"Cursor adjusted back one tab spot from middle of sample list.");
    }

    Log::Comment(L"Find next tab from end.");
    {
        cursor.SetXPosition(30);

        auto coordCursorExpected = cursor.GetPosition();
        coordCursorExpected.x = inputData.back();

        stateMachine.ProcessString(nextReverseTab);
        const auto coordCursorResult = cursor.GetPosition();
        VERIFY_ARE_EQUAL(coordCursorExpected,
                         coordCursorResult,
                         L"Cursor adjusted to last item in the sample list from position beyond end.");
    }
}

void ScreenBufferTests::TestAltBufferTabStops()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.LockConsole(); // Lock must be taken to swap buffers.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    auto& mainBuffer = gci.GetActiveOutputBuffer();
    // Make sure we're in VT mode
    WI_SetFlag(mainBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    VERIFY_IS_TRUE(WI_IsFlagSet(mainBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING));

    Log::Comment(L"Add an initial set of tab in the main buffer.");
    std::list<til::CoordType> expectedStops = { 3, 5, 6, 10, 15, 17 };
    _SetTabStops(mainBuffer, expectedStops, true);
    VERIFY_ARE_EQUAL(expectedStops, _GetTabStops(mainBuffer));

    VERIFY_SUCCEEDED(mainBuffer.UseAlternateScreenBuffer({}));
    auto& altBuffer = gci.GetActiveOutputBuffer();
    auto useMain = wil::scope_exit([&] { altBuffer.UseMainScreenBuffer(); });

    Log::Comment(NoThrowString().Format(
        L"Manually enable VT mode for the alt buffer - "
        L"usually the ctor will pick this up from GCI, but not in the tests."));
    WI_SetFlag(altBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    VERIFY_IS_TRUE(WI_IsFlagSet(altBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING));

    Log::Comment(L"Make sure the tabs are still set in the alt buffer.");
    VERIFY_ARE_EQUAL(expectedStops, _GetTabStops(altBuffer));

    Log::Comment(L"Add a new set of tabs in the alt buffer.");
    expectedStops = { 4, 8, 12, 16 };
    _SetTabStops(altBuffer, expectedStops, true);
    VERIFY_ARE_EQUAL(expectedStops, _GetTabStops(altBuffer));

    Log::Comment(L"Make sure the tabs are still set in the main buffer.");
    useMain.release();
    altBuffer.UseMainScreenBuffer();
    VERIFY_ARE_EQUAL(expectedStops, _GetTabStops(mainBuffer));
}

void ScreenBufferTests::EraseAllTests()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si._textBuffer->GetCursor();

    const auto eraseAll = L"\033[2J";

    VERIFY_ARE_EQUAL(si.GetViewport().Top(), 0);

    ////////////////////////////////////////////////////////////////////////
    Log::Comment(L"Case 1: Erase a single line of text in the buffer\n");

    stateMachine.ProcessString(L"foo");
    til::point originalRelativePosition = { 3, 0 };
    VERIFY_ARE_EQUAL(si.GetViewport().Top(), 0);
    VERIFY_ARE_EQUAL(cursor.GetPosition(), originalRelativePosition);

    stateMachine.ProcessString(eraseAll);

    auto viewport = si._viewport;
    VERIFY_ARE_EQUAL(viewport.Top(), 1);
    auto newRelativePos = originalRelativePosition;
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

    stateMachine.ProcessString(L"bar\nbar\nbar");
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

    stateMachine.ProcessString(eraseAll);
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
    stateMachine.ProcessString(L"bar\nbar\nbar");
    viewport = si._viewport;
    VERIFY_ARE_EQUAL(cursor.GetPosition().x, 3);
    VERIFY_ARE_EQUAL(cursor.GetPosition().y, 277);
    originalRelativePosition = cursor.GetPosition();
    viewport.ConvertToOrigin(&originalRelativePosition);

    Log::Comment(NoThrowString().Format(
        L"viewport={L:%d,T:%d,R:%d,B:%d}",
        viewport.Left(),
        viewport.Top(),
        viewport.RightInclusive(),
        viewport.BottomInclusive()));
    stateMachine.ProcessString(eraseAll);

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

void ScreenBufferTests::InactiveControlCharactersTest()
{
    // These are the control characters that don't write anything to the
    // output buffer, and are expected not to move the cursor position.

    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:ordinal", L"{0, 1, 2, 3, 4, 5, 6, 7, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 28, 29, 30, 31}")
    END_TEST_METHOD_PROPERTIES()

    unsigned ordinal;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"ordinal", ordinal));
    const auto ch = static_cast<wchar_t>(ordinal);

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si._textBuffer->GetCursor();

    VERIFY_ARE_EQUAL(0, cursor.GetPosition().x);
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().y);

    Log::Comment(L"Writing a single control character");
    const auto singleChar = std::wstring(1, ch);
    stateMachine.ProcessString(singleChar);
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().x);
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().y);

    Log::Comment(L"Writing many control characters");
    const auto manyChars = std::wstring(8, ch);
    stateMachine.ProcessString(manyChars);
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().x);
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().y);

    Log::Comment(L"Testing a single control character followed by real text");
    stateMachine.ProcessString(singleChar + L"foo");
    VERIFY_ARE_EQUAL(3, cursor.GetPosition().x);
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().y);

    stateMachine.ProcessString(L"\n");
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().x);
    VERIFY_ARE_EQUAL(1, cursor.GetPosition().y);

    Log::Comment(L"Writing controls in between other strings");
    stateMachine.ProcessString(singleChar + L"foo" + singleChar + L"bar" + singleChar);
    VERIFY_ARE_EQUAL(6, cursor.GetPosition().x);
    VERIFY_ARE_EQUAL(1, cursor.GetPosition().y);
}

void ScreenBufferTests::VtResize()
{
    // Run this test in isolation - for one reason or another, this breaks other tests.
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
    END_TEST_METHOD_PROPERTIES()

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = tbi.GetCursor();
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

    stateMachine.ProcessString(L"\x1b[8;30;80t");

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

    stateMachine.ProcessString(L"\x1b[8;40;80t");

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

    stateMachine.ProcessString(L"\x1b[8;40;90t");

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

    stateMachine.ProcessString(L"\x1b[8;12;12t");

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

    stateMachine.ProcessString(L"\x1b[8;0;0t");

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

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = tbi.GetCursor();
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

    stateMachine.ProcessString(ss.str());

    auto newViewHeight = si.GetViewport().Height();
    auto newViewWidth = si.GetViewport().Width();

    VERIFY_ARE_EQUAL(expectedViewWidth, newViewWidth);
    VERIFY_ARE_EQUAL(expectedViewHeight, newViewHeight);
}

til::rect _GetRelativeScrollMargins()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    auto& stateMachine = si.GetStateMachine();
    const auto viewport = si.GetViewport();
    auto& cursor = si.GetTextBuffer().GetCursor();
    const auto savePos = cursor.GetPosition();

    // We can't access the AdaptDispatch internals where the margins are stored,
    // but we calculate their boundaries by using VT sequences to move down and
    // up as far as possible, and read the cursor positions at the two limits.
    stateMachine.ProcessString(L"\033[H\033[9999B");
    const auto bottom = cursor.GetPosition().y - viewport.Top();
    stateMachine.ProcessString(L"\033[9999A");
    const auto top = cursor.GetPosition().y - viewport.Top();

    cursor.SetPosition(savePos);
    const auto noMargins = (top == 0 && bottom == viewport.Height() - 1);
    return noMargins ? til::rect{} : til::rect{ 0, top, 0, bottom };
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
    auto areMarginsSet = [&]() {
        const auto margins = _GetRelativeScrollMargins();
        return margins.bottom > margins.top;
    };

    stateMachine.ProcessString(setInitialMargins);
    stateMachine.ProcessString(setInitialCursor);
    auto initialMargins = _GetRelativeScrollMargins();
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

    VERIFY_IS_TRUE(areMarginsSet());
    VERIFY_ARE_EQUAL(initialMargins, _GetRelativeScrollMargins());
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

    VERIFY_IS_FALSE(areMarginsSet());
    VERIFY_ARE_EQUAL(til::point(0, 0), getRelativeCursorPosition());
    VERIFY_ARE_EQUAL(initialSbHeight, newSbHeight);
    VERIFY_ARE_EQUAL(initialViewHeight, newViewHeight);
    VERIFY_ARE_EQUAL(132, newSbWidth);
    VERIFY_ARE_EQUAL(132, newViewWidth);

    stateMachine.ProcessString(setInitialMargins);
    stateMachine.ProcessString(setInitialCursor);
    initialMargins = _GetRelativeScrollMargins();
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

    VERIFY_IS_TRUE(areMarginsSet());
    VERIFY_ARE_EQUAL(initialMargins, _GetRelativeScrollMargins());
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

    VERIFY_IS_FALSE(areMarginsSet());
    VERIFY_ARE_EQUAL(til::point(0, 0), getRelativeCursorPosition());
    VERIFY_ARE_EQUAL(initialSbHeight, newSbHeight);
    VERIFY_ARE_EQUAL(initialViewHeight, newViewHeight);
    VERIFY_ARE_EQUAL(80, newSbWidth);
    VERIFY_ARE_EQUAL(80, newViewWidth);
}

void ScreenBufferTests::VtResizePreservingAttributes()
{
    // Run this test in isolation - for one reason or another, this breaks other tests.
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
        TEST_METHOD_PROPERTY(L"Data:resizeType", L"{0, 1}")
    END_TEST_METHOD_PROPERTIES()

    unsigned resizeType;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"resizeType", resizeType));

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    auto& stateMachine = si.GetStateMachine();
    WI_SetFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    // Set the attributes to something not supported by the legacy console.
    auto testAttr = TextAttribute{ RGB(12, 34, 56), RGB(78, 90, 12) };
    testAttr.SetCrossedOut(true);
    testAttr.SetDoublyUnderlined(true);
    testAttr.SetItalic(true);
    si.GetTextBuffer().SetCurrentAttributes(testAttr);

    switch (resizeType)
    {
    case 0:
        Log::Comment(L"Resize the screen with CSI 8 t");
        stateMachine.ProcessString(L"\x1b[8;24;132t"); // Set width to 132
        VERIFY_ARE_EQUAL(132, si.GetTextBuffer().GetSize().Width());
        stateMachine.ProcessString(L"\x1b[8;24;80t"); // Set width to 80
        VERIFY_ARE_EQUAL(80, si.GetTextBuffer().GetSize().Width());
        break;
    case 1:
        Log::Comment(L"Resize the screen with DECCOLM");
        stateMachine.ProcessString(L"\x1b[?40h"); // Allow DECCOLM
        stateMachine.ProcessString(L"\x1b[?3h"); // Set width to 132
        VERIFY_ARE_EQUAL(132, si.GetTextBuffer().GetSize().Width());
        stateMachine.ProcessString(L"\x1b[?3l"); // Set width to 80
        VERIFY_ARE_EQUAL(80, si.GetTextBuffer().GetSize().Width());
        break;
    }

    Log::Comment(L"Attributes should be preserved after resize");
    VERIFY_ARE_EQUAL(testAttr, si.GetTextBuffer().GetCurrentAttributes());
}

void ScreenBufferTests::VtSoftResetCursorPosition()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();
    const auto& cursor = tbi.GetCursor();

    Log::Comment(NoThrowString().Format(L"Make sure the viewport is at 0,0"));
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, til::point(0, 0), true));

    Log::Comment(NoThrowString().Format(
        L"Move the cursor to 2,2, then execute a soft reset.\n"
        L"The cursor should not move."));

    stateMachine.ProcessString(L"\x1b[2;2H");
    VERIFY_ARE_EQUAL(til::point(1, 1), cursor.GetPosition());

    stateMachine.ProcessString(L"\x1b[!p");
    VERIFY_ARE_EQUAL(til::point(1, 1), cursor.GetPosition());

    Log::Comment(NoThrowString().Format(
        L"Set some margins. The cursor should move home."));

    stateMachine.ProcessString(L"\x1b[2;10r");
    VERIFY_ARE_EQUAL(til::point(0, 0), cursor.GetPosition());

    Log::Comment(NoThrowString().Format(
        L"Move the cursor to 2,2, then execute a soft reset.\n"
        L"The cursor should not move, even though there are margins."));
    stateMachine.ProcessString(L"\x1b[2;2H");
    VERIFY_ARE_EQUAL(til::point(1, 1), cursor.GetPosition());
    stateMachine.ProcessString(L"\x1b[!p");
    VERIFY_ARE_EQUAL(til::point(1, 1), cursor.GetPosition());

    Log::Comment(
        L"Set the origin mode, some margins, and move the cursor to 2,2.\n"
        L"The position should be relative to the top-left of the margin area.");
    stateMachine.ProcessString(L"\x1b[?6h");
    stateMachine.ProcessString(L"\x1b[5;10r");
    stateMachine.ProcessString(L"\x1b[2;2H");
    VERIFY_ARE_EQUAL(til::point(1, 5), cursor.GetPosition());

    Log::Comment(
        L"Execute a soft reset, reapply the margins, and move the cursor to 2,2.\n"
        L"The position should now be relative to the top-left of the screen.");
    stateMachine.ProcessString(L"\x1b[!p");
    stateMachine.ProcessString(L"\x1b[5;10r");
    stateMachine.ProcessString(L"\x1b[2;2H");
    VERIFY_ARE_EQUAL(til::point(1, 1), cursor.GetPosition());
}

void ScreenBufferTests::VtScrollMarginsNewlineColor()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si.GetTextBuffer().GetCursor();
    auto& renderSettings = gci.GetRenderSettings();

    Log::Comment(NoThrowString().Format(L"Make sure the viewport is at 0,0"));
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, til::point(0, 0), true));
    cursor.SetPosition(til::point(0, 0));

    const auto yellow = RGB(255, 255, 0);
    const auto magenta = RGB(255, 0, 255);
    gci.SetColorTableEntry(TextColor::DEFAULT_FOREGROUND, yellow);
    gci.SetColorTableEntry(TextColor::DEFAULT_BACKGROUND, magenta);
    gci.CalculateDefaultColorIndices();
    const TextAttribute defaultAttrs = {};
    si.SetAttributes(defaultAttrs);

    Log::Comment(NoThrowString().Format(L"Begin by clearing the screen."));

    stateMachine.ProcessString(L"\x1b[2J");
    stateMachine.ProcessString(L"\x1b[m");

    Log::Comment(NoThrowString().Format(
        L"Set the margins to 2, 5, then emit 10 'X\\n' strings. "
        L"Each time, check that rows 0-10 have default attributes in their entire row."));
    stateMachine.ProcessString(L"\x1b[2;5r");
    // Make sure we clear the margins to not screw up another test.
    auto clearMargins = wil::scope_exit([&] { stateMachine.ProcessString(L"\x1b[r"); });

    for (auto iteration = 0; iteration < 10; iteration++)
    {
        Log::Comment(NoThrowString().Format(
            L"Iteration:%d", iteration));
        stateMachine.ProcessString(L"X");
        stateMachine.ProcessString(L"\n");

        const auto cursorPos = cursor.GetPosition();

        Log::Comment(NoThrowString().Format(
            L"Cursor=%s",
            VerifyOutputTraits<til::point>::ToString(cursorPos).GetBuffer()));
        const auto viewport = si.GetViewport();
        Log::Comment(NoThrowString().Format(
            L"Viewport=%s",
            VerifyOutputTraits<til::inclusive_rect>::ToString(viewport.ToInclusive()).GetBuffer()));
        const auto viewTop = viewport.Top();
        for (int y = viewTop; y < viewTop + 10; y++)
        {
            SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);
            const auto& row = tbi.GetRowByOffset(y);
            const std::vector<TextAttribute> attrs{ row.AttrBegin(), row.AttrEnd() };
            for (auto x = 0; x < viewport.RightInclusive(); x++)
            {
                const auto& attr = attrs[x];
                VERIFY_ARE_EQUAL(false, attr.IsLegacy());
                VERIFY_ARE_EQUAL(defaultAttrs, attr);
                VERIFY_ARE_EQUAL(std::make_pair(yellow, magenta), renderSettings.GetAttributeColors(attr));
            }
        }
    }
}

void ScreenBufferTests::VtNewlinePastViewport()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si.GetTextBuffer().GetCursor();

    // Make sure we're in VT mode
    WI_SetFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    VERIFY_IS_TRUE(WI_IsFlagSet(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING));

    Log::Comment(NoThrowString().Format(L"Make sure the viewport is at 0,0"));
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, til::point(0, 0), true));
    cursor.SetPosition(til::point(0, 0));

    stateMachine.ProcessString(L"\x1b[m");
    stateMachine.ProcessString(L"\x1b[2J");

    const TextAttribute defaultAttrs{};

    Log::Comment(NoThrowString().Format(
        L"Move the cursor to the bottom of the viewport"));

    const auto initialViewport = si.GetViewport();
    Log::Comment(NoThrowString().Format(
        L"initialViewport=%s",
        VerifyOutputTraits<til::inclusive_rect>::ToString(initialViewport.ToInclusive()).GetBuffer()));

    cursor.SetPosition({ 0, initialViewport.BottomInclusive() });

    // Set the attributes that will be used to initialize new rows.
    auto fillAttr = TextAttribute{ RGB(12, 34, 56), RGB(78, 90, 12) };
    fillAttr.SetCrossedOut(true);
    fillAttr.SetReverseVideo(true);
    fillAttr.SetUnderlined(true);
    si.SetAttributes(fillAttr);
    // But note that the meta attributes are expected to be cleared.
    auto expectedFillAttr = fillAttr;
    expectedFillAttr.SetStandardErase();

    stateMachine.ProcessString(L"\n");

    const auto viewport = si.GetViewport();
    Log::Comment(NoThrowString().Format(
        L"viewport=%s",
        VerifyOutputTraits<til::inclusive_rect>::ToString(viewport.ToInclusive()).GetBuffer()));

    VERIFY_ARE_EQUAL(viewport.BottomInclusive(), cursor.GetPosition().y);
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().x);

    for (int y = viewport.Top(); y < viewport.BottomInclusive(); y++)
    {
        SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);
        const auto& row = tbi.GetRowByOffset(y);
        const std::vector<TextAttribute> attrs{ row.AttrBegin(), row.AttrEnd() };
        for (auto x = 0; x < viewport.RightInclusive(); x++)
        {
            const auto& attr = attrs[x];
            VERIFY_ARE_EQUAL(defaultAttrs, attr);
        }
    }

    const auto& row = tbi.GetRowByOffset(viewport.BottomInclusive());
    const std::vector<TextAttribute> attrs{ row.AttrBegin(), row.AttrEnd() };
    for (auto x = 0; x < viewport.RightInclusive(); x++)
    {
        const auto& attr = attrs[x];
        VERIFY_ARE_EQUAL(expectedFillAttr, attr);
    }
}

void ScreenBufferTests::VtNewlinePastEndOfBuffer()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si.GetTextBuffer().GetCursor();

    // Make sure we're in VT mode
    WI_SetFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    VERIFY_IS_TRUE(WI_IsFlagSet(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING));

    Log::Comment(NoThrowString().Format(L"Make sure the viewport is at 0,0"));
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, til::point(0, 0), true));
    cursor.SetPosition(til::point(0, 0));

    stateMachine.ProcessString(L"\x1b[m");
    stateMachine.ProcessString(L"\x1b[2J");

    const TextAttribute defaultAttrs{};

    Log::Comment(L"Move the cursor to the bottom of the buffer");
    for (auto i = 0; i < si.GetBufferSize().Height(); i++)
    {
        stateMachine.ProcessString(L"\n");
    }

    const auto initialViewport = si.GetViewport();
    Log::Comment(NoThrowString().Format(
        L"initialViewport=%s",
        VerifyOutputTraits<til::inclusive_rect>::ToString(initialViewport.ToInclusive()).GetBuffer()));

    cursor.SetPosition({ 0, initialViewport.BottomInclusive() });

    // Set the attributes that will be used to initialize new rows.
    auto fillAttr = TextAttribute{ RGB(12, 34, 56), RGB(78, 90, 12) };
    fillAttr.SetCrossedOut(true);
    fillAttr.SetReverseVideo(true);
    fillAttr.SetUnderlined(true);
    si.SetAttributes(fillAttr);
    // But note that the meta attributes are expected to be cleared.
    auto expectedFillAttr = fillAttr;
    expectedFillAttr.SetStandardErase();

    stateMachine.ProcessString(L"\n");

    const auto viewport = si.GetViewport();
    Log::Comment(NoThrowString().Format(
        L"viewport=%s",
        VerifyOutputTraits<til::inclusive_rect>::ToString(viewport.ToInclusive()).GetBuffer()));

    VERIFY_ARE_EQUAL(viewport.BottomInclusive(), cursor.GetPosition().y);
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().x);

    for (int y = viewport.Top(); y < viewport.BottomInclusive(); y++)
    {
        SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);
        const auto& row = tbi.GetRowByOffset(y);
        const std::vector<TextAttribute> attrs{ row.AttrBegin(), row.AttrEnd() };
        for (auto x = 0; x < viewport.RightInclusive(); x++)
        {
            const auto& attr = attrs[x];
            VERIFY_ARE_EQUAL(defaultAttrs, attr);
        }
    }

    const auto& row = tbi.GetRowByOffset(viewport.BottomInclusive());
    const std::vector<TextAttribute> attrs{ row.AttrBegin(), row.AttrEnd() };
    for (auto x = 0; x < viewport.RightInclusive(); x++)
    {
        const auto& attr = attrs[x];
        VERIFY_ARE_EQUAL(expectedFillAttr, attr);
    }
}

void ScreenBufferTests::VtNewlineOutsideMargins()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si.GetTextBuffer().GetCursor();

    const auto viewportTop = si.GetViewport().Top();
    const auto viewportBottom = si.GetViewport().BottomInclusive();
    // Make sure the bottom margin will fit inside the viewport.
    VERIFY_IS_TRUE(si.GetViewport().Height() > 5);

    Log::Comment(L"LF at bottom of viewport scrolls the viewport");
    cursor.SetPosition({ 0, viewportBottom });
    stateMachine.ProcessString(L"\n");
    VERIFY_ARE_EQUAL(til::point(0, viewportBottom + 1), cursor.GetPosition());
    VERIFY_ARE_EQUAL(til::point(0, viewportTop + 1), si.GetViewport().Origin());

    Log::Comment(L"Reset viewport and apply DECSTBM margins");
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, { 0, viewportTop }, true));
    si.UpdateBottom();
    stateMachine.ProcessString(L"\x1b[1;5r");
    // Make sure we clear the margins on exit so they can't break other tests.
    auto clearMargins = wil::scope_exit([&] { stateMachine.ProcessString(L"\x1b[r"); });

    Log::Comment(L"LF no longer scrolls the viewport when below bottom margin");
    cursor.SetPosition({ 0, viewportBottom });
    stateMachine.ProcessString(L"\n");
    VERIFY_ARE_EQUAL(til::point(0, viewportBottom), cursor.GetPosition());
    VERIFY_ARE_EQUAL(til::point(0, viewportTop), si.GetViewport().Origin());
}

void ScreenBufferTests::VtSetColorTable()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    auto& stateMachine = si.GetStateMachine();

    // Start with a known value
    gci.SetColorTableEntry(0, RGB(0, 0, 0));

    Log::Comment(NoThrowString().Format(
        L"Process some valid sequences for setting the table"));

    stateMachine.ProcessString(L"\x1b]4;0;rgb:1/1/1\x7");
    VERIFY_ARE_EQUAL(RGB(0x11, 0x11, 0x11), gci.GetColorTableEntry(0));

    stateMachine.ProcessString(L"\x1b]4;1;rgb:1/23/1\x7");
    VERIFY_ARE_EQUAL(RGB(0x11, 0x23, 0x11), gci.GetColorTableEntry(1));

    stateMachine.ProcessString(L"\x1b]4;2;rgb:1/23/12\x7");
    VERIFY_ARE_EQUAL(RGB(0x11, 0x23, 0x12), gci.GetColorTableEntry(2));

    stateMachine.ProcessString(L"\x1b]4;3;rgb:12/23/12\x7");
    VERIFY_ARE_EQUAL(RGB(0x12, 0x23, 0x12), gci.GetColorTableEntry(3));

    stateMachine.ProcessString(L"\x1b]4;4;rgb:ff/a1/1b\x7");
    VERIFY_ARE_EQUAL(RGB(0xff, 0xa1, 0x1b), gci.GetColorTableEntry(4));

    stateMachine.ProcessString(L"\x1b]4;5;rgb:ff/a1/1b\x1b\\");
    VERIFY_ARE_EQUAL(RGB(0xff, 0xa1, 0x1b), gci.GetColorTableEntry(5));

    Log::Comment(NoThrowString().Format(
        L"Try a bunch of invalid sequences."));
    Log::Comment(NoThrowString().Format(
        L"First start by setting an entry to a known value to compare to."));
    stateMachine.ProcessString(L"\x1b]4;5;rgb:09/09/09\x1b\\");
    VERIFY_ARE_EQUAL(RGB(9, 9, 9), gci.GetColorTableEntry(5));

    Log::Comment(NoThrowString().Format(
        L"invalid: Missing the first component"));
    stateMachine.ProcessString(L"\x1b]4;5;rgb:/1/1\x1b\\");
    VERIFY_ARE_EQUAL(RGB(9, 9, 9), gci.GetColorTableEntry(5));

    Log::Comment(NoThrowString().Format(
        L"invalid: too many components"));
    stateMachine.ProcessString(L"\x1b]4;5;rgb:1/1/1/1\x1b\\");
    VERIFY_ARE_EQUAL(RGB(9, 9, 9), gci.GetColorTableEntry(5));

    Log::Comment(NoThrowString().Format(
        L"invalid: no second component"));
    stateMachine.ProcessString(L"\x1b]4;5;rgb:1//1\x1b\\");
    VERIFY_ARE_EQUAL(RGB(9, 9, 9), gci.GetColorTableEntry(5));

    Log::Comment(NoThrowString().Format(
        L"invalid: no components"));
    stateMachine.ProcessString(L"\x1b]4;5;rgb://\x1b\\");
    VERIFY_ARE_EQUAL(RGB(9, 9, 9), gci.GetColorTableEntry(5));

    Log::Comment(NoThrowString().Format(
        L"invalid: no third component"));
    stateMachine.ProcessString(L"\x1b]4;5;rgb:1/11/\x1b\\");
    VERIFY_ARE_EQUAL(RGB(9, 9, 9), gci.GetColorTableEntry(5));

    Log::Comment(NoThrowString().Format(
        L"invalid: rgbi is not a supported color space"));
    stateMachine.ProcessString(L"\x1b]4;5;rgbi:1/1/1\x1b\\");
    VERIFY_ARE_EQUAL(RGB(9, 9, 9), gci.GetColorTableEntry(5));

    Log::Comment(NoThrowString().Format(
        L"invalid: cmyk is not a supported color space"));
    stateMachine.ProcessString(L"\x1b]4;5;cmyk:1/1/1\x1b\\");
    VERIFY_ARE_EQUAL(RGB(9, 9, 9), gci.GetColorTableEntry(5));

    Log::Comment(NoThrowString().Format(
        L"invalid: no table index should do nothing"));
    stateMachine.ProcessString(L"\x1b]4;;rgb:1/1/1\x1b\\");
    VERIFY_ARE_EQUAL(RGB(9, 9, 9), gci.GetColorTableEntry(5));

    Log::Comment(NoThrowString().Format(
        L"invalid: need to specify a color space"));
    stateMachine.ProcessString(L"\x1b]4;5;1/1/1\x1b\\");
    VERIFY_ARE_EQUAL(RGB(9, 9, 9), gci.GetColorTableEntry(5));
}

void ScreenBufferTests::VtRestoreColorTableReport()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    auto& stateMachine = si.GetStateMachine();

    // Set everything to white to start with.
    for (auto i = 0; i < 16; i++)
    {
        gci.SetColorTableEntry(i, RGB(255, 255, 255));
    }

    // The test cases below are copied from the VT340 default color table, but
    // note that our HLS conversion algorithm doesn't exactly match the VT340,
    // so some of the component values may be off by 1%.

    Log::Comment(L"HLS color definitions");

    // HLS(0°,0%,0%) -> RGB(0,0,0)
    stateMachine.ProcessString(L"\033P2$p0;1;0;0;0\033\\");
    VERIFY_ARE_EQUAL(RGB(0, 0, 0), gci.GetColorTableEntry(0));

    // HLS(0°,49%,59%) -> RGB(51,51,199)
    stateMachine.ProcessString(L"\033P2$p1;1;0;49;59\033\\");
    VERIFY_ARE_EQUAL(RGB(51, 51, 199), gci.GetColorTableEntry(1));

    // HLS(120°,46%,71%) -> RGB(201,34,34)
    stateMachine.ProcessString(L"\033P2$p2;1;120;46;71\033\\");
    VERIFY_ARE_EQUAL(RGB(201, 34, 34), gci.GetColorTableEntry(2));

    // HLS(240°,49%,59%) -> RGB(51,199,51)
    stateMachine.ProcessString(L"\033P2$p3;1;240;49;59\033\\");
    VERIFY_ARE_EQUAL(RGB(51, 199, 51), gci.GetColorTableEntry(3));

    // HLS(60°,49%,59%) -> RGB(199,51,199)
    stateMachine.ProcessString(L"\033P2$p4;1;60;49;59\033\\");
    VERIFY_ARE_EQUAL(RGB(199, 51, 199), gci.GetColorTableEntry(4));

    // HLS(300°,49%,59%) -> RGB(51,199,199)
    stateMachine.ProcessString(L"\033P2$p5;1;300;49;59\033\\");
    VERIFY_ARE_EQUAL(RGB(51, 199, 199), gci.GetColorTableEntry(5));

    // HLS(180°,49%,59%) -> RGB(199,199,51)
    stateMachine.ProcessString(L"\033P2$p6;1;180;49;59\033\\");
    VERIFY_ARE_EQUAL(RGB(199, 199, 51), gci.GetColorTableEntry(6));

    // HLS(0°,46%,0%) -> RGB(117,117,117)
    stateMachine.ProcessString(L"\033P2$p7;1;0;46;0\033\\");
    VERIFY_ARE_EQUAL(RGB(117, 117, 117), gci.GetColorTableEntry(7));

    // HLS(0°,26%,0%) -> RGB(66,66,66)
    stateMachine.ProcessString(L"\033P2$p8;1;0;26;0\033\\");
    VERIFY_ARE_EQUAL(RGB(66, 66, 66), gci.GetColorTableEntry(8));

    // HLS(0°,46%,28%) -> RGB(84,84,150)
    stateMachine.ProcessString(L"\033P2$p9;1;0;46;28\033\\");
    VERIFY_ARE_EQUAL(RGB(84, 84, 150), gci.GetColorTableEntry(9));

    // HLS(120°,42%,38%) -> RGB(148,66,66)
    stateMachine.ProcessString(L"\033P2$p10;1;120;42;38\033\\");
    VERIFY_ARE_EQUAL(RGB(148, 66, 66), gci.GetColorTableEntry(10));

    // HLS(240°,46%,28%) -> RGB(84,150,84)
    stateMachine.ProcessString(L"\033P2$p11;1;240;46;28\033\\");
    VERIFY_ARE_EQUAL(RGB(84, 150, 84), gci.GetColorTableEntry(11));

    // HLS(60°,46%,28%) -> RGB(150,84,150)
    stateMachine.ProcessString(L"\033P2$p12;1;60;46;28\033\\");
    VERIFY_ARE_EQUAL(RGB(150, 84, 150), gci.GetColorTableEntry(12));

    // HLS(300°,46%,28%) -> RGB(84,150,150)
    stateMachine.ProcessString(L"\033P2$p13;1;300;46;28\033\\");
    VERIFY_ARE_EQUAL(RGB(84, 150, 150), gci.GetColorTableEntry(13));

    // HLS(180°,46%,28%) -> RGB(150,150,84)
    stateMachine.ProcessString(L"\033P2$p14;1;180;46;28\033\\");
    VERIFY_ARE_EQUAL(RGB(150, 150, 84), gci.GetColorTableEntry(14));

    // HLS(0°,79%,0%) -> RGB(201,201,201)
    stateMachine.ProcessString(L"\033P2$p15;1;0;79;0\033\\");
    VERIFY_ARE_EQUAL(RGB(201, 201, 201), gci.GetColorTableEntry(15));

    // Reset everything to white again.
    for (auto i = 0; i < 16; i++)
    {
        gci.SetColorTableEntry(i, RGB(255, 255, 255));
    }

    Log::Comment(L"RGB color definitions");

    // RGB(0%,0%,0%) -> RGB(0,0,0)
    stateMachine.ProcessString(L"\033P2$p0;2;0;0;0\033\\");
    VERIFY_ARE_EQUAL(RGB(0, 0, 0), gci.GetColorTableEntry(0));

    // RGB(20%,20%,78%) -> RGB(51,51,199)
    stateMachine.ProcessString(L"\033P2$p1;2;20;20;78\033\\");
    VERIFY_ARE_EQUAL(RGB(51, 51, 199), gci.GetColorTableEntry(1));

    // RGB(79%,13%,13%) -> RGB(201,33,33)
    stateMachine.ProcessString(L"\033P2$p2;2;79;13;13\033\\");
    VERIFY_ARE_EQUAL(RGB(201, 33, 33), gci.GetColorTableEntry(2));

    // RGB(20%,78%,20%) -> RGB(51,199,51)
    stateMachine.ProcessString(L"\033P2$p3;2;20;78;20\033\\");
    VERIFY_ARE_EQUAL(RGB(51, 199, 51), gci.GetColorTableEntry(3));

    // RGB(78%,20%,78%) -> RGB(199,51,199)
    stateMachine.ProcessString(L"\033P2$p4;2;78;20;78\033\\");
    VERIFY_ARE_EQUAL(RGB(199, 51, 199), gci.GetColorTableEntry(4));

    // RGB(20%,78%,78%) -> RGB(51,199,199)
    stateMachine.ProcessString(L"\033P2$p5;2;20;78;78\033\\");
    VERIFY_ARE_EQUAL(RGB(51, 199, 199), gci.GetColorTableEntry(5));

    // RGB(78%,78%,20%) -> RGB(199,199,51)
    stateMachine.ProcessString(L"\033P2$p6;2;78;78;20\033\\");
    VERIFY_ARE_EQUAL(RGB(199, 199, 51), gci.GetColorTableEntry(6));

    // RGB(46%,46%,46%) -> RGB(117,117,117)
    stateMachine.ProcessString(L"\033P2$p7;2;46;46;46\033\\");
    VERIFY_ARE_EQUAL(RGB(117, 117, 117), gci.GetColorTableEntry(7));

    // RGB(26%,26%,26%) -> RGB(66,66,66)
    stateMachine.ProcessString(L"\033P2$p8;2;26;26;26\033\\");
    VERIFY_ARE_EQUAL(RGB(66, 66, 66), gci.GetColorTableEntry(8));

    // RGB(33%,33%,59%) -> RGB(84,84,150)
    stateMachine.ProcessString(L"\033P2$p9;2;33;33;59\033\\");
    VERIFY_ARE_EQUAL(RGB(84, 84, 150), gci.GetColorTableEntry(9));

    // RGB(58%,26%,26%) -> RGB(148,66,66)
    stateMachine.ProcessString(L"\033P2$p10;2;58;26;26\033\\");
    VERIFY_ARE_EQUAL(RGB(148, 66, 66), gci.GetColorTableEntry(10));

    // RGB(33%,59%,33%) -> RGB(84,150,84)
    stateMachine.ProcessString(L"\033P2$p11;2;33;59;33\033\\");
    VERIFY_ARE_EQUAL(RGB(84, 150, 84), gci.GetColorTableEntry(11));

    // RGB(59%,33%,59%) -> RGB(150,84,150)
    stateMachine.ProcessString(L"\033P2$p12;2;59;33;59\033\\");
    VERIFY_ARE_EQUAL(RGB(150, 84, 150), gci.GetColorTableEntry(12));

    // RGB(33%,59%,59%) -> RGB(84,150,150)
    stateMachine.ProcessString(L"\033P2$p13;2;33;59;59\033\\");
    VERIFY_ARE_EQUAL(RGB(84, 150, 150), gci.GetColorTableEntry(13));

    // RGB(59%,59%,33%) -> RGB(150,150,84)
    stateMachine.ProcessString(L"\033P2$p14;2;59;59;33\033\\");
    VERIFY_ARE_EQUAL(RGB(150, 150, 84), gci.GetColorTableEntry(14));

    // RGB(79%,79%,79%) -> RGB(201,201,201)
    stateMachine.ProcessString(L"\033P2$p15;2;79;79;79\033\\");
    VERIFY_ARE_EQUAL(RGB(201, 201, 201), gci.GetColorTableEntry(15));

    // Reset everything to white again.
    for (auto i = 0; i < 16; i++)
    {
        gci.SetColorTableEntry(i, RGB(255, 255, 255));
    }

    Log::Comment(L"Multiple color definitions");

    // Setting colors 0, 2, and 4 to red, green, and blue (HLS).
    stateMachine.ProcessString(L"\033P2$p0;1;120;50;100/2;1;240;50;100/4;1;360;50;100\033\\");
    VERIFY_ARE_EQUAL(RGB(255, 0, 0), gci.GetColorTableEntry(0));
    VERIFY_ARE_EQUAL(RGB(0, 255, 0), gci.GetColorTableEntry(2));
    VERIFY_ARE_EQUAL(RGB(0, 0, 255), gci.GetColorTableEntry(4));

    // Setting colors 1, 3, and 5 to red, green, and blue (RGB).
    stateMachine.ProcessString(L"\033P2$p1;2;100;0;0/3;2;0;100;0/5;2;0;0;100\033\\");
    VERIFY_ARE_EQUAL(RGB(255, 0, 0), gci.GetColorTableEntry(1));
    VERIFY_ARE_EQUAL(RGB(0, 255, 0), gci.GetColorTableEntry(3));
    VERIFY_ARE_EQUAL(RGB(0, 0, 255), gci.GetColorTableEntry(5));

    // The interpretation of omitted and out of range parameter values is based
    // on the VT240 and VT340 sixel implementations. It is assumed that color
    // parsing is handled in the same way for other operations.

    Log::Comment(L"Omitted parameter values");

    // Omitted hue interpreted as 0° (blue)
    stateMachine.ProcessString(L"\033P2$p6;1;;50;100\033\\");
    VERIFY_ARE_EQUAL(RGB(0, 0, 255), gci.GetColorTableEntry(6));

    // Omitted luminosity interpreted as 0% (black)
    stateMachine.ProcessString(L"\033P2$p7;1;120;;100\033\\");
    VERIFY_ARE_EQUAL(RGB(0, 0, 0), gci.GetColorTableEntry(7));

    // Omitted saturation interpreted as 0% (gray)
    stateMachine.ProcessString(L"\033P2$p8;1;120;50\033\\");
    VERIFY_ARE_EQUAL(RGB(128, 128, 128), gci.GetColorTableEntry(8));

    // Omitted red component interpreted as 0%
    stateMachine.ProcessString(L"\033P2$p6;2;;50;100\033\\");
    VERIFY_ARE_EQUAL(RGB(0, 128, 255), gci.GetColorTableEntry(6));

    // Omitted green component interpreted as 0%
    stateMachine.ProcessString(L"\033P2$p7;2;50;;100\033\\");
    VERIFY_ARE_EQUAL(RGB(128, 0, 255), gci.GetColorTableEntry(7));

    // Omitted blue component interpreted as 0%
    stateMachine.ProcessString(L"\033P2$p8;2;50;100\033\\");
    VERIFY_ARE_EQUAL(RGB(128, 255, 0), gci.GetColorTableEntry(8));

    Log::Comment(L"Out of range parameter values");

    // Hue wraps at 360°, so 480° interpreted as 120° (red)
    stateMachine.ProcessString(L"\033P2$p9;1;480;50;100\033\\");
    VERIFY_ARE_EQUAL(RGB(255, 0, 0), gci.GetColorTableEntry(9));

    // Luminosity is clamped at 100%, so 150% interpreted as 100%
    stateMachine.ProcessString(L"\033P2$p10;1;240;150;100\033\\");
    VERIFY_ARE_EQUAL(RGB(255, 255, 255), gci.GetColorTableEntry(10));

    // Saturation is clamped at 100%, so 120% interpreted as 100%
    stateMachine.ProcessString(L"\033P2$p11;1;0;50;120\033\\");
    VERIFY_ARE_EQUAL(RGB(0, 0, 255), gci.GetColorTableEntry(11));

    // Red component is clamped at 100%, so 150% interpreted as 100%
    stateMachine.ProcessString(L"\033P2$p12;2;150;0;0\033\\");
    VERIFY_ARE_EQUAL(RGB(255, 0, 0), gci.GetColorTableEntry(12));

    // Green component is clamped at 100%, so 150% interpreted as 100%
    stateMachine.ProcessString(L"\033P2$p13;2;0;150;0\033\\");
    VERIFY_ARE_EQUAL(RGB(0, 255, 0), gci.GetColorTableEntry(13));

    // Blue component is clamped at 100%, so 150% interpreted as 100%
    stateMachine.ProcessString(L"\033P2$p14;2;0;0;150\033\\");
    VERIFY_ARE_EQUAL(RGB(0, 0, 255), gci.GetColorTableEntry(14));
}

void ScreenBufferTests::ResizeTraditionalDoesNotDoubleFreeAttrRows()
{
    // there is not much to verify here, this test passes if the console doesn't crash.
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();

    gci.SetWrapText(false);
    auto newBufferSize = si.GetBufferSize().Dimensions();
    newBufferSize.height--;

    VERIFY_SUCCEEDED(si.ResizeTraditional(newBufferSize));
}

void ScreenBufferTests::ResizeCursorUnchanged()
{
    // Created for MSFT:19863799. Make sure when we resize the buffer, the
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

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const auto& initialCursor = si.GetTextBuffer().GetCursor();

    // Get initial cursor values
    const auto initialType = initialCursor.GetType();
    const auto initialSize = initialCursor.GetSize();

    // set our wrap mode accordingly - ResizeScreenBuffer will be smart enough
    //  to call the appropriate implementation
    gci.SetWrapText(useResizeWithReflow);

    auto newBufferSize = si.GetBufferSize().Dimensions();
    newBufferSize.width += dx;
    newBufferSize.height += dy;

    VERIFY_SUCCEEDED(si.ResizeScreenBuffer(newBufferSize, false));

    const auto& finalCursor = si.GetTextBuffer().GetCursor();
    const auto finalType = finalCursor.GetType();
    const auto finalSize = finalCursor.GetSize();

    VERIFY_ARE_EQUAL(initialType, finalType);
    VERIFY_ARE_EQUAL(initialSize, finalSize);
}

void ScreenBufferTests::ResizeAltBuffer()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.LockConsole(); // Lock must be taken to manipulate buffer.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    auto& stateMachine = si.GetStateMachine();

    Log::Comment(NoThrowString().Format(
        L"Try resizing the alt buffer. Make sure the call doesn't stack overflow."));

    VERIFY_IS_FALSE(si._IsAltBuffer());
    const auto originalMainSize = Viewport(si._viewport);

    Log::Comment(NoThrowString().Format(
        L"Switch to alt buffer"));
    stateMachine.ProcessString(L"\x1b[?1049h");

    VERIFY_IS_FALSE(si._IsAltBuffer());
    VERIFY_IS_NOT_NULL(si._psiAlternateBuffer);
    const auto psiAlt = si._psiAlternateBuffer;

    auto newSize = originalMainSize.Dimensions();
    newSize.width += 2;
    newSize.height += 2;

    Log::Comment(NoThrowString().Format(
        L"MSFT:15917333 This call shouldn't stack overflow"));
    psiAlt->SetViewportSize(&newSize);
    VERIFY_IS_TRUE(true);

    Log::Comment(NoThrowString().Format(
        L"Switch back from buffer"));
    stateMachine.ProcessString(L"\x1b[?1049l");
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
    auto& gci = g.getConsoleInformation();
    gci.LockConsole(); // Lock must be taken to manipulate buffer.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    auto& mainBuffer = gci.GetActiveOutputBuffer().GetActiveBuffer();
    auto& stateMachine = mainBuffer.GetStateMachine();

    VERIFY_IS_FALSE(mainBuffer._IsAltBuffer());
    const auto originalMainSize = Viewport(mainBuffer._viewport);

    Log::Comment(NoThrowString().Format(
        L"Switch to alt buffer"));
    stateMachine.ProcessString(L"\x1b[?1049h");

    VERIFY_IS_FALSE(mainBuffer._IsAltBuffer());
    VERIFY_IS_NOT_NULL(mainBuffer._psiAlternateBuffer);

    auto& altBuffer = *(mainBuffer._psiAlternateBuffer);
    auto useMain = wil::scope_exit([&] { altBuffer.UseMainScreenBuffer(); });

    auto newBufferSize = originalMainSize.Dimensions();
    newBufferSize.width += dx;
    newBufferSize.height += dy;

    const auto originalAltSize = Viewport(altBuffer._viewport);

    VERIFY_ARE_EQUAL(originalMainSize.Width(), originalAltSize.Width());
    VERIFY_ARE_EQUAL(originalMainSize.Height(), originalAltSize.Height());

    altBuffer.SetViewportSize(&newBufferSize);

    CONSOLE_SCREEN_BUFFER_INFOEX csbiex{ 0 };
    g.api->GetConsoleScreenBufferInfoExImpl(mainBuffer, csbiex);
    const auto newActualMainView = mainBuffer.GetViewport();
    const auto newActualAltView = altBuffer.GetViewport();

    const auto newApiViewport = Viewport::FromExclusive(til::wrap_exclusive_small_rect(csbiex.srWindow));

    VERIFY_ARE_NOT_EQUAL(originalAltSize.Width(), newActualAltView.Width());
    VERIFY_ARE_NOT_EQUAL(originalAltSize.Height(), newActualAltView.Height());

    VERIFY_ARE_NOT_EQUAL(originalMainSize.Width(), newActualAltView.Width());
    VERIFY_ARE_NOT_EQUAL(originalMainSize.Height(), newActualAltView.Height());

    VERIFY_ARE_EQUAL(newActualAltView.Width(), newApiViewport.Width());
    VERIFY_ARE_EQUAL(newActualAltView.Height(), newApiViewport.Height());
}

void ScreenBufferTests::VtEraseAllPersistCursor()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();
    const auto& cursor = tbi.GetCursor();

    Log::Comment(NoThrowString().Format(
        L"Make sure the viewport is at 0,0"));
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, til::point(0, 0), true));

    Log::Comment(NoThrowString().Format(
        L"Move the cursor to 2,2, then execute a Erase All.\n"
        L"The cursor should not move relative to the viewport."));

    stateMachine.ProcessString(L"\x1b[2;2H");
    VERIFY_ARE_EQUAL(til::point(1, 1), cursor.GetPosition());

    stateMachine.ProcessString(L"\x1b[2J");

    auto newViewport = si._viewport;
    til::point expectedCursor = { 1, 1 };
    newViewport.ConvertFromOrigin(&expectedCursor);

    VERIFY_ARE_EQUAL(expectedCursor, cursor.GetPosition());
}

void ScreenBufferTests::VtEraseAllPersistCursorFillColor()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();

    Log::Comment(NoThrowString().Format(
        L"Make sure the viewport is at 0,0"));
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, til::point(0, 0), true));

    Log::Comment(NoThrowString().Format(
        L"Change the colors to dark_red on bright_blue, then execute a Erase All.\n"
        L"The viewport should be full of dark_red on bright_blue"));

    auto expectedAttr = TextAttribute{};
    expectedAttr.SetIndexedForeground(TextColor::DARK_RED);
    expectedAttr.SetIndexedBackground(TextColor::BRIGHT_BLUE);
    stateMachine.ProcessString(L"\x1b[31;104m");

    VERIFY_ARE_EQUAL(expectedAttr, si.GetAttributes());

    stateMachine.ProcessString(L"\x1b[2J");

    VERIFY_ARE_EQUAL(expectedAttr, si.GetAttributes());

    auto newViewport = si._viewport;
    Log::Comment(NoThrowString().Format(
        L"new Viewport: %s",
        VerifyOutputTraits<til::inclusive_rect>::ToString(newViewport.ToInclusive()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"Buffer Size: %s",
        VerifyOutputTraits<til::inclusive_rect>::ToString(si.GetBufferSize().ToInclusive()).GetBuffer()));

    auto iter = tbi.GetCellDataAt(newViewport.Origin());
    auto height = newViewport.Height();
    auto width = newViewport.Width();
    for (auto i = 0; i < height; i++)
    {
        for (auto j = 0; j < width; j++)
        {
            VERIFY_ARE_EQUAL(expectedAttr, iter->TextAttr());
            iter++;
        }
    }
}

void ScreenBufferTests::GetWordBoundary()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();

    const auto text = L"This is some test text for word boundaries.";
    const auto length = wcslen(text);

    // Make the buffer as big as our test text.
    const til::size newBufferSize = { gsl::narrow<til::CoordType>(length), 10 };
    VERIFY_SUCCEEDED(si.GetTextBuffer().ResizeTraditional(newBufferSize));

    const OutputCellIterator it(text, si.GetAttributes());
    si.Write(it, { 0, 0 });

    // Now find some words in it.
    Log::Comment(L"Find first word from its front.");
    til::point expectedFirst = { 0, 0 };
    til::point expectedSecond = { 4, 0 };

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
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();

    const auto text = L"000fe12 0xfe12 0Xfe12 0nfe12 0Nfe12";
    const auto length = wcslen(text);

    // Make the buffer as big as our test text.
    const til::size newBufferSize = { gsl::narrow<til::CoordType>(length), 10 };
    VERIFY_SUCCEEDED(si.GetTextBuffer().ResizeTraditional(newBufferSize));

    const OutputCellIterator it(text, si.GetAttributes());
    si.Write(it, { 0, 0 });

    gci.SetTrimLeadingZeros(on);

    til::point expectedFirst;
    til::point expectedSecond;
    std::pair<til::point, til::point> boundary;

    Log::Comment(L"Find lead with 000");
    expectedFirst = on ? til::point{ 3, 0 } : til::point{ 0, 0 };
    expectedSecond = { 7, 0 };
    boundary = si.GetWordBoundary({ 0, 0 });
    VERIFY_ARE_EQUAL(expectedFirst, boundary.first);
    VERIFY_ARE_EQUAL(expectedSecond, boundary.second);

    Log::Comment(L"Find lead with 0x");
    expectedFirst = { 8, 0 };
    expectedSecond = { 14, 0 };
    boundary = si.GetWordBoundary({ 8, 0 });
    VERIFY_ARE_EQUAL(expectedFirst, boundary.first);
    VERIFY_ARE_EQUAL(expectedSecond, boundary.second);

    Log::Comment(L"Find lead with 0X");
    expectedFirst = { 15, 0 };
    expectedSecond = { 21, 0 };
    boundary = si.GetWordBoundary({ 15, 0 });
    VERIFY_ARE_EQUAL(expectedFirst, boundary.first);
    VERIFY_ARE_EQUAL(expectedSecond, boundary.second);

    Log::Comment(L"Find lead with 0n");
    expectedFirst = { 22, 0 };
    expectedSecond = { 28, 0 };
    boundary = si.GetWordBoundary({ 22, 0 });
    VERIFY_ARE_EQUAL(expectedFirst, boundary.first);
    VERIFY_ARE_EQUAL(expectedSecond, boundary.second);

    Log::Comment(L"Find lead with 0N");
    expectedFirst = on ? til::point{ 30, 0 } : til::point{ 29, 0 };
    expectedSecond = { 35, 0 };
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
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.LockConsole(); // Lock must be taken to manipulate buffer.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    Log::Comment(L"Creating one alternate buffer");
    auto& original = gci.GetActiveOutputBuffer();
    VERIFY_IS_NULL(original._psiAlternateBuffer);
    VERIFY_IS_NULL(original._psiMainBuffer);

    auto Status = original.UseAlternateScreenBuffer({});
    if (VERIFY_NT_SUCCESS(Status))
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
        VERIFY_ARE_EQUAL(mainCursor.GetType(), altCursor.GetType());
    }
}

void ScreenBufferTests::TestAltBufferVtDispatching()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.LockConsole(); // Lock must be taken to manipulate buffer.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });
    Log::Comment(L"Creating one alternate buffer");
    auto& mainBuffer = gci.GetActiveOutputBuffer();
    // Make sure we're in VT mode
    WI_SetFlag(mainBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    // Make sure we're suing the default attributes at the start of the test,
    // Otherwise they could be polluted from a previous test.
    mainBuffer.SetAttributes({});

    VERIFY_IS_NULL(mainBuffer._psiAlternateBuffer);
    VERIFY_IS_NULL(mainBuffer._psiMainBuffer);

    auto Status = mainBuffer.UseAlternateScreenBuffer({});
    if (VERIFY_NT_SUCCESS(Status))
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

        const til::point origin;
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
        auto seqCb = 2 * seq.size();
        VERIFY_SUCCEEDED(DoWriteConsole(&seq[0], &seqCb, mainBuffer, false, waiter));

        VERIFY_ARE_EQUAL(til::point(0, 0), mainCursor.GetPosition());
        // recall: vt coordinates are (row, column), 1-indexed
        VERIFY_ARE_EQUAL(til::point(5, 4), altCursor.GetPosition());

        const TextAttribute expectedDefaults = {};
        auto expectedRgb = expectedDefaults;
        expectedRgb.SetBackground(RGB(255, 0, 255));

        VERIFY_ARE_EQUAL(expectedDefaults, mainBuffer.GetAttributes());
        VERIFY_ARE_EQUAL(expectedDefaults, alternate.GetAttributes());

        seq = L"\x1b[48;2;255;0;255m";
        seqCb = 2 * seq.size();
        VERIFY_SUCCEEDED(DoWriteConsole(&seq[0], &seqCb, mainBuffer, false, waiter));

        VERIFY_ARE_EQUAL(expectedDefaults, mainBuffer.GetAttributes());
        VERIFY_ARE_EQUAL(expectedRgb, alternate.GetAttributes());

        seq = L"X";
        seqCb = 2 * seq.size();
        VERIFY_SUCCEEDED(DoWriteConsole(&seq[0], &seqCb, mainBuffer, false, waiter));

        VERIFY_ARE_EQUAL(til::point(0, 0), mainCursor.GetPosition());
        VERIFY_ARE_EQUAL(til::point(6, 4), altCursor.GetPosition());

        // Recall we didn't print an 'X' to the main buffer, so there's no
        //      char to inspect the attributes of.
        const auto& altRow = alternate.GetTextBuffer().GetRowByOffset(altCursor.GetPosition().y);
        const std::vector<TextAttribute> altAttrs{ altRow.AttrBegin(), altRow.AttrEnd() };
        const auto altAttrA = altAttrs[altCursor.GetPosition().x - 1];
        VERIFY_ARE_EQUAL(expectedRgb, altAttrA);
    }
}

void ScreenBufferTests::TestAltBufferRIS()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.LockConsole(); // Lock must be taken to manipulate buffer.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    auto& si = gci.GetActiveOutputBuffer();
    auto& stateMachine = si.GetStateMachine();

    Log::Comment(L"Initially in main buffer");
    VERIFY_IS_FALSE(gci.GetActiveOutputBuffer()._IsAltBuffer());

    Log::Comment(L"Switch to alt buffer");
    stateMachine.ProcessString(L"\x1b[?1049h");
    VERIFY_IS_TRUE(gci.GetActiveOutputBuffer()._IsAltBuffer());

    Log::Comment(L"RIS returns to main buffer");
    stateMachine.ProcessString(L"\033c");
    VERIFY_IS_FALSE(gci.GetActiveOutputBuffer()._IsAltBuffer());
}

void ScreenBufferTests::SetDefaultsIndividuallyBothDefault()
{
    // Tests MSFT:19828103

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si.GetTextBuffer().GetCursor();
    auto& renderSettings = gci.GetRenderSettings();

    Log::Comment(NoThrowString().Format(L"Make sure the viewport is at 0,0"));
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, til::point(0, 0), true));
    cursor.SetPosition({ 0, 0 });

    auto magenta = RGB(255, 0, 255);
    auto yellow = RGB(255, 255, 0);
    auto brightGreen = gci.GetColorTableEntry(TextColor::BRIGHT_GREEN);
    auto darkBlue = gci.GetColorTableEntry(TextColor::DARK_BLUE);

    gci.SetColorTableEntry(TextColor::DEFAULT_FOREGROUND, yellow);
    gci.SetColorTableEntry(TextColor::DEFAULT_BACKGROUND, magenta);
    gci.CalculateDefaultColorIndices();
    si.SetDefaultAttributes({}, TextAttribute{ gci.GetPopupFillAttribute() });

    Log::Comment(NoThrowString().Format(L"Write 6 X's:"));
    Log::Comment(NoThrowString().Format(L"  The first in default-fg on default-bg (yellow on magenta)"));
    Log::Comment(NoThrowString().Format(L"  The second with bright-green on dark-blue"));
    Log::Comment(NoThrowString().Format(L"  The third with default-fg on dark-blue"));
    Log::Comment(NoThrowString().Format(L"  The fourth in default-fg on default-bg (yellow on magenta)"));
    Log::Comment(NoThrowString().Format(L"  The fifth with bright-green on dark-blue"));
    Log::Comment(NoThrowString().Format(L"  The sixth with bright-green on default-bg"));

    stateMachine.ProcessString(L"\x1b[m"); // Reset to defaults
    stateMachine.ProcessString(L"X");

    stateMachine.ProcessString(L"\x1b[92;44m"); // bright-green on dark-blue
    stateMachine.ProcessString(L"X");

    stateMachine.ProcessString(L"\x1b[39m"); // reset fg
    stateMachine.ProcessString(L"X");

    stateMachine.ProcessString(L"\x1b[49m"); // reset bg
    stateMachine.ProcessString(L"X");

    stateMachine.ProcessString(L"\x1b[92;44m"); // bright-green on dark-blue
    stateMachine.ProcessString(L"X");

    stateMachine.ProcessString(L"\x1b[49m"); // reset bg
    stateMachine.ProcessString(L"X");

    // See the log comment above for description of these values.
    TextAttribute expectedDefaults{};
    TextAttribute expectedTwo;
    expectedTwo.SetIndexedForeground(TextColor::BRIGHT_GREEN);
    expectedTwo.SetIndexedBackground(TextColor::DARK_BLUE);
    auto expectedThree = expectedTwo;
    expectedThree.SetDefaultForeground();
    // Four is the same as Defaults
    // Five is the same as two
    auto expectedSix = expectedTwo;
    expectedSix.SetDefaultBackground();

    til::point expectedCursor{ 6, 0 };
    VERIFY_ARE_EQUAL(expectedCursor, cursor.GetPosition());

    const auto& row = tbi.GetRowByOffset(0);
    const std::vector<TextAttribute> attrs{ row.AttrBegin(), row.AttrEnd() };
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

    VERIFY_ARE_EQUAL(std::make_pair(yellow, magenta), renderSettings.GetAttributeColors(attrA));
    VERIFY_ARE_EQUAL(std::make_pair(brightGreen, darkBlue), renderSettings.GetAttributeColors(attrB));
    VERIFY_ARE_EQUAL(std::make_pair(yellow, darkBlue), renderSettings.GetAttributeColors(attrC));
    VERIFY_ARE_EQUAL(std::make_pair(yellow, magenta), renderSettings.GetAttributeColors(attrD));
    VERIFY_ARE_EQUAL(std::make_pair(brightGreen, darkBlue), renderSettings.GetAttributeColors(attrE));
    VERIFY_ARE_EQUAL(std::make_pair(brightGreen, magenta), renderSettings.GetAttributeColors(attrF));
}

void ScreenBufferTests::SetDefaultsTogether()
{
    // Tests MSFT:19828103

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si.GetTextBuffer().GetCursor();
    auto& renderSettings = gci.GetRenderSettings();

    Log::Comment(NoThrowString().Format(L"Make sure the viewport is at 0,0"));
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, til::point(0, 0), true));
    cursor.SetPosition({ 0, 0 });

    auto magenta = RGB(255, 0, 255);
    auto yellow = RGB(255, 255, 0);
    auto color250 = gci.GetColorTableEntry(250);

    gci.SetColorTableEntry(TextColor::DEFAULT_FOREGROUND, yellow);
    gci.SetColorTableEntry(TextColor::DEFAULT_BACKGROUND, magenta);
    gci.CalculateDefaultColorIndices();
    si.SetDefaultAttributes({}, TextAttribute{ gci.GetPopupFillAttribute() });

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
    expectedTwo.SetIndexedBackground256(250);

    til::point expectedCursor{ 3, 0 };
    VERIFY_ARE_EQUAL(expectedCursor, cursor.GetPosition());

    const auto& row = tbi.GetRowByOffset(0);
    const std::vector<TextAttribute> attrs{ row.AttrBegin(), row.AttrEnd() };
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

    VERIFY_ARE_EQUAL(std::make_pair(yellow, magenta), renderSettings.GetAttributeColors(attrA));
    VERIFY_ARE_EQUAL(std::make_pair(yellow, color250), renderSettings.GetAttributeColors(attrB));
    VERIFY_ARE_EQUAL(std::make_pair(yellow, magenta), renderSettings.GetAttributeColors(attrC));
}

void ScreenBufferTests::ReverseResetWithDefaultBackground()
{
    // Tests MSFT:19694089
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si.GetTextBuffer().GetCursor();
    auto& renderSettings = gci.GetRenderSettings();

    Log::Comment(NoThrowString().Format(L"Make sure the viewport is at 0,0"));
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, til::point(0, 0), true));
    cursor.SetPosition({ 0, 0 });

    auto magenta = RGB(255, 0, 255);

    gci.SetColorTableEntry(TextColor::DEFAULT_FOREGROUND, INVALID_COLOR);
    gci.SetColorTableEntry(TextColor::DEFAULT_BACKGROUND, magenta);
    gci.CalculateDefaultColorIndices();
    si.SetDefaultAttributes({}, TextAttribute{ gci.GetPopupFillAttribute() });

    Log::Comment(NoThrowString().Format(L"Write 3 X's:"));
    Log::Comment(NoThrowString().Format(L"  The first in default-attr on default color (magenta)"));
    Log::Comment(NoThrowString().Format(L"  The second with reversed attrs"));
    Log::Comment(NoThrowString().Format(L"  The third after resetting the attrs back"));

    stateMachine.ProcessString(L"X");
    stateMachine.ProcessString(L"\x1b[7m");
    stateMachine.ProcessString(L"X");
    stateMachine.ProcessString(L"\x1b[27m");
    stateMachine.ProcessString(L"X");

    TextAttribute expectedDefaults{ gci.GetFillAttribute() };
    expectedDefaults.SetDefaultBackground();
    auto expectedReversed = expectedDefaults;
    expectedReversed.Invert();

    til::point expectedCursor{ 3, 0 };
    VERIFY_ARE_EQUAL(expectedCursor, cursor.GetPosition());

    const auto& row = tbi.GetRowByOffset(0);
    const std::vector<TextAttribute> attrs{ row.AttrBegin(), row.AttrEnd() };
    const auto attrA = attrs[0];
    const auto attrB = attrs[1];
    const auto attrC = attrs[2];

    LOG_ATTR(attrA);
    LOG_ATTR(attrB);
    LOG_ATTR(attrC);

    VERIFY_ARE_EQUAL(false, attrA.IsLegacy());
    VERIFY_ARE_EQUAL(false, attrB.IsLegacy());
    VERIFY_ARE_EQUAL(false, attrC.IsLegacy());

    VERIFY_ARE_EQUAL(false, attrA.IsReverseVideo());
    VERIFY_ARE_EQUAL(true, attrB.IsReverseVideo());
    VERIFY_ARE_EQUAL(false, attrC.IsReverseVideo());

    VERIFY_ARE_EQUAL(expectedDefaults, attrA);
    VERIFY_ARE_EQUAL(expectedReversed, attrB);
    VERIFY_ARE_EQUAL(expectedDefaults, attrC);

    VERIFY_ARE_EQUAL(magenta, renderSettings.GetAttributeColors(attrA).second);
    VERIFY_ARE_EQUAL(magenta, renderSettings.GetAttributeColors(attrB).first);
    VERIFY_ARE_EQUAL(magenta, renderSettings.GetAttributeColors(attrC).second);
}

void ScreenBufferTests::BackspaceDefaultAttrs()
{
    // Created for MSFT:19735050, but doesn't actually test that.
    // That bug actually involves the input line, and that needs to use
    //      TextAttributes instead of WORDs

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si.GetTextBuffer().GetCursor();
    auto& renderSettings = gci.GetRenderSettings();

    Log::Comment(NoThrowString().Format(L"Make sure the viewport is at 0,0"));
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, til::point(0, 0), true));
    cursor.SetPosition({ 0, 0 });

    auto magenta = RGB(255, 0, 255);

    gci.SetColorTableEntry(TextColor::DEFAULT_BACKGROUND, magenta);
    gci.CalculateDefaultColorIndices();
    si.SetDefaultAttributes({}, TextAttribute{ gci.GetPopupFillAttribute() });

    Log::Comment(NoThrowString().Format(L"Write 2 X's, then backspace one."));

    stateMachine.ProcessString(L"\x1b[m");
    stateMachine.ProcessString(L"XX");
    stateMachine.ProcessString({ &UNICODE_BACKSPACE, 1 });

    TextAttribute expectedDefaults{};
    expectedDefaults.SetDefaultBackground();

    til::point expectedCursor{ 1, 0 };
    VERIFY_ARE_EQUAL(expectedCursor, cursor.GetPosition());

    const auto& row = tbi.GetRowByOffset(0);
    const std::vector<TextAttribute> attrs{ row.AttrBegin(), row.AttrEnd() };
    const auto attrA = attrs[0];
    const auto attrB = attrs[1];

    LOG_ATTR(attrA);
    LOG_ATTR(attrB);

    VERIFY_ARE_EQUAL(false, attrA.IsLegacy());
    VERIFY_ARE_EQUAL(false, attrB.IsLegacy());

    VERIFY_ARE_EQUAL(expectedDefaults, attrA);
    VERIFY_ARE_EQUAL(expectedDefaults, attrB);

    VERIFY_ARE_EQUAL(magenta, renderSettings.GetAttributeColors(attrA).second);
    VERIFY_ARE_EQUAL(magenta, renderSettings.GetAttributeColors(attrB).second);
}

void ScreenBufferTests::BackspaceDefaultAttrsWriteCharsLegacy()
{
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:writeSingly", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:writeCharsLegacyMode", L"{0, 1, 2}")
    END_TEST_METHOD_PROPERTIES();

    bool writeSingly;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"writeSingly", writeSingly), L"Write one at a time = true, all at the same time = false");

    DWORD writeCharsLegacyMode;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"writeCharsLegacyMode", writeCharsLegacyMode), L"");

    // Created for MSFT:19735050.
    // Kinda the same as above, but with WriteCharsLegacy instead.
    // The variable that really breaks this scenario

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si.GetTextBuffer().GetCursor();
    auto& renderSettings = gci.GetRenderSettings();

    Log::Comment(NoThrowString().Format(L"Make sure the viewport is at 0,0"));
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, til::point(0, 0), true));
    cursor.SetPosition({ 0, 0 });

    auto magenta = RGB(255, 0, 255);

    gci.SetColorTableEntry(TextColor::DEFAULT_BACKGROUND, magenta);
    gci.CalculateDefaultColorIndices();
    si.SetDefaultAttributes({}, TextAttribute{ gci.GetPopupFillAttribute() });

    Log::Comment(NoThrowString().Format(L"Write 2 X's, then backspace one."));

    stateMachine.ProcessString(L"\x1b[m");

    if (writeSingly)
    {
        auto str = L"X";
        size_t seqCb = 2;
        VERIFY_NT_SUCCESS(WriteCharsLegacy(si, str, str, str, &seqCb, nullptr, cursor.GetPosition().x, writeCharsLegacyMode, nullptr));
        VERIFY_NT_SUCCESS(WriteCharsLegacy(si, str, str, str, &seqCb, nullptr, cursor.GetPosition().x, writeCharsLegacyMode, nullptr));
        str = L"\x08";
        VERIFY_NT_SUCCESS(WriteCharsLegacy(si, str, str, str, &seqCb, nullptr, cursor.GetPosition().x, writeCharsLegacyMode, nullptr));
    }
    else
    {
        const auto str = L"XX\x08";
        size_t seqCb = 6;
        VERIFY_NT_SUCCESS(WriteCharsLegacy(si, str, str, str, &seqCb, nullptr, cursor.GetPosition().x, writeCharsLegacyMode, nullptr));
    }

    TextAttribute expectedDefaults{};
    expectedDefaults.SetDefaultBackground();

    til::point expectedCursor{ 1, 0 };
    VERIFY_ARE_EQUAL(expectedCursor, cursor.GetPosition());

    const auto& row = tbi.GetRowByOffset(0);
    const std::vector<TextAttribute> attrs{ row.AttrBegin(), row.AttrEnd() };
    const auto attrA = attrs[0];
    const auto attrB = attrs[1];

    LOG_ATTR(attrA);
    LOG_ATTR(attrB);

    VERIFY_ARE_EQUAL(false, attrA.IsLegacy());
    VERIFY_ARE_EQUAL(false, attrB.IsLegacy());

    VERIFY_ARE_EQUAL(expectedDefaults, attrA);
    VERIFY_ARE_EQUAL(expectedDefaults, attrB);

    VERIFY_ARE_EQUAL(magenta, renderSettings.GetAttributeColors(attrA).second);
    VERIFY_ARE_EQUAL(magenta, renderSettings.GetAttributeColors(attrB).second);
}

void ScreenBufferTests::BackspaceDefaultAttrsInPrompt()
{
    // Tests MSFT:19853701 - when you edit the prompt line at a bash prompt,
    //  make sure that the end of the line isn't filled with default/garbage attributes.

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si.GetTextBuffer().GetCursor();

    // Make sure we're in VT mode
    WI_SetFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    VERIFY_IS_TRUE(WI_IsFlagSet(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING));

    Log::Comment(NoThrowString().Format(L"Make sure the viewport is at 0,0"));
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, til::point(0, 0), true));
    cursor.SetPosition({ 0, 0 });

    auto magenta = RGB(255, 0, 255);

    gci.SetColorTableEntry(TextColor::DEFAULT_BACKGROUND, magenta);
    gci.CalculateDefaultColorIndices();
    si.SetDefaultAttributes({}, TextAttribute{ gci.GetPopupFillAttribute() });
    TextAttribute expectedDefaults{};

    Log::Comment(NoThrowString().Format(L"Write 3 X's, move to the left, then delete-char the second."));
    Log::Comment(NoThrowString().Format(L"This emulates editing the prompt line on bash"));

    stateMachine.ProcessString(L"\x1b[m");
    Log::Comment(NoThrowString().Format(
        L"Clear the screen - make sure the line is filled with the current attributes."));
    stateMachine.ProcessString(L"\x1b[2J");

    const auto viewport = si.GetViewport();
    const auto& row = tbi.GetRowByOffset(cursor.GetPosition().y);

    {
        SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);
        Log::Comment(NoThrowString().Format(
            L"Make sure the row contains what we're expecting before we start."
            L"It should entirely be filled with defaults"));

        const std::vector<TextAttribute> initialAttrs{ row.AttrBegin(), row.AttrEnd() };
        for (auto x = 0; x <= viewport.RightInclusive(); x++)
        {
            const auto& attr = initialAttrs[x];
            VERIFY_ARE_EQUAL(expectedDefaults, attr);
        }
    }
    Log::Comment(NoThrowString().Format(
        L"Print 'XXX', move the cursor left 2, delete a character."));

    stateMachine.ProcessString(L"XXX");
    stateMachine.ProcessString(L"\x1b[2D");
    stateMachine.ProcessString(L"\x1b[P");

    til::point expectedCursor{ 1, 0 };
    VERIFY_ARE_EQUAL(expectedCursor, cursor.GetPosition());

    const std::vector<TextAttribute> attrs{ row.AttrBegin(), row.AttrEnd() };
    for (auto x = 0; x <= viewport.RightInclusive(); x++)
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

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.LockConsole(); // Lock must be taken to swap buffers.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    auto& mainBuffer = gci.GetActiveOutputBuffer();
    VERIFY_IS_FALSE(mainBuffer._IsAltBuffer());
    WI_SetFlag(mainBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    VERIFY_IS_TRUE(WI_IsFlagSet(mainBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING));

    auto& stateMachine = mainBuffer.GetStateMachine();
    auto& mainCursor = mainBuffer.GetTextBuffer().GetCursor();
    const auto& renderSettings = gci.GetRenderSettings();

    Log::Comment(NoThrowString().Format(L"Make sure the viewport is at 0,0"));
    VERIFY_SUCCEEDED(mainBuffer.SetViewportOrigin(true, til::point(0, 0), true));
    mainCursor.SetPosition({ 0, 0 });

    const auto originalRed = gci.GetColorTableEntry(TextColor::DARK_RED);
    const auto testColor = RGB(0x11, 0x22, 0x33);
    VERIFY_ARE_NOT_EQUAL(originalRed, testColor);

    stateMachine.ProcessString(L"\x1b[41m");
    stateMachine.ProcessString(L"X");
    til::point expectedCursor{ 1, 0 };
    VERIFY_ARE_EQUAL(expectedCursor, mainCursor.GetPosition());
    {
        const auto& row = mainBuffer.GetTextBuffer().GetRowByOffset(mainCursor.GetPosition().y);
        const std::vector<TextAttribute> attrs{ row.AttrBegin(), row.AttrEnd() };
        const auto attrA = attrs[0];
        LOG_ATTR(attrA);
        VERIFY_ARE_EQUAL(originalRed, renderSettings.GetAttributeColors(attrA).second);
    }

    Log::Comment(NoThrowString().Format(L"Create an alt buffer"));

    VERIFY_SUCCEEDED(mainBuffer.UseAlternateScreenBuffer({}));
    auto& altBuffer = gci.GetActiveOutputBuffer();
    auto useMain = wil::scope_exit([&] { altBuffer.UseMainScreenBuffer(); });

    WI_SetFlag(altBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    VERIFY_IS_TRUE(WI_IsFlagSet(altBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING));

    auto& altCursor = altBuffer.GetTextBuffer().GetCursor();
    altCursor.SetPosition({ 0, 0 });

    Log::Comment(NoThrowString().Format(
        L"Print one X in red, should be the original red color"));
    stateMachine.ProcessString(L"\x1b[41m");
    stateMachine.ProcessString(L"X");
    VERIFY_ARE_EQUAL(expectedCursor, altCursor.GetPosition());
    {
        const auto& row = altBuffer.GetTextBuffer().GetRowByOffset(altCursor.GetPosition().y);
        const std::vector<TextAttribute> attrs{ row.AttrBegin(), row.AttrEnd() };
        const auto attrA = attrs[0];
        LOG_ATTR(attrA);
        VERIFY_ARE_EQUAL(originalRed, renderSettings.GetAttributeColors(attrA).second);
    }

    Log::Comment(NoThrowString().Format(L"Change the value of red to RGB(0x11, 0x22, 0x33)"));
    stateMachine.ProcessString(L"\x1b]4;1;rgb:11/22/33\x07");
    Log::Comment(NoThrowString().Format(
        L"Print another X, both should be the new \"red\" color"));
    stateMachine.ProcessString(L"X");
    VERIFY_ARE_EQUAL(til::point(2, 0), altCursor.GetPosition());
    {
        const auto& row = altBuffer.GetTextBuffer().GetRowByOffset(altCursor.GetPosition().y);
        const std::vector<TextAttribute> attrs{ row.AttrBegin(), row.AttrEnd() };
        const auto attrA = attrs[0];
        const auto attrB = attrs[1];
        LOG_ATTR(attrA);
        LOG_ATTR(attrB);
        VERIFY_ARE_EQUAL(testColor, renderSettings.GetAttributeColors(attrA).second);
        VERIFY_ARE_EQUAL(testColor, renderSettings.GetAttributeColors(attrB).second);
    }

    Log::Comment(NoThrowString().Format(L"Switch back to the main buffer"));
    useMain.release();
    altBuffer.UseMainScreenBuffer();

    const auto& mainBufferPostSwitch = gci.GetActiveOutputBuffer();
    VERIFY_ARE_EQUAL(&mainBufferPostSwitch, &mainBuffer);

    Log::Comment(NoThrowString().Format(
        L"Print another X, both should be the new \"red\" color"));
    stateMachine.ProcessString(L"X");
    VERIFY_ARE_EQUAL(til::point(2, 0), mainCursor.GetPosition());
    {
        const auto& row = mainBuffer.GetTextBuffer().GetRowByOffset(mainCursor.GetPosition().y);
        const std::vector<TextAttribute> attrs{ row.AttrBegin(), row.AttrEnd() };
        const auto attrA = attrs[0];
        const auto attrB = attrs[1];
        LOG_ATTR(attrA);
        LOG_ATTR(attrB);
        VERIFY_ARE_EQUAL(testColor, renderSettings.GetAttributeColors(attrA).second);
        VERIFY_ARE_EQUAL(testColor, renderSettings.GetAttributeColors(attrB).second);
    }
}

void ScreenBufferTests::SetColorTableThreeDigits()
{
    // Created for MSFT:19723934.
    // Changing the value of the color table above index 99 should work

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.LockConsole(); // Lock must be taken to swap buffers.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    auto& mainBuffer = gci.GetActiveOutputBuffer();
    VERIFY_IS_FALSE(mainBuffer._IsAltBuffer());
    WI_SetFlag(mainBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    VERIFY_IS_TRUE(WI_IsFlagSet(mainBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING));

    auto& stateMachine = mainBuffer.GetStateMachine();
    auto& mainCursor = mainBuffer.GetTextBuffer().GetCursor();
    const auto& renderSettings = gci.GetRenderSettings();

    Log::Comment(NoThrowString().Format(L"Make sure the viewport is at 0,0"));
    VERIFY_SUCCEEDED(mainBuffer.SetViewportOrigin(true, til::point(0, 0), true));
    mainCursor.SetPosition({ 0, 0 });

    const auto originalRed = gci.GetColorTableEntry(123);
    const auto testColor = RGB(0x11, 0x22, 0x33);
    VERIFY_ARE_NOT_EQUAL(originalRed, testColor);

    stateMachine.ProcessString(L"\x1b[48;5;123m");
    stateMachine.ProcessString(L"X");
    til::point expectedCursor{ 1, 0 };
    VERIFY_ARE_EQUAL(expectedCursor, mainCursor.GetPosition());
    {
        const auto& row = mainBuffer.GetTextBuffer().GetRowByOffset(mainCursor.GetPosition().y);
        const std::vector<TextAttribute> attrs{ row.AttrBegin(), row.AttrEnd() };
        const auto attrA = attrs[0];
        LOG_ATTR(attrA);
        VERIFY_ARE_EQUAL(originalRed, renderSettings.GetAttributeColors(attrA).second);
    }

    Log::Comment(NoThrowString().Format(L"Create an alt buffer"));

    VERIFY_SUCCEEDED(mainBuffer.UseAlternateScreenBuffer({}));
    auto& altBuffer = gci.GetActiveOutputBuffer();
    auto useMain = wil::scope_exit([&] { altBuffer.UseMainScreenBuffer(); });

    WI_SetFlag(altBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    VERIFY_IS_TRUE(WI_IsFlagSet(altBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING));

    auto& altCursor = altBuffer.GetTextBuffer().GetCursor();
    altCursor.SetPosition({ 0, 0 });

    Log::Comment(NoThrowString().Format(
        L"Print one X in red, should be the original red color"));
    stateMachine.ProcessString(L"\x1b[48;5;123m");
    stateMachine.ProcessString(L"X");
    VERIFY_ARE_EQUAL(expectedCursor, altCursor.GetPosition());
    {
        const auto& row = altBuffer.GetTextBuffer().GetRowByOffset(altCursor.GetPosition().y);
        const std::vector<TextAttribute> attrs{ row.AttrBegin(), row.AttrEnd() };
        const auto attrA = attrs[0];
        LOG_ATTR(attrA);
        VERIFY_ARE_EQUAL(originalRed, renderSettings.GetAttributeColors(attrA).second);
    }

    Log::Comment(NoThrowString().Format(L"Change the value of red to RGB(0x11, 0x22, 0x33)"));
    stateMachine.ProcessString(L"\x1b]4;123;rgb:11/22/33\x07");
    Log::Comment(NoThrowString().Format(
        L"Print another X, it should be the new \"red\" color"));
    // TODO MSFT:20105972 -
    // You shouldn't need to manually update the attributes again.
    stateMachine.ProcessString(L"\x1b[48;5;123m");
    stateMachine.ProcessString(L"X");
    VERIFY_ARE_EQUAL(til::point(2, 0), altCursor.GetPosition());
    {
        const auto& row = altBuffer.GetTextBuffer().GetRowByOffset(altCursor.GetPosition().y);
        const std::vector<TextAttribute> attrs{ row.AttrBegin(), row.AttrEnd() };
        const auto attrB = attrs[1];
        // TODO MSFT:20105972 - attrA and attrB should both be the same color now
        LOG_ATTR(attrB);
        VERIFY_ARE_EQUAL(testColor, renderSettings.GetAttributeColors(attrB).second);
    }
}

void ScreenBufferTests::SetDefaultForegroundColor()
{
    // Setting the default foreground color should work

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.LockConsole(); // Lock must be taken to swap buffers.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    auto& mainBuffer = gci.GetActiveOutputBuffer();
    VERIFY_IS_FALSE(mainBuffer._IsAltBuffer());
    WI_SetFlag(mainBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    VERIFY_IS_TRUE(WI_IsFlagSet(mainBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING));

    auto& stateMachine = mainBuffer.GetStateMachine();

    auto originalColor = gci.GetColorTableEntry(TextColor::DEFAULT_FOREGROUND);
    auto newColor = gci.GetColorTableEntry(TextColor::DEFAULT_FOREGROUND);
    auto testColor = RGB(0x33, 0x66, 0x99);
    VERIFY_ARE_NOT_EQUAL(originalColor, testColor);

    Log::Comment(L"Valid Hexadecimal Notation");
    stateMachine.ProcessString(L"\x1b]10;rgb:33/66/99\x1b\\");

    newColor = gci.GetColorTableEntry(TextColor::DEFAULT_FOREGROUND);
    VERIFY_ARE_EQUAL(testColor, newColor);

    Log::Comment(L"Valid Hexadecimal Notation");
    originalColor = newColor;
    testColor = RGB(0xff, 0xff, 0xff);
    stateMachine.ProcessString(L"\x1b]10;rgb:ff/ff/ff\x1b\\");

    newColor = gci.GetColorTableEntry(TextColor::DEFAULT_FOREGROUND);
    VERIFY_ARE_EQUAL(testColor, newColor);

    Log::Comment(L"Invalid syntax");
    originalColor = newColor;
    testColor = RGB(153, 102, 51);
    stateMachine.ProcessString(L"\x1b]10;99/66/33\x1b\\");

    newColor = gci.GetColorTableEntry(TextColor::DEFAULT_FOREGROUND);
    VERIFY_ARE_NOT_EQUAL(testColor, newColor);
    // it will, in fact leave the color the way it was
    VERIFY_ARE_EQUAL(originalColor, newColor);
}

void ScreenBufferTests::SetDefaultBackgroundColor()
{
    // Setting the default Background color should work

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.LockConsole(); // Lock must be taken to swap buffers.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    auto& mainBuffer = gci.GetActiveOutputBuffer();
    VERIFY_IS_FALSE(mainBuffer._IsAltBuffer());
    WI_SetFlag(mainBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    VERIFY_IS_TRUE(WI_IsFlagSet(mainBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING));

    auto& stateMachine = mainBuffer.GetStateMachine();

    auto originalColor = gci.GetColorTableEntry(TextColor::DEFAULT_BACKGROUND);
    auto newColor = gci.GetColorTableEntry(TextColor::DEFAULT_BACKGROUND);
    auto testColor = RGB(0x33, 0x66, 0x99);
    VERIFY_ARE_NOT_EQUAL(originalColor, testColor);

    Log::Comment(L"Valid Hexadecimal Notation");
    stateMachine.ProcessString(L"\x1b]11;rgb:33/66/99\x1b\\");

    newColor = gci.GetColorTableEntry(TextColor::DEFAULT_BACKGROUND);
    VERIFY_ARE_EQUAL(testColor, newColor);

    Log::Comment(L"Valid Hexadecimal Notation");
    originalColor = newColor;
    testColor = RGB(0xff, 0xff, 0xff);
    stateMachine.ProcessString(L"\x1b]11;rgb:ff/ff/ff\x1b\\");

    newColor = gci.GetColorTableEntry(TextColor::DEFAULT_BACKGROUND);
    VERIFY_ARE_EQUAL(testColor, newColor);

    Log::Comment(L"Invalid Syntax");
    originalColor = newColor;
    testColor = RGB(153, 102, 51);
    stateMachine.ProcessString(L"\x1b]11;99/66/33\x1b\\");

    newColor = gci.GetColorTableEntry(TextColor::DEFAULT_BACKGROUND);
    VERIFY_ARE_NOT_EQUAL(testColor, newColor);
    // it will, in fact leave the color the way it was
    VERIFY_ARE_EQUAL(originalColor, newColor);
}

void ScreenBufferTests::AssignColorAliases()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& stateMachine = gci.GetActiveOutputBuffer().GetStateMachine();
    auto& renderSettings = gci.GetRenderSettings();

    const auto defaultFg = renderSettings.GetColorAliasIndex(ColorAlias::DefaultForeground);
    const auto defaultBg = renderSettings.GetColorAliasIndex(ColorAlias::DefaultBackground);
    const auto frameFg = renderSettings.GetColorAliasIndex(ColorAlias::FrameForeground);
    const auto frameBg = renderSettings.GetColorAliasIndex(ColorAlias::FrameBackground);

    auto resetAliases = wil::scope_exit([&] {
        renderSettings.SetColorAliasIndex(ColorAlias::DefaultForeground, defaultFg);
        renderSettings.SetColorAliasIndex(ColorAlias::DefaultBackground, defaultBg);
        renderSettings.SetColorAliasIndex(ColorAlias::FrameForeground, frameFg);
        renderSettings.SetColorAliasIndex(ColorAlias::FrameBackground, frameBg);
    });

    Log::Comment(L"Test invalid item color assignment");
    stateMachine.ProcessString(L"\033[0;12;34,|");
    VERIFY_ARE_EQUAL(defaultFg, renderSettings.GetColorAliasIndex(ColorAlias::DefaultForeground));
    VERIFY_ARE_EQUAL(defaultBg, renderSettings.GetColorAliasIndex(ColorAlias::DefaultBackground));
    VERIFY_ARE_EQUAL(frameFg, renderSettings.GetColorAliasIndex(ColorAlias::FrameForeground));
    VERIFY_ARE_EQUAL(frameBg, renderSettings.GetColorAliasIndex(ColorAlias::FrameBackground));

    Log::Comment(L"Test normal text color assignment");
    stateMachine.ProcessString(L"\033[1;23;45,|");
    VERIFY_ARE_EQUAL(23u, renderSettings.GetColorAliasIndex(ColorAlias::DefaultForeground));
    VERIFY_ARE_EQUAL(45u, renderSettings.GetColorAliasIndex(ColorAlias::DefaultBackground));

    Log::Comment(L"Test window frame color assignment");
    stateMachine.ProcessString(L"\033[2;34;56,|");
    VERIFY_ARE_EQUAL(34u, renderSettings.GetColorAliasIndex(ColorAlias::FrameForeground));
    VERIFY_ARE_EQUAL(56u, renderSettings.GetColorAliasIndex(ColorAlias::FrameBackground));
}

void ScreenBufferTests::DeleteCharsNearEndOfLine()
{
    // Created for MSFT:19888564.
    // There are some cases when you DCH N chars, where there are artifacts left
    //       from the previous contents of the row after the DCH finishes.
    // If you are deleting N chars,
    // and there are N+X chars left in the row after the cursor, such that X<N,
    // We'll move the X chars to the left, and delete X chars both at the cursor
    //       pos and at cursor.x+N, but the region of characters at
    //      [cursor.x+X, cursor.x+N] is left untouched.
    //
    // Which is the case:
    // `(d - 1 > v_w - 1 - c_x - d) && (v_w - 1 - c_x - d >= 0)`
    // where:
    // - `d`: num chars to delete
    // - `v_w`: viewport.Width()
    // - `c_x`: cursor.x
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

    VERIFY_ARE_EQUAL(til::point(0, 0), mainCursor.GetPosition());
    VERIFY_ARE_EQUAL(mainBuffer.GetBufferSize().Width(), mainView.Width());
    VERIFY_IS_GREATER_THAN(mainView.Width(), (dx + numCharsToDelete));

    for (auto x = 0; x < mainView.Width(); x++)
    {
        stateMachine.ProcessString(L"X");
    }

    VERIFY_ARE_EQUAL(til::point(mainView.Width() - 1, 0), mainCursor.GetPosition());

    mainCursor.SetPosition({ mainView.Width() - dx, 0 });
    std::wstringstream ss;
    ss << L"\x1b[" << numCharsToDelete << L"P"; // Delete N chars
    stateMachine.ProcessString(ss.str());

    VERIFY_ARE_EQUAL(til::point(mainView.Width() - dx, 0), mainCursor.GetPosition());
    auto iter = tbi.GetCellDataAt({ 0, 0 });
    auto expectedNumSpaces = std::min(dx, numCharsToDelete);
    for (auto x = 0; x < mainView.Width() - expectedNumSpaces; x++)
    {
        SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);
        if (iter->Chars() != L"X")
        {
            Log::Comment(NoThrowString().Format(L"character [%d] was mismatched", x));
        }
        VERIFY_ARE_EQUAL(L"X", iter->Chars());
        iter++;
    }
    for (auto x = mainView.Width() - expectedNumSpaces; x < mainView.Width(); x++)
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

    const til::size newViewSize{ newBufferWidth, mainBuffer.GetViewport().Height() };
    mainBuffer.SetViewportSize(&newViewSize);
    auto& tbi = mainBuffer.GetTextBuffer();
    auto& mainView = mainBuffer.GetViewport();
    auto& mainCursor = tbi.GetCursor();

    VERIFY_ARE_EQUAL(til::point(0, 0), mainCursor.GetPosition());
    VERIFY_ARE_EQUAL(newBufferWidth, mainView.Width());
    VERIFY_ARE_EQUAL(mainBuffer.GetBufferSize().Width(), mainView.Width());

    stateMachine.ProcessString(L"ABCDEFG");

    VERIFY_ARE_EQUAL(til::point(7, 0), mainCursor.GetPosition());
    // Place the cursor on the 'D'
    mainCursor.SetPosition({ 3, 0 });

    // Delete 3 chars - [D, E, F]
    std::wstringstream ss;
    ss << L"\x1b[" << 3 << L"P";
    stateMachine.ProcessString(ss.str());

    // Cursor shouldn't have moved
    VERIFY_ARE_EQUAL(til::point(3, 0), mainCursor.GetPosition());

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

    const til::size newViewSize{ newBufferWidth, mainBuffer.GetViewport().Height() };
    mainBuffer.SetViewportSize(&newViewSize);
    auto& tbi = mainBuffer.GetTextBuffer();
    auto& mainView = mainBuffer.GetViewport();
    auto& mainCursor = tbi.GetCursor();

    VERIFY_ARE_EQUAL(til::point(0, 0), mainCursor.GetPosition());
    VERIFY_ARE_EQUAL(newBufferWidth, mainView.Width());
    VERIFY_ARE_EQUAL(mainBuffer.GetBufferSize().Width(), mainView.Width());

    stateMachine.ProcessString(L"ABCDEFG");

    VERIFY_ARE_EQUAL(til::point(7, 0), mainCursor.GetPosition());

    // Place the cursor on the 'C'
    mainCursor.SetPosition({ 2, 0 });

    // Delete 4 chars - [C, D, E, F]
    std::wstringstream ss;
    ss << L"\x1b[" << 4 << L"P";
    stateMachine.ProcessString(ss.str());

    VERIFY_ARE_EQUAL(til::point(2, 0), mainCursor.GetPosition());

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
    const auto& renderSettings = gci.GetRenderSettings();

    VERIFY_NT_SUCCESS(si.SetViewportOrigin(true, { 0, 1 }, true));
    cursor.SetPosition({ 0, si.GetViewport().BottomInclusive() });
    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<til::point>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"viewport=%s", VerifyOutputTraits<til::inclusive_rect>::ToString(si.GetViewport().ToInclusive()).GetBuffer()));
    const auto darkRed = gci.GetColorTableEntry(TextColor::DARK_RED);
    const auto darkBlue = gci.GetColorTableEntry(TextColor::DARK_BLUE);
    const auto darkBlack = gci.GetColorTableEntry(TextColor::DARK_BLACK);
    const auto darkWhite = gci.GetColorTableEntry(TextColor::DARK_WHITE);
    stateMachine.ProcessString(L"\x1b[31;44m");
    stateMachine.ProcessString(L"X");
    stateMachine.ProcessString(L"\x1b[m");
    stateMachine.ProcessString(L"X");

    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<til::point>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"viewport=%s", VerifyOutputTraits<til::inclusive_rect>::ToString(si.GetViewport().ToInclusive()).GetBuffer()));
    VERIFY_ARE_EQUAL(2, cursor.GetPosition().x);
    {
        const auto& row = tbi.GetRowByOffset(cursor.GetPosition().y);
        const std::vector<TextAttribute> attrs{ row.AttrBegin(), row.AttrEnd() };
        const auto attrA = attrs[0];
        const auto attrB = attrs[1];
        LOG_ATTR(attrA);
        LOG_ATTR(attrB);
        VERIFY_ARE_EQUAL(std::make_pair(darkRed, darkBlue), renderSettings.GetAttributeColors(attrA));

        VERIFY_ARE_EQUAL(std::make_pair(darkWhite, darkBlack), renderSettings.GetAttributeColors(attrB));
    }

    Log::Comment(NoThrowString().Format(L"Emulate scrolling up with the mouse"));
    VERIFY_NT_SUCCESS(si.SetViewportOrigin(true, { 0, 0 }, false));

    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<til::point>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"viewport=%s", VerifyOutputTraits<til::inclusive_rect>::ToString(si.GetViewport().ToInclusive()).GetBuffer()));

    VERIFY_IS_GREATER_THAN(cursor.GetPosition().y, si.GetViewport().BottomInclusive());

    stateMachine.ProcessString(L"X");

    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<til::point>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"viewport=%s", VerifyOutputTraits<til::inclusive_rect>::ToString(si.GetViewport().ToInclusive()).GetBuffer()));

    VERIFY_ARE_EQUAL(3, cursor.GetPosition().x);
    {
        const auto& row = tbi.GetRowByOffset(cursor.GetPosition().y);
        const std::vector<TextAttribute> attrs{ row.AttrBegin(), row.AttrEnd() };
        const auto attrA = attrs[0];
        const auto attrB = attrs[1];
        const auto attrC = attrs[1];
        LOG_ATTR(attrA);
        LOG_ATTR(attrB);
        LOG_ATTR(attrC);
        VERIFY_ARE_EQUAL(std::make_pair(darkRed, darkBlue), renderSettings.GetAttributeColors(attrA));

        VERIFY_ARE_EQUAL(std::make_pair(darkWhite, darkBlack), renderSettings.GetAttributeColors(attrB));

        VERIFY_ARE_EQUAL(std::make_pair(darkWhite, darkBlack), renderSettings.GetAttributeColors(attrC));
    }
}

template<class T>
void _FillLine(til::point position, T fillContent, TextAttribute fillAttr)
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    auto& row = si.GetTextBuffer().GetRowByOffset(position.y);
    row.WriteCells({ fillContent, fillAttr }, position.x, false);
}

template<class T>
void _FillLine(int line, T fillContent, TextAttribute fillAttr)
{
    _FillLine({ 0, line }, fillContent, fillAttr);
}

template<class T>
void _FillLines(int startLine, int endLine, T fillContent, TextAttribute fillAttr)
{
    for (auto line = startLine; line < endLine; ++line)
    {
        _FillLine(line, fillContent, fillAttr);
    }
}

template<class... T>
bool _ValidateLineContains(til::point position, T&&... expectedContent)
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    auto actual = si.GetCellLineDataAt(position);
    auto expected = OutputCellIterator{ std::forward<T>(expectedContent)... };
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
    return _ValidateLineContains({ 0, line }, expectedContent, expectedAttr);
}

template<class T>
auto _ValidateLinesContain(int startCol, int startLine, int endLine, T expectedContent, TextAttribute expectedAttr)
{
    for (auto line = startLine; line < endLine; ++line)
    {
        if (!_ValidateLineContains({ startCol, line }, expectedContent, expectedAttr))
        {
            return false;
        }
    }
    return true;
};

template<class T>
auto _ValidateLinesContain(int startLine, int endLine, T expectedContent, TextAttribute expectedAttr)
{
    return _ValidateLinesContain(0, startLine, endLine, expectedContent, expectedAttr);
}

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

    // Set the attributes that will be used to fill the revealed area.
    auto fillAttr = TextAttribute{ RGB(12, 34, 56), RGB(78, 90, 12) };
    fillAttr.SetCrossedOut(true);
    fillAttr.SetReverseVideo(true);
    fillAttr.SetUnderlined(true);
    si.SetAttributes(fillAttr);
    // But note that the meta attributes are expected to be cleared.
    auto expectedFillAttr = fillAttr;
    expectedFillAttr.SetStandardErase();

    // Place the cursor in the center.
    auto cursorPos = til::point{ bufferWidth / 2, (viewportStart + viewportEnd) / 2 };
    // Unless this is reverse index, which has to be be at the top of the viewport.
    if (scrollType == ReverseIndex)
    {
        cursorPos.y = viewportStart;
    }

    Log::Comment(L"Set the cursor position and perform the operation.");
    VERIFY_SUCCEEDED(si.SetCursorPosition(cursorPos, true));
    stateMachine.ProcessString(escapeSequence.str());

    // The cursor shouldn't move.
    auto expectedCursorPos = cursorPos;
    // Unless this is an IL or DL control, which moves the cursor to the left margin.
    if (scrollType == InsertLine || scrollType == DeleteLine)
    {
        expectedCursorPos.x = 0;
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
    const auto scrollStart = (scrollType == InsertLine || scrollType == DeleteLine) ? cursorPos.y : viewportStart;

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
    viewportLine += insertedLines; // Lines skipped when inserting
    while (viewportLine < viewportEnd - deletedLines)
    {
        VERIFY_IS_TRUE(_ValidateLineContains(viewportLine++, viewportChar++, viewportAttr));
    }

    Log::Comment(L"The revealed area should now be blank, with standard erase attributes.");
    const auto revealedStart = scrollDirection == Up ? viewportEnd - deletedLines : scrollStart;
    const auto revealedEnd = revealedStart + scrollMagnitude;
    VERIFY_IS_TRUE(_ValidateLinesContain(revealedStart, revealedEnd, L' ', expectedFillAttr));
}

void ScreenBufferTests::InsertReplaceMode()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    auto& stateMachine = si.GetStateMachine();
    WI_SetFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    const auto bufferHeight = si.GetBufferSize().Height();
    const auto viewport = si.GetViewport();
    const auto targetRow = viewport.Top() + 5;
    const auto targetCol = til::CoordType{ 10 };

    // Fill the entire buffer with Zs. Blue on Green.
    const auto bufferChar = L'Z';
    const auto bufferAttr = TextAttribute{ FOREGROUND_BLUE | BACKGROUND_GREEN };
    _FillLines(0, bufferHeight, bufferChar, bufferAttr);

    // Fill the target row with asterisks and a range of letters at the start. Red on Blue.
    const auto initialChars = L"ABCDEFGHIJKLMNOPQRST";
    const auto initialAttr = TextAttribute{ FOREGROUND_RED | BACKGROUND_BLUE };
    _FillLine(targetRow, L'*', initialAttr);
    _FillLine(targetRow, initialChars, initialAttr);

    // Set the attributes that will be used for the new content.
    auto newAttr = TextAttribute{ RGB(12, 34, 56), RGB(78, 90, 12) };
    newAttr.SetCrossedOut(true);
    newAttr.SetReverseVideo(true);
    newAttr.SetUnderlined(true);
    si.SetAttributes(newAttr);

    Log::Comment(L"Write additional content into a line of text with IRM mode enabled.");

    // Set the cursor position partway through the target row.
    VERIFY_SUCCEEDED(si.SetCursorPosition({ targetCol, targetRow }, true));
    // Enable Insert/Replace mode.
    stateMachine.ProcessString(L"\033[4h");
    // Write out some new content.
    const auto newChars = L"12345";
    stateMachine.ProcessString(newChars);

    VERIFY_IS_TRUE(_ValidateLineContains({ 0, targetRow }, L"ABCDEFGHIJ", initialAttr),
                   L"First half of the line should remain unchanged.");
    VERIFY_IS_TRUE(_ValidateLineContains({ targetCol, targetRow }, newChars, newAttr),
                   L"New content should be inserted at the cursor position with active attributes.");
    VERIFY_IS_TRUE(_ValidateLineContains({ targetCol + 5, targetRow }, L"KLMNOPQRST", initialAttr),
                   L"Second half of the line should have moved 5 columns across.");
    VERIFY_IS_TRUE(_ValidateLineContains({ targetCol + 25, targetRow }, L'*', initialAttr),
                   L"With the remainder of the line filled with asterisks.");
    VERIFY_IS_TRUE(_ValidateLineContains(targetRow + 1, bufferChar, bufferAttr),
                   L"The following line should be unaffected.");

    // Fill the target row with the initial content again.
    _FillLine(targetRow, L'*', initialAttr);
    _FillLine(targetRow, initialChars, initialAttr);

    Log::Comment(L"Write additional content into a line of text with IRM mode disabled.");

    // Set the cursor position partway through the target row.
    VERIFY_SUCCEEDED(si.SetCursorPosition({ targetCol, targetRow }, true));
    // Disable Insert/Replace mode.
    stateMachine.ProcessString(L"\033[4l");
    // Write out some new content.
    stateMachine.ProcessString(newChars);

    VERIFY_IS_TRUE(_ValidateLineContains({ 0, targetRow }, L"ABCDEFGHIJ", initialAttr),
                   L"First half of the line should remain unchanged.");
    VERIFY_IS_TRUE(_ValidateLineContains({ targetCol, targetRow }, newChars, newAttr),
                   L"New content should be added at the cursor position with active attributes.");
    VERIFY_IS_TRUE(_ValidateLineContains({ targetCol + 5, targetRow }, L"PQRST", initialAttr),
                   L"Second half of the line should have been partially overwritten.");
    VERIFY_IS_TRUE(_ValidateLineContains({ targetCol + 25, targetRow }, L'*', initialAttr),
                   L"With the remainder of the line filled with asterisks.");
    VERIFY_IS_TRUE(_ValidateLineContains(targetRow + 1, bufferChar, bufferAttr),
                   L"The following line should be unaffected.");
}

void ScreenBufferTests::InsertChars()
{
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:setVerticalMargins", L"{false, true}")
    END_TEST_METHOD_PROPERTIES();

    bool setVerticalMargins;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"setVerticalMargins", setVerticalMargins));

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

    // Tests are run both with and without the DECSTBM margins set. And while they
    // don't affect the ICH operation directly, when we're outside the vertical
    // margins, the horizontal margins won't apply, and that does affect ICH.
    const auto horizontalMarginsActive = !setVerticalMargins;
    stateMachine.ProcessString(L"\x1b[?69h");
    stateMachine.ProcessString(L"\x1b[11;30s");
    stateMachine.ProcessString(setVerticalMargins ? L"\x1b[15;20r" : L"\x1b[r");
    // Make sure we clear the margins on exit so they can't break other tests.
    auto clearMargins = wil::scope_exit([&] {
        stateMachine.ProcessString(L"\x1b[r");
        stateMachine.ProcessString(L"\x1b[s");
        stateMachine.ProcessString(L"\x1b[?69l");
    });

    Log::Comment(
        L"Test 1: Fill the line with Qs. Write some text within the viewport boundaries. "
        L"Then insert 5 spaces at the cursor. Watch spaces get inserted, text slides right "
        L"up to the right margin or off the edge of the buffer.");

    const auto insertLine = til::CoordType{ 10 };
    auto insertPos = til::CoordType{ 20 };

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

    // Set the attributes that will be used to fill the revealed area.
    auto fillAttr = TextAttribute{ RGB(12, 34, 56), RGB(78, 90, 12) };
    fillAttr.SetCrossedOut(true);
    fillAttr.SetReverseVideo(true);
    fillAttr.SetUnderlined(true);
    si.SetAttributes(fillAttr);
    // But note that the meta attributes are expected to be cleared.
    auto expectedFillAttr = fillAttr;
    expectedFillAttr.SetStandardErase();

    // Insert 5 spaces at the cursor position.
    // Before: QQQQQQQQQQABCDEFGHIJKLMNOPQRSTQQQQQQQQQQ
    //  After: QQQQQQQQQQABCDEFGHIJ     KLMNOQQQQQQQQQQ (horizontal margins active)
    //  After: QQQQQQQQQQABCDEFGHIJ     KLMNOPQRSTQQQQQ (horizontal margins inactive)
    Log::Comment(L"Inserting 5 spaces in the middle of the line.");
    auto before = si.GetTextBuffer().GetRowByOffset(insertLine).GetText();
    stateMachine.ProcessString(L"\x1b[5@");
    auto after = si.GetTextBuffer().GetRowByOffset(insertLine).GetText();

    // Verify cursor didn't move.
    VERIFY_ARE_EQUAL(expectedCursor, cursor.GetPosition(), L"Verify cursor didn't move from insert operation.");

    // Verify the updated structure of the line.
    VERIFY_IS_TRUE(_ValidateLineContains({ 0, insertLine }, L"QQQQQQQQQQ", bufferAttr),
                   L"Field of Qs left of the viewport should remain unchanged.");
    VERIFY_IS_TRUE(_ValidateLineContains({ viewportStart, insertLine }, L"ABCDEFGHIJ", textAttr),
                   L"First half of the alphabet should remain unchanged.");
    VERIFY_IS_TRUE(_ValidateLineContains({ insertPos, insertLine }, L"     ", expectedFillAttr),
                   L"Spaces should be inserted with standard erase attributes at the cursor position.");

    if (horizontalMarginsActive)
    {
        VERIFY_IS_TRUE(_ValidateLineContains({ insertPos + 5, insertLine }, L"KLMNO", textAttr),
                       L"Second half of the alphabet should have moved to the right but only up to right margin.");
        VERIFY_IS_TRUE(_ValidateLineContains({ viewportEnd, insertLine }, L"QQQQQ", bufferAttr),
                       L"Field of Qs right of the margin area should remain unchanged.");
    }
    else
    {
        VERIFY_IS_TRUE(_ValidateLineContains({ insertPos + 5, insertLine }, L"KLMNOPQRST", textAttr),
                       L"Second half of the alphabet should have moved to the right by the number of spaces inserted.");
        VERIFY_IS_TRUE(_ValidateLineContains({ viewportEnd + 5, insertLine }, L"QQQQQ", bufferAttr),
                       L"Field of Qs right of the viewport should be moved right, half pushed outside the buffer.");
    }

    Log::Comment(
        L"Test 2: Inserting at the exact end of the scroll area. Same line structure. "
        L"Move cursor to right edge of window or right margin and insert > 1 space. "
        L"Only 1 should be inserted, everything else unchanged.");

    // Move cursor to right edge.
    insertPos = horizontalMarginsActive ? viewportEnd - 1 : bufferWidth - 1;
    VERIFY_SUCCEEDED(si.SetCursorPosition({ insertPos, insertLine }, true));
    expectedCursor = cursor.GetPosition();

    // Fill the entire line with Qs. Blue on Green.
    _FillLine(insertLine, bufferChar, bufferAttr);

    // Fill the viewport range with text. Red on Blue.
    _FillLine({ viewportStart, insertLine }, textChars, textAttr);

    // Insert 5 spaces at the right edge. Only 1 should be inserted.
    // Before: QQQQQQQQQQABCDEFGHIJKLMNOPQRSTQQQQQQQQQQ
    //  After: QQQQQQQQQQABCDEFGHIJKLMNOPQRS QQQQQQQQQQ (horizontal margins active)
    //  After: QQQQQQQQQQABCDEFGHIJKLMNOPQRSTQQQQQQQQQ  (horizontal margins inactive)
    Log::Comment(L"Inserting 5 spaces at the right edge.");
    before = si.GetTextBuffer().GetRowByOffset(insertLine).GetText();
    stateMachine.ProcessString(L"\x1b[5@");
    after = si.GetTextBuffer().GetRowByOffset(insertLine).GetText();

    // Verify cursor didn't move.
    VERIFY_ARE_EQUAL(expectedCursor, cursor.GetPosition(), L"Verify cursor didn't move from insert operation.");

    // Verify the updated structure of the line.
    VERIFY_IS_TRUE(_ValidateLineContains({ 0, insertLine }, L"QQQQQQQQQQ", bufferAttr),
                   L"Field of Qs left of the viewport should remain unchanged.");
    VERIFY_IS_TRUE(_ValidateLineContains({ insertPos, insertLine }, L" ", expectedFillAttr),
                   L"One space should be inserted with standard erase attributes at the cursor position.");

    if (horizontalMarginsActive)
    {
        VERIFY_IS_TRUE(_ValidateLineContains({ viewportStart, insertLine }, L"ABCDEFGHIJKLMNOPQRS", textAttr),
                       L"Viewport range should remain unchanged except for the last spot.");
        VERIFY_IS_TRUE(_ValidateLineContains({ viewportEnd, insertLine }, L"QQQQQQQQQQ", bufferAttr),
                       L"Field of Qs right of the viewport should remain unchanged.");
    }
    else
    {
        VERIFY_IS_TRUE(_ValidateLineContains({ viewportStart, insertLine }, L"ABCDEFGHIJKLMNOPQRST", textAttr),
                       L"Entire viewport range should remain unchanged.");
        VERIFY_IS_TRUE(_ValidateLineContains({ viewportEnd, insertLine }, L"QQQQQQQQQ", bufferAttr),
                       L"Field of Qs right of the viewport should remain unchanged except for the last spot.");
    }

    Log::Comment(
        L"Test 3: Inserting at the beginning of the scroll area. Same line structure. "
        L"Move cursor to left edge of buffer or left margin and insert > buffer width of space. "
        L"The whole scroll area should be replaced with spaces.");

    // Move cursor to left edge.
    insertPos = horizontalMarginsActive ? viewportStart : 0;
    VERIFY_SUCCEEDED(si.SetCursorPosition({ insertPos, insertLine }, true));
    expectedCursor = cursor.GetPosition();

    // Fill the entire line with Qs. Blue on Green.
    _FillLine(insertLine, bufferChar, bufferAttr);

    // Fill the viewport range with text. Red on Blue.
    _FillLine({ viewportStart, insertLine }, textChars, textAttr);

    // Insert greater than the buffer width at the left edge.
    // Before: QQQQQQQQQQABCDEFGHIJKLMNOPQRSTQQQQQQQQQQ
    //  After: QQQQQQQQQQ                    QQQQQQQQQQ (horizontal margins active)
    //  After:                                          (horizontal margins inactive)
    Log::Comment(L"Inserting 100 spaces at the left edge.");
    before = si.GetTextBuffer().GetRowByOffset(insertLine).GetText();
    stateMachine.ProcessString(L"\x1b[100@");
    after = si.GetTextBuffer().GetRowByOffset(insertLine).GetText();

    // Verify cursor didn't move.
    VERIFY_ARE_EQUAL(expectedCursor, cursor.GetPosition(), L"Verify cursor didn't move from insert operation.");

    // Verify the updated structure of the line.
    if (horizontalMarginsActive)
    {
        VERIFY_IS_TRUE(_ValidateLineContains({ 0, insertLine }, L"QQQQQQQQQQ", bufferAttr),
                       L"Field of Qs left of the viewport should remain unchanged.");
        VERIFY_IS_TRUE(_ValidateLineContains({ insertPos, insertLine }, L"                    ", expectedFillAttr),
                       L"Entire viewport range should be erased with spaces.");
        VERIFY_IS_TRUE(_ValidateLineContains({ viewportEnd, insertLine }, L"QQQQQQQQQQ", bufferAttr),
                       L"Field of Qs right of the viewport should remain unchanged.");
    }
    else
    {
        VERIFY_IS_TRUE(_ValidateLineContains(insertLine, L' ', expectedFillAttr),
                       L"A whole line of spaces was inserted at the start, erasing the line.");
    }
}

void ScreenBufferTests::DeleteChars()
{
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:setVerticalMargins", L"{false, true}")
    END_TEST_METHOD_PROPERTIES();

    bool setVerticalMargins;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"setVerticalMargins", setVerticalMargins));

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

    // Tests are run both with and without the DECSTBM margins set. And while they
    // don't affect the DCH operation directly, when we're outside the vertical
    // margins, the horizontal margins won't apply, and that does affect DCH.
    const auto horizontalMarginsActive = !setVerticalMargins;
    stateMachine.ProcessString(L"\x1b[?69h");
    stateMachine.ProcessString(L"\x1b[11;30s");
    stateMachine.ProcessString(setVerticalMargins ? L"\x1b[15;20r" : L"\x1b[r");
    // Make sure we clear the margins on exit so they can't break other tests.
    auto clearMargins = wil::scope_exit([&] {
        stateMachine.ProcessString(L"\x1b[r");
        stateMachine.ProcessString(L"\x1b[s");
        stateMachine.ProcessString(L"\x1b[?69l");
    });

    Log::Comment(
        L"Test 1: Fill the line with Qs. Write some text within the viewport boundaries. "
        L"Then delete 5 characters at the cursor. Watch the rest of the line slide left, "
        L"with spaces inserted at the end of the line or the right margin.");

    const auto deleteLine = til::CoordType{ 10 };
    auto deletePos = til::CoordType{ 20 };

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

    // Set the attributes that will be used to fill the revealed area.
    auto fillAttr = TextAttribute{ RGB(12, 34, 56), RGB(78, 90, 12) };
    fillAttr.SetCrossedOut(true);
    fillAttr.SetReverseVideo(true);
    fillAttr.SetUnderlined(true);
    si.SetAttributes(fillAttr);
    // But note that the meta attributes are expected to be cleared.
    auto expectedFillAttr = fillAttr;
    expectedFillAttr.SetStandardErase();

    // Delete 5 characters at the cursor position.
    // Before: QQQQQQQQQQABCDEFGHIJKLMNOPQRSTQQQQQQQQQQ
    //  After: QQQQQQQQQQABCDEFGHIJPQRST     QQQQQQQQQQ (horizontal margins active)
    //  After: QQQQQQQQQQABCDEFGHIJPQRSTQQQQQQQQQQ      (horizontal margins inactive)
    Log::Comment(L"Deleting 5 characters in the middle of the line.");
    auto before = si.GetTextBuffer().GetRowByOffset(deleteLine).GetText();
    stateMachine.ProcessString(L"\x1b[5P");
    auto after = si.GetTextBuffer().GetRowByOffset(deleteLine).GetText();

    // Verify cursor didn't move.
    VERIFY_ARE_EQUAL(expectedCursor, cursor.GetPosition(), L"Verify cursor didn't move from delete operation.");

    // Verify the updated structure of the line.
    VERIFY_IS_TRUE(_ValidateLineContains({ 0, deleteLine }, L"QQQQQQQQQQ", bufferAttr),
                   L"Field of Qs left of the viewport should remain unchanged.");
    VERIFY_IS_TRUE(_ValidateLineContains({ viewportStart, deleteLine }, L"ABCDEFGHIJ", textAttr),
                   L"First half of the alphabet should remain unchanged.");
    VERIFY_IS_TRUE(_ValidateLineContains({ deletePos, deleteLine }, L"PQRST", textAttr),
                   L"Only half of the second part of the alphabet remains.");

    if (horizontalMarginsActive)
    {
        VERIFY_IS_TRUE(_ValidateLineContains({ viewportEnd - 5, deleteLine }, L"     ", expectedFillAttr),
                       L"The rest of the margin area should be replaced with spaces with standard erase attributes.");
        VERIFY_IS_TRUE(_ValidateLineContains({ viewportEnd, deleteLine }, L"QQQQQQQQQQ", bufferAttr),
                       L"Field of Qs right of the viewport should remain unchanged.");
    }
    else
    {
        VERIFY_IS_TRUE(_ValidateLineContains({ viewportEnd - 5, deleteLine }, L"QQQQQQQQQQ", bufferAttr),
                       L"Field of Qs right of the viewport should be moved left.");
        VERIFY_IS_TRUE(_ValidateLineContains({ bufferWidth - 5, deleteLine }, L"     ", expectedFillAttr),
                       L"The rest of the line should be replaced with spaces with standard erase attributes.");
    }

    Log::Comment(
        L"Test 2: Deleting at the exact end of the scroll area. Same line structure. "
        L"Move cursor to right edge of window or right margin and delete > 1 character. "
        L"Only 1 should be deleted, everything else unchanged.");

    // Move cursor to right edge.
    deletePos = horizontalMarginsActive ? viewportEnd - 1 : bufferWidth - 1;
    VERIFY_SUCCEEDED(si.SetCursorPosition({ deletePos, deleteLine }, true));
    expectedCursor = cursor.GetPosition();

    // Fill the entire line with Qs. Blue on Green.
    _FillLine(deleteLine, bufferChar, bufferAttr);

    // Fill the viewport range with text. Red on Blue.
    _FillLine({ viewportStart, deleteLine }, textChars, textAttr);

    // Delete 5 characters at the right edge. Only 1 should be deleted.
    // Before: QQQQQQQQQQABCDEFGHIJKLMNOPQRSTQQQQQQQQQQ
    //  After: QQQQQQQQQQABCDEFGHIJKLMNOPQRS QQQQQQQQQQ (horizontal margins active)
    //  After: QQQQQQQQQQABCDEFGHIJKLMNOPQRSTQQQQQQQQQ  (horizontal margins inactive)
    Log::Comment(L"Deleting 5 characters at the right edge.");
    before = si.GetTextBuffer().GetRowByOffset(deleteLine).GetText();
    stateMachine.ProcessString(L"\x1b[5P");
    after = si.GetTextBuffer().GetRowByOffset(deleteLine).GetText();

    // Verify cursor didn't move.
    VERIFY_ARE_EQUAL(expectedCursor, cursor.GetPosition(), L"Verify cursor didn't move from delete operation.");

    // Verify the updated structure of the line.
    VERIFY_IS_TRUE(_ValidateLineContains({ 0, deleteLine }, L"QQQQQQQQQQ", bufferAttr),
                   L"Field of Qs left of the viewport should remain unchanged.");
    VERIFY_IS_TRUE(_ValidateLineContains({ deletePos, deleteLine }, L" ", expectedFillAttr),
                   L"One character should be erased with standard erase attributes at the cursor position.");

    if (horizontalMarginsActive)
    {
        VERIFY_IS_TRUE(_ValidateLineContains({ viewportStart, deleteLine }, L"ABCDEFGHIJKLMNOPQRS", textAttr),
                       L"Viewport range should remain unchanged except for the last spot.");
        VERIFY_IS_TRUE(_ValidateLineContains({ viewportEnd, deleteLine }, L"QQQQQQQQQQ", bufferAttr),
                       L"Field of Qs right of the viewport should remain unchanged.");
    }
    else
    {
        VERIFY_IS_TRUE(_ValidateLineContains({ viewportStart, deleteLine }, L"ABCDEFGHIJKLMNOPQRST", textAttr),
                       L"Entire viewport range should remain unchanged.");
        VERIFY_IS_TRUE(_ValidateLineContains({ viewportEnd, deleteLine }, L"QQQQQQQQQ", bufferAttr),
                       L"Field of Qs right of the viewport should remain unchanged except for the last spot.");
    }

    Log::Comment(
        L"Test 3: Deleting at the beginning of the scroll area. Same line structure. "
        L"Move cursor to left edge of buffer or left margin and delete > buffer width of characters. "
        L"The whole scroll area should be replaced with spaces.");

    // Move cursor to left edge.
    deletePos = horizontalMarginsActive ? viewportStart : 0;
    VERIFY_SUCCEEDED(si.SetCursorPosition({ deletePos, deleteLine }, true));
    expectedCursor = cursor.GetPosition();

    // Fill the entire line with Qs. Blue on Green.
    _FillLine(deleteLine, bufferChar, bufferAttr);

    // Fill the viewport range with text. Red on Blue.
    _FillLine({ viewportStart, deleteLine }, textChars, textAttr);

    // Delete greater than the buffer width at the left edge.
    // Before: QQQQQQQQQQABCDEFGHIJKLMNOPQRSTQQQQQQQQQQ
    //  After: QQQQQQQQQQ                    QQQQQQQQQQ (horizontal margins active)
    //  After:                                          (horizontal margins inactive)
    Log::Comment(L"Deleting 100 characters at the left edge.");
    before = si.GetTextBuffer().GetRowByOffset(deleteLine).GetText();
    stateMachine.ProcessString(L"\x1b[100P");
    after = si.GetTextBuffer().GetRowByOffset(deleteLine).GetText();

    // Verify cursor didn't move.
    VERIFY_ARE_EQUAL(expectedCursor, cursor.GetPosition(), L"Verify cursor didn't move from delete operation.");

    // Verify the updated structure of the line.
    if (horizontalMarginsActive)
    {
        VERIFY_IS_TRUE(_ValidateLineContains({ 0, deleteLine }, L"QQQQQQQQQQ", bufferAttr),
                       L"Field of Qs left of the viewport should remain unchanged.");
        VERIFY_IS_TRUE(_ValidateLineContains({ deletePos, deleteLine }, L"                    ", expectedFillAttr),
                       L"Entire viewport range should be erased with spaces.");
        VERIFY_IS_TRUE(_ValidateLineContains({ viewportEnd, deleteLine }, L"QQQQQQQQQQ", bufferAttr),
                       L"Field of Qs right of the viewport should remain unchanged.");
    }
    else
    {
        VERIFY_IS_TRUE(_ValidateLineContains(deleteLine, L' ', expectedFillAttr),
                       L"A whole line of spaces was inserted from the right, erasing the line.");
    }
}

void ScreenBufferTests::HorizontalScrollOperations()
{
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:op", L"{0, 1, 2, 3}")
    END_TEST_METHOD_PROPERTIES();

    enum Op : int
    {
        DECIC,
        DECDC,
        DECFI,
        DECBI
    } op;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"op", (int&)op));

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

    // Set the margin area to columns 10 to 29 and rows 14 to 19 (zero based).
    stateMachine.ProcessString(L"\x1b[?69h");
    stateMachine.ProcessString(L"\x1b[11;30s");
    stateMachine.ProcessString(L"\x1b[15;20r");
    // Make sure we clear the margins on exit so they can't break other tests.
    auto clearMargins = wil::scope_exit([&] {
        stateMachine.ProcessString(L"\x1b[r");
        stateMachine.ProcessString(L"\x1b[s");
        stateMachine.ProcessString(L"\x1b[?69l");
    });

    // Fill the buffer with text. Red on Blue.
    const auto bufferChars = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmn";
    const auto bufferAttr = TextAttribute{ FOREGROUND_RED | BACKGROUND_BLUE };
    _FillLines(0, 25, bufferChars, bufferAttr);

    // Set the attributes that will be used to fill the revealed area.
    auto fillAttr = TextAttribute{ RGB(12, 34, 56), RGB(78, 90, 12) };
    fillAttr.SetCrossedOut(true);
    fillAttr.SetReverseVideo(true);
    fillAttr.SetUnderlined(true);
    si.SetAttributes(fillAttr);
    // But note that the meta attributes are expected to be cleared.
    auto expectedFillAttr = fillAttr;
    expectedFillAttr.SetStandardErase();

    auto& cursor = si.GetTextBuffer().GetCursor();
    switch (op)
    {
    case DECIC:
        Log::Comment(L"Insert 4 columns in the middle of the margin area.");
        cursor.SetPosition({ 20, 17 });
        stateMachine.ProcessString(L"\x1b[4'}");
        VERIFY_ARE_EQUAL(til::point(20, 17), cursor.GetPosition(), L"The cursor should not move.");
        VERIFY_IS_TRUE(_ValidateLinesContain(10, 14, 20, L"KLMNOPQRST", bufferAttr),
                       L"The margin area left of the cursor position should remain unchanged.");
        VERIFY_IS_TRUE(_ValidateLinesContain(20, 14, 20, L"    ", expectedFillAttr),
                       L"4 blank columns should be inserted at the cursor position.");
        VERIFY_IS_TRUE(_ValidateLinesContain(24, 14, 20, L"UVWXYZ", bufferAttr),
                       L"The area right of that should be scrolled right by 4 columns.");
        break;
    case DECDC:
        Log::Comment(L"Delete 4 columns in the middle of the margin area.");
        cursor.SetPosition({ 20, 17 });
        stateMachine.ProcessString(L"\x1b[4'~");
        VERIFY_ARE_EQUAL(til::point(20, 17), cursor.GetPosition(), L"The cursor should not move.");
        VERIFY_IS_TRUE(_ValidateLinesContain(10, 14, 20, L"KLMNOPQRSTYZabcd", bufferAttr),
                       L"The area right of the cursor position should be scrolled left by 4 columns.");
        VERIFY_IS_TRUE(_ValidateLinesContain(26, 14, 20, L"    ", expectedFillAttr),
                       L"4 blank columns should be inserted at the right of the margin area.");
        break;
    case DECFI:
        Log::Comment(L"Forward index 4 times, 2 columns before the right margin.");
        cursor.SetPosition({ 27, 17 });
        stateMachine.ProcessString(L"\x1b\x39\x1b\x39\x1b\x39\x1b\x39");
        VERIFY_ARE_EQUAL(til::point(29, 17), cursor.GetPosition(), L"The cursor should not pass the right margin.");
        VERIFY_IS_TRUE(_ValidateLinesContain(10, 14, 20, L"MNOPQRSTUVWXYZabcd", bufferAttr),
                       L"The margin area should scroll left by 2 columns.");
        VERIFY_IS_TRUE(_ValidateLinesContain(28, 14, 20, L"  ", expectedFillAttr),
                       L"2 blank columns should be inserted at the right of the margin area.");
        break;
    case DECBI:
        Log::Comment(L"Back index 4 times, 2 columns before the left margin.");
        cursor.SetPosition({ 12, 17 });
        stateMachine.ProcessString(L"\x1b\x36\x1b\x36\x1b\x36\x1b\x36");
        VERIFY_ARE_EQUAL(til::point(10, 17), cursor.GetPosition(), L"The cursor should not pass the left margin.");
        VERIFY_IS_TRUE(_ValidateLinesContain(10, 14, 20, L"  ", expectedFillAttr),
                       L"2 blank columns should be inserted at the left of the margin area.");
        VERIFY_IS_TRUE(_ValidateLinesContain(12, 14, 20, L"KLMNOPQRSTUVWXYZab", bufferAttr),
                       L"The rest of the margin area should scroll right by 2 columns.");
        break;
    }

    VERIFY_IS_TRUE(_ValidateLinesContain(0, 14, bufferChars, bufferAttr),
                   L"Content above the top margin should remain unchanged.");
    VERIFY_IS_TRUE(_ValidateLinesContain(20, 25, bufferChars, bufferAttr),
                   L"Content below the bottom margin should remain unchanged.");
    VERIFY_IS_TRUE(_ValidateLinesContain(0, 14, 20, L"ABCDEFGHIJ", bufferAttr),
                   L"Content before the left margin should remain unchanged.");
    VERIFY_IS_TRUE(_ValidateLinesContain(30, 14, 20, L"efghijklmn", bufferAttr),
                   L"Content beyond the right margin should remain unchanged.");
}

void ScreenBufferTests::ScrollingWideCharsHorizontally()
{
    // The point of this test is to make sure wide characters can be
    // moved horizontally by one cell without erasing themselves.

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    auto& stateMachine = si.GetStateMachine();
    WI_SetFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    const auto viewport = si.GetViewport();
    const auto testRow = viewport.Top();

    Log::Comment(L"Fill the test row with content containing wide chars");
    const auto testChars = L"こんにちは World";
    const auto testAttr = TextAttribute{ FOREGROUND_RED | BACKGROUND_BLUE };
    _FillLine(testRow, testChars, testAttr);

    Log::Comment(L"Position the cursor at the start of the test row");
    VERIFY_SUCCEEDED(si.SetCursorPosition({ 0, testRow }, true));

    Log::Comment(L"Insert 1 cell at the start of the test row");
    stateMachine.ProcessString(L"\033[@");
    VERIFY_IS_TRUE(_ValidateLineContains({ 1, testRow }, testChars, testAttr));

    Log::Comment(L"Delete 1 cell from the start of the test row");
    stateMachine.ProcessString(L"\033[P");
    VERIFY_IS_TRUE(_ValidateLineContains(testRow, testChars, testAttr));

    Log::Comment(L"Copy the test row 1 cell to the right");
    stateMachine.ProcessString(L"\033[1;1;1;;;1;2$v");
    VERIFY_IS_TRUE(_ValidateLineContains({ 1, testRow }, testChars, testAttr));

    Log::Comment(L"Copy the test row 1 cell to the left");
    stateMachine.ProcessString(L"\033[1;2;1;;;1;1$v");
    VERIFY_IS_TRUE(_ValidateLineContains(testRow, testChars, testAttr));

    Log::Comment(L"Scroll the test row 1 cell to the right");
    const auto testRect = til::inclusive_rect{ 0, testRow, viewport.Width() - 2, testRow };
    ScrollRegion(si, testRect, std::nullopt, { 1, testRow }, L' ', testAttr);
    VERIFY_IS_TRUE(_ValidateLineContains({ 1, testRow }, testChars, testAttr));

    Log::Comment(L"Scroll the test row 1 cell to the left");
    const auto testRect2 = til::inclusive_rect{ 1, testRow, viewport.Width() - 1, testRow };
    ScrollRegion(si, testRect2, std::nullopt, { 0, testRow }, L' ', testAttr);
    VERIFY_IS_TRUE(_ValidateLineContains(testRow, testChars, testAttr));
}

void ScreenBufferTests::EraseScrollbackTests()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    auto& stateMachine = si.GetStateMachine();
    const auto& cursor = si.GetTextBuffer().GetCursor();
    WI_SetFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    const auto bufferWidth = si.GetBufferSize().Width();
    const auto bufferHeight = si.GetBufferSize().Height();

    // Move the viewport down a few lines, and only cover part of the buffer width.
    si.SetViewport(Viewport::FromDimensions({ 5, 10 }, { bufferWidth - 10, 10 }), true);
    const auto viewport = si.GetViewport();

    // Fill the entire buffer with Zs. Blue on Green.
    const auto bufferChar = L'Z';
    const auto bufferAttr = TextAttribute{ FOREGROUND_BLUE | BACKGROUND_GREEN };
    _FillLines(0, bufferHeight, bufferChar, bufferAttr);

    // Fill the viewport with a range of letters to see if they move. Red on Blue.
    const auto viewportAttr = TextAttribute{ FOREGROUND_RED | BACKGROUND_BLUE };
    auto viewportChar = L'A';
    auto viewportLine = viewport.Top();
    while (viewportLine < viewport.BottomExclusive())
    {
        _FillLine(viewportLine++, viewportChar++, viewportAttr);
    }

    // Set the colors to Green on Red. This should have no effect on the results.
    si.SetAttributes(TextAttribute{ FOREGROUND_GREEN | BACKGROUND_RED });

    // Place the cursor in the center.
    const auto centerX = bufferWidth / 2;
    const auto centerY = (si.GetViewport().Top() + si.GetViewport().BottomExclusive()) / 2;
    const auto cursorPos = til::point{ centerX, centerY };

    Log::Comment(L"Set the cursor position and erase the scrollback.");
    VERIFY_SUCCEEDED(si.SetCursorPosition(cursorPos, true));
    stateMachine.ProcessString(L"\x1b[3J");

    // The viewport should move to the top of the buffer, while the cursor
    // maintains the same relative position.
    const auto expectedOffset = til::point{ 0, -viewport.Top() };
    const auto expectedViewport = Viewport::Offset(viewport, expectedOffset);
    const auto expectedCursorPos = til::point{ cursorPos.x, cursorPos.y + expectedOffset.y };

    Log::Comment(L"Verify expected viewport.");
    VERIFY_ARE_EQUAL(expectedViewport, si.GetViewport());

    Log::Comment(L"Verify expected cursor position.");
    VERIFY_ARE_EQUAL(expectedCursorPos, cursor.GetPosition());

    Log::Comment(L"Viewport contents should have moved to the new location.");
    viewportChar = L'A';
    viewportLine = expectedViewport.Top();
    while (viewportLine < expectedViewport.BottomExclusive())
    {
        VERIFY_IS_TRUE(_ValidateLineContains(viewportLine++, viewportChar++, viewportAttr));
    }

    Log::Comment(L"The rest of the buffer should be cleared with default attributes.");
    VERIFY_IS_TRUE(_ValidateLinesContain(viewportLine, bufferHeight, L' ', TextAttribute{}));
}

void ScreenBufferTests::EraseTests()
{
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:eraseType", L"{0, 1, 2}") // corresponds to options in DispatchTypes::EraseType
        TEST_METHOD_PROPERTY(L"Data:eraseScreen", L"{false, true}") // corresponds to Line (false) or Screen (true)
        TEST_METHOD_PROPERTY(L"Data:selectiveErase", L"{false, true}")
    END_TEST_METHOD_PROPERTIES()

    int eraseTypeValue;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"eraseType", eraseTypeValue));
    bool eraseScreen;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"eraseScreen", eraseScreen));
    bool selectiveErase;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"selectiveErase", selectiveErase));

    const auto eraseType = gsl::narrow<DispatchTypes::EraseType>(eraseTypeValue);

    std::wstringstream escapeSequence;
    escapeSequence << "\x1b[";

    if (selectiveErase)
    {
        Log::Comment(L"Erasing unprotected cells only.");
        escapeSequence << "?";
    }

    switch (eraseType)
    {
    case DispatchTypes::EraseType::ToEnd:
        Log::Comment(L"Erasing line from cursor to end.");
        escapeSequence << "0";
        break;
    case DispatchTypes::EraseType::FromBeginning:
        Log::Comment(L"Erasing line from beginning to cursor.");
        escapeSequence << "1";
        break;
    case DispatchTypes::EraseType::All:
        Log::Comment(L"Erasing all.");
        escapeSequence << "2";
        break;
    default:
        VERIFY_FAIL(L"Unsupported erase type.");
    }

    if (!eraseScreen)
    {
        Log::Comment(L"Erasing just one line (the cursor's line).");
        escapeSequence << "K";
    }
    else
    {
        Log::Comment(L"Erasing entire display (viewport). May be bounded by the cursor.");
        escapeSequence << "J";
    }

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    auto& stateMachine = si.GetStateMachine();
    WI_SetFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    const auto bufferWidth = si.GetBufferSize().Width();
    const auto bufferHeight = si.GetBufferSize().Height();

    // Move the viewport down a few lines, and only cover part of the buffer width.
    si.SetViewport(Viewport::FromDimensions({ 5, 10 }, { bufferWidth - 10, 10 }), true);
    const auto& viewport = si.GetViewport();

    // Fill the entire buffer with Zs. Blue on Green.
    const auto bufferChar = L'Z';
    const auto bufferAttr = TextAttribute{ FOREGROUND_BLUE | BACKGROUND_GREEN };
    _FillLines(0, bufferHeight, bufferChar, bufferAttr);

    // For selective erasure tests, we protect the first five columns.
    if (selectiveErase)
    {
        auto protectedAttr = bufferAttr;
        protectedAttr.SetProtected(true);
        for (auto line = viewport.Top(); line < viewport.BottomExclusive(); ++line)
        {
            _FillLine(line, L"ZZZZZ", protectedAttr);
        }
    }

    // Set the attributes that will be used to fill the erased area.
    auto fillAttr = TextAttribute{ RGB(12, 34, 56), RGB(78, 90, 12) };
    fillAttr.SetCrossedOut(true);
    fillAttr.SetReverseVideo(true);
    fillAttr.SetUnderlined(true);
    si.SetAttributes(fillAttr);
    // But note that the meta attributes are expected to be cleared.
    auto expectedFillAttr = fillAttr;
    expectedFillAttr.SetStandardErase();
    // And with selective erasure the original attributes are maintained.
    if (selectiveErase)
    {
        expectedFillAttr = bufferAttr;
    }

    // Place the cursor in the center.
    const auto centerX = bufferWidth / 2;
    const auto centerY = (viewport.Top() + viewport.BottomExclusive()) / 2;

    Log::Comment(L"Set the cursor position and perform the operation.");
    VERIFY_SUCCEEDED(si.SetCursorPosition({ centerX, centerY }, true));
    stateMachine.ProcessString(escapeSequence.str());

    // Get cursor position and viewport range.
    const auto cursorPos = si.GetTextBuffer().GetCursor().GetPosition();
    const auto viewportStart = viewport.Top();
    const auto viewportEnd = viewport.BottomExclusive();

    Log::Comment(L"Lines outside the viewport should remain unchanged.");
    VERIFY_IS_TRUE(_ValidateLinesContain(0, viewportStart, bufferChar, bufferAttr));
    VERIFY_IS_TRUE(_ValidateLinesContain(viewportEnd, bufferHeight, bufferChar, bufferAttr));

    // With selective erasure, there's a protected range to account for at the start of the line.
    const auto protectedOffset = selectiveErase ? 5 : 0;

    // 1. Lines before cursor line
    if (eraseScreen && eraseType != DispatchTypes::EraseType::ToEnd)
    {
        // For eraseScreen, if we're not erasing to the end, these rows will be cleared.
        Log::Comment(L"Lines before the cursor line should be erased.");
        VERIFY_IS_TRUE(_ValidateLinesContain(protectedOffset, viewportStart, cursorPos.y, L' ', expectedFillAttr));
    }
    else
    {
        // Otherwise we'll be left with the original buffer content.
        Log::Comment(L"Lines before the cursor line should remain unchanged.");
        VERIFY_IS_TRUE(_ValidateLinesContain(protectedOffset, viewportStart, cursorPos.y, bufferChar, bufferAttr));
    }

    // 2. Cursor Line
    auto prefixPos = til::point{ protectedOffset, cursorPos.y };
    auto suffixPos = cursorPos;
    // When erasing from the beginning, the cursor column is included in the range.
    suffixPos.x += (eraseType == DispatchTypes::EraseType::FromBeginning);
    size_t prefixWidth = suffixPos.x - prefixPos.x;
    size_t suffixWidth = bufferWidth - suffixPos.x;
    if (eraseType == DispatchTypes::EraseType::ToEnd)
    {
        Log::Comment(L"The start of the cursor line should remain unchanged.");
        VERIFY_IS_TRUE(_ValidateLineContains(prefixPos, bufferChar, bufferAttr, prefixWidth));
        Log::Comment(L"The end of the cursor line should be erased.");
        VERIFY_IS_TRUE(_ValidateLineContains(suffixPos, L' ', expectedFillAttr, suffixWidth));
    }
    if (eraseType == DispatchTypes::EraseType::FromBeginning)
    {
        Log::Comment(L"The start of the cursor line should be erased.");
        VERIFY_IS_TRUE(_ValidateLineContains(prefixPos, L' ', expectedFillAttr, prefixWidth));
        Log::Comment(L"The end of the cursor line should remain unchanged.");
        VERIFY_IS_TRUE(_ValidateLineContains(suffixPos, bufferChar, bufferAttr, suffixWidth));
    }
    if (eraseType == DispatchTypes::EraseType::All)
    {
        Log::Comment(L"The entire cursor line should be erased.");
        VERIFY_IS_TRUE(_ValidateLineContains(prefixPos, L' ', expectedFillAttr));
    }

    // 3. Lines after cursor line
    if (eraseScreen && eraseType != DispatchTypes::EraseType::FromBeginning)
    {
        // For eraseScreen, if we're not erasing from the beginning, these rows will be cleared.
        Log::Comment(L"Lines after the cursor line should be erased.");
        VERIFY_IS_TRUE(_ValidateLinesContain(protectedOffset, cursorPos.y + 1, viewportEnd, L' ', expectedFillAttr));
    }
    else
    {
        // Otherwise we'll be left with the original buffer content.
        Log::Comment(L"Lines after the cursor line should remain unchanged.");
        VERIFY_IS_TRUE(_ValidateLinesContain(protectedOffset, cursorPos.y + 1, viewportEnd, bufferChar, bufferAttr));
    }

    // 4. Protected columns
    if (selectiveErase)
    {
        Log::Comment(L"First five columns should remain unchanged.");
        auto protectedAttr = bufferAttr;
        protectedAttr.SetProtected(true);
        for (auto line = viewport.Top(); line < viewport.BottomExclusive(); ++line)
        {
            VERIFY_IS_TRUE(_ValidateLineContains(line, L"ZZZZZ", protectedAttr));
        }
    }
}

void ScreenBufferTests::ProtectedAttributeTests()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    auto& textBuffer = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();
    WI_SetFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    const auto cursorPosition = textBuffer.GetCursor().GetPosition();
    auto unprotectedAttribute = textBuffer.GetCurrentAttributes();
    unprotectedAttribute.SetProtected(false);
    auto protectedAttribute = textBuffer.GetCurrentAttributes();
    protectedAttribute.SetProtected(true);

    Log::Comment(L"DECSCA default should be unprotected");
    textBuffer.GetCursor().SetPosition(cursorPosition);
    textBuffer.SetCurrentAttributes(protectedAttribute);
    stateMachine.ProcessString(L"\x1b[\"qZZZZZ");
    VERIFY_IS_FALSE(textBuffer.GetCurrentAttributes().IsProtected());
    VERIFY_IS_TRUE(_ValidateLineContains(cursorPosition, L"ZZZZZ", unprotectedAttribute));

    Log::Comment(L"DECSCA 0 should be unprotected");
    textBuffer.GetCursor().SetPosition(cursorPosition);
    textBuffer.SetCurrentAttributes(protectedAttribute);
    stateMachine.ProcessString(L"\x1b[0\"qZZZZZ");
    VERIFY_IS_FALSE(textBuffer.GetCurrentAttributes().IsProtected());
    VERIFY_IS_TRUE(_ValidateLineContains(cursorPosition, L"ZZZZZ", unprotectedAttribute));

    Log::Comment(L"DECSCA 1 should be protected");
    textBuffer.GetCursor().SetPosition(cursorPosition);
    textBuffer.SetCurrentAttributes(unprotectedAttribute);
    stateMachine.ProcessString(L"\x1b[1\"qZZZZZ");
    VERIFY_IS_TRUE(textBuffer.GetCurrentAttributes().IsProtected());
    VERIFY_IS_TRUE(_ValidateLineContains(cursorPosition, L"ZZZZZ", protectedAttribute));

    Log::Comment(L"DECSCA 2 should be unprotected");
    textBuffer.GetCursor().SetPosition(cursorPosition);
    textBuffer.SetCurrentAttributes(protectedAttribute);
    stateMachine.ProcessString(L"\x1b[2\"qZZZZZ");
    VERIFY_IS_FALSE(textBuffer.GetCurrentAttributes().IsProtected());
    VERIFY_IS_TRUE(_ValidateLineContains(cursorPosition, L"ZZZZZ", unprotectedAttribute));

    Log::Comment(L"DECSCA 2;1 should be protected");
    textBuffer.GetCursor().SetPosition(cursorPosition);
    textBuffer.SetCurrentAttributes(unprotectedAttribute);
    stateMachine.ProcessString(L"\x1b[2;1\"qZZZZZ");
    VERIFY_IS_TRUE(textBuffer.GetCurrentAttributes().IsProtected());
    VERIFY_IS_TRUE(_ValidateLineContains(cursorPosition, L"ZZZZZ", protectedAttribute));

    Log::Comment(L"DECSCA 1;2 should be unprotected");
    textBuffer.GetCursor().SetPosition(cursorPosition);
    textBuffer.SetCurrentAttributes(protectedAttribute);
    stateMachine.ProcessString(L"\x1b[1;2\"qZZZZZ");
    VERIFY_IS_FALSE(textBuffer.GetCurrentAttributes().IsProtected());
    VERIFY_IS_TRUE(_ValidateLineContains(cursorPosition, L"ZZZZZ", unprotectedAttribute));
}

void _CommonScrollingSetup()
{
    // Used for testing MSFT:20204600
    // Place As on the first line, and Bs on the 6th line (index 5).
    // Set the scrolling region in between those lines (so scrolling won't affect them.)
    // First write "1111\n2222\n3333\n4444", to put 1-4 on the lines in between the A and B.
    // the viewport will look like:
    // AAAAAA...
    // 111111...
    // 222222...
    // 333333...
    // 444444...
    // BBBBBB...
    // then write "\n5555\n6666\n7777\n", which will cycle around the scroll region a bit.
    // the viewport will look like:
    // AAAAAA...
    // 555555...
    // 666666...
    // 777777...
    //
    // BBBBBB...

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si.GetTextBuffer().GetCursor();
    const auto oldView = si.GetViewport();
    const auto view = Viewport::FromDimensions({ 0, 0 }, { oldView.Width(), 6 });
    const auto bufferWidth = tbi.GetSize().Width();
    const auto attr = tbi.GetCurrentAttributes();

    si.SetViewport(view, true);
    cursor.SetPosition({ 0, 0 });
    stateMachine.ProcessString(std::wstring(bufferWidth, L'A'));
    cursor.SetPosition({ 0, 5 });
    stateMachine.ProcessString(std::wstring(bufferWidth, L'B'));
    stateMachine.ProcessString(L"\x1b[2;5r");
    stateMachine.ProcessString(L"\x1b[2;1H");
    stateMachine.ProcessString(std::wstring(bufferWidth, L'1'));
    stateMachine.ProcessCharacter(L'\n');
    stateMachine.ProcessString(std::wstring(bufferWidth, L'2'));
    stateMachine.ProcessCharacter(L'\n');
    stateMachine.ProcessString(std::wstring(bufferWidth, L'3'));
    stateMachine.ProcessCharacter(L'\n');
    stateMachine.ProcessString(std::wstring(bufferWidth, L'4'));

    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<til::point>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"viewport=%s", VerifyOutputTraits<til::inclusive_rect>::ToString(si.GetViewport().ToInclusive()).GetBuffer()));

    VERIFY_ARE_EQUAL(bufferWidth - 1, cursor.GetPosition().x);
    VERIFY_ARE_EQUAL(4, cursor.GetPosition().y);

    VERIFY_IS_TRUE(_ValidateLineContains(0, L'A', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(1, L'1', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(2, L'2', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(3, L'3', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(4, L'4', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(5, L'B', attr));

    stateMachine.ProcessCharacter(L'\n');
    stateMachine.ProcessString(std::wstring(bufferWidth, L'5'));
    stateMachine.ProcessCharacter(L'\n');
    stateMachine.ProcessString(std::wstring(bufferWidth, L'6'));
    stateMachine.ProcessCharacter(L'\n');
    stateMachine.ProcessString(std::wstring(bufferWidth, L'7'));
    stateMachine.ProcessCharacter(L'\n');

    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<til::point>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"viewport=%s", VerifyOutputTraits<til::inclusive_rect>::ToString(si.GetViewport().ToInclusive()).GetBuffer()));

    VERIFY_ARE_EQUAL(0, cursor.GetPosition().x);
    VERIFY_ARE_EQUAL(4, cursor.GetPosition().y);

    VERIFY_IS_TRUE(_ValidateLineContains(0, L'A', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(1, L'5', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(2, L'6', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(3, L'7', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(4, L' ', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(5, L'B', attr));
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
    auto& cursor = tbi.GetCursor();
    const auto attr = tbi.GetCurrentAttributes();

    // Execute a Scroll Up command
    stateMachine.ProcessString(L"\x1b[S");

    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<til::point>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"viewport=%s", VerifyOutputTraits<til::inclusive_rect>::ToString(si.GetViewport().ToInclusive()).GetBuffer()));

    VERIFY_ARE_EQUAL(0, cursor.GetPosition().x);
    VERIFY_ARE_EQUAL(4, cursor.GetPosition().y);

    VERIFY_IS_TRUE(_ValidateLineContains(0, L'A', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(1, L'6', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(2, L'7', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(3, L' ', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(4, L' ', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(5, L'B', attr));

    _CommonScrollingSetup();
    // Set horizontal margins
    stateMachine.ProcessString(L"\x1b[?69h");
    stateMachine.ProcessString(L"\x1b[3;6s");
    auto resetMargins = wil::scope_exit([&] { stateMachine.ProcessString(L"\x1b[?69l"); });

    // Execute a Scroll Up command
    stateMachine.ProcessString(L"\x1b[S");

    VERIFY_IS_TRUE(_ValidateLineContains(0, L'A', attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 0, 1 }, L"55", attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 2, 1 }, L"6666", attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 6, 1 }, L'5', attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 0, 2 }, L"66", attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 2, 2 }, L"7777", attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 6, 2 }, L'6', attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 0, 3 }, L"77", attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 2, 3 }, L"    ", attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 6, 3 }, L'7', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(4, L' ', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(5, L'B', attr));
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
    auto& cursor = tbi.GetCursor();
    const auto attr = tbi.GetCurrentAttributes();

    // Execute a Scroll Down command
    stateMachine.ProcessString(L"\x1b[T");

    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<til::point>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"viewport=%s", VerifyOutputTraits<til::inclusive_rect>::ToString(si.GetViewport().ToInclusive()).GetBuffer()));

    VERIFY_ARE_EQUAL(0, cursor.GetPosition().x);
    VERIFY_ARE_EQUAL(4, cursor.GetPosition().y);

    VERIFY_IS_TRUE(_ValidateLineContains(0, L'A', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(1, L' ', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(2, L'5', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(3, L'6', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(4, L'7', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(5, L'B', attr));

    _CommonScrollingSetup();
    // Set horizontal margins
    stateMachine.ProcessString(L"\x1b[?69h");
    stateMachine.ProcessString(L"\x1b[3;6s");
    auto resetMargins = wil::scope_exit([&] { stateMachine.ProcessString(L"\x1b[?69l"); });

    // Execute a Scroll Down command
    stateMachine.ProcessString(L"\x1b[T");

    VERIFY_IS_TRUE(_ValidateLineContains(0, L'A', attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 0, 1 }, L"55", attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 2, 1 }, L"    ", attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 6, 1 }, L'5', attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 0, 2 }, L"66", attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 2, 2 }, L"5555", attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 6, 2 }, L'6', attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 0, 3 }, L"77", attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 2, 3 }, L"6666", attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 6, 3 }, L'7', attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 0, 4 }, L"  ", attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 2, 4 }, L"7777", attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 6, 4 }, L' ', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(5, L'B', attr));
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
    auto& cursor = tbi.GetCursor();
    const auto attr = tbi.GetCurrentAttributes();

    // Move to column 5 of line 3
    stateMachine.ProcessString(L"\x1b[3;5H");
    // Insert 2 lines
    stateMachine.ProcessString(L"\x1b[2L");

    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<til::point>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"viewport=%s", VerifyOutputTraits<til::inclusive_rect>::ToString(si.GetViewport().ToInclusive()).GetBuffer()));

    // Verify cursor moved to left margin.
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().x);
    VERIFY_ARE_EQUAL(2, cursor.GetPosition().y);

    VERIFY_IS_TRUE(_ValidateLineContains(0, L'A', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(1, L'5', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(2, L' ', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(3, L' ', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(4, L'6', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(5, L'B', attr));

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
        L"cursor=%s", VerifyOutputTraits<til::point>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"viewport=%s", VerifyOutputTraits<til::inclusive_rect>::ToString(si.GetViewport().ToInclusive()).GetBuffer()));

    // Verify cursor moved to left margin.
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().x);
    VERIFY_ARE_EQUAL(1, cursor.GetPosition().y);

    VERIFY_IS_TRUE(_ValidateLineContains(0, L'A', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(1, L' ', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(2, L'5', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(3, L'6', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(4, L'7', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(5, L' ', attr));

    Log::Comment(
        L"Does the common scrolling setup, then inserts two lines inside the "
        L"margin boundaries, and verifies the rows have what we'd expect.");

    _CommonScrollingSetup();
    // Set horizontal margins
    stateMachine.ProcessString(L"\x1b[?69h");
    stateMachine.ProcessString(L"\x1b[3;6s");
    auto resetMargins = wil::scope_exit([&] { stateMachine.ProcessString(L"\x1b[?69l"); });
    // Move to column 5 of line 3
    stateMachine.ProcessString(L"\x1b[3;5H");
    // Insert 2 lines
    stateMachine.ProcessString(L"\x1b[2L");

    // Verify cursor moved to left margin.
    VERIFY_ARE_EQUAL(2, cursor.GetPosition().x);
    VERIFY_ARE_EQUAL(2, cursor.GetPosition().y);

    VERIFY_IS_TRUE(_ValidateLineContains(0, L'A', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(1, L'5', attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 0, 2 }, L"66", attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 2, 2 }, L"    ", attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 6, 2 }, L'6', attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 0, 3 }, L"77", attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 2, 3 }, L"    ", attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 6, 3 }, L'7', attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 0, 4 }, L"  ", attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 2, 4 }, L"6666", attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 6, 4 }, L' ', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(5, L'B', attr));
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
    auto& cursor = tbi.GetCursor();
    const auto attr = tbi.GetCurrentAttributes();

    // Move to column 5 of line 3
    stateMachine.ProcessString(L"\x1b[3;5H");
    // Delete 2 lines
    stateMachine.ProcessString(L"\x1b[2M");

    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<til::point>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"viewport=%s", VerifyOutputTraits<til::inclusive_rect>::ToString(si.GetViewport().ToInclusive()).GetBuffer()));

    // Verify cursor moved to left margin.
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().x);
    VERIFY_ARE_EQUAL(2, cursor.GetPosition().y);

    VERIFY_IS_TRUE(_ValidateLineContains(0, L'A', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(1, L'5', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(2, L' ', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(3, L' ', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(4, L' ', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(5, L'B', attr));

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
        L"cursor=%s", VerifyOutputTraits<til::point>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"viewport=%s", VerifyOutputTraits<til::inclusive_rect>::ToString(si.GetViewport().ToInclusive()).GetBuffer()));

    // Verify cursor moved to left margin.
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().x);
    VERIFY_ARE_EQUAL(1, cursor.GetPosition().y);

    VERIFY_IS_TRUE(_ValidateLineContains(0, L'A', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(1, L'6', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(2, L'7', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(3, L' ', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(4, L'B', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(5, L' ', attr));

    Log::Comment(
        L"Does the common scrolling setup, then deletes two lines inside the "
        L"margin boundaries, and verifies the rows have what we'd expect.");

    _CommonScrollingSetup();
    // Set horizontal margins
    stateMachine.ProcessString(L"\x1b[?69h");
    stateMachine.ProcessString(L"\x1b[3;6s");
    auto resetMargins = wil::scope_exit([&] { stateMachine.ProcessString(L"\x1b[?69l"); });
    // Move to column 5 of line 3
    stateMachine.ProcessString(L"\x1b[3;5H");
    // Delete 2 lines
    stateMachine.ProcessString(L"\x1b[2M");

    // Verify cursor moved to left margin.
    VERIFY_ARE_EQUAL(2, cursor.GetPosition().x);
    VERIFY_ARE_EQUAL(2, cursor.GetPosition().y);

    VERIFY_IS_TRUE(_ValidateLineContains(0, L'A', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(1, L'5', attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 0, 2 }, L"66", attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 2, 2 }, L"    ", attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 6, 2 }, L'6', attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 0, 3 }, L"77", attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 2, 3 }, L"    ", attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 6, 3 }, L'7', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(4, L' ', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(5, L'B', attr));
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
    auto& cursor = tbi.GetCursor();
    const auto attr = tbi.GetCurrentAttributes();

    // Move to column 5 of line 2, the top margin
    stateMachine.ProcessString(L"\x1b[2;5H");
    // Execute a reverse line feed (RI)
    stateMachine.ProcessString(L"\x1bM");

    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<til::point>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"viewport=%s", VerifyOutputTraits<til::inclusive_rect>::ToString(si.GetViewport().ToInclusive()).GetBuffer()));

    VERIFY_ARE_EQUAL(4, cursor.GetPosition().x);
    VERIFY_ARE_EQUAL(1, cursor.GetPosition().y);

    VERIFY_IS_TRUE(_ValidateLineContains(0, L'A', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(1, L' ', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(2, L'5', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(3, L'6', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(4, L'7', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(5, L'B', attr));

    Log::Comment(
        L"Does the common scrolling setup, then executes a reverse line feed "
        L"with the top margin at the top of the screen, and verifies the rows "
        L"have what we'd expect.");

    _CommonScrollingSetup();
    // Set the top scroll margin to the top of the screen
    stateMachine.ProcessString(L"\x1b[1;5r");
    // Make sure we clear the margins on exit so they can't break other tests.
    auto clearMargins = wil::scope_exit([&] { stateMachine.ProcessString(L"\x1b[r"); });
    // Move to column 5 of line 1, the top of the screen
    stateMachine.ProcessString(L"\x1b[1;5H");
    // Execute a reverse line feed (RI)
    stateMachine.ProcessString(L"\x1bM");

    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<til::point>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"viewport=%s", VerifyOutputTraits<til::inclusive_rect>::ToString(si.GetViewport().ToInclusive()).GetBuffer()));

    VERIFY_ARE_EQUAL(4, cursor.GetPosition().x);
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().y);

    VERIFY_IS_TRUE(_ValidateLineContains(0, L' ', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(1, L'A', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(2, L'5', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(3, L'6', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(4, L'7', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(5, L'B', attr));

    Log::Comment(
        L"Does the common scrolling setup, then executes a reverse line feed "
        L"below the top margin, and verifies the rows have what we'd expect.");

    _CommonScrollingSetup();
    // Set horizontal margins
    stateMachine.ProcessString(L"\x1b[?69h");
    stateMachine.ProcessString(L"\x1b[3;6s");
    auto resetMargins = wil::scope_exit([&] { stateMachine.ProcessString(L"\x1b[?69l"); });
    // Move to column 5 of line 2, the top margin
    stateMachine.ProcessString(L"\x1b[2;5H");
    // Execute a reverse line feed (RI)
    stateMachine.ProcessString(L"\x1bM");

    VERIFY_ARE_EQUAL(4, cursor.GetPosition().x);
    VERIFY_ARE_EQUAL(1, cursor.GetPosition().y);

    VERIFY_IS_TRUE(_ValidateLineContains(0, L'A', attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 0, 1 }, L"55", attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 2, 1 }, L"    ", attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 6, 1 }, L'5', attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 0, 2 }, L"66", attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 2, 2 }, L"5555", attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 6, 2 }, L'6', attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 0, 3 }, L"77", attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 2, 3 }, L"6666", attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 6, 3 }, L'7', attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 0, 4 }, L"  ", attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 2, 4 }, L"7777", attr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 6, 4 }, L' ', attr));
    VERIFY_IS_TRUE(_ValidateLineContains(5, L'B', attr));
}

void ScreenBufferTests::LineFeedEscapeSequences()
{
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:withReturn", L"{true, false}")
    END_TEST_METHOD_PROPERTIES()

    bool withReturn;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"withReturn", withReturn));

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si.GetTextBuffer().GetCursor();

    std::wstring escapeSequence;
    if (withReturn)
    {
        Log::Comment(L"Testing line feed with carriage return (NEL).");
        escapeSequence = L"\033E";
    }
    else
    {
        Log::Comment(L"Testing line feed without carriage return (IND).");
        escapeSequence = L"\033D";
    }

    // Set the viewport to a reasonable size.
    const auto view = Viewport::FromDimensions({ 0, 0 }, { 80, 25 });
    si.SetViewport(view, true);

    // We'll place the cursor in the center of the line.
    // If we are performing a line feed with carriage return,
    // the cursor should move to the leftmost column.
    const auto initialX = view.Width() / 2;
    const auto expectedX = withReturn ? 0 : initialX;

    {
        Log::Comment(L"Starting at the top of viewport");
        const auto initialY = 0;
        const auto expectedY = initialY + 1;
        const auto expectedViewportTop = si.GetViewport().Top();
        cursor.SetPosition({ initialX, initialY });
        stateMachine.ProcessString(escapeSequence);

        VERIFY_ARE_EQUAL(expectedX, cursor.GetPosition().x);
        VERIFY_ARE_EQUAL(expectedY, cursor.GetPosition().y);
        VERIFY_ARE_EQUAL(expectedViewportTop, si.GetViewport().Top());
    }

    {
        Log::Comment(L"Starting at the bottom of viewport");
        const auto initialY = si.GetViewport().BottomInclusive();
        const auto expectedY = initialY + 1;
        const auto expectedViewportTop = si.GetViewport().Top() + 1;
        cursor.SetPosition({ initialX, initialY });
        stateMachine.ProcessString(escapeSequence);

        VERIFY_ARE_EQUAL(expectedX, cursor.GetPosition().x);
        VERIFY_ARE_EQUAL(expectedY, cursor.GetPosition().y);
        VERIFY_ARE_EQUAL(expectedViewportTop, si.GetViewport().Top());
    }

    {
        Log::Comment(L"Starting at the bottom of the scroll margins");
        // Set the margins to rows 5 to 10.
        stateMachine.ProcessString(L"\x1b[5;10r");
        // Make sure we clear the margins on exit so they can't break other tests.
        auto clearMargins = wil::scope_exit([&] { stateMachine.ProcessString(L"\x1b[r"); });

        const auto initialY = si.GetViewport().Top() + 9;
        const auto expectedY = initialY;
        const auto expectedViewportTop = si.GetViewport().Top();
        _FillLine(initialY, L'Q', {});
        cursor.SetPosition({ initialX, initialY });
        stateMachine.ProcessString(escapeSequence);

        VERIFY_ARE_EQUAL(expectedX, cursor.GetPosition().x);
        VERIFY_ARE_EQUAL(expectedY, cursor.GetPosition().y);
        VERIFY_ARE_EQUAL(expectedViewportTop, si.GetViewport().Top());
        // Verify the line of Qs has been scrolled up.
        VERIFY_IS_TRUE(_ValidateLineContains(initialY - 1, L'Q', {}));
        VERIFY_IS_TRUE(_ValidateLineContains(initialY, L' ', si.GetAttributes()));
    }

    {
        Log::Comment(L"Starting at the bottom of the scroll margins with horizontal margins");
        // Set the margins to rows 5 to 10.
        stateMachine.ProcessString(L"\x1b[5;10r");
        // Set the margins to columns 3 to 6.
        stateMachine.ProcessString(L"\x1b[?69h");
        stateMachine.ProcessString(L"\x1b[3;6s");
        // Make sure we clear the margins on exit so they can't break other tests.
        auto clearMargins = wil::scope_exit([&] {
            stateMachine.ProcessString(L"\x1b[r");
            stateMachine.ProcessString(L"\x1b[s");
            stateMachine.ProcessString(L"\x1b[?69l");
        });

        const auto initialY = si.GetViewport().Top() + 9;
        const auto expectedY = initialY;
        const auto initialXInMargins = 5;
        const auto expectedXInMargins = withReturn ? 2 : initialXInMargins;
        const auto expectedViewportTop = si.GetViewport().Top();
        _FillLine(initialY, L'R', {});
        cursor.SetPosition({ initialXInMargins, initialY });
        stateMachine.ProcessString(escapeSequence);

        VERIFY_ARE_EQUAL(expectedXInMargins, cursor.GetPosition().x);
        VERIFY_ARE_EQUAL(expectedY, cursor.GetPosition().y);
        VERIFY_ARE_EQUAL(expectedViewportTop, si.GetViewport().Top());
        // Verify the line of Rs has been scrolled up only within the margins.
        const auto defaultAttr = TextAttribute{};
        VERIFY_IS_TRUE(_ValidateLineContains({ 0, initialY - 1 }, L"QQ", defaultAttr));
        VERIFY_IS_TRUE(_ValidateLineContains({ 2, initialY - 1 }, L"RRRR", defaultAttr));
        VERIFY_IS_TRUE(_ValidateLineContains({ 6, initialY - 1 }, L'Q', defaultAttr));
        VERIFY_IS_TRUE(_ValidateLineContains({ 0, initialY }, L"RR", defaultAttr));
        VERIFY_IS_TRUE(_ValidateLineContains({ 2, initialY }, L"    ", si.GetAttributes()));
        VERIFY_IS_TRUE(_ValidateLineContains({ 6, initialY }, L'R', defaultAttr));
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
    const auto Use16Color = 0;
    const auto Use256Color = 1;
    const auto UseRGBColor = 2;

    // scrollType will be used to control whether we use InsertLines,
    // DeleteLines, or ReverseIndex to scroll the contents of the buffer.
    const auto InsertLines = 0;
    const auto DeleteLines = 1;
    const auto ReverseLineFeed = 2;

    int scrollType;
    int colorStyle;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"scrollType", scrollType), L"controls whether to use InsertLines, DeleteLines or ReverseLineFeed");
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

    auto expectedAttr{ si.GetAttributes() };
    std::wstring_view sgrSeq = L"\x1b[42m";
    if (colorStyle == Use16Color)
    {
        expectedAttr.SetIndexedBackground(2);
    }
    else if (colorStyle == Use256Color)
    {
        expectedAttr.SetIndexedBackground256(20);
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
    std::wstring_view scrollSeq = L"";
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
        L"cursor=%s", VerifyOutputTraits<til::point>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"viewport=%s", VerifyOutputTraits<til::inclusive_rect>::ToString(si.GetViewport().ToInclusive()).GetBuffer()));

    VERIFY_ARE_EQUAL(0, cursor.GetPosition().x);
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().y);

    stateMachine.ProcessString(L"foo");
    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<til::point>::ToString(cursor.GetPosition()).GetBuffer()));
    VERIFY_ARE_EQUAL(3, cursor.GetPosition().x);
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().y);
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

void ScreenBufferTests::SetLineFeedMode()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& stateMachine = si.GetStateMachine();
    const auto& terminalInput = gci.GetActiveInputBuffer()->GetTerminalInput();

    // We need to start with newline auto return disabled for LNM to be active.
    WI_SetFlag(si.OutputMode, DISABLE_NEWLINE_AUTO_RETURN);
    auto restoreMode = wil::scope_exit([&] { WI_ClearFlag(si.OutputMode, DISABLE_NEWLINE_AUTO_RETURN); });

    Log::Comment(L"When LNM is set, newline auto return and line feed mode are enabled.");
    stateMachine.ProcessString(L"\x1B[20h");
    VERIFY_IS_TRUE(WI_IsFlagClear(si.OutputMode, DISABLE_NEWLINE_AUTO_RETURN));
    VERIFY_IS_TRUE(terminalInput.GetInputMode(TerminalInput::Mode::LineFeed));

    Log::Comment(L"When LNM is reset, newline auto return and line feed mode are disabled.");
    stateMachine.ProcessString(L"\x1B[20l");
    VERIFY_IS_FALSE(WI_IsFlagClear(si.OutputMode, DISABLE_NEWLINE_AUTO_RETURN));
    VERIFY_IS_FALSE(terminalInput.GetInputMode(TerminalInput::Mode::LineFeed));
}

void ScreenBufferTests::SetScreenMode()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& stateMachine = si.GetStateMachine();
    const auto& renderSettings = gci.GetRenderSettings();

    const auto rgbForeground = RGB(12, 34, 56);
    const auto rgbBackground = RGB(78, 90, 12);
    const auto testAttr = TextAttribute{ rgbForeground, rgbBackground };

    Log::Comment(L"By default the screen mode is normal.");
    VERIFY_IS_FALSE(renderSettings.GetRenderMode(RenderSettings::Mode::ScreenReversed));
    VERIFY_ARE_EQUAL(std::make_pair(rgbForeground, rgbBackground), renderSettings.GetAttributeColors(testAttr));

    Log::Comment(L"When DECSCNM is set, background and foreground colors are switched.");
    stateMachine.ProcessString(L"\x1B[?5h");
    VERIFY_IS_TRUE(renderSettings.GetRenderMode(RenderSettings::Mode::ScreenReversed));
    VERIFY_ARE_EQUAL(std::make_pair(rgbBackground, rgbForeground), renderSettings.GetAttributeColors(testAttr));

    Log::Comment(L"When DECSCNM is reset, the colors are normal again.");
    stateMachine.ProcessString(L"\x1B[?5l");
    VERIFY_IS_FALSE(renderSettings.GetRenderMode(RenderSettings::Mode::ScreenReversed));
    VERIFY_ARE_EQUAL(std::make_pair(rgbForeground, rgbBackground), renderSettings.GetAttributeColors(testAttr));
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
    VERIFY_ARE_EQUAL(til::point(0, 0), cursor.GetPosition());
    stateMachine.ProcessString(L"\x1B[?69h");
    cursor.SetPosition({ 40, 12 });
    stateMachine.ProcessString(L"\x1B[31;50s");
    VERIFY_ARE_EQUAL(til::point(0, 0), cursor.GetPosition());
    Log::Comment(L"Cursor addressing is relative to the top-left of the screen.");
    stateMachine.ProcessString(L"\x1B[13;41H");
    VERIFY_ARE_EQUAL(til::point(40, 12), cursor.GetPosition());
    Log::Comment(L"The cursor can be moved past the bottom right margins.");
    stateMachine.ProcessString(L"\x1B[23;61H");
    VERIFY_ARE_EQUAL(til::point(60, 22), cursor.GetPosition());

    // Testing the effects of DECOM being set (relative cursor addressing)
    Log::Comment(L"Setting DECOM moves the cursor to the top-left of the margin area.");
    cursor.SetPosition({ 40, 12 });
    stateMachine.ProcessString(L"\x1B[?6h");
    VERIFY_ARE_EQUAL(til::point(30, 5), cursor.GetPosition());
    Log::Comment(L"Setting a margin moves the cursor to the top-left of the margin area.");
    cursor.SetPosition({ 40, 12 });
    stateMachine.ProcessString(L"\x1B[6;20r");
    VERIFY_ARE_EQUAL(til::point(30, 5), cursor.GetPosition());
    cursor.SetPosition({ 40, 12 });
    stateMachine.ProcessString(L"\x1B[31;50s");
    VERIFY_ARE_EQUAL(til::point(30, 5), cursor.GetPosition());
    Log::Comment(L"Cursor addressing is relative to the top-left of the margin area.");
    stateMachine.ProcessString(L"\x1B[8;11H");
    VERIFY_ARE_EQUAL(til::point(40, 12), cursor.GetPosition());
    Log::Comment(L"The cursor cannot be moved past the bottom right margins.");
    stateMachine.ProcessString(L"\x1B[100;100H");
    VERIFY_ARE_EQUAL(til::point(49, 19), cursor.GetPosition());

    // Testing the effects of DECOM being reset (absolute cursor addressing)
    Log::Comment(L"Resetting DECOM moves the cursor to the top-left of the screen.");
    cursor.SetPosition({ 40, 12 });
    stateMachine.ProcessString(L"\x1B[?6l");
    VERIFY_ARE_EQUAL(til::point(0, 0), cursor.GetPosition());
    Log::Comment(L"Setting a margin moves the cursor to the top-left of the screen.");
    cursor.SetPosition({ 40, 12 });
    stateMachine.ProcessString(L"\x1B[6;20r");
    VERIFY_ARE_EQUAL(til::point(0, 0), cursor.GetPosition());
    cursor.SetPosition({ 40, 12 });
    stateMachine.ProcessString(L"\x1B[31;50s");
    VERIFY_ARE_EQUAL(til::point(0, 0), cursor.GetPosition());
    Log::Comment(L"Cursor addressing is relative to the top-left of the screen.");
    stateMachine.ProcessString(L"\x1B[13;41H");
    VERIFY_ARE_EQUAL(til::point(40, 12), cursor.GetPosition());
    Log::Comment(L"The cursor can be moved past the bottom right margins.");
    stateMachine.ProcessString(L"\x1B[23;61H");
    VERIFY_ARE_EQUAL(til::point(60, 22), cursor.GetPosition());

    // Testing the effects of DECOM being set with no margins
    Log::Comment(L"With no margins, setting DECOM moves the cursor to the top-left of the screen.");
    stateMachine.ProcessString(L"\x1B[r");
    stateMachine.ProcessString(L"\x1B[s");
    cursor.SetPosition({ 40, 12 });
    stateMachine.ProcessString(L"\x1B[?6h");
    VERIFY_ARE_EQUAL(til::point(0, 0), cursor.GetPosition());
    Log::Comment(L"Cursor addressing is still relative to the top-left of the screen.");
    stateMachine.ProcessString(L"\x1B[13;41H");
    VERIFY_ARE_EQUAL(til::point(40, 12), cursor.GetPosition());

    // Reset DECOM and DECLRMM so we don't affect future tests
    stateMachine.ProcessString(L"\x1B[?6;69l");
}

void ScreenBufferTests::SetAutoWrapMode()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si.GetTextBuffer().GetCursor();
    const auto attributes = si.GetAttributes();
    WI_SetFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    const auto view = Viewport::FromDimensions({ 0, 0 }, { 80, 25 });
    si.SetViewport(view, true);

    Log::Comment(L"By default, output should wrap onto the next line.");
    // Output 6 characters, 3 spaces from the end of the line.
    auto startLine = 0;
    cursor.SetPosition({ 80 - 3, startLine });
    stateMachine.ProcessString(L"abcdef");
    // Half of the content should wrap onto the next line.
    VERIFY_IS_TRUE(_ValidateLineContains({ 80 - 3, startLine }, L"abc", attributes));
    VERIFY_IS_TRUE(_ValidateLineContains({ 0, startLine + 1 }, L"def", attributes));
    VERIFY_ARE_EQUAL(til::point(3, startLine + 1), cursor.GetPosition());

    Log::Comment(L"When DECAWM is reset, output is clamped to the line width.");
    stateMachine.ProcessString(L"\x1b[?7l");
    // Output 6 characters, 3 spaces from the end of the line.
    startLine = 2;
    cursor.SetPosition({ 80 - 3, startLine });
    stateMachine.ProcessString(L"abcdef");
    // Content should be clamped to the line width, overwriting the last char.
    VERIFY_IS_TRUE(_ValidateLineContains({ 80 - 3, startLine }, L"abf", attributes));
    VERIFY_ARE_EQUAL(til::point(79, startLine), cursor.GetPosition());
    // Writing a wide glyph into the last 2 columns and overwriting it with a narrow one.
    cursor.SetPosition({ 80 - 3, startLine });
    stateMachine.ProcessString(L"a\U0001F604b");
    VERIFY_IS_TRUE(_ValidateLineContains({ 80 - 3, startLine }, L"a b", attributes));
    VERIFY_ARE_EQUAL(til::point(79, startLine), cursor.GetPosition());
    // Writing a wide glyph into the last column and overwriting it with a narrow one.
    cursor.SetPosition({ 80 - 3, startLine });
    stateMachine.ProcessString(L"ab\U0001F604c");
    VERIFY_IS_TRUE(_ValidateLineContains({ 80 - 3, startLine }, L"abc", attributes));
    VERIFY_ARE_EQUAL(til::point(79, startLine), cursor.GetPosition());

    Log::Comment(L"When DECAWM is set, output is wrapped again.");
    stateMachine.ProcessString(L"\x1b[?7h");
    // Output 6 characters, 3 spaces from the end of the line.
    startLine = 4;
    cursor.SetPosition({ 80 - 3, startLine });
    stateMachine.ProcessString(L"abcdef");
    // Half of the content should wrap onto the next line.
    VERIFY_IS_TRUE(_ValidateLineContains({ 80 - 3, startLine }, L"abc", attributes));
    VERIFY_IS_TRUE(_ValidateLineContains({ 0, startLine + 1 }, L"def", attributes));
    VERIFY_ARE_EQUAL(til::point(3, startLine + 1), cursor.GetPosition());
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
    VERIFY_ARE_EQUAL(til::point(0, 1), cursor.GetPosition());

    Log::Comment(L"After a reset, buffer should be clear, with cursor at 0,0");
    stateMachine.ProcessString(resetToInitialState);
    VERIFY_IS_TRUE(isBufferClear());
    VERIFY_ARE_EQUAL(til::point(0, 0), cursor.GetPosition());

    Log::Comment(L"Set the background color to red");
    stateMachine.ProcessString(L"\x1b[41m");
    Log::Comment(L"Write multiple pages of text to the buffer");
    for (auto i = 0; i < viewport.Height() * 2; i++)
    {
        stateMachine.ProcessString(L"Hello World!\n");
    }
    VERIFY_IS_FALSE(isBufferClear());
    VERIFY_IS_GREATER_THAN(viewport.Top(), viewport.Height());
    VERIFY_IS_GREATER_THAN(cursor.GetPosition().y, viewport.Height());

    Log::Comment(L"After a reset, buffer should be clear, with viewport and cursor at 0,0");
    stateMachine.ProcessString(resetToInitialState);
    VERIFY_IS_TRUE(isBufferClear());
    VERIFY_ARE_EQUAL(til::point(0, 0), viewport.Origin());
    VERIFY_ARE_EQUAL(til::point(0, 0), cursor.GetPosition());
    VERIFY_ARE_EQUAL(TextAttribute{}, si.GetAttributes());
}

void ScreenBufferTests::RestoreDownAltBufferWithTerminalScrolling()
{
    // This is a test for microsoft/terminal#1206. Refer to that issue for more
    // context

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.SetTerminalScrolling(true);
    gci.LockConsole(); // Lock must be taken to manipulate buffer.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    auto& siMain = gci.GetActiveOutputBuffer();
    auto coordFontSize = siMain.GetScreenFontSize();
    siMain._virtualBottom = siMain._viewport.BottomInclusive();

    auto originalView = siMain._viewport;

    VERIFY_IS_NULL(siMain._psiMainBuffer);
    VERIFY_IS_NULL(siMain._psiAlternateBuffer);

    Log::Comment(L"Create an alternate buffer");
    if (VERIFY_NT_SUCCESS(siMain.UseAlternateScreenBuffer({})))
    {
        VERIFY_IS_NOT_NULL(siMain._psiAlternateBuffer);
        auto& altBuffer = *siMain._psiAlternateBuffer;
        VERIFY_ARE_EQUAL(0, altBuffer._viewport.Top());
        VERIFY_ARE_EQUAL(altBuffer._viewport.BottomInclusive(), altBuffer._virtualBottom);

        auto useMain = wil::scope_exit([&] { altBuffer.UseMainScreenBuffer(); });

        const auto originalSize = originalView.Dimensions();
        const til::size doubledSize{ originalSize.width * 2, originalSize.height * 2 };

        // Create some RECTs, which are dimensions in pixels, because
        // ProcessResizeWindow needs to work on rects in screen _pixel_
        // dimensions, not character sizes.
        til::rect originalClientRect, maximizedClientRect;

        originalClientRect.right = originalSize.width * coordFontSize.width;
        originalClientRect.bottom = originalSize.height * coordFontSize.height;

        maximizedClientRect.right = doubledSize.width * coordFontSize.width;
        maximizedClientRect.bottom = doubledSize.height * coordFontSize.height;

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
    auto& gci = g.getConsoleInformation();
    gci.SetTerminalScrolling(true);
    gci.LockConsole(); // Lock must be taken to manipulate buffer.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    auto& si = gci.GetActiveOutputBuffer();
    auto& cursor = si.GetTextBuffer().GetCursor();
    const auto originalView = si._viewport;
    si._virtualBottom = originalView.BottomInclusive();

    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<til::point>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"originalView=%s", VerifyOutputTraits<til::inclusive_rect>::ToString(originalView.ToInclusive()).GetBuffer()));

    Log::Comment(NoThrowString().Format(
        L"First set the viewport somewhere lower in the buffer, as if the text "
        L"was output there. Manually move the cursor there as well, so the "
        L"cursor is within that viewport."));
    const til::point secondWindowOrigin{ 0, 10 };
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, secondWindowOrigin, true));
    si.GetTextBuffer().GetCursor().SetPosition(secondWindowOrigin);

    const auto secondView = si._viewport;
    const auto secondVirtualBottom = si._virtualBottom;
    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<til::point>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"secondView=%s", VerifyOutputTraits<til::inclusive_rect>::ToString(secondView.ToInclusive()).GetBuffer()));

    VERIFY_ARE_EQUAL(10, secondView.Top());
    VERIFY_ARE_EQUAL(originalView.Height() + 10, secondView.BottomExclusive());
    VERIFY_ARE_EQUAL(originalView.Height() + 10 - 1, secondVirtualBottom);

    Log::Comment(NoThrowString().Format(
        L"Emulate scrolling upwards with the mouse (not moving the virtual view)"));

    const til::point thirdWindowOrigin{ 0, 2 };
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, thirdWindowOrigin, false));

    const auto thirdView = si._viewport;
    const auto thirdVirtualBottom = si._virtualBottom;

    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<til::point>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"thirdView=%s", VerifyOutputTraits<til::inclusive_rect>::ToString(thirdView.ToInclusive()).GetBuffer()));

    VERIFY_ARE_EQUAL(2, thirdView.Top());
    VERIFY_ARE_EQUAL(originalView.Height() + 2, thirdView.BottomExclusive());
    VERIFY_ARE_EQUAL(secondVirtualBottom, thirdVirtualBottom);

    Log::Comment(NoThrowString().Format(
        L"Call SetConsoleCursorPosition to snap to the cursor"));
    VERIFY_SUCCEEDED(g.api->SetConsoleCursorPositionImpl(si, secondWindowOrigin));

    const auto fourthView = si._viewport;
    const auto fourthVirtualBottom = si._virtualBottom;

    Log::Comment(NoThrowString().Format(
        L"cursor=%s", VerifyOutputTraits<til::point>::ToString(cursor.GetPosition()).GetBuffer()));
    Log::Comment(NoThrowString().Format(
        L"thirdView=%s", VerifyOutputTraits<til::inclusive_rect>::ToString(fourthView.ToInclusive()).GetBuffer()));

    VERIFY_ARE_EQUAL(10, fourthView.Top());
    VERIFY_ARE_EQUAL(originalView.Height() + 10, fourthView.BottomExclusive());
    VERIFY_ARE_EQUAL(secondVirtualBottom, fourthVirtualBottom);
}

void ScreenBufferTests::ClearAlternateBuffer()
{
    // This is a test for microsoft/terminal#1189. Refer to that issue for more
    // context

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& g = ServiceLocator::LocateGlobals();
    gci.LockConsole(); // Lock must be taken to manipulate buffer.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    auto& siMain = gci.GetActiveOutputBuffer();
    auto WriteText = [&](TextBuffer& tbi) {
        // Write text to buffer
        auto& stateMachine = siMain.GetStateMachine();
        auto& cursor = tbi.GetCursor();
        stateMachine.ProcessString(L"foo\nfoo");
        VERIFY_ARE_EQUAL(cursor.GetPosition().x, 3);
        VERIFY_ARE_EQUAL(cursor.GetPosition().y, 1);
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
    if (VERIFY_NT_SUCCESS(siMain.UseAlternateScreenBuffer({})))
    {
        VERIFY_IS_NOT_NULL(siMain._psiAlternateBuffer);
        auto& altBuffer = *siMain._psiAlternateBuffer;
        VERIFY_ARE_EQUAL(0, altBuffer._viewport.Top());
        VERIFY_ARE_EQUAL(altBuffer._viewport.BottomInclusive(), altBuffer._virtualBottom);

        auto useMain = wil::scope_exit([&] { altBuffer.UseMainScreenBuffer(); });

        // Set the position to home, otherwise it's inherited from the main buffer.
        VERIFY_SUCCEEDED(altBuffer.SetCursorPosition({ 0, 0 }, true));

        WriteText(altBuffer.GetTextBuffer());
        VerifyText(altBuffer.GetTextBuffer());

#pragma region Test ScrollConsoleScreenBufferWImpl()
        // Clear text of alt buffer (same params as in CMD)
        VERIFY_SUCCEEDED(g.api->ScrollConsoleScreenBufferWImpl(siMain,
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
        VERIFY_SUCCEEDED(g.api->SetConsoleCursorPositionImpl(siMain, {}));

        // Verify state of alt buffer
        auto& altBufferCursor = altBuffer.GetTextBuffer().GetCursor();
        VERIFY_ARE_EQUAL(altBufferCursor.GetPosition().x, 0);
        VERIFY_ARE_EQUAL(altBufferCursor.GetPosition().y, 0);
#pragma endregion
    }

    // Verify state of main buffer is untouched
    auto& cursor = siMain.GetTextBuffer().GetCursor();
    VERIFY_ARE_EQUAL(cursor.GetPosition().x, 3);
    VERIFY_ARE_EQUAL(cursor.GetPosition().y, 1);

    VerifyText(siMain.GetTextBuffer());
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
        TEST_METHOD_PROPERTY(L"Data:intense", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:faint", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:italics", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:underlined", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:doublyUnderlined", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:blink", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:invisible", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:crossedOut", L"{false, true}")
    END_TEST_METHOD_PROPERTIES()

    bool intense, faint, italics, underlined, doublyUnderlined, blink, invisible, crossedOut;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"intense", intense));
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"faint", faint));
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"italics", italics));
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"underlined", underlined));
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"doublyUnderlined", doublyUnderlined));
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"blink", blink));
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"invisible", invisible));
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"crossedOut", crossedOut));

    auto& g = ServiceLocator::LocateGlobals();
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = tbi.GetCursor();

    auto expectedAttrs{ CharacterAttributes::Normal };
    std::wstring vtSeq = L"";

    // Collect up a VT sequence to set the state given the method properties
    if (intense)
    {
        WI_SetFlag(expectedAttrs, CharacterAttributes::Intense);
        vtSeq += L"\x1b[1m";
    }
    if (faint)
    {
        WI_SetFlag(expectedAttrs, CharacterAttributes::Faint);
        vtSeq += L"\x1b[2m";
    }
    if (italics)
    {
        WI_SetFlag(expectedAttrs, CharacterAttributes::Italics);
        vtSeq += L"\x1b[3m";
    }
    if (underlined)
    {
        WI_SetFlag(expectedAttrs, CharacterAttributes::Underlined);
        vtSeq += L"\x1b[4m";
    }
    if (doublyUnderlined)
    {
        WI_SetFlag(expectedAttrs, CharacterAttributes::DoublyUnderlined);
        vtSeq += L"\x1b[21m";
    }
    if (blink)
    {
        WI_SetFlag(expectedAttrs, CharacterAttributes::Blinking);
        vtSeq += L"\x1b[5m";
    }
    if (invisible)
    {
        WI_SetFlag(expectedAttrs, CharacterAttributes::Invisible);
        vtSeq += L"\x1b[8m";
    }
    if (crossedOut)
    {
        WI_SetFlag(expectedAttrs, CharacterAttributes::CrossedOut);
        vtSeq += L"\x1b[9m";
    }

    // Helper lambda to write a VT sequence, then an "X", then check that the
    // attributes of the "X" match what we think they should be.
    auto validate = [&](const CharacterAttributes expectedAttrs,
                        const std::wstring& vtSequence) {
        auto cursorPos = cursor.GetPosition();

        // Convert the vtSequence to something printable. Lets not set these
        // attrs on the test console
        auto debugString = vtSequence;
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
        auto currentAttrs = iter->TextAttr().GetCharacterAttributes();
        VERIFY_ARE_EQUAL(expectedAttrs, currentAttrs);
    };

    // Check setting all the states collected above
    validate(expectedAttrs, vtSeq);

    // One-by-one, turn off each of these states with VT, then check that the
    // state matched.
    if (intense || faint)
    {
        // The intense and faint attributes share the same reset sequence.
        WI_ClearAllFlags(expectedAttrs, CharacterAttributes::Intense | CharacterAttributes::Faint);
        vtSeq = L"\x1b[22m";
        validate(expectedAttrs, vtSeq);
    }
    if (italics)
    {
        WI_ClearFlag(expectedAttrs, CharacterAttributes::Italics);
        vtSeq = L"\x1b[23m";
        validate(expectedAttrs, vtSeq);
    }
    if (underlined || doublyUnderlined)
    {
        // The two underlined attributes share the same reset sequence.
        WI_ClearAllFlags(expectedAttrs, CharacterAttributes::Underlined | CharacterAttributes::DoublyUnderlined);
        vtSeq = L"\x1b[24m";
        validate(expectedAttrs, vtSeq);
    }
    if (blink)
    {
        WI_ClearFlag(expectedAttrs, CharacterAttributes::Blinking);
        vtSeq = L"\x1b[25m";
        validate(expectedAttrs, vtSeq);
    }
    if (invisible)
    {
        WI_ClearFlag(expectedAttrs, CharacterAttributes::Invisible);
        vtSeq = L"\x1b[28m";
        validate(expectedAttrs, vtSeq);
    }
    if (crossedOut)
    {
        WI_ClearFlag(expectedAttrs, CharacterAttributes::CrossedOut);
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
        TEST_METHOD_PROPERTY(L"Data:intense", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:faint", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:italics", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:underlined", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:doublyUnderlined", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:blink", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:invisible", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:crossedOut", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:setForegroundType", L"{0, 1, 2, 3}")
        TEST_METHOD_PROPERTY(L"Data:setBackgroundType", L"{0, 1, 2, 3}")
    END_TEST_METHOD_PROPERTIES()

    // colorStyle will be used to control whether we use a color from the 16
    // color table, a color from the 256 color table, or a pure RGB color.
    const auto UseDefault = 0;
    const auto Use16Color = 1;
    const auto Use256Color = 2;
    const auto UseRGBColor = 3;

    bool intense, faint, italics, underlined, doublyUnderlined, blink, invisible, crossedOut;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"intense", intense));
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"faint", faint));
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"italics", italics));
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"underlined", underlined));
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"doublyUnderlined", doublyUnderlined));
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

    auto expectedAttr{ si.GetAttributes() };
    std::wstring vtSeq = L"";

    // Collect up a VT sequence to set the state given the method properties
    if (intense)
    {
        expectedAttr.SetIntense(true);
        vtSeq += L"\x1b[1m";
    }
    if (faint)
    {
        expectedAttr.SetFaint(true);
        vtSeq += L"\x1b[2m";
    }
    if (italics)
    {
        expectedAttr.SetItalic(true);
        vtSeq += L"\x1b[3m";
    }
    if (underlined)
    {
        expectedAttr.SetUnderlined(true);
        vtSeq += L"\x1b[4m";
    }
    if (doublyUnderlined)
    {
        expectedAttr.SetDoublyUnderlined(true);
        vtSeq += L"\x1b[21m";
    }
    if (blink)
    {
        expectedAttr.SetBlinking(true);
        vtSeq += L"\x1b[5m";
    }
    if (invisible)
    {
        expectedAttr.SetInvisible(true);
        vtSeq += L"\x1b[8m";
    }
    if (crossedOut)
    {
        expectedAttr.SetCrossedOut(true);
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
        expectedAttr.SetIndexedForeground(2);
        vtSeq += L"\x1b[32m";
    }
    else if (setForegroundType == Use256Color)
    {
        expectedAttr.SetIndexedForeground256(20);
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
        expectedAttr.SetIndexedBackground(2);
        vtSeq += L"\x1b[42m";
    }
    else if (setBackgroundType == Use256Color)
    {
        expectedAttr.SetIndexedBackground256(20);
        vtSeq += L"\x1b[48;5;20m";
    }
    else if (setBackgroundType == UseRGBColor)
    {
        expectedAttr.SetBackground(RGB(1, 2, 3));
        vtSeq += L"\x1b[48;2;1;2;3m";
    }

    // Helper lambda to write a VT sequence, then an "X", then check that the
    // attributes of the "X" match what we think they should be.
    auto validate = [&](const TextAttribute attr,
                        const std::wstring& vtSequence) {
        auto cursorPos = cursor.GetPosition();

        // Convert the vtSequence to something printable. Lets not set these
        // attrs on the test console
        auto debugString = vtSequence;
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
        const auto currentAttrs = iter->TextAttr();
        VERIFY_ARE_EQUAL(attr, currentAttrs);
    };

    // Check setting all the states collected above
    validate(expectedAttr, vtSeq);

    // One-by-one, turn off each of these states with VT, then check that the
    // state matched.
    if (intense || faint)
    {
        // The intense and faint attributes share the same reset sequence.
        expectedAttr.SetIntense(false);
        expectedAttr.SetFaint(false);
        vtSeq = L"\x1b[22m";
        validate(expectedAttr, vtSeq);
    }
    if (italics)
    {
        expectedAttr.SetItalic(false);
        vtSeq = L"\x1b[23m";
        validate(expectedAttr, vtSeq);
    }
    if (underlined || doublyUnderlined)
    {
        // The two underlined attributes share the same reset sequence.
        expectedAttr.SetUnderlined(false);
        expectedAttr.SetDoublyUnderlined(false);
        vtSeq = L"\x1b[24m";
        validate(expectedAttr, vtSeq);
    }
    if (blink)
    {
        expectedAttr.SetBlinking(false);
        vtSeq = L"\x1b[25m";
        validate(expectedAttr, vtSeq);
    }
    if (invisible)
    {
        expectedAttr.SetInvisible(false);
        vtSeq = L"\x1b[28m";
        validate(expectedAttr, vtSeq);
    }
    if (crossedOut)
    {
        expectedAttr.SetCrossedOut(false);
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
    VERIFY_ARE_EQUAL(23, cursor.GetPosition().y);

    stateMachine.ProcessString(L"\x1b[99A");
    VERIFY_ARE_EQUAL(5, cursor.GetPosition().y);
    stateMachine.ProcessString(L"X");
    {
        auto iter = tbi.GetCellDataAt({ 0, 5 });
        VERIFY_ARE_EQUAL(L"X", iter->Chars());
    }
    stateMachine.ProcessString(L"\x1b[1H");
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().y);

    stateMachine.ProcessString(L"\x1b[99B");
    VERIFY_ARE_EQUAL(18, cursor.GetPosition().y);
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

    // This test is different because the end location of the vertical movement
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
    VERIFY_ARE_EQUAL(23, cursor.GetPosition().y);

    stateMachine.ProcessString(L"\x1b[1A");
    VERIFY_ARE_EQUAL(22, cursor.GetPosition().y);
    stateMachine.ProcessString(L"X");
    {
        auto iter = tbi.GetCellDataAt({ 0, 22 });
        VERIFY_ARE_EQUAL(L"X", iter->Chars());
    }
    stateMachine.ProcessString(L"\x1b[1H");
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().y);

    stateMachine.ProcessString(L"\x1b[1B");
    VERIFY_ARE_EQUAL(1, cursor.GetPosition().y);
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

    // This test is different because the starting location for these scroll
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
    VERIFY_ARE_EQUAL(18, cursor.GetPosition().y);
    stateMachine.ProcessString(L"\x1b[1B");
    VERIFY_ARE_EQUAL(18, cursor.GetPosition().y);
    stateMachine.ProcessString(L"1");
    {
        auto iter = tbi.GetCellDataAt({ 0, 18 });
        VERIFY_ARE_EQUAL(L"1", iter->Chars());
    }

    stateMachine.ProcessString(L"\x1b[1A");
    VERIFY_ARE_EQUAL(17, cursor.GetPosition().y);
    stateMachine.ProcessString(L"2");
    {
        auto iter = tbi.GetCellDataAt({ 1, 17 });
        VERIFY_ARE_EQUAL(L"2", iter->Chars());
    }

    stateMachine.ProcessString(L"\x1b[6;1H");
    VERIFY_ARE_EQUAL(5, cursor.GetPosition().y);

    stateMachine.ProcessString(L"\x1b[1A");
    VERIFY_ARE_EQUAL(5, cursor.GetPosition().y);
    stateMachine.ProcessString(L"3");
    {
        auto iter = tbi.GetCellDataAt({ 0, 5 });
        VERIFY_ARE_EQUAL(L"3", iter->Chars());
    }

    stateMachine.ProcessString(L"\x1b[1B");
    VERIFY_ARE_EQUAL(6, cursor.GetPosition().y);
    stateMachine.ProcessString(L"4");
    {
        auto iter = tbi.GetCellDataAt({ 1, 6 });
        VERIFY_ARE_EQUAL(L"4", iter->Chars());
    }

    stateMachine.ProcessString(L"\x1b[r");
}

void ScreenBufferTests::CursorLeftRightAcrossMargins()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si.GetTextBuffer().GetCursor();

    VERIFY_IS_TRUE(si.GetViewport().BottomInclusive() > 24);

    // Set some scrolling margins
    stateMachine.ProcessString(L"\x1b[?69h");
    stateMachine.ProcessString(L"\x1b[31;50s");

    stateMachine.ProcessString(L"\x1b[12;40H");
    VERIFY_ARE_EQUAL(39, cursor.GetPosition().x);
    stateMachine.ProcessString(L"\x1b[99C");
    VERIFY_ARE_EQUAL(49, cursor.GetPosition().x);
    stateMachine.ProcessString(L"X");
    {
        auto iter = tbi.GetCellDataAt({ 49, 11 });
        VERIFY_ARE_EQUAL(L"X", iter->Chars());
    }

    stateMachine.ProcessString(L"\x1b[12;40H");
    VERIFY_ARE_EQUAL(39, cursor.GetPosition().x);
    stateMachine.ProcessString(L"\x1b[99D");
    VERIFY_ARE_EQUAL(30, cursor.GetPosition().x);
    stateMachine.ProcessString(L"Y");
    {
        auto iter = tbi.GetCellDataAt({ 30, 11 });
        VERIFY_ARE_EQUAL(L"Y", iter->Chars());
    }

    stateMachine.ProcessString(L"\x1b[s");
    stateMachine.ProcessString(L"\x1b[?69l");
}

void ScreenBufferTests::CursorLeftRightOutsideMargins()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si.GetTextBuffer().GetCursor();

    VERIFY_IS_TRUE(si.GetViewport().BottomInclusive() > 24);

    // Set some scrolling margins
    stateMachine.ProcessString(L"\x1b[?69h");
    stateMachine.ProcessString(L"\x1b[31;50s");

    stateMachine.ProcessString(L"\x1b[12;1H");
    VERIFY_ARE_EQUAL(0, cursor.GetPosition().x);
    stateMachine.ProcessString(L"\x1b[1C");
    VERIFY_ARE_EQUAL(1, cursor.GetPosition().x);
    stateMachine.ProcessString(L"Y");
    {
        auto iter = tbi.GetCellDataAt({ 1, 11 });
        VERIFY_ARE_EQUAL(L"Y", iter->Chars());
    }

    stateMachine.ProcessString(L"\x1b[12;80H");
    VERIFY_ARE_EQUAL(79, cursor.GetPosition().x);
    stateMachine.ProcessString(L"\x1b[1D");
    VERIFY_ARE_EQUAL(78, cursor.GetPosition().x);
    stateMachine.ProcessString(L"X");
    {
        auto iter = tbi.GetCellDataAt({ 78, 11 });
        VERIFY_ARE_EQUAL(L"X", iter->Chars());
    }

    stateMachine.ProcessString(L"\x1b[s");
    stateMachine.ProcessString(L"\x1b[?69l");
}

void ScreenBufferTests::CursorLeftRightExactlyAtMargins()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si.GetTextBuffer().GetCursor();

    VERIFY_IS_TRUE(si.GetViewport().BottomInclusive() > 24);

    // Set some scrolling margins
    stateMachine.ProcessString(L"\x1b[?69h");
    stateMachine.ProcessString(L"\x1b[31;50s");

    stateMachine.ProcessString(L"\x1b[12;50H");
    VERIFY_ARE_EQUAL(49, cursor.GetPosition().x);
    stateMachine.ProcessString(L"\x1b[1C");
    VERIFY_ARE_EQUAL(49, cursor.GetPosition().x);
    stateMachine.ProcessString(L"1");
    {
        auto iter = tbi.GetCellDataAt({ 49, 11 });
        VERIFY_ARE_EQUAL(L"1", iter->Chars());
    }

    stateMachine.ProcessString(L"\x1b[1D");
    VERIFY_ARE_EQUAL(48, cursor.GetPosition().x);
    stateMachine.ProcessString(L"2");
    {
        auto iter = tbi.GetCellDataAt({ 48, 11 });
        VERIFY_ARE_EQUAL(L"2", iter->Chars());
    }

    stateMachine.ProcessString(L"\x1b[12;31H");
    VERIFY_ARE_EQUAL(30, cursor.GetPosition().x);
    stateMachine.ProcessString(L"\x1b[1D");
    VERIFY_ARE_EQUAL(30, cursor.GetPosition().x);
    stateMachine.ProcessString(L"3");
    {
        auto iter = tbi.GetCellDataAt({ 30, 11 });
        VERIFY_ARE_EQUAL(L"3", iter->Chars());
    }

    stateMachine.ProcessString(L"\x1b[1C");
    VERIFY_ARE_EQUAL(32, cursor.GetPosition().x);
    stateMachine.ProcessString(L"4");
    {
        auto iter = tbi.GetCellDataAt({ 32, 11 });
        VERIFY_ARE_EQUAL(L"4", iter->Chars());
    }

    stateMachine.ProcessString(L"\x1b[s");
    stateMachine.ProcessString(L"\x1b[?69l");
}

void ScreenBufferTests::CursorNextPreviousLine()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si.GetTextBuffer().GetCursor();

    Log::Comment(L"Make sure the viewport is at 0,0");
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, til::point(0, 0), true));

    Log::Comment(L"CNL without margins");
    // Starting from column 20 of line 10.
    cursor.SetPosition({ 20, 10 });
    // Move down 5 lines (CNL).
    stateMachine.ProcessString(L"\x1b[5E");
    // We should end up in column 0 of line 15.
    VERIFY_ARE_EQUAL(til::point(0, 15), cursor.GetPosition());

    Log::Comment(L"CPL without margins");
    // Starting from column 20 of line 10.
    cursor.SetPosition({ 20, 10 });
    // Move up 5 lines (CPL).
    stateMachine.ProcessString(L"\x1b[5F");
    // We should end up in column 0 of line 5.
    VERIFY_ARE_EQUAL(til::point(0, 5), cursor.GetPosition());

    // Enable DECLRMM margin mode
    stateMachine.ProcessString(L"\x1b[?69h");
    // Set horizontal margins to 10:29 (11:30 in VT coordinates).
    stateMachine.ProcessString(L"\x1b[11;30s");
    // Set vertical margins to 8:12 (9:13 in VT coordinates).
    stateMachine.ProcessString(L"\x1b[9;13r");
    // Make sure we clear the margins on exit so they can't break other tests.
    auto clearMargins = wil::scope_exit([&] {
        stateMachine.ProcessString(L"\x1b[r");
        stateMachine.ProcessString(L"\x1b[s");
        stateMachine.ProcessString(L"\x1b[?69l");
    });

    Log::Comment(L"CNL inside margins");
    // Starting from column 20 of line 10.
    cursor.SetPosition({ 20, 10 });
    // Move down 5 lines (CNL).
    stateMachine.ProcessString(L"\x1b[5E");
    // We should stop on column 10, line 12, the bottom left margins.
    VERIFY_ARE_EQUAL(til::point(10, 12), cursor.GetPosition());

    Log::Comment(L"CPL inside margins");
    // Starting from column 20 of line 10.
    cursor.SetPosition({ 20, 10 });
    // Move up 5 lines (CPL).
    stateMachine.ProcessString(L"\x1b[5F");
    // We should stop on column 10, line 8, the top left margins.
    VERIFY_ARE_EQUAL(til::point(10, 8), cursor.GetPosition());

    Log::Comment(L"CNL below bottom");
    // Starting from column 20 of line 13 (1 below bottom margin).
    cursor.SetPosition({ 20, 13 });
    // Move down 5 lines (CNL).
    stateMachine.ProcessString(L"\x1b[5E");
    // We should end up in column 0 of line 18.
    VERIFY_ARE_EQUAL(til::point(0, 18), cursor.GetPosition());

    Log::Comment(L"CPL above top margin");
    // Starting from column 20 of line 7 (1 above top margin).
    cursor.SetPosition({ 20, 7 });
    // Move up 5 lines (CPL).
    stateMachine.ProcessString(L"\x1b[5F");
    // We should end up in column 0 of line 2.
    VERIFY_ARE_EQUAL(til::point(0, 2), cursor.GetPosition());
}

void ScreenBufferTests::CursorPositionRelative()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = si.GetTextBuffer().GetCursor();

    Log::Comment(L"Make sure the viewport is at 0,0");
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, til::point(0, 0), true));

    Log::Comment(L"HPR without margins");
    // Starting from column 20 of line 10.
    cursor.SetPosition({ 20, 10 });
    // Move forward 5 columns (HPR).
    stateMachine.ProcessString(L"\x1b[5a");
    // We should end up in column 25.
    VERIFY_ARE_EQUAL(til::point(25, 10), cursor.GetPosition());

    Log::Comment(L"VPR without margins");
    // Starting from column 20 of line 10.
    cursor.SetPosition({ 20, 10 });
    // Move down 5 lines (VPR).
    stateMachine.ProcessString(L"\x1b[5e");
    // We should end up on line 15.
    VERIFY_ARE_EQUAL(til::point(20, 15), cursor.GetPosition());

    // Enable DECLRMM margin mode (future proofing for when we support it)
    stateMachine.ProcessString(L"\x1b[?69h");
    // Set horizontal margins to 18:22 (19:23 in VT coordinates).
    stateMachine.ProcessString(L"\x1b[19;23s");
    // Set vertical margins to 8:12 (9:13 in VT coordinates).
    stateMachine.ProcessString(L"\x1b[9;13r");
    // Make sure we clear the margins on exit so they can't break other tests.
    auto clearMargins = wil::scope_exit([&] {
        stateMachine.ProcessString(L"\x1b[r");
        stateMachine.ProcessString(L"\x1b[s");
        stateMachine.ProcessString(L"\x1b[?69l");
    });

    Log::Comment(L"HPR inside margins");
    // Starting from column 20 of line 10.
    cursor.SetPosition({ 20, 10 });
    // Move forward 5 columns (HPR).
    stateMachine.ProcessString(L"\x1b[5a");
    // We should end up in column 25 (outside the right margin).
    VERIFY_ARE_EQUAL(til::point(25, 10), cursor.GetPosition());

    Log::Comment(L"VPR inside margins");
    // Starting from column 20 of line 10.
    cursor.SetPosition({ 20, 10 });
    // Move down 5 lines (VPR).
    stateMachine.ProcessString(L"\x1b[5e");
    // We should end up on line 15 (outside the bottom margin).
    VERIFY_ARE_EQUAL(til::point(20, 15), cursor.GetPosition());

    Log::Comment(L"HPR to end of line");
    // Starting from column 20 of line 10.
    cursor.SetPosition({ 20, 10 });
    // Move forward 9999 columns (HPR).
    stateMachine.ProcessString(L"\x1b[9999a");
    // We should end up in the rightmost column.
    const auto screenWidth = si.GetBufferSize().Width();
    VERIFY_ARE_EQUAL(til::point(screenWidth - 1, 10), cursor.GetPosition());

    Log::Comment(L"VPR to bottom of screen");
    // Starting from column 20 of line 10.
    cursor.SetPosition({ 20, 10 });
    // Move down 9999 lines (VPR).
    stateMachine.ProcessString(L"\x1b[9999e");
    // We should end up on the last line.
    const auto screenHeight = si.GetViewport().Height();
    VERIFY_ARE_EQUAL(til::point(20, screenHeight - 1), cursor.GetPosition());
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
    const auto saveCursor = L"\x1b\x37";
    const auto restoreCursor = L"\x1b\x38";
    const auto setDECOM = L"\x1b[?6h";
    const auto resetDECOM = L"\x1b[?6l";

    Log::Comment(L"Make sure the viewport is at 0,0");
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, til::point(0, 0), true));

    Log::Comment(L"Restore after save.");
    // Set the cursor position, delayed wrap, attributes, and character set.
    cursor.SetPosition({ 20, 10 });
    cursor.DelayEOLWrap();
    si.SetAttributes(colorAttrs);
    stateMachine.ProcessString(selectGraphicsChars);
    // Save state.
    stateMachine.ProcessString(saveCursor);
    // Reset the cursor position, delayed wrap, attributes, and character set.
    cursor.SetPosition({ 0, 0 });
    si.SetAttributes(defaultAttrs);
    stateMachine.ProcessString(selectAsciiChars);
    // Restore state.
    stateMachine.ProcessString(restoreCursor);
    // Verify initial position, delayed wrap, colors, and graphic character set.
    VERIFY_ARE_EQUAL(til::point(20, 10), cursor.GetPosition());
    VERIFY_IS_TRUE(cursor.IsDelayedEOLWrap());
    cursor.ResetDelayEOLWrap();
    VERIFY_ARE_EQUAL(colorAttrs, si.GetAttributes());
    stateMachine.ProcessString(asciiText);
    VERIFY_IS_TRUE(_ValidateLineContains({ 20, 10 }, graphicText, colorAttrs));

    Log::Comment(L"Restore again without save.");
    // Reset the cursor position, delayed wrap, attributes, and character set.
    cursor.SetPosition({ 0, 0 });
    si.SetAttributes(defaultAttrs);
    stateMachine.ProcessString(selectAsciiChars);
    // Restore state.
    stateMachine.ProcessString(restoreCursor);
    // Verify initial saved position, delayed wrap, colors, and graphic character set.
    VERIFY_ARE_EQUAL(til::point(20, 10), cursor.GetPosition());
    VERIFY_IS_TRUE(cursor.IsDelayedEOLWrap());
    cursor.ResetDelayEOLWrap();
    VERIFY_ARE_EQUAL(colorAttrs, si.GetAttributes());
    stateMachine.ProcessString(asciiText);
    VERIFY_IS_TRUE(_ValidateLineContains({ 20, 10 }, graphicText, colorAttrs));

    Log::Comment(L"Restore after reset.");
    // Soft reset.
    stateMachine.ProcessString(L"\x1b[!p");
    // Set the cursor position, delayed wrap, attributes, and character set.
    cursor.SetPosition({ 20, 10 });
    cursor.DelayEOLWrap();
    si.SetAttributes(colorAttrs);
    stateMachine.ProcessString(selectGraphicsChars);
    // Restore state.
    stateMachine.ProcessString(restoreCursor);
    // Verify home position, no delayed wrap, default attributes, and ascii character set.
    VERIFY_ARE_EQUAL(til::point(0, 0), cursor.GetPosition());
    VERIFY_IS_FALSE(cursor.IsDelayedEOLWrap());
    VERIFY_ARE_EQUAL(defaultAttrs, si.GetAttributes());
    stateMachine.ProcessString(asciiText);
    VERIFY_IS_TRUE(_ValidateLineContains(til::point(0, 0), asciiText, defaultAttrs));

    Log::Comment(L"Restore origin mode.");
    // Set margins and origin mode to relative.
    stateMachine.ProcessString(L"\x1b[?69h");
    stateMachine.ProcessString(L"\x1b[10;20r");
    stateMachine.ProcessString(L"\x1b[31;50s");
    stateMachine.ProcessString(setDECOM);
    // Verify home position inside margins.
    VERIFY_ARE_EQUAL(til::point(30, 9), cursor.GetPosition());
    // Save state and reset origin mode to absolute.
    stateMachine.ProcessString(saveCursor);
    stateMachine.ProcessString(resetDECOM);
    // Verify home position at origin.
    VERIFY_ARE_EQUAL(til::point(0, 0), cursor.GetPosition());
    // Restore state and move to home position.
    stateMachine.ProcessString(restoreCursor);
    stateMachine.ProcessString(L"\x1b[H");
    // Verify home position inside margins, i.e. relative origin mode restored.
    VERIFY_ARE_EQUAL(til::point(30, 9), cursor.GetPosition());

    Log::Comment(L"Restore relative to new origin.");
    // Reset margins, with absolute origin, and set cursor position.
    stateMachine.ProcessString(L"\x1b[r");
    stateMachine.ProcessString(L"\x1b[s");
    stateMachine.ProcessString(setDECOM);
    cursor.SetPosition({ 5, 5 });
    // Save state.
    stateMachine.ProcessString(saveCursor);
    // Set margins and restore state.
    stateMachine.ProcessString(L"\x1b[15;25r");
    stateMachine.ProcessString(L"\x1b[31;50s");
    stateMachine.ProcessString(restoreCursor);
    // Verify position is now relative to new top left margins
    VERIFY_ARE_EQUAL(til::point(35, 19), cursor.GetPosition());

    Log::Comment(L"Clamp inside bottom margin.");
    // Reset margins, with absolute origin, and set cursor position.
    stateMachine.ProcessString(L"\x1b[r");
    stateMachine.ProcessString(L"\x1b[s");
    stateMachine.ProcessString(setDECOM);
    cursor.SetPosition({ 15, 15 });
    // Save state.
    stateMachine.ProcessString(saveCursor);
    // Set margins and restore state.
    stateMachine.ProcessString(L"\x1b[1;10r");
    stateMachine.ProcessString(L"\x1b[1;10s");
    stateMachine.ProcessString(restoreCursor);
    // Verify position is clamped inside the bottom right margins
    VERIFY_ARE_EQUAL(til::point(9, 9), cursor.GetPosition());

    // Reset origin mode and margins.
    stateMachine.ProcessString(resetDECOM);
    stateMachine.ProcessString(L"\x1b[r");
    stateMachine.ProcessString(L"\x1b[s");
    stateMachine.ProcessString(L"\x1b[?69l");
}

void ScreenBufferTests::ScreenAlignmentPattern()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    auto& stateMachine = si.GetStateMachine();
    const auto& cursor = si.GetTextBuffer().GetCursor();
    WI_SetFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    auto areMarginsSet = [&]() {
        const auto margins = _GetRelativeScrollMargins();
        return margins.bottom > margins.top;
    };

    Log::Comment(L"Set the initial buffer state.");

    const auto bufferWidth = si.GetBufferSize().Width();
    const auto bufferHeight = si.GetBufferSize().Height();

    // Move the viewport down a few lines, and only cover part of the buffer width.
    si.SetViewport(Viewport::FromDimensions({ 5, 10 }, { bufferWidth - 10, 30 }), true);
    const auto viewportStart = si.GetViewport().Top();
    const auto viewportEnd = si.GetViewport().BottomExclusive();

    // Fill the entire buffer with Zs. Blue on Green.
    const auto bufferAttr = TextAttribute{ FOREGROUND_BLUE | BACKGROUND_GREEN };
    _FillLines(0, bufferHeight, L'Z', bufferAttr);

    // Set the initial attributes.
    auto initialAttr = TextAttribute{ RGB(12, 34, 56), RGB(78, 90, 12) };
    initialAttr.SetReverseVideo(true);
    initialAttr.SetUnderlined(true);
    si.SetAttributes(initialAttr);

    // Set some margins.
    stateMachine.ProcessString(L"\x1b[10;20r");
    VERIFY_IS_TRUE(areMarginsSet());

    // Place the cursor in the center.
    auto cursorPos = til::point{ bufferWidth / 2, (viewportStart + viewportEnd) / 2 };
    VERIFY_SUCCEEDED(si.SetCursorPosition(cursorPos, true));

    Log::Comment(L"Execute the DECALN escape sequence.");
    stateMachine.ProcessString(L"\x1b#8");

    Log::Comment(L"Lines within view should be filled with Es, with default attributes.");
    auto defaultAttr = TextAttribute{};
    VERIFY_IS_TRUE(_ValidateLinesContain(viewportStart, viewportEnd, L'E', defaultAttr));

    Log::Comment(L"Field of Zs outside viewport should remain unchanged.");
    VERIFY_IS_TRUE(_ValidateLinesContain(0, viewportStart, L'Z', bufferAttr));
    VERIFY_IS_TRUE(_ValidateLinesContain(viewportEnd, bufferHeight, L'Z', bufferAttr));

    Log::Comment(L"Margins should not be set.");
    VERIFY_IS_FALSE(areMarginsSet());

    Log::Comment(L"Cursor position should be moved to home.");
    auto homePosition = til::point{ 0, viewportStart };
    VERIFY_ARE_EQUAL(homePosition, cursor.GetPosition());

    Log::Comment(L"Meta/rendition attributes should be reset.");
    auto expectedAttr = initialAttr;
    expectedAttr.SetStandardErase();
    VERIFY_ARE_EQUAL(expectedAttr, si.GetAttributes());
}

void ScreenBufferTests::TestCursorIsOn()
{
    auto& g = ServiceLocator::LocateGlobals();
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& cursor = tbi.GetCursor();

    stateMachine.ProcessString(L"Hello World");
    VERIFY_IS_TRUE(cursor.IsOn());
    VERIFY_IS_TRUE(cursor.IsBlinkingAllowed());
    VERIFY_IS_TRUE(cursor.IsVisible());

    stateMachine.ProcessString(L"\x1b[?12l");
    VERIFY_IS_TRUE(cursor.IsOn());
    VERIFY_IS_FALSE(cursor.IsBlinkingAllowed());
    VERIFY_IS_TRUE(cursor.IsVisible());

    stateMachine.ProcessString(L"\x1b[?12h");
    VERIFY_IS_TRUE(cursor.IsOn());
    VERIFY_IS_TRUE(cursor.IsBlinkingAllowed());
    VERIFY_IS_TRUE(cursor.IsVisible());

    cursor.SetIsOn(false);
    stateMachine.ProcessString(L"\x1b[?12l");
    VERIFY_IS_TRUE(cursor.IsOn());
    VERIFY_IS_FALSE(cursor.IsBlinkingAllowed());
    VERIFY_IS_TRUE(cursor.IsVisible());

    stateMachine.ProcessString(L"\x1b[?12h");
    VERIFY_IS_TRUE(cursor.IsOn());
    VERIFY_IS_TRUE(cursor.IsBlinkingAllowed());
    VERIFY_IS_TRUE(cursor.IsVisible());

    stateMachine.ProcessString(L"\x1b[?25l");
    VERIFY_IS_TRUE(cursor.IsOn());
    VERIFY_IS_TRUE(cursor.IsBlinkingAllowed());
    VERIFY_IS_FALSE(cursor.IsVisible());

    stateMachine.ProcessString(L"\x1b[?25h");
    VERIFY_IS_TRUE(cursor.IsOn());
    VERIFY_IS_TRUE(cursor.IsBlinkingAllowed());
    VERIFY_IS_TRUE(cursor.IsVisible());

    stateMachine.ProcessString(L"\x1b[?12;25l");
    VERIFY_IS_TRUE(cursor.IsOn());
    VERIFY_IS_FALSE(cursor.IsBlinkingAllowed());
    VERIFY_IS_FALSE(cursor.IsVisible());
}

void ScreenBufferTests::TestAddHyperlink()
{
    auto& g = ServiceLocator::LocateGlobals();
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();

    // Process the opening osc 8 sequence with no custom id
    stateMachine.ProcessString(L"\x1b]8;;test.url\x1b\\");
    VERIFY_IS_TRUE(tbi.GetCurrentAttributes().IsHyperlink());
    VERIFY_ARE_EQUAL(tbi.GetHyperlinkUriFromId(tbi.GetCurrentAttributes().GetHyperlinkId()), L"test.url");

    // Send any other text
    stateMachine.ProcessString(L"Hello World");
    VERIFY_IS_TRUE(tbi.GetCurrentAttributes().IsHyperlink());
    VERIFY_ARE_EQUAL(tbi.GetHyperlinkUriFromId(tbi.GetCurrentAttributes().GetHyperlinkId()), L"test.url");

    // Process the closing osc 8 sequences
    stateMachine.ProcessString(L"\x1b]8;;\x1b\\");
    VERIFY_IS_FALSE(tbi.GetCurrentAttributes().IsHyperlink());
}

void ScreenBufferTests::TestAddHyperlinkCustomId()
{
    auto& g = ServiceLocator::LocateGlobals();
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();

    // Process the opening osc 8 sequence with a custom id
    stateMachine.ProcessString(L"\x1b]8;id=myId;test.url\x1b\\");
    VERIFY_IS_TRUE(tbi.GetCurrentAttributes().IsHyperlink());
    VERIFY_ARE_EQUAL(tbi.GetHyperlinkUriFromId(tbi.GetCurrentAttributes().GetHyperlinkId()), L"test.url");
    VERIFY_ARE_EQUAL(tbi.GetHyperlinkId(L"test.url", L"myId"), tbi.GetCurrentAttributes().GetHyperlinkId());

    // Send any other text
    stateMachine.ProcessString(L"Hello World");
    VERIFY_IS_TRUE(tbi.GetCurrentAttributes().IsHyperlink());
    VERIFY_ARE_EQUAL(tbi.GetHyperlinkUriFromId(tbi.GetCurrentAttributes().GetHyperlinkId()), L"test.url");
    VERIFY_ARE_EQUAL(tbi.GetHyperlinkId(L"test.url", L"myId"), tbi.GetCurrentAttributes().GetHyperlinkId());

    // Process the closing osc 8 sequences
    stateMachine.ProcessString(L"\x1b]8;;\x1b\\");
    VERIFY_IS_FALSE(tbi.GetCurrentAttributes().IsHyperlink());
}

void ScreenBufferTests::TestAddHyperlinkCustomIdDifferentUri()
{
    auto& g = ServiceLocator::LocateGlobals();
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& tbi = si.GetTextBuffer();
    auto& stateMachine = si.GetStateMachine();

    // Process the opening osc 8 sequence with a custom id
    stateMachine.ProcessString(L"\x1b]8;id=myId;test.url\x1b\\");
    VERIFY_IS_TRUE(tbi.GetCurrentAttributes().IsHyperlink());
    VERIFY_ARE_EQUAL(tbi.GetHyperlinkUriFromId(tbi.GetCurrentAttributes().GetHyperlinkId()), L"test.url");
    VERIFY_ARE_EQUAL(tbi.GetHyperlinkId(L"test.url", L"myId"), tbi.GetCurrentAttributes().GetHyperlinkId());

    const auto oldAttributes{ tbi.GetCurrentAttributes() };

    // Send any other text
    stateMachine.ProcessString(L"\x1b]8;id=myId;other.url\x1b\\");
    VERIFY_IS_TRUE(tbi.GetCurrentAttributes().IsHyperlink());
    VERIFY_ARE_EQUAL(tbi.GetHyperlinkUriFromId(tbi.GetCurrentAttributes().GetHyperlinkId()), L"other.url");
    VERIFY_ARE_EQUAL(tbi.GetHyperlinkId(L"other.url", L"myId"), tbi.GetCurrentAttributes().GetHyperlinkId());

    // This second URL should not change the URL of the original ID!
    VERIFY_ARE_EQUAL(tbi.GetHyperlinkUriFromId(oldAttributes.GetHyperlinkId()), L"test.url");
    VERIFY_ARE_NOT_EQUAL(oldAttributes.GetHyperlinkId(), tbi.GetCurrentAttributes().GetHyperlinkId());
}

void ScreenBufferTests::UpdateVirtualBottomWhenCursorMovesBelowIt()
{
    auto& g = ServiceLocator::LocateGlobals();
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& cursor = si.GetTextBuffer().GetCursor();

    Log::Comment(L"Make sure the initial virtual bottom is at the bottom of the viewport");
    si.UpdateBottom();
    const auto initialVirtualBottom = si.GetViewport().BottomInclusive();
    VERIFY_ARE_EQUAL(initialVirtualBottom, si._virtualBottom);

    Log::Comment(L"Set the initial cursor position on that virtual bottom line");
    const auto initialCursorPos = til::point{ 0, initialVirtualBottom };
    cursor.SetPosition(initialCursorPos);
    VERIFY_ARE_EQUAL(initialCursorPos, cursor.GetPosition());

    Log::Comment(L"Pan down so the initial viewport has the cursor in the middle");
    const auto initialOrigin = til::point{ 0, si.GetViewport().Top() + si.GetViewport().Height() / 2 };
    gci.SetTerminalScrolling(false);
    VERIFY_SUCCEEDED(si.SetViewportOrigin(false, initialOrigin, false));
    VERIFY_ARE_EQUAL(initialOrigin, si.GetViewport().Origin());

    Log::Comment(L"Confirm that the virtual bottom has not changed");
    VERIFY_ARE_EQUAL(initialVirtualBottom, si._virtualBottom);

    Log::Comment(L"Now write several lines of content using WriteCharsLegacy");
    const auto content = L"1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n";
    auto numBytes = wcslen(content) * sizeof(wchar_t);
    VERIFY_NT_SUCCESS(WriteCharsLegacy(si, content, content, content, &numBytes, nullptr, 0, 0, nullptr));

    Log::Comment(L"Confirm that the cursor position has moved down 10 lines");
    const auto newCursorPos = til::point{ initialCursorPos.x, initialCursorPos.y + 10 };
    VERIFY_ARE_EQUAL(newCursorPos, cursor.GetPosition());

    Log::Comment(L"Confirm that the virtual bottom matches that new cursor position");
    const auto newVirtualBottom = newCursorPos.y;
    VERIFY_ARE_EQUAL(newVirtualBottom, si._virtualBottom);

    Log::Comment(L"The viewport itself should not have changed at this point");
    VERIFY_ARE_EQUAL(initialOrigin, si.GetViewport().Origin());

    Log::Comment(L"But after moving to the virtual viewport, we should align with the new virtual bottom");
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, si.GetVirtualViewport().Origin(), true));
    VERIFY_ARE_EQUAL(newVirtualBottom, si.GetViewport().BottomInclusive());
}

void ScreenBufferTests::UpdateVirtualBottomWithSetConsoleCursorPosition()
{
    auto& g = ServiceLocator::LocateGlobals();
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& cursor = si.GetTextBuffer().GetCursor();

    Log::Comment(L"Pan down so the initial viewport is a couple of pages down");
    const auto initialOrigin = til::point{ 0, si.GetViewport().Height() * 2 };
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, initialOrigin, true));
    VERIFY_ARE_EQUAL(initialOrigin, si.GetViewport().Origin());

    Log::Comment(L"Make sure the initial virtual bottom is at the bottom of the viewport");
    const auto initialVirtualBottom = si.GetViewport().BottomInclusive();
    VERIFY_ARE_EQUAL(initialVirtualBottom, si._virtualBottom);

    Log::Comment(L"Pan to the top of the buffer without changing the virtual bottom");
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, { 0, 0 }, false));
    VERIFY_ARE_EQUAL(initialVirtualBottom, si._virtualBottom);

    Log::Comment(L"Set the cursor position to the initial origin");
    VERIFY_SUCCEEDED(g.api->SetConsoleCursorPositionImpl(si, initialOrigin));
    VERIFY_ARE_EQUAL(initialOrigin, cursor.GetPosition());

    Log::Comment(L"Confirm that the viewport has moved down");
    VERIFY_ARE_EQUAL(initialOrigin, si.GetViewport().Origin());

    Log::Comment(L"Confirm that the virtual bottom has not changed");
    VERIFY_ARE_EQUAL(initialVirtualBottom, si._virtualBottom);

    Log::Comment(L"Pan further down so the viewport is below the cursor");
    const auto belowCursor = til::point{ 0, cursor.GetPosition().y + 10 };
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, belowCursor, false));
    VERIFY_ARE_EQUAL(belowCursor, si.GetViewport().Origin());

    Log::Comment(L"Set the cursor position one line down, still inside the virtual viewport");
    const auto oneLineDown = til::point{ 0, cursor.GetPosition().y + 1 };
    VERIFY_SUCCEEDED(g.api->SetConsoleCursorPositionImpl(si, oneLineDown));
    VERIFY_ARE_EQUAL(oneLineDown, cursor.GetPosition());

    Log::Comment(L"Confirm that the viewport has moved back to the initial origin");
    VERIFY_ARE_EQUAL(initialOrigin, si.GetViewport().Origin());

    Log::Comment(L"Confirm that the virtual bottom has not changed");
    VERIFY_ARE_EQUAL(initialVirtualBottom, si._virtualBottom);

    Log::Comment(L"Set the cursor position to the top of the buffer");
    const auto topOfBuffer = til::point{ 0, 0 };
    VERIFY_SUCCEEDED(g.api->SetConsoleCursorPositionImpl(si, topOfBuffer));
    VERIFY_ARE_EQUAL(topOfBuffer, cursor.GetPosition());

    Log::Comment(L"Confirm that the viewport has moved to the top of the buffer");
    VERIFY_ARE_EQUAL(topOfBuffer, si.GetViewport().Origin());

    Log::Comment(L"Confirm that the virtual bottom has also moved up");
    VERIFY_ARE_EQUAL(si.GetViewport().BottomInclusive(), si._virtualBottom);
}

void ScreenBufferTests::UpdateVirtualBottomAfterInternalSetViewportSize()
{
    auto& g = ServiceLocator::LocateGlobals();
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& cursor = si.GetTextBuffer().GetCursor();
    auto& stateMachine = si.GetStateMachine();

    Log::Comment(L"Pan down so the initial viewport is a couple of pages down");
    const auto initialOrigin = til::point{ 0, si.GetViewport().Height() * 2 };
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, initialOrigin, true));
    VERIFY_ARE_EQUAL(initialOrigin, si.GetViewport().Origin());

    Log::Comment(L"Make sure the initial virtual bottom is at the bottom of the viewport");
    const auto initialVirtualBottom = si.GetViewport().BottomInclusive();
    VERIFY_ARE_EQUAL(initialVirtualBottom, si._virtualBottom);

    Log::Comment(L"Set the cursor to the bottom of the current page");
    stateMachine.ProcessString(L"\033[9999H");
    VERIFY_ARE_EQUAL(initialVirtualBottom, cursor.GetPosition().y);

    Log::Comment(L"Pan to the top of the buffer without changing the virtual bottom");
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, { 0, 0 }, false));
    VERIFY_ARE_EQUAL(initialVirtualBottom, si._virtualBottom);

    Log::Comment(L"Shrink the viewport height by two lines");
    auto viewportSize = si.GetViewport().Dimensions();
    viewportSize.height -= 2;
    si._InternalSetViewportSize(&viewportSize, false, false);
    VERIFY_ARE_EQUAL(viewportSize, si.GetViewport().Dimensions());

    Log::Comment(L"Confirm that the virtual bottom has not changed");
    VERIFY_ARE_EQUAL(initialVirtualBottom, si._virtualBottom);

    Log::Comment(L"Position the viewport just above the virtual bottom");
    auto viewportTop = si._virtualBottom - viewportSize.height;
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, { 0, viewportTop }, false));
    VERIFY_ARE_EQUAL(si._virtualBottom - 1, si.GetViewport().BottomInclusive());

    Log::Comment(L"Expand the viewport height so it 'passes through' the virtual bottom");
    viewportSize.height += 2;
    si._InternalSetViewportSize(&viewportSize, false, false);
    VERIFY_ARE_EQUAL(viewportSize, si.GetViewport().Dimensions());

    Log::Comment(L"Confirm that the virtual bottom has aligned with the viewport bottom");
    VERIFY_ARE_EQUAL(si._virtualBottom, si.GetViewport().BottomInclusive());

    Log::Comment(L"Position the viewport bottom just below the virtual bottom");
    viewportTop = si._virtualBottom - viewportSize.height + 2;
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, { 0, viewportTop }, false));
    VERIFY_ARE_EQUAL(si._virtualBottom + 1, si.GetViewport().BottomInclusive());

    Log::Comment(L"Shrink the viewport height so it 'passes through' the virtual bottom");
    viewportSize.height -= 2;
    si._InternalSetViewportSize(&viewportSize, false, false);
    VERIFY_ARE_EQUAL(viewportSize, si.GetViewport().Dimensions());

    Log::Comment(L"Confirm that the virtual bottom has aligned with the viewport bottom");
    VERIFY_ARE_EQUAL(si._virtualBottom, si.GetViewport().BottomInclusive());
}

void ScreenBufferTests::UpdateVirtualBottomAfterResizeWithReflow()
{
    auto& g = ServiceLocator::LocateGlobals();
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& cursor = si.GetTextBuffer().GetCursor();
    auto& stateMachine = si.GetStateMachine();

    Log::Comment(L"Output a couple of pages of content");
    auto bufferSize = si.GetTextBuffer().GetSize().Dimensions();
    const auto viewportSize = si.GetViewport().Dimensions();
    const auto line = std::wstring(bufferSize.width - 1, L'X') + L'\n';
    for (auto i = 0; i < viewportSize.height * 2; i++)
    {
        stateMachine.ProcessString(line);
    }

    Log::Comment(L"Set the cursor to the top of the current page");
    stateMachine.ProcessString(L"\033[H");
    VERIFY_ARE_EQUAL(si.GetViewport().Origin(), cursor.GetPosition());

    Log::Comment(L"Pan to the top of the buffer without changing the virtual bottom");
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, { 0, 0 }, false));

    Log::Comment(L"Shrink the viewport width by a half");
    bufferSize.width /= 2;
    VERIFY_NT_SUCCESS(si.ResizeWithReflow(bufferSize));

    Log::Comment(L"Confirm that the virtual viewport includes the last non-space row");
    const auto lastNonSpaceRow = si.GetTextBuffer().GetLastNonSpaceCharacter().y;
    VERIFY_IS_GREATER_THAN_OR_EQUAL(si._virtualBottom, lastNonSpaceRow);

    Log::Comment(L"Clear the screen and note the cursor distance to the virtual bottom");
    stateMachine.ProcessString(L"\033[H\033[2J");
    const auto cursorDistanceFromBottom = si._virtualBottom - si.GetTextBuffer().GetCursor().GetPosition().y;
    VERIFY_ARE_EQUAL(si.GetViewport().Height() - 1, cursorDistanceFromBottom);

    Log::Comment(L"Stretch the viewport back to full width");
    bufferSize.width *= 2;
    VERIFY_NT_SUCCESS(si.ResizeWithReflow(bufferSize));

    Log::Comment(L"Confirm cursor distance to the virtual bottom is unchanged");
    VERIFY_ARE_EQUAL(cursorDistanceFromBottom, si._virtualBottom - si.GetTextBuffer().GetCursor().GetPosition().y);
}

void ScreenBufferTests::DontShrinkVirtualBottomDuringResizeWithReflowAtTop()
{
    auto& g = ServiceLocator::LocateGlobals();
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& cursor = si.GetTextBuffer().GetCursor();
    auto& stateMachine = si.GetStateMachine();

    Log::Comment(L"Make sure the viewport is at the top of the buffer");
    const auto bufferTop = til::point{ 0, 0 };
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, bufferTop, true));
    VERIFY_ARE_EQUAL(bufferTop, si.GetViewport().Origin());

    Log::Comment(L"Make sure the virtual bottom is at the bottom of the viewport");
    VERIFY_ARE_EQUAL(si.GetViewport().BottomInclusive(), si._virtualBottom);

    Log::Comment(L"Make sure the cursor is at the top of the buffer");
    stateMachine.ProcessString(L"\033[H");
    VERIFY_ARE_EQUAL(bufferTop, cursor.GetPosition());

    Log::Comment(L"Shrink the viewport width by a half");
    auto bufferSize = si.GetTextBuffer().GetSize().Dimensions();
    bufferSize.width /= 2;
    VERIFY_NT_SUCCESS(si.ResizeWithReflow(bufferSize));

    Log::Comment(L"Confirm that the virtual bottom is still at the bottom of the viewport");
    VERIFY_ARE_EQUAL(si.GetViewport().BottomInclusive(), si._virtualBottom);
}

void ScreenBufferTests::DontChangeVirtualBottomWithOffscreenLinefeed()
{
    auto& g = ServiceLocator::LocateGlobals();
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& cursor = si.GetTextBuffer().GetCursor();
    auto& stateMachine = si.GetStateMachine();

    Log::Comment(L"Pan down so the initial viewport is a couple of pages down");
    const auto initialOrigin = til::point{ 0, si.GetViewport().Height() * 2 };
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, initialOrigin, true));
    VERIFY_ARE_EQUAL(initialOrigin, si.GetViewport().Origin());

    Log::Comment(L"Make sure the initial virtual bottom is at the bottom of the viewport");
    const auto initialVirtualBottom = si.GetViewport().BottomInclusive();
    VERIFY_ARE_EQUAL(initialVirtualBottom, si._virtualBottom);

    Log::Comment(L"Set the cursor to the top of the current page");
    stateMachine.ProcessString(L"\033[H");
    VERIFY_ARE_EQUAL(initialOrigin, cursor.GetPosition());

    Log::Comment(L"Pan to the top of the buffer without changing the virtual bottom");
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, { 0, 0 }, false));
    VERIFY_ARE_EQUAL(initialVirtualBottom, si._virtualBottom);

    Log::Comment(L"Output a line feed");
    stateMachine.ProcessString(L"\n");

    Log::Comment(L"Confirm that the virtual bottom has not changed");
    VERIFY_ARE_EQUAL(initialVirtualBottom, si._virtualBottom);
}

void ScreenBufferTests::DontChangeVirtualBottomAfterResizeWindow()
{
    auto& g = ServiceLocator::LocateGlobals();
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& cursor = si.GetTextBuffer().GetCursor();
    auto& stateMachine = si.GetStateMachine();

    Log::Comment(L"Pan down so the initial viewport is a couple of pages down");
    const auto initialOrigin = til::point{ 0, si.GetViewport().Height() * 2 };
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, initialOrigin, true));
    VERIFY_ARE_EQUAL(initialOrigin, si.GetViewport().Origin());

    Log::Comment(L"Make sure the initial virtual bottom is at the bottom of the viewport");
    const auto initialVirtualBottom = si.GetViewport().BottomInclusive();
    VERIFY_ARE_EQUAL(initialVirtualBottom, si._virtualBottom);

    Log::Comment(L"Set the cursor to the bottom of the current page");
    stateMachine.ProcessString(L"\033[9999H");
    VERIFY_ARE_EQUAL(initialVirtualBottom, cursor.GetPosition().y);

    Log::Comment(L"Pan to the top of the buffer without changing the virtual bottom");
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, { 0, 0 }, false));
    VERIFY_ARE_EQUAL(initialVirtualBottom, si._virtualBottom);

    Log::Comment(L"Shrink the viewport height");
    std::wstringstream ss;
    auto viewportWidth = si.GetViewport().Width();
    auto viewportHeight = si.GetViewport().Height() - 2;
    ss << L"\x1b[8;" << viewportHeight << L";" << viewportWidth << L"t";
    stateMachine.ProcessString(ss.str());
    VERIFY_ARE_EQUAL(viewportHeight, si.GetViewport().Height());

    Log::Comment(L"Confirm that the virtual bottom has not changed");
    VERIFY_ARE_EQUAL(initialVirtualBottom, si._virtualBottom);
}

void ScreenBufferTests::DontChangeVirtualBottomWithMakeCursorVisible()
{
    auto& g = ServiceLocator::LocateGlobals();
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& cursor = si.GetTextBuffer().GetCursor();
    auto& stateMachine = si.GetStateMachine();

    Log::Comment(L"Pan down so the initial viewport is a couple of pages down");
    const auto initialOrigin = til::point{ 0, si.GetViewport().Height() * 2 };
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, initialOrigin, true));
    VERIFY_ARE_EQUAL(initialOrigin, si.GetViewport().Origin());

    Log::Comment(L"Make sure the initial virtual bottom is at the bottom of the viewport");
    const auto initialVirtualBottom = si.GetViewport().BottomInclusive();
    VERIFY_ARE_EQUAL(initialVirtualBottom, si._virtualBottom);

    Log::Comment(L"Set the cursor to the top of the current page");
    stateMachine.ProcessString(L"\033[H");
    VERIFY_ARE_EQUAL(initialOrigin, cursor.GetPosition());

    Log::Comment(L"Pan to the top of the buffer without changing the virtual bottom");
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, { 0, 0 }, false));
    VERIFY_ARE_EQUAL(initialVirtualBottom, si._virtualBottom);

    Log::Comment(L"Make the cursor visible");
    si.MakeCurrentCursorVisible();
    VERIFY_ARE_EQUAL(si.GetViewport().BottomInclusive(), cursor.GetPosition().y);

    Log::Comment(L"Confirm that the virtual bottom has not changed");
    VERIFY_ARE_EQUAL(initialVirtualBottom, si._virtualBottom);

    Log::Comment(L"Pan further down so the viewport is below the cursor");
    const auto belowCursor = til::point{ 0, cursor.GetPosition().y + 10 };
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, belowCursor, false));
    VERIFY_ARE_EQUAL(belowCursor, si.GetViewport().Origin());

    Log::Comment(L"Make the cursor visible");
    si.MakeCurrentCursorVisible();
    VERIFY_ARE_EQUAL(si.GetViewport().Top(), cursor.GetPosition().y);

    Log::Comment(L"Confirm that the virtual bottom has not changed");
    VERIFY_ARE_EQUAL(initialVirtualBottom, si._virtualBottom);
}

void ScreenBufferTests::RetainHorizontalOffsetWhenMovingToBottom()
{
    auto& g = ServiceLocator::LocateGlobals();
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& cursor = si.GetTextBuffer().GetCursor();

    Log::Comment(L"Make the viewport half the default width");
    auto initialSize = til::size{ CommonState::s_csWindowWidth / 2, CommonState::s_csWindowHeight };
    si.SetViewportSize(&initialSize);

    Log::Comment(L"Offset the viewport both vertically and horizontally");
    auto initialOrigin = til::point{ 10, 20 };
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, initialOrigin, true));

    Log::Comment(L"Verify that the virtual viewport is where it's expected to be");
    VERIFY_ARE_EQUAL(initialSize, si.GetVirtualViewport().Dimensions());
    VERIFY_ARE_EQUAL(initialOrigin, si.GetVirtualViewport().Origin());

    Log::Comment(L"Set the cursor position at the viewport origin");
    cursor.SetPosition(initialOrigin);
    VERIFY_ARE_EQUAL(initialOrigin, cursor.GetPosition());

    Log::Comment(L"Pan the viewport up by 10 lines");
    VERIFY_SUCCEEDED(si.SetViewportOrigin(false, { 0, -10 }, false));

    Log::Comment(L"Verify Y offset has moved up and X is unchanged");
    VERIFY_ARE_EQUAL(initialOrigin.y - 10, si.GetViewport().Top());
    VERIFY_ARE_EQUAL(initialOrigin.x, si.GetViewport().Left());

    Log::Comment(L"Move the viewport back to the virtual bottom");
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, si.GetVirtualViewport().Origin(), true));

    Log::Comment(L"Verify Y offset has moved back and X is unchanged");
    VERIFY_ARE_EQUAL(initialOrigin.y, si.GetViewport().Top());
    VERIFY_ARE_EQUAL(initialOrigin.x, si.GetViewport().Left());
}

void ScreenBufferTests::TestWriteConsoleVTQuirkMode()
{
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:useQuirk", L"{false, true}")
    END_TEST_METHOD_PROPERTIES()

    bool useQuirk;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"useQuirk", useQuirk), L"whether to enable the quirk");

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.LockConsole(); // Lock must be taken to manipulate buffer.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    auto& mainBuffer = gci.GetActiveOutputBuffer();
    auto& cursor = mainBuffer.GetTextBuffer().GetCursor();
    // Make sure we're in VT mode
    WI_SetFlag(mainBuffer.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    const TextAttribute defaultAttribute{};
    // Make sure we're using the default attributes at the start of the test,
    // Otherwise they could be polluted from a previous test.
    mainBuffer.SetAttributes(defaultAttribute);

    const auto verifyLastAttribute = [&](const TextAttribute& expected) {
        const auto& row = mainBuffer.GetTextBuffer().GetRowByOffset(cursor.GetPosition().y);
        auto iter{ row.AttrBegin() };
        iter += cursor.GetPosition().x - 1;
        VERIFY_ARE_EQUAL(expected, *iter);
    };

    std::unique_ptr<WriteData> waiter;

    std::wstring seq{};
    size_t seqCb{ 0 };

    /* Write red on blue, verify that it comes through */
    {
        TextAttribute vtRedOnBlueAttribute{};
        vtRedOnBlueAttribute.SetForeground(TextColor{ TextColor::DARK_RED, false });
        vtRedOnBlueAttribute.SetBackground(TextColor{ TextColor::DARK_BLUE, false });

        seq = L"\x1b[31;44m";
        seqCb = 2 * seq.size();
        VERIFY_SUCCEEDED(DoWriteConsole(&seq[0], &seqCb, mainBuffer, useQuirk, waiter));

        VERIFY_ARE_EQUAL(vtRedOnBlueAttribute, mainBuffer.GetAttributes());

        seq = L"X";
        seqCb = 2 * seq.size();
        VERIFY_SUCCEEDED(DoWriteConsole(&seq[0], &seqCb, mainBuffer, useQuirk, waiter));

        verifyLastAttribute(vtRedOnBlueAttribute);
    }

    /* Write white on black, verify that it acts as expected for the quirk mode */
    {
        TextAttribute vtWhiteOnBlackAttribute{};
        vtWhiteOnBlackAttribute.SetForeground(TextColor{ TextColor::DARK_WHITE, false });
        vtWhiteOnBlackAttribute.SetBackground(TextColor{ TextColor::DARK_BLACK, false });

        const auto quirkExpectedAttribute{ useQuirk ? defaultAttribute : vtWhiteOnBlackAttribute };

        seq = L"\x1b[37;40m"; // the quirk should suppress this, turning it into "defaults"
        seqCb = 2 * seq.size();
        VERIFY_SUCCEEDED(DoWriteConsole(&seq[0], &seqCb, mainBuffer, useQuirk, waiter));

        VERIFY_ARE_EQUAL(quirkExpectedAttribute, mainBuffer.GetAttributes());

        seq = L"X";
        seqCb = 2 * seq.size();
        VERIFY_SUCCEEDED(DoWriteConsole(&seq[0], &seqCb, mainBuffer, useQuirk, waiter));

        verifyLastAttribute(quirkExpectedAttribute);
    }

    /* Write bright white on black, verify that it acts as expected for the quirk mode */
    {
        TextAttribute vtBrightWhiteOnBlackAttribute{};
        vtBrightWhiteOnBlackAttribute.SetForeground(TextColor{ TextColor::DARK_WHITE, false });
        vtBrightWhiteOnBlackAttribute.SetBackground(TextColor{ TextColor::DARK_BLACK, false });
        vtBrightWhiteOnBlackAttribute.SetIntense(true);

        auto vtBrightWhiteOnDefaultAttribute{ vtBrightWhiteOnBlackAttribute }; // copy the above attribute
        vtBrightWhiteOnDefaultAttribute.SetDefaultBackground();

        const auto quirkExpectedAttribute{ useQuirk ? vtBrightWhiteOnDefaultAttribute : vtBrightWhiteOnBlackAttribute };

        seq = L"\x1b[1;37;40m"; // the quirk should suppress black only, turning it into "default background"
        seqCb = 2 * seq.size();
        VERIFY_SUCCEEDED(DoWriteConsole(&seq[0], &seqCb, mainBuffer, useQuirk, waiter));

        VERIFY_ARE_EQUAL(quirkExpectedAttribute, mainBuffer.GetAttributes());

        seq = L"X";
        seqCb = 2 * seq.size();
        VERIFY_SUCCEEDED(DoWriteConsole(&seq[0], &seqCb, mainBuffer, useQuirk, waiter));

        verifyLastAttribute(quirkExpectedAttribute);
    }

    /* Write a 256-color white on a 256-color black, make sure the quirk does not suppress it */
    {
        TextAttribute vtWhiteOnBlack256Attribute{};
        vtWhiteOnBlack256Attribute.SetForeground(TextColor{ TextColor::DARK_WHITE, true });
        vtWhiteOnBlack256Attribute.SetBackground(TextColor{ TextColor::DARK_BLACK, true });

        // reset (disable intense from the last test) before setting both colors
        seq = L"\x1b[m\x1b[38;5;7;48;5;0m"; // the quirk should *not* suppress this (!)
        seqCb = 2 * seq.size();
        VERIFY_SUCCEEDED(DoWriteConsole(&seq[0], &seqCb, mainBuffer, useQuirk, waiter));

        VERIFY_ARE_EQUAL(vtWhiteOnBlack256Attribute, mainBuffer.GetAttributes());

        seq = L"X";
        seqCb = 2 * seq.size();
        VERIFY_SUCCEEDED(DoWriteConsole(&seq[0], &seqCb, mainBuffer, useQuirk, waiter));

        verifyLastAttribute(vtWhiteOnBlack256Attribute);
    }
}

void ScreenBufferTests::TestReflowEndOfLineColor()
{
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:dx", L"{-1, 0, 1}")
        TEST_METHOD_PROPERTY(L"Data:dy", L"{-1, 0, 1}")
    END_TEST_METHOD_PROPERTIES();

    INIT_TEST_PROPERTY(int, dx, L"The change in width of the buffer");
    INIT_TEST_PROPERTY(int, dy, L"The change in height of the buffer");

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& stateMachine = si.GetStateMachine();

    gci.SetWrapText(true);

    auto defaultAttrs = si.GetAttributes();
    auto red = defaultAttrs;
    red.SetIndexedBackground(TextColor::DARK_RED);
    auto green = defaultAttrs;
    green.SetIndexedBackground(TextColor::DARK_GREEN);
    auto blue = defaultAttrs;
    blue.SetIndexedBackground(TextColor::DARK_BLUE);
    auto yellow = defaultAttrs;
    yellow.SetIndexedBackground(TextColor::DARK_YELLOW);

    Log::Comment(L"Make sure the viewport is at 0,0");
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, til::point(0, 0), true));

    Log::Comment(L"Fill buffer with some data");
    stateMachine.ProcessString(L"\x1b[H");
    stateMachine.ProcessString(L"\x1b[41m"); // Red BG
    stateMachine.ProcessString(L"AAAAA"); // AAAAA
    stateMachine.ProcessString(L"\x1b[42m"); // Green BG
    stateMachine.ProcessString(L"\nBBBBB\n"); // BBBBB
    stateMachine.ProcessString(L"\x1b[44m"); // Blue BG
    stateMachine.ProcessString(L" CCC \n"); // " abc " (with spaces on either side)
    stateMachine.ProcessString(L"\x1b[43m"); // yellow BG
    stateMachine.ProcessString(L"\U0001F643\n"); // GH#12837: Support for surrogate characters during reflow
    stateMachine.ProcessString(L"\x1b[K"); // Erase line
    stateMachine.ProcessString(L"\x1b[2;6H"); // move the cursor to the end of the BBBBB's

    auto verifyBuffer = [&](const TextBuffer& tb, const til::rect& /*viewport*/, const bool /*before*/) {
        const auto width = tb.GetSize().Width();
        Log::Comment(NoThrowString().Format(L"Buffer width: %d", width));

        auto iter0 = TestUtils::VerifyLineContains(tb, { 0, 0 }, L'A', red, 5u);
        TestUtils::VerifyLineContains(iter0, L' ', defaultAttrs, static_cast<size_t>(width - 5));

        auto iter1 = tb.GetCellLineDataAt({ 0, 1 });
        TestUtils::VerifyLineContains(iter1, L'B', green, 5u);
        TestUtils::VerifyLineContains(iter1, L' ', defaultAttrs, static_cast<size_t>(width - 5));

        auto iter2 = tb.GetCellLineDataAt({ 0, 2 });
        TestUtils::VerifyLineContains(iter2, L' ', blue, 1u);
        TestUtils::VerifyLineContains(iter2, L'C', blue, 3u);
        TestUtils::VerifyLineContains(iter2, L' ', blue, 1u);
        TestUtils::VerifyLineContains(iter2, L' ', defaultAttrs, static_cast<size_t>(width - 5));

        auto iter3 = tb.GetCellLineDataAt({ 0, 3 });
        TestUtils::VerifyLineContains(iter3, L"\U0001F643", yellow, 2u);
        TestUtils::VerifyLineContains(iter3, L' ', defaultAttrs, static_cast<size_t>(width - 2));

        auto iter4 = tb.GetCellLineDataAt({ 0, 4 });
        TestUtils::VerifyLineContains(iter4, L' ', yellow, static_cast<size_t>(width));
    };

    Log::Comment(L"========== Checking the buffer state (before) ==========");
    verifyBuffer(si.GetTextBuffer(), si.GetViewport().ToExclusive(), true);

    Log::Comment(L"========== resize buffer ==========");
    const til::point delta{ dx, dy };
    const auto oldSize = si.GetBufferSize().Dimensions();
    const auto newSize{ oldSize + delta };
    VERIFY_SUCCEEDED(si.ResizeWithReflow(newSize));

    Log::Comment(L"========== Checking the buffer state (after) ==========");
    verifyBuffer(si.GetTextBuffer(), si.GetViewport().ToExclusive(), false);
}

void ScreenBufferTests::TestReflowSmallerLongLineWithColor()
{
    Log::Comment(L"Reflow the buffer such that a long, line of text flows onto "
                 L"the next line. Check that the trailing attributes were copied"
                 L" to the new row.");

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& stateMachine = si.GetStateMachine();

    gci.SetWrapText(true);

    auto defaultAttrs = si.GetAttributes();
    auto red = defaultAttrs;
    red.SetIndexedBackground(TextColor::DARK_RED);
    auto green = defaultAttrs;
    green.SetIndexedBackground(TextColor::DARK_GREEN);
    auto blue = defaultAttrs;
    blue.SetIndexedBackground(TextColor::DARK_BLUE);
    auto yellow = defaultAttrs;
    yellow.SetIndexedBackground(TextColor::DARK_YELLOW);

    Log::Comment(L"Make sure the viewport is at 0,0");
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, til::point(0, 0), true));

    Log::Comment(L"Fill buffer with some data");
    stateMachine.ProcessString(L"\x1b[H");
    stateMachine.ProcessString(L"\x1b[41m"); // Red BG
    stateMachine.ProcessString(L"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"); // 35 A's
    stateMachine.ProcessString(L"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"); // 35 more, 70 total
    stateMachine.ProcessString(L"\x1b[42m"); // Green BG
    stateMachine.ProcessString(L" BBB \n"); // " BBB " with spaces on either side).
    // Attributes fill 70 red, 5 green, the last 5 are {whatever it was before}

    auto verifyBuffer = [&](const TextBuffer& tb, const til::rect& /*viewport*/, const bool before) {
        const auto width = tb.GetSize().Width();
        Log::Comment(NoThrowString().Format(L"Buffer width: %d", width));

        if (before)
        {
            auto iter0 = TestUtils::VerifyLineContains(tb, { 0, 0 }, L'A', red, 70u);
            TestUtils::VerifyLineContains(iter0, L' ', green, 1u);
            TestUtils::VerifyLineContains(iter0, L'B', green, 3u);
            TestUtils::VerifyLineContains(iter0, L' ', green, 1u);
            TestUtils::VerifyLineContains(iter0, L' ', defaultAttrs, 5u);
        }
        else
        {
            auto iter0 = TestUtils::VerifyLineContains(tb, { 0, 0 }, L'A', red, 65u);

            auto iter1 = tb.GetCellLineDataAt({ 0, 1 });
            TestUtils::VerifyLineContains(iter1, L'A', red, 5u);
            TestUtils::VerifyLineContains(iter1, L' ', green, 1u);
            TestUtils::VerifyLineContains(iter1, L'B', green, 3u);
            TestUtils::VerifyLineContains(iter1, L' ', green, 1u);

            // We don't want to necessarily verify the contents of the rest of the
            // line, but we will anyways. Right now, we expect the last attrs on the
            // original row to fill the remainder of the row it flowed onto. We may
            // want to change that in the future. If we do, this check can always be
            // changed.
            TestUtils::VerifyLineContains(iter1, L' ', defaultAttrs, static_cast<size_t>(width - 10));
        }
    };

    Log::Comment(L"========== Checking the buffer state (before) ==========");
    verifyBuffer(si.GetTextBuffer(), si.GetViewport().ToExclusive(), true);

    Log::Comment(L"========== resize buffer ==========");
    const til::point delta{ -15, 0 };
    const auto oldSize = si.GetBufferSize().Dimensions();
    const auto newSize{ oldSize + delta };
    VERIFY_SUCCEEDED(si.ResizeWithReflow(newSize));

    // Buffer is now 65 wide. 65 A's that wrapped onto the next row, where there
    // are also 3 B's wrapped in spaces.
    Log::Comment(L"========== Checking the buffer state (after) ==========");
    verifyBuffer(si.GetTextBuffer(), si.GetViewport().ToExclusive(), false);
}

void ScreenBufferTests::TestReflowBiggerLongLineWithColor()
{
    Log::Comment(L"Reflow the buffer such that a wrapped line of text 'de-flows' onto the previous line.");

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& stateMachine = si.GetStateMachine();

    gci.SetWrapText(true);

    auto defaultAttrs = si.GetAttributes();
    auto red = defaultAttrs;
    red.SetIndexedBackground(TextColor::DARK_RED);
    auto green = defaultAttrs;
    green.SetIndexedBackground(TextColor::DARK_GREEN);
    auto blue = defaultAttrs;
    blue.SetIndexedBackground(TextColor::DARK_BLUE);
    auto yellow = defaultAttrs;
    yellow.SetIndexedBackground(TextColor::DARK_YELLOW);

    Log::Comment(L"Make sure the viewport is at 0,0");
    VERIFY_SUCCEEDED(si.SetViewportOrigin(true, til::point(0, 0), true));

    Log::Comment(L"Fill buffer with some data");
    stateMachine.ProcessString(L"\x1b[H");
    stateMachine.ProcessString(L"\x1b[41m"); // Red BG
    stateMachine.ProcessString(L"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"); // 40 A's
    stateMachine.ProcessString(L"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"); // 40 more, to wrap
    stateMachine.ProcessString(L"AAAAA"); // 5 more. 85 total.
    stateMachine.ProcessString(L"\x1b[42m"); // Green BG
    stateMachine.ProcessString(L" BBB \n"); // " BBB " with spaces on either side).
    // Attributes fill 85 red, 5 green, the rest are {whatever it was before}

    auto verifyBuffer = [&](const TextBuffer& tb, const til::rect& /*viewport*/, const bool before) {
        const auto width = tb.GetSize().Width();
        Log::Comment(NoThrowString().Format(L"Buffer width: %d", width));

        if (before)
        {
            auto iter0 = TestUtils::VerifyLineContains(tb, { 0, 0 }, L'A', red, 80u);

            auto iter1 = tb.GetCellLineDataAt({ 0, 1 });
            TestUtils::VerifyLineContains(iter1, L'A', red, 5u);
            TestUtils::VerifyLineContains(iter1, L' ', green, 1u);
            TestUtils::VerifyLineContains(iter1, L'B', green, 3u);
            TestUtils::VerifyLineContains(iter1, L' ', green, 1u);
            TestUtils::VerifyLineContains(iter1, L' ', defaultAttrs, static_cast<size_t>(width - 10));
        }
        else
        {
            auto iter0 = TestUtils::VerifyLineContains(tb, { 0, 0 }, L'A', red, 85u);
            TestUtils::VerifyLineContains(iter0, L' ', green, 1u);
            TestUtils::VerifyLineContains(iter0, L'B', green, 3u);
            TestUtils::VerifyLineContains(iter0, L' ', green, 1u);

            // We don't want to necessarily verify the contents of the rest of
            // the line, but we will anyways. Differently than
            // TestReflowSmallerLongLineWithColor: In this test, the since we're
            // de-flowing row 1 onto row 0, and the trailing spaces in row 1 are
            // default attrs, then that's the attrs we'll use to finish filling
            // up the 0th row in the new buffer. Again, we may want to change
            // that in the future. If we do, this check can always be changed.
            TestUtils::VerifyLineContains(iter0, L' ', defaultAttrs, static_cast<size_t>(width - 90));
        }
    };

    Log::Comment(L"========== Checking the buffer state (before) ==========");
    verifyBuffer(si.GetTextBuffer(), si.GetViewport().ToExclusive(), true);

    Log::Comment(L"========== resize buffer ==========");
    const til::point delta{ 15, 0 };
    const auto oldSize = si.GetBufferSize().Dimensions();
    const auto newSize{ oldSize + delta };
    VERIFY_SUCCEEDED(si.ResizeWithReflow(newSize));

    // Buffer is now 95 wide. 85 A's that de-flowed onto the first row, where
    // there are also 3 B's wrapped in spaces, and finally 5 trailing spaces.
    Log::Comment(L"========== Checking the buffer state (after) ==========");
    verifyBuffer(si.GetTextBuffer(), si.GetViewport().ToExclusive(), false);
}

void ScreenBufferTests::TestDeferredMainBufferResize()
{
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:inConpty", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:reEnterAltBuffer", L"{false, true}")
    END_TEST_METHOD_PROPERTIES();

    INIT_TEST_PROPERTY(bool, inConpty, L"Should we pretend to be in conpty mode?");
    INIT_TEST_PROPERTY(bool, reEnterAltBuffer, L"Should we re-enter the alt buffer when we're already in it?");

    // A test for https://github.com/microsoft/terminal/pull/12719#discussion_r834860330
    Log::Comment(L"Resize the alt buffer. We should defer the resize of the "
                 L"main buffer till we swap back to it, for performance.");

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.LockConsole(); // Lock must be taken to manipulate buffer.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    // HUGELY cribbed from ConptyRoundtripTests::MethodSetup. This fakes the
    // console into thinking that it's in ConPTY mode. Yes, we need all this
    // just to get gci.IsInVtIoMode() to return true. The screen buffer gates
    // all sorts of internal checks on that.
    //
    // This could theoretically be a helper if other tests need it.
    if (inConpty)
    {
        Log::Comment(L"Set up ConPTY");

        auto& currentBuffer = gci.GetActiveOutputBuffer();
        // Set up an xterm-256 renderer for conpty
        wil::unique_hfile hFile = wil::unique_hfile(INVALID_HANDLE_VALUE);
        auto initialViewport = currentBuffer.GetViewport();
        auto vtRenderEngine = std::make_unique<Microsoft::Console::Render::Xterm256Engine>(std::move(hFile),
                                                                                           initialViewport);
        // We don't care about the output, so let it just drain to the void.
        vtRenderEngine->SetTestCallback([](auto&&, auto&&) -> bool { return true; });
        gci.GetActiveOutputBuffer().SetTerminalConnection(vtRenderEngine.get());
        // Manually set the console into conpty mode. We're not actually going
        // to set up the pipes for conpty, but we want the console to behave
        // like it would in conpty mode.
        ServiceLocator::LocateGlobals().EnableConptyModeForTests(std::move(vtRenderEngine));
    }

    auto* siMain = &gci.GetActiveOutputBuffer();
    auto& stateMachine = siMain->GetStateMachine();

    const til::size oldSize{ siMain->GetBufferSize().Dimensions() };
    const til::size oldView{ siMain->GetViewport().Dimensions() };
    Log::Comment(NoThrowString().Format(L"Original buffer size: %dx%d", oldSize.width, oldSize.height));
    Log::Comment(NoThrowString().Format(L"Original viewport: %dx%d", oldView.width, oldView.height));
    VERIFY_ARE_NOT_EQUAL(oldSize, oldView);

    // printf "\x1b[?1049h" ; sleep 1 ; printf "\x1b[8;24;60t" ; sleep 1 ; printf "\x1b[?1049l" ; sleep 1 ; printf "\n"
    Log::Comment(L"Switch to alt buffer");
    stateMachine.ProcessString(L"\x1b[?1049h");

    auto* siAlt = &gci.GetActiveOutputBuffer();
    VERIFY_ARE_NOT_EQUAL(siMain, siAlt);

    const til::size newSize{ siAlt->GetBufferSize().Dimensions() };
    const til::size newView{ siAlt->GetViewport().Dimensions() };
    VERIFY_ARE_EQUAL(oldView, newSize);
    VERIFY_ARE_EQUAL(oldView, newSize);
    VERIFY_ARE_EQUAL(newView, newSize);

    Log::Comment(L"Resize alt buffer");
    stateMachine.ProcessString(L"\x1b[8;24;60t");
    const til::size expectedSize{ 60, 24 };
    const til::size altPostResizeSize{ siAlt->GetBufferSize().Dimensions() };
    const til::size altPostResizeView{ siAlt->GetViewport().Dimensions() };
    const til::size mainPostResizeSize{ siMain->GetBufferSize().Dimensions() };
    const til::size mainPostResizeView{ siMain->GetViewport().Dimensions() };
    VERIFY_ARE_NOT_EQUAL(oldView, altPostResizeSize);
    VERIFY_ARE_EQUAL(altPostResizeView, altPostResizeSize);
    VERIFY_ARE_EQUAL(expectedSize, altPostResizeSize);

    Log::Comment(L"Main buffer should not have resized yet.");
    VERIFY_ARE_EQUAL(oldSize, mainPostResizeSize);
    VERIFY_ARE_EQUAL(oldView, mainPostResizeView);

    if (reEnterAltBuffer)
    {
        Log::Comment(L"re-enter alt buffer");
        stateMachine.ProcessString(L"\x1b[?1049h");

        auto* siAlt2 = &gci.GetActiveOutputBuffer();
        VERIFY_ARE_NOT_EQUAL(siMain, siAlt2);
        VERIFY_ARE_NOT_EQUAL(siAlt, siAlt2);

        const til::size alt2Size{ siAlt2->GetBufferSize().Dimensions() };
        const til::size alt2View{ siAlt2->GetViewport().Dimensions() };
        VERIFY_ARE_EQUAL(altPostResizeSize, alt2Size);
        VERIFY_ARE_EQUAL(altPostResizeView, alt2View);
    }

    Log::Comment(L"Switch to main buffer");
    stateMachine.ProcessString(L"\x1b[?1049l");

    auto* siFinal = &gci.GetActiveOutputBuffer();
    VERIFY_ARE_NOT_EQUAL(siAlt, siFinal);
    VERIFY_ARE_EQUAL(siMain, siFinal);

    const til::size mainPostRestoreSize{ siMain->GetBufferSize().Dimensions() };
    const til::size mainPostRestoreView{ siMain->GetViewport().Dimensions() };
    VERIFY_ARE_NOT_EQUAL(oldView, mainPostRestoreView);
    VERIFY_ARE_NOT_EQUAL(oldSize, mainPostRestoreSize);
    VERIFY_ARE_EQUAL(altPostResizeView, mainPostRestoreView);
    VERIFY_ARE_EQUAL(expectedSize, mainPostRestoreView);
}

void ScreenBufferTests::RectangularAreaOperations()
{
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:rectOp", L"{0, 1, 2, 3, 4, 5}")
    END_TEST_METHOD_PROPERTIES();

    enum RectOp : int
    {
        DECFRA,
        DECERA,
        DECSERA,
        DECCARA,
        DECRARA,
        DECCRA
    };

    RectOp rectOp;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"rectOp", (int&)rectOp));

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    auto& stateMachine = si.GetStateMachine();
    WI_SetFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    const auto bufferWidth = si.GetBufferSize().Width();
    const auto bufferHeight = si.GetBufferSize().Height();

    // Move the viewport down a few lines, and only cover part of the buffer width.
    si.SetViewport(Viewport::FromDimensions({ 5, 10 }, { bufferWidth - 10, 20 }), true);
    const auto viewport = si.GetViewport();

    // Fill the entire buffer with Zs. Blue on Green and Underlined.
    const auto bufferChar = L'Z';
    auto bufferAttr = TextAttribute{ FOREGROUND_BLUE | BACKGROUND_GREEN };
    bufferAttr.SetUnderlined(true);
    _FillLines(0, bufferHeight, bufferChar, bufferAttr);

    // Set the active attributes to Red on Blue and Intense;
    auto activeAttr = TextAttribute{ FOREGROUND_RED | BACKGROUND_BLUE };
    activeAttr.SetIntense(true);
    si.SetAttributes(activeAttr);

    // The area we're targeting in all the operations below is 27;3 to 54;6.
    // But VT coordinates use origin 1;1 so we need to subtract 1, and til::rect
    // expects exclusive coordinates, so the bottom/right also need to add 1.
    const auto targetArea = til::rect{ 27 - 1, viewport.Top() + 3 - 1, 54, viewport.Top() + 6 };

    wchar_t expectedChar{};
    TextAttribute expectedAttr;

    // DECCARA and DECRARA can apply to both a stream of character positions or
    // a rectangular area, but these tests only cover the rectangular option.
    if (rectOp == DECCARA || rectOp == DECRARA)
    {
        Log::Comment(L"Request a rectangular change extent with DECSACE");
        stateMachine.ProcessString(L"\033[2*x");
    }

    switch (rectOp)
    {
    case DECFRA:
        Log::Comment(L"DECFRA: fill a rectangle with the active attributes and a given character value");
        expectedAttr = activeAttr;
        expectedChar = wchar_t{ 42 };
        // The first parameter specifies the fill character.
        stateMachine.ProcessString(L"\033[42;3;27;6;54$x");
        break;
    case DECERA:
        Log::Comment(L"DECERA: erase a rectangle using the active colors but no rendition attributes");
        expectedAttr = activeAttr;
        expectedAttr.SetStandardErase();
        expectedChar = L' ';
        stateMachine.ProcessString(L"\033[3;27;6;54$z");
        break;
    case DECSERA:
        Log::Comment(L"DECSERA: erase the text in a rectangle but leave the attributes unchanged");
        expectedAttr = bufferAttr;
        expectedChar = L' ';
        stateMachine.ProcessString(L"\033[3;27;6;54${");
        break;
    case DECCARA:
        Log::Comment(L"DECCARA: update the attributes in a rectangle but leave the text unchanged");
        expectedAttr = bufferAttr;
        expectedAttr.SetReverseVideo(true);
        expectedChar = bufferChar;
        // The final parameter specifies the reverse video attribute that will be set.
        stateMachine.ProcessString(L"\033[3;27;6;54;7$r");
        break;
    case DECRARA:
        Log::Comment(L"DECRARA: reverse the attributes in a rectangle but leave the text unchanged");
        expectedAttr = bufferAttr;
        expectedAttr.SetUnderlined(false);
        expectedChar = bufferChar;
        // The final parameter specifies the underline attribute that will be reversed.
        stateMachine.ProcessString(L"\033[3;27;6;54;4$t");
        break;
    case DECCRA:
        Log::Comment(L"DECCRA: copy a rectangle from the lower part of the viewport to the top");
        expectedAttr = TextAttribute{ FOREGROUND_GREEN | BACKGROUND_RED };
        expectedChar = L'*';
        // Fill the lower part of the viewport with some different content.
        _FillLines(viewport.Top() + 10, viewport.BottomExclusive(), expectedChar, expectedAttr);
        // Copy a rectangle from that lower part up to the top with DECCRA.
        stateMachine.ProcessString(L"\033[11;27;14;54;1;3;27;1;4$v");
        // Reset the lower part back to its original content.
        _FillLines(viewport.Top() + 10, viewport.BottomExclusive(), bufferChar, bufferAttr);
        break;
    default:
        VERIFY_FAIL(L"Unsupported operation");
    }

    const auto expectedChars = std::wstring(targetArea.width(), expectedChar);
    VERIFY_IS_TRUE(_ValidateLinesContain(targetArea.left, targetArea.top, targetArea.bottom, expectedChars, expectedAttr));

    Log::Comment(L"Everything above and below the target area should remain unchanged");
    VERIFY_IS_TRUE(_ValidateLinesContain(0, targetArea.top, bufferChar, bufferAttr));
    VERIFY_IS_TRUE(_ValidateLinesContain(targetArea.bottom, bufferHeight, bufferChar, bufferAttr));

    Log::Comment(L"Everything to the left and right of the target area should remain unchanged");
    const auto bufferChars = std::wstring(targetArea.left, bufferChar);
    VERIFY_IS_TRUE(_ValidateLinesContain(targetArea.top, targetArea.bottom, bufferChars, bufferAttr));
    VERIFY_IS_TRUE(_ValidateLinesContain(targetArea.right, targetArea.top, targetArea.bottom, bufferChar, bufferAttr));
}

void ScreenBufferTests::CopyDoubleWidthRectangularArea()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    auto& stateMachine = si.GetStateMachine();
    auto& textBuffer = si.GetTextBuffer();
    WI_SetFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    // Fill the entire buffer with Zs. Blue on Green and Underlined.
    const auto bufferChar = L'Z';
    const auto bufferHeight = si.GetBufferSize().Height();
    auto bufferAttr = TextAttribute{ FOREGROUND_BLUE | BACKGROUND_GREEN };
    bufferAttr.SetUnderlined(true);
    _FillLines(0, bufferHeight, bufferChar, bufferAttr);

    // Fill the first three lines with Cs. Green on Red and Intense.
    const auto copyChar = L'C';
    auto copyAttr = TextAttribute{ FOREGROUND_GREEN | BACKGROUND_RED };
    copyAttr.SetIntense(true);
    _FillLines(0, 3, copyChar, copyAttr);

    // Set the active attributes to Red on Blue;
    auto activeAttr = TextAttribute{ FOREGROUND_RED | BACKGROUND_BLUE };
    si.SetAttributes(activeAttr);

    // Make the second line (offset 1) double width.
    textBuffer.GetCursor().SetPosition({ 0, 1 });
    textBuffer.SetCurrentLineRendition(LineRendition::DoubleWidth, activeAttr);

    // Copy a segment of the top three lines with DECCRA.
    stateMachine.ProcessString(L"\033[1;31;3;50;1;4;31;1$v");

    Log::Comment(L"The first 30 columns of the target area should remain unchanged");
    const auto thirtyBufferChars = std::wstring(30, bufferChar);
    VERIFY_IS_TRUE(_ValidateLinesContain(3, 6, thirtyBufferChars, bufferAttr));

    Log::Comment(L"The next 20 columns should contain the copied content");
    const auto twentyCopyChars = std::wstring(20, copyChar);
    VERIFY_IS_TRUE(_ValidateLineContains({ 30, 3 }, twentyCopyChars, copyAttr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 30, 5 }, twentyCopyChars, copyAttr));

    Log::Comment(L"But the second target row ends after 10, because of the double-width source");
    const auto tenCopyChars = std::wstring(10, copyChar);
    VERIFY_IS_TRUE(_ValidateLineContains({ 30, 4 }, tenCopyChars, copyAttr));

    Log::Comment(L"The subsequent columns in each row should remain unchanged");
    VERIFY_IS_TRUE(_ValidateLineContains({ 50, 3 }, bufferChar, bufferAttr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 40, 4 }, bufferChar, bufferAttr));
    VERIFY_IS_TRUE(_ValidateLineContains({ 50, 5 }, bufferChar, bufferAttr));
}

void ScreenBufferTests::DelayedWrapReset()
{
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
        TEST_METHOD_PROPERTY(L"Data:op", L"{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35}")
    END_TEST_METHOD_PROPERTIES();

    int opIndex;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"op", opIndex));

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    auto& stateMachine = si.GetStateMachine();
    const auto width = si.GetTextBuffer().GetSize().Width();
    const auto halfWidth = width / 2;
    WI_SetFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    WI_SetFlag(si.OutputMode, DISABLE_NEWLINE_AUTO_RETURN);

    const auto startRow = 5;
    const auto startCol = width - 1;

    // The operations below are all those that the DEC STD 070 reference has
    // documented as needing to reset the Last Column Flag (see page D-13). The
    // only controls that we haven't included are HT and SUB, because most DEC
    // terminals did *not* trigger a reset after executing those sequences, and
    // most modern terminals also seem to have agreed that that is the correct
    // approach to take.
    const struct
    {
        std::wstring_view name;
        std::wstring_view sequence;
        til::point expectedPos = {};
        bool absolutePos = false;
    } ops[] = {
        { L"DECSTBM", L"\033[1;10r", { 0, 0 }, true },
        { L"DECSLRM", L"\033[1;10s", { 0, 0 }, true },
        { L"DECSWL", L"\033#5" },
        { L"DECDWL", L"\033#6", { halfWidth - 1, startRow }, true },
        { L"DECDHL (top)", L"\033#3", { halfWidth - 1, startRow }, true },
        { L"DECDHL (bottom)", L"\033#4", { halfWidth - 1, startRow }, true },
        { L"DECCOLM set", L"\033[?3h", { 0, 0 }, true },
        { L"DECOM set", L"\033[?6h", { 0, 0 }, true },
        { L"DECCOLM reset", L"\033[?3l", { 0, 0 }, true },
        { L"DECOM reset", L"\033[?6l", { 0, 0 }, true },
        { L"DECAWM reset", L"\033[?7l" },
        { L"CUU", L"\033[A", { 0, -1 } },
        { L"CUD", L"\033[B", { 0, 1 } },
        { L"CUF", L"\033[C" },
        { L"CUB", L"\033[D", { -1, 0 } },
        { L"CUP", L"\033[3;7H", { 6, 2 }, true },
        { L"HVP", L"\033[3;7f", { 6, 2 }, true },
        { L"BS", L"\b", { -1, 0 } },
        { L"LF", L"\n", { 0, 1 } },
        { L"VT", L"\v", { 0, 1 } },
        { L"FF", L"\f", { 0, 1 } },
        { L"CR", L"\r", { 0, startRow }, true },
        { L"IND", L"\033D", { 0, 1 } },
        { L"RI", L"\033M", { 0, -1 } },
        { L"NEL", L"\033E", { 0, startRow + 1 }, true },
        { L"ECH", L"\033[X" },
        { L"DCH", L"\033[P" },
        { L"ICH", L"\033[@" },
        { L"EL", L"\033[K" },
        { L"DECSEL", L"\033[?K" },
        { L"DL", L"\033[M", { 0, startRow }, true },
        { L"IL", L"\033[L", { 0, startRow }, true },
        { L"ED", L"\033[J" },
        { L"ED (all)", L"\033[2J" },
        { L"ED (scrollback)", L"\033[3J" },
        { L"DECSED", L"\033[?J" },
    };
    const auto& op = gsl::at(ops, opIndex);

    if (op.name == L"DECSLRM")
    {
        // Private mode 69 makes sure DECSLRM is allowed
        stateMachine.ProcessString(L"\033[?69h");
    }
    if (op.name.starts_with(L"DECCOLM"))
    {
        // Private mode 40 makes sure DECCOLM is allowed
        stateMachine.ProcessString(L"\033[?40h");
    }

    Log::Comment(L"Writing a character at the end of the line should set delayed EOL wrap");
    const auto startPos = til::point{ startCol, startRow };
    si.GetTextBuffer().GetCursor().SetPosition(startPos);
    stateMachine.ProcessCharacter(L'X');
    {
        auto& cursor = si.GetTextBuffer().GetCursor();
        VERIFY_IS_TRUE(cursor.IsDelayedEOLWrap());
        VERIFY_ARE_EQUAL(startPos, cursor.GetPosition());
    }

    Log::Comment(NoThrowString().Format(
        L"Executing a %s control should reset delayed EOL wrap",
        op.name.data()));
    const auto expectedPos = op.absolutePos ? op.expectedPos : startPos + op.expectedPos;
    stateMachine.ProcessString(op.sequence);
    {
        auto& cursor = si.GetTextBuffer().GetCursor();
        const auto actualPos = cursor.GetPosition() - si.GetViewport().Origin();
        VERIFY_IS_FALSE(cursor.IsDelayedEOLWrap());
        VERIFY_ARE_EQUAL(expectedPos, actualPos);
    }
}

void ScreenBufferTests::EraseColorMode()
{
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:op", L"{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19}")
        TEST_METHOD_PROPERTY(L"Data:eraseMode", L"{false,true}")
    END_TEST_METHOD_PROPERTIES();

    bool eraseMode;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"eraseMode", eraseMode));
    int opIndex;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"op", opIndex));

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    auto& stateMachine = si.GetStateMachine();
    WI_SetFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    WI_SetFlag(si.OutputMode, DISABLE_NEWLINE_AUTO_RETURN);

    const auto bufferWidth = si.GetBufferSize().Width();
    const auto bufferHeight = si.GetBufferSize().Height();

    // Set the viewport to 24 lines.
    si.SetViewport(Viewport::FromDimensions({ 0, 0 }, { bufferWidth, 24 }), true);
    const auto viewport = si.GetViewport();

    // Fill the buffer with text. Red on Blue.
    const auto bufferChars = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmn";
    const auto bufferAttr = TextAttribute{ FOREGROUND_RED | BACKGROUND_BLUE };
    _FillLines(0, bufferHeight, bufferChars, bufferAttr);

    // Set the active attributes with a mix of color and meta attributes.
    auto activeAttr = TextAttribute{ RGB(12, 34, 56), RGB(78, 90, 12) };
    activeAttr.SetCrossedOut(true);
    activeAttr.SetReverseVideo(true);
    activeAttr.SetUnderlined(true);
    si.SetAttributes(activeAttr);

    // By default, the meta attributes are expected to be cleared when erasing.
    auto standardEraseAttr = activeAttr;
    standardEraseAttr.SetStandardErase();

    const struct
    {
        std::wstring_view name;
        std::wstring_view sequence;
        til::point startPos = {};
        til::point erasePos = {};
    } ops[] = {
        { L"LF", L"\n", { 0, 23 }, { 0, 24 } }, // pans down on last row
        { L"VT", L"\v", { 0, 23 }, { 0, 24 } }, // pans down on last row
        { L"FF", L"\f", { 0, 23 }, { 0, 24 } }, // pans down on last row
        { L"NEL", L"\033E", { 0, 23 }, { 0, 24 } }, // pans down on last row
        { L"IND", L"\033D", { 0, 23 }, { 0, 24 } }, // pans down on last row
        { L"RI", L"\033M" },
        { L"DECBI", L"\033\066" },
        { L"DECFI", L"\033\071", { 79, 0 }, { 79, 0 } }, // scrolls in last column
        { L"DECIC", L"\033['}" },
        { L"DECDC", L"\033['~", {}, { 79, 0 } }, // last column erased
        { L"ICH", L"\033[@" },
        { L"DCH", L"\033[P", {}, { 79, 0 } }, // last column erased
        { L"IL", L"\033[L" },
        { L"DL", L"\033[M", {}, { 0, 23 } }, // last row erased
        { L"ECH", L"\033[X" },
        { L"EL", L"\033[K" },
        { L"ED", L"\033[J" },
        { L"DECERA", L"\033[$z" },
        { L"SU", L"\033[S", {}, { 0, 23 } }, // last row erased
        { L"SD", L"\033[T" },
    };
    const auto& op = gsl::at(ops, opIndex);

    si.GetTextBuffer().GetCursor().SetPosition(op.startPos);

    Log::Comment(eraseMode ? L"Enable DECECM" : L"Disable DECECM");
    stateMachine.ProcessString(eraseMode ? L"\033[?117h" : L"\033[?117l");

    Log::Comment(NoThrowString().Format(L"Execute %s", op.name.data()));
    stateMachine.ProcessString(op.sequence);

    Log::Comment(L"Verify expected erase attributes");
    const auto expectedEraseAttr = eraseMode ? TextAttribute{} : standardEraseAttr;
    const auto cellData = si.GetCellDataAt(op.erasePos);
    VERIFY_ARE_EQUAL(expectedEraseAttr, cellData->TextAttr());
    VERIFY_ARE_EQUAL(L" ", cellData->Chars());
}
