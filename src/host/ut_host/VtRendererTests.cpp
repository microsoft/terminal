// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include <wextestclass.h>
#include "../../inc/consoletaeftemplates.hpp"
#include "../../types/inc/Viewport.hpp"

#include "../../terminal/adapter/DispatchTypes.hpp"
#include "../host/RenderData.hpp"
#include "../../renderer/vt/Xterm256Engine.hpp"
#include "../../renderer/vt/XtermEngine.hpp"
#include "../Settings.hpp"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace Microsoft
{
    namespace Console
    {
        namespace Render
        {
            class VtRendererTest;
        };
    };
};
using namespace Microsoft::Console;
using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::VirtualTerminal::DispatchTypes;

static const std::string CLEAR_SCREEN = "\x1b[2J";
static const std::string CURSOR_HOME = "\x1b[H";
// Sometimes when we're expecting the renderengine to not write anything,
// we'll add this to the expected input, and manually write this to the callback
// to make sure nothing else gets written.
// We don't use null because that will confuse the VERIFY macros re: string length.
const char* const EMPTY_CALLBACK_SENTINEL = "\xff";

class Microsoft::Console::Render::VtRendererTest
{
    TEST_CLASS(VtRendererTest);

    TEST_CLASS_SETUP(ClassSetup)
    {
        return true;
    }

    TEST_CLASS_CLEANUP(ClassCleanup)
    {
        return true;
    }

    TEST_METHOD_SETUP(MethodSetup)
    {
        qExpectedInput.clear();
        return true;
    }

    // Defining a TEST_METHOD_CLEANUP seemed to break x86 test pass. Not sure why,
    //  something about the clipboard tests and
    //  YOU_CAN_ONLY_DESIGNATE_ONE_CLASS_METHOD_TO_BE_A_TEST_METHOD_SETUP_METHOD
    // It's probably more correct to leave it out anyways.

    TEST_METHOD(VtSequenceHelperTests);

    TEST_METHOD(Xterm256TestInvalidate);
    TEST_METHOD(Xterm256TestColors);
    TEST_METHOD(Xterm256TestITUColors);
    TEST_METHOD(Xterm256TestCursor);
    TEST_METHOD(Xterm256TestExtendedAttributes);
    TEST_METHOD(Xterm256TestAttributesAcrossReset);
    TEST_METHOD(Xterm256TestDoublyUnderlinedResetBeforeSettingStyle);

    TEST_METHOD(XtermTestInvalidate);
    TEST_METHOD(XtermTestColors);
    TEST_METHOD(XtermTestCursor);
    TEST_METHOD(XtermTestAttributesAcrossReset);

    TEST_METHOD(FormattedString);

    TEST_METHOD(TestWrapping);

    TEST_METHOD(TestResize);

    TEST_METHOD(TestCursorVisibility);

    void Test16Colors(VtEngine* engine);

    std::deque<std::string> qExpectedInput;
    bool WriteCallback(const char* const pch, const size_t cch);
    void TestPaint(VtEngine& engine, std::function<void()> pfn);
    Viewport SetUpViewport();

    void VerifyFirstPaint(VtEngine& engine)
    {
        // Verify the first BeginPaint emits a clear and go home
        qExpectedInput.push_back("\x1b[2J");
        // Verify the first EndPaint sets the cursor state
        qExpectedInput.push_back("\x1b[?25l");
        VERIFY_IS_TRUE(engine._firstPaint);
        TestPaint(engine, [&]() {
            VERIFY_IS_FALSE(engine._firstPaint);
        });
    }

    void VerifyExpectedInputsDrained()
    {
        if (!qExpectedInput.empty())
        {
            for (const auto& exp : qExpectedInput)
            {
                Log::Error(NoThrowString().Format(L"EXPECTED INPUT NEVER RECEIVED: %hs", exp.c_str()));
            }
            VERIFY_FAIL(L"there should be no remaining un-drained expected input");
        }
    }
};

Viewport VtRendererTest::SetUpViewport()
{
    til::inclusive_rect view;
    view.top = view.left = 0;
    view.bottom = 31;
    view.right = 79;

    return Viewport::FromInclusive(view);
}

bool VtRendererTest::WriteCallback(const char* const pch, const size_t cch)
{
    auto actualString = std::string(pch, cch);
    VERIFY_IS_GREATER_THAN(qExpectedInput.size(),
                           static_cast<size_t>(0),
                           NoThrowString().Format(L"writing=\"%hs\", expecting %u strings", actualString.c_str(), qExpectedInput.size()));

    auto first = qExpectedInput.front();
    qExpectedInput.pop_front();

    Log::Comment(NoThrowString().Format(L"Expected =\t\"%hs\"", first.c_str()));
    Log::Comment(NoThrowString().Format(L"Actual =\t\"%hs\"", actualString.c_str()));

    VERIFY_ARE_EQUAL(first.length(), cch);
    VERIFY_ARE_EQUAL(first, actualString);

    return true;
}

// Function Description:
// - Small helper to do a series of testing wrapped by StartPaint/EndPaint calls
// Arguments:
// - engine: the engine to operate on
// - pfn: A function pointer to some test code to run.
// Return Value:
// - <none>
void VtRendererTest::TestPaint(VtEngine& engine, std::function<void()> pfn)
{
    VERIFY_SUCCEEDED(engine.StartPaint());
    pfn();
    VERIFY_SUCCEEDED(engine.EndPaint());
}

void VtRendererTest::VtSequenceHelperTests()
{
    auto hFile = wil::unique_hfile(INVALID_HANDLE_VALUE);
    auto engine = std::make_unique<Xterm256Engine>(std::move(hFile), SetUpViewport());
    auto pfn = std::bind(&VtRendererTest::WriteCallback, this, std::placeholders::_1, std::placeholders::_2);

    engine->SetTestCallback(pfn);

    qExpectedInput.push_back("\x1b[?12l");
    VERIFY_SUCCEEDED(engine->_StopCursorBlinking());

    qExpectedInput.push_back("\x1b[?12h");
    VERIFY_SUCCEEDED(engine->_StartCursorBlinking());

    qExpectedInput.push_back("\x1b[?25l");
    VERIFY_SUCCEEDED(engine->_HideCursor());

    qExpectedInput.push_back("\x1b[?25h");
    VERIFY_SUCCEEDED(engine->_ShowCursor());

    qExpectedInput.push_back("\x1b[K");
    VERIFY_SUCCEEDED(engine->_EraseLine());

    qExpectedInput.push_back("\x1b[M");
    VERIFY_SUCCEEDED(engine->_DeleteLine(1));

    qExpectedInput.push_back("\x1b[2M");
    VERIFY_SUCCEEDED(engine->_DeleteLine(2));

    qExpectedInput.push_back("\x1b[L");
    VERIFY_SUCCEEDED(engine->_InsertLine(1));

    qExpectedInput.push_back("\x1b[2L");
    VERIFY_SUCCEEDED(engine->_InsertLine(2));

    qExpectedInput.push_back("\x1b[2X");
    VERIFY_SUCCEEDED(engine->_EraseCharacter(2));

    qExpectedInput.push_back("\x1b[2;3H");
    VERIFY_SUCCEEDED(engine->_CursorPosition({ 2, 1 }));

    qExpectedInput.push_back("\x1b[1;1H");
    VERIFY_SUCCEEDED(engine->_CursorPosition({ 0, 0 }));

    qExpectedInput.push_back("\x1b[H");
    VERIFY_SUCCEEDED(engine->_CursorHome());

    qExpectedInput.push_back("\x1b[8;32;80t");
    VERIFY_SUCCEEDED(engine->_ResizeWindow(80, 32));

    qExpectedInput.push_back("\x1b[2J");
    VERIFY_SUCCEEDED(engine->_ClearScreen());

    qExpectedInput.push_back("\x1b[10C");
    VERIFY_SUCCEEDED(engine->_CursorForward(10));
}

void VtRendererTest::Xterm256TestInvalidate()
{
    auto hFile = wil::unique_hfile(INVALID_HANDLE_VALUE);
    auto engine = std::make_unique<Xterm256Engine>(std::move(hFile), SetUpViewport());
    auto pfn = std::bind(&VtRendererTest::WriteCallback, this, std::placeholders::_1, std::placeholders::_2);
    engine->SetTestCallback(pfn);

    VerifyFirstPaint(*engine);

    const auto view = SetUpViewport();

    Log::Comment(NoThrowString().Format(
        L"Make sure that invalidating all invalidates the whole viewport."));
    VERIFY_SUCCEEDED(engine->InvalidateAll());
    TestPaint(*engine, [&]() {
        VERIFY_IS_TRUE(engine->_invalidMap.all());
    });

    Log::Comment(NoThrowString().Format(
        L"Make sure that invalidating anything only invalidates that portion"));
    til::rect invalid = { 1, 1, 2, 2 };
    VERIFY_SUCCEEDED(engine->Invalidate(&invalid));
    TestPaint(*engine, [&]() {
        VERIFY_IS_TRUE(engine->_invalidMap.one());
        VERIFY_ARE_EQUAL(invalid, *(engine->_invalidMap.begin()));
    });

    Log::Comment(NoThrowString().Format(
        L"Make sure that scrolling only invalidates part of the viewport, and sends the right sequences"));
    til::point scrollDelta = { 0, 1 };
    VERIFY_SUCCEEDED(engine->InvalidateScroll(&scrollDelta));
    TestPaint(*engine, [&]() {
        Log::Comment(NoThrowString().Format(
            L"---- Scrolled one down, only top line is invalid. ----"));
        invalid = view.ToExclusive();
        invalid.bottom = 1;

        const auto runs = engine->_invalidMap.runs();
        VERIFY_ARE_EQUAL(1u, runs.size());
        VERIFY_ARE_EQUAL(invalid, runs.front());
        qExpectedInput.push_back("\x1b[H"); // Go Home
        qExpectedInput.push_back("\x1b[L"); // insert a line

        VERIFY_SUCCEEDED(engine->ScrollFrame());
    });

    scrollDelta = { 0, 3 };
    VERIFY_SUCCEEDED(engine->InvalidateScroll(&scrollDelta));

    TestPaint(*engine, [&]() {
        Log::Comment(NoThrowString().Format(
            L"---- Scrolled three down, only top 3 lines are invalid. ----"));
        invalid = view.ToExclusive();
        invalid.bottom = 3;

        // we should have 3 runs and build a rectangle out of them
        const auto runs = engine->_invalidMap.runs();
        VERIFY_ARE_EQUAL(3u, runs.size());
        auto invalidRect = runs.front();
        for (size_t i = 1; i < runs.size(); ++i)
        {
            invalidRect |= runs[i];
        }

        // verify the rect matches the invalid one.
        VERIFY_ARE_EQUAL(invalid, invalidRect);
        // We would expect a CUP here, but the cursor is already at the home position
        qExpectedInput.push_back("\x1b[3L"); // insert 3 lines
        VERIFY_SUCCEEDED(engine->ScrollFrame());
    });

    scrollDelta = { 0, -1 };
    VERIFY_SUCCEEDED(engine->InvalidateScroll(&scrollDelta));
    TestPaint(*engine, [&]() {
        Log::Comment(NoThrowString().Format(
            L"---- Scrolled one up, only bottom line is invalid. ----"));
        invalid = view.ToExclusive();
        invalid.top = invalid.bottom - 1;

        const auto runs = engine->_invalidMap.runs();
        VERIFY_ARE_EQUAL(1u, runs.size());
        VERIFY_ARE_EQUAL(invalid, runs.front());

        qExpectedInput.push_back("\x1b[32;1H"); // Bottom of buffer
        qExpectedInput.push_back("\n"); // Scroll down once
        VERIFY_SUCCEEDED(engine->ScrollFrame());
    });

    scrollDelta = { 0, -3 };
    VERIFY_SUCCEEDED(engine->InvalidateScroll(&scrollDelta));
    TestPaint(*engine, [&]() {
        Log::Comment(NoThrowString().Format(
            L"---- Scrolled three up, only bottom 3 lines are invalid. ----"));
        invalid = view.ToExclusive();
        invalid.top = invalid.bottom - 3;

        // we should have 3 runs and build a rectangle out of them
        const auto runs = engine->_invalidMap.runs();
        VERIFY_ARE_EQUAL(3u, runs.size());
        auto invalidRect = runs.front();
        for (size_t i = 1; i < runs.size(); ++i)
        {
            invalidRect |= runs[i];
        }

        // verify the rect matches the invalid one.
        VERIFY_ARE_EQUAL(invalid, invalidRect);

        // We would expect a CUP here, but we're already at the bottom from the last call.
        qExpectedInput.push_back("\n\n\n"); // Scroll down three times
        VERIFY_SUCCEEDED(engine->ScrollFrame());
    });

    Log::Comment(NoThrowString().Format(
        L"Multiple scrolls are coalesced"));

    scrollDelta = { 0, 1 };
    VERIFY_SUCCEEDED(engine->InvalidateScroll(&scrollDelta));
    scrollDelta = { 0, 2 };
    VERIFY_SUCCEEDED(engine->InvalidateScroll(&scrollDelta));
    TestPaint(*engine, [&]() {
        Log::Comment(NoThrowString().Format(
            L"---- Scrolled three down, only top 3 lines are invalid. ----"));
        invalid = view.ToExclusive();
        invalid.bottom = 3;

        // we should have 3 runs and build a rectangle out of them
        const auto runs = engine->_invalidMap.runs();
        VERIFY_ARE_EQUAL(3u, runs.size());
        auto invalidRect = runs.front();
        for (size_t i = 1; i < runs.size(); ++i)
        {
            invalidRect |= runs[i];
        }

        // verify the rect matches the invalid one.
        VERIFY_ARE_EQUAL(invalid, invalidRect);

        qExpectedInput.push_back("\x1b[H"); // Go to home
        qExpectedInput.push_back("\x1b[3L"); // insert 3 lines
        VERIFY_SUCCEEDED(engine->ScrollFrame());
    });

    scrollDelta = { 0, 1 };
    VERIFY_SUCCEEDED(engine->InvalidateScroll(&scrollDelta));
    Log::Comment(engine->_invalidMap.to_string().c_str());

    scrollDelta = { 0, -1 };
    VERIFY_SUCCEEDED(engine->InvalidateScroll(&scrollDelta));
    Log::Comment(engine->_invalidMap.to_string().c_str());

    qExpectedInput.push_back("\x1b[2J");
    TestPaint(*engine, [&]() {
        Log::Comment(NoThrowString().Format(
            L"---- Scrolled one down and one up, nothing should change ----"
            L" But it still does for now MSFT:14169294"));

        const auto runs = engine->_invalidMap.runs();
        auto invalidRect = runs.front();
        for (size_t i = 1; i < runs.size(); ++i)
        {
            invalidRect |= runs[i];
        }

        // only the bottom line should be dirty.
        // When we scrolled down, the bitmap looked like this:
        // 1111
        // 0000
        // 0000
        // 0000
        // And then we scrolled up and the top line fell off and a bottom
        // line was filled in like this:
        // 0000
        // 0000
        // 0000
        // 1111
        const til::rect expected{ til::point{ view.Left(), view.BottomInclusive() }, til::size{ view.Width(), 1 } };
        VERIFY_ARE_EQUAL(expected, invalidRect);

        VERIFY_SUCCEEDED(engine->ScrollFrame());
    });
}

void VtRendererTest::Xterm256TestColors()
{
    auto hFile = wil::unique_hfile(INVALID_HANDLE_VALUE);
    auto engine = std::make_unique<Xterm256Engine>(std::move(hFile), SetUpViewport());
    auto pfn = std::bind(&VtRendererTest::WriteCallback, this, std::placeholders::_1, std::placeholders::_2);
    engine->SetTestCallback(pfn);
    RenderSettings renderSettings;
    RenderData renderData;

    VerifyFirstPaint(*engine);

    auto view = SetUpViewport();

    Log::Comment(NoThrowString().Format(
        L"Test changing the text attributes"));

    Log::Comment(NoThrowString().Format(
        L"Begin by setting some test values - FG,BG = (1,2,3), (4,5,6) to start. "
        L"These values were picked for ease of formatting raw COLORREF values."));
    qExpectedInput.push_back("\x1b[38;2;1;2;3m");
    qExpectedInput.push_back("\x1b[48;2;5;6;7m");
    VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes({ 0x00030201, 0x00070605 },
                                                  renderSettings,
                                                  &renderData,
                                                  false,
                                                  false));

    TestPaint(*engine, [&]() {
        Log::Comment(NoThrowString().Format(
            L"----Change only the BG----"));
        qExpectedInput.push_back("\x1b[48;2;7;8;9m");
        VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes({ 0x00030201, 0x00090807 },
                                                      renderSettings,
                                                      &renderData,
                                                      false,
                                                      false));

        Log::Comment(NoThrowString().Format(
            L"----Change only the FG----"));
        qExpectedInput.push_back("\x1b[38;2;10;11;12m");
        VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes({ 0x000c0b0a, 0x00090807 },
                                                      renderSettings,
                                                      &renderData,
                                                      false,
                                                      false));
    });

    TestPaint(*engine, [&]() {
        Log::Comment(NoThrowString().Format(
            L"Make sure that color setting persists across EndPaint/StartPaint"));
        qExpectedInput.push_back(EMPTY_CALLBACK_SENTINEL);
        VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes({ 0x000c0b0a, 0x00090807 },
                                                      renderSettings,
                                                      &renderData,
                                                      false,
                                                      false));
        WriteCallback(EMPTY_CALLBACK_SENTINEL, 1); // This will make sure nothing was written to the callback
    });

    // Now also do the body of the 16color test as well.
    // However, instead of using a closest match ANSI color, we can reproduce
    // the exact RGB or 256-color index value stored in the TextAttribute.

    Log::Comment(NoThrowString().Format(
        L"Begin by setting the default colors"));

    qExpectedInput.push_back("\x1b[m");
    VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes({},
                                                  renderSettings,
                                                  &renderData,
                                                  false,
                                                  false));

    TestPaint(*engine, [&]() {
        TextAttribute textAttributes;

        Log::Comment(NoThrowString().Format(
            L"----Change only the BG----"));
        textAttributes.SetIndexedBackground(TextColor::DARK_RED);
        qExpectedInput.push_back("\x1b[41m"); // Background DARK_RED
        VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes,
                                                      renderSettings,
                                                      &renderData,
                                                      false,
                                                      false));

        Log::Comment(NoThrowString().Format(
            L"----Change only the FG----"));
        textAttributes.SetIndexedForeground(TextColor::DARK_WHITE);
        qExpectedInput.push_back("\x1b[37m"); // Foreground DARK_WHITE
        VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes,
                                                      renderSettings,
                                                      &renderData,
                                                      false,
                                                      false));

        Log::Comment(NoThrowString().Format(
            L"----Change only the BG to an RGB value----"));
        textAttributes.SetBackground(RGB(19, 161, 14));
        qExpectedInput.push_back("\x1b[48;2;19;161;14m"); // Background RGB(19,161,14)
        VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes,
                                                      renderSettings,
                                                      &renderData,
                                                      false,
                                                      false));

        Log::Comment(NoThrowString().Format(
            L"----Change only the FG to an RGB value----"));
        textAttributes.SetForeground(RGB(193, 156, 0));
        qExpectedInput.push_back("\x1b[38;2;193;156;0m"); // Foreground RGB(193,156,0)
        VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes,
                                                      renderSettings,
                                                      &renderData,
                                                      false,
                                                      false));

        Log::Comment(NoThrowString().Format(
            L"----Change only the BG to the 'Default' background----"));
        textAttributes.SetDefaultBackground();
        qExpectedInput.push_back("\x1b[49m"); // Background default
        VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes,
                                                      renderSettings,
                                                      &renderData,
                                                      false,
                                                      false));

        Log::Comment(NoThrowString().Format(
            L"----Change only the FG to a 256-color index----"));
        textAttributes.SetIndexedForeground256(TextColor::DARK_WHITE);
        qExpectedInput.push_back("\x1b[38;5;7m"); // Foreground DARK_WHITE (256-Color Index)
        VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes,
                                                      renderSettings,
                                                      &renderData,
                                                      false,
                                                      false));

        Log::Comment(NoThrowString().Format(
            L"----Change only the BG to a 256-color index----"));
        textAttributes.SetIndexedBackground256(TextColor::DARK_RED);
        qExpectedInput.push_back("\x1b[48;5;1m"); // Background DARK_RED (256-Color Index)
        VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes,
                                                      renderSettings,
                                                      &renderData,
                                                      false,
                                                      false));

        Log::Comment(NoThrowString().Format(
            L"----Change only the FG to the 'Default' foreground----"));
        textAttributes.SetDefaultForeground();
        qExpectedInput.push_back("\x1b[39m"); // Background default
        VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes,
                                                      renderSettings,
                                                      &renderData,
                                                      false,
                                                      false));

        Log::Comment(NoThrowString().Format(
            L"----Back to defaults----"));
        textAttributes = {};
        qExpectedInput.push_back("\x1b[m");
        VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes,
                                                      renderSettings,
                                                      &renderData,
                                                      false,
                                                      false));
    });

    TestPaint(*engine, [&]() {
        Log::Comment(NoThrowString().Format(
            L"Make sure that color setting persists across EndPaint/StartPaint"));
        qExpectedInput.push_back(EMPTY_CALLBACK_SENTINEL);
        VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes({},
                                                      renderSettings,
                                                      &renderData,
                                                      false,
                                                      false));
        WriteCallback(EMPTY_CALLBACK_SENTINEL, 1); // This will make sure nothing was written to the callback
    });
}

void VtRendererTest::Xterm256TestITUColors()
{
    auto hFile = wil::unique_hfile(INVALID_HANDLE_VALUE);
    auto engine = std::make_unique<Xterm256Engine>(std::move(hFile), SetUpViewport());
    auto pfn = std::bind(&VtRendererTest::WriteCallback, this, std::placeholders::_1, std::placeholders::_2);
    engine->SetTestCallback(pfn);
    RenderSettings renderSettings;
    RenderData renderData;

    VerifyFirstPaint(*engine);

    auto view = SetUpViewport();

    Log::Comment(NoThrowString().Format(
        L"Test changing the text attributes"));

    // Internally _lastTextAttributes starts with a fg and bg set to INVALID_COLOR(s),
    // and initializing textAttributes with the default colors will output "\e[39m\e[49m"
    // in the beginning.
    auto textAttributes = TextAttribute{};
    qExpectedInput.push_back("\x1b[39m");
    qExpectedInput.push_back("\x1b[49m");

    Log::Comment(NoThrowString().Format(
        L"Begin by setting some test values - UL = (1,2,3) to start. "
        L"This value is picked for ease of formatting raw COLORREF values."));
    qExpectedInput.push_back("\x1b[58:2::1:2:3m");
    textAttributes.SetUnderlineColor(RGB(1, 2, 3));
    VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes,
                                                  renderSettings,
                                                  &renderData,
                                                  false,
                                                  false));

    TestPaint(*engine, [&]() {
        Log::Comment(NoThrowString().Format(
            L"----Change the color----"));
        qExpectedInput.push_back("\x1b[58:2::7:8:9m");
        textAttributes.SetUnderlineColor(RGB(7, 8, 9));
        VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes,
                                                      renderSettings,
                                                      &renderData,
                                                      false,
                                                      false));
    });

    TestPaint(*engine, [&]() {
        Log::Comment(NoThrowString().Format(
            L"Make sure that color setting persists across EndPaint/StartPaint"));
        qExpectedInput.push_back(EMPTY_CALLBACK_SENTINEL);
        VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes,
                                                      renderSettings,
                                                      &renderData,
                                                      false,
                                                      false));
        WriteCallback(EMPTY_CALLBACK_SENTINEL, 1); // This will make sure nothing was written to the callback
    });

    TestPaint(*engine, [&]() {
        Log::Comment(NoThrowString().Format(
            L"----Change the UL color to a 256-color index----"));
        textAttributes.SetUnderlineColor(TextColor{ TextColor::DARK_RED, true });
        qExpectedInput.push_back("\x1b[58:5:1m");
        VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes,
                                                      renderSettings,
                                                      &renderData,
                                                      false,
                                                      false));

        // to test the sequence for the default underline color, temporarily modify fg and bg to be something else.
        textAttributes.SetForeground(RGB(9, 10, 11));
        qExpectedInput.push_back("\x1b[38;2;9;10;11m");
        textAttributes.SetBackground(RGB(5, 6, 7));
        qExpectedInput.push_back("\x1b[48;2;5;6;7m");

        Log::Comment(NoThrowString().Format(
            L"----Change only the UL color to the 'Default'----"));
        textAttributes.SetDefaultUnderlineColor();
        qExpectedInput.push_back("\x1b[59m");
        VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes,
                                                      renderSettings,
                                                      &renderData,
                                                      false,
                                                      false));

        Log::Comment(NoThrowString().Format(
            L"----Back to defaults (all colors)----"));
        textAttributes = {};
        qExpectedInput.push_back("\x1b[m");
        VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes,
                                                      renderSettings,
                                                      &renderData,
                                                      false,
                                                      false));
    });

    TestPaint(*engine, [&]() {
        Log::Comment(NoThrowString().Format(
            L"Make sure that color setting persists across EndPaint/StartPaint"));
        qExpectedInput.push_back(EMPTY_CALLBACK_SENTINEL);
        VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes({},
                                                      renderSettings,
                                                      &renderData,
                                                      false,
                                                      false));
        WriteCallback(EMPTY_CALLBACK_SENTINEL, 1); // This will make sure nothing was written to the callback
    });
}

void VtRendererTest::Xterm256TestCursor()
{
    auto hFile = wil::unique_hfile(INVALID_HANDLE_VALUE);
    auto engine = std::make_unique<Xterm256Engine>(std::move(hFile), SetUpViewport());
    auto pfn = std::bind(&VtRendererTest::WriteCallback, this, std::placeholders::_1, std::placeholders::_2);
    engine->SetTestCallback(pfn);

    VerifyFirstPaint(*engine);

    auto view = SetUpViewport();

    Log::Comment(NoThrowString().Format(
        L"Test moving the cursor around. Every sequence should have both params to CUP explicitly."));
    TestPaint(*engine, [&]() {
        qExpectedInput.push_back("\x1b[2;2H");
        VERIFY_SUCCEEDED(engine->_MoveCursor({ 1, 1 }));

        Log::Comment(NoThrowString().Format(
            L"----Only move Y coord----"));
        qExpectedInput.push_back("\x1b[31;2H");
        VERIFY_SUCCEEDED(engine->_MoveCursor({ 1, 30 }));

        Log::Comment(NoThrowString().Format(
            L"----Only move X coord----"));
        qExpectedInput.push_back("\x1b[29C");
        VERIFY_SUCCEEDED(engine->_MoveCursor({ 30, 30 }));

        Log::Comment(NoThrowString().Format(
            L"----Sending the same move sends nothing----"));
        qExpectedInput.push_back(EMPTY_CALLBACK_SENTINEL);
        VERIFY_SUCCEEDED(engine->_MoveCursor({ 30, 30 }));
        WriteCallback(EMPTY_CALLBACK_SENTINEL, 1);

        Log::Comment(NoThrowString().Format(
            L"----moving home sends a simple sequence----"));
        qExpectedInput.push_back("\x1b[H");
        VERIFY_SUCCEEDED(engine->_MoveCursor({ 0, 0 }));

        Log::Comment(NoThrowString().Format(
            L"----move into the line to test some other sequences----"));
        qExpectedInput.push_back("\x1b[7C");
        VERIFY_SUCCEEDED(engine->_MoveCursor({ 7, 0 }));

        Log::Comment(NoThrowString().Format(
            L"----move down one line (x stays the same)----"));
        qExpectedInput.push_back("\n");
        VERIFY_SUCCEEDED(engine->_MoveCursor({ 7, 1 }));

        Log::Comment(NoThrowString().Format(
            L"----move to the start of the next line----"));
        qExpectedInput.push_back("\r\n");
        VERIFY_SUCCEEDED(engine->_MoveCursor({ 0, 2 }));

        Log::Comment(NoThrowString().Format(
            L"----move into the line to test some other sequences----"));
        qExpectedInput.push_back("\x1b[2;8H");
        VERIFY_SUCCEEDED(engine->_MoveCursor({ 7, 1 }));

        Log::Comment(NoThrowString().Format(
            L"----move to the start of this line (y stays the same)----"));
        qExpectedInput.push_back("\r");
        VERIFY_SUCCEEDED(engine->_MoveCursor({ 0, 1 }));
    });

    TestPaint(*engine, [&]() {
        Log::Comment(NoThrowString().Format(
            L"Sending the same move across paint calls sends nothing."
            L"The cursor's last \"real\" position was 0,0"));
        qExpectedInput.push_back(EMPTY_CALLBACK_SENTINEL);
        VERIFY_SUCCEEDED(engine->_MoveCursor({ 0, 1 }));
        WriteCallback(EMPTY_CALLBACK_SENTINEL, 1);

        Log::Comment(NoThrowString().Format(
            L"Paint some text at 0,0, then try moving the cursor to where it currently is."));
        qExpectedInput.push_back("\x1b[1C");
        qExpectedInput.push_back("asdfghjkl");

        const auto line = L"asdfghjkl";
        const unsigned char rgWidths[] = { 1, 1, 1, 1, 1, 1, 1, 1, 1 };

        std::vector<Cluster> clusters;
        for (size_t i = 0; i < wcslen(line); i++)
        {
            clusters.emplace_back(std::wstring_view{ &line[i], 1 }, static_cast<til::CoordType>(rgWidths[i]));
        }

        VERIFY_SUCCEEDED(engine->PaintBufferLine({ clusters.data(), clusters.size() }, { 1, 1 }, false, false));

        qExpectedInput.push_back(EMPTY_CALLBACK_SENTINEL);
        VERIFY_SUCCEEDED(engine->_MoveCursor({ 10, 1 }));
        WriteCallback(EMPTY_CALLBACK_SENTINEL, 1);
    });

    // Note that only PaintBufferLine updates the "Real" cursor position, which
    //  the cursor is moved back to at the end of each paint
    TestPaint(*engine, [&]() {
        Log::Comment(NoThrowString().Format(
            L"Sending the same move across paint calls sends nothing."));
        qExpectedInput.push_back(EMPTY_CALLBACK_SENTINEL);
        VERIFY_SUCCEEDED(engine->_MoveCursor({ 10, 1 }));
        WriteCallback(EMPTY_CALLBACK_SENTINEL, 1);
    });
}

void VtRendererTest::Xterm256TestExtendedAttributes()
{
    // Run this test for each and every possible combination of states.
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:faint", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:underlined", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:doublyUnderlined", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:italics", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:blink", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:invisible", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:crossedOut", L"{false, true}")
    END_TEST_METHOD_PROPERTIES()

    bool faint, underlined, doublyUnderlined, italics, blink, invisible, crossedOut;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"faint", faint));
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"underlined", underlined));
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"doublyUnderlined", doublyUnderlined));
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"italics", italics));
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"blink", blink));
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"invisible", invisible));
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"crossedOut", crossedOut));

    TextAttribute desiredAttrs;
    std::vector<std::string> onSequences, offSequences;

    // Collect up a VT sequence to set the state given the method properties
    if (faint)
    {
        desiredAttrs.SetFaint(true);
        onSequences.push_back("\x1b[2m");
        offSequences.push_back("\x1b[22m");
    }

    // underlined and doublyUnderlined are mutually exclusive
    if (underlined)
    {
        desiredAttrs.SetUnderlineStyle(UnderlineStyle::DashedUnderlined);
        onSequences.push_back("\x1b[4:5m");
        offSequences.push_back("\x1b[24m");
    }
    else if (doublyUnderlined)
    {
        desiredAttrs.SetUnderlineStyle(UnderlineStyle::DoublyUnderlined);
        onSequences.push_back("\x1b[21m");
        offSequences.push_back("\x1b[24m");
    }

    if (italics)
    {
        desiredAttrs.SetItalic(true);
        onSequences.push_back("\x1b[3m");
        offSequences.push_back("\x1b[23m");
    }
    if (blink)
    {
        desiredAttrs.SetBlinking(true);
        onSequences.push_back("\x1b[5m");
        offSequences.push_back("\x1b[25m");
    }
    if (invisible)
    {
        desiredAttrs.SetInvisible(true);
        onSequences.push_back("\x1b[8m");
        offSequences.push_back("\x1b[28m");
    }
    if (crossedOut)
    {
        desiredAttrs.SetCrossedOut(true);
        onSequences.push_back("\x1b[9m");
        offSequences.push_back("\x1b[29m");
    }

    auto hFile = wil::unique_hfile(INVALID_HANDLE_VALUE);
    auto engine = std::make_unique<Xterm256Engine>(std::move(hFile), SetUpViewport());
    auto pfn = std::bind(&VtRendererTest::WriteCallback, this, std::placeholders::_1, std::placeholders::_2);
    engine->SetTestCallback(pfn);

    VerifyFirstPaint(*engine);

    auto view = SetUpViewport();

    Log::Comment(NoThrowString().Format(
        L"Test changing the text attributes"));

    Log::Comment(NoThrowString().Format(
        L"----Turn the extended attributes on----"));
    TestPaint(*engine, [&]() {
        // Merge the "on" sequences into expected input.
        std::copy(onSequences.cbegin(), onSequences.cend(), std::back_inserter(qExpectedInput));
        VERIFY_SUCCEEDED(engine->_UpdateExtendedAttrs(desiredAttrs));
    });

    Log::Comment(NoThrowString().Format(
        L"----Turn the extended attributes off----"));
    TestPaint(*engine, [&]() {
        std::copy(offSequences.cbegin(), offSequences.cend(), std::back_inserter(qExpectedInput));
        VERIFY_SUCCEEDED(engine->_UpdateExtendedAttrs({}));
    });

    Log::Comment(NoThrowString().Format(
        L"----Turn the extended attributes back on----"));
    TestPaint(*engine, [&]() {
        std::copy(onSequences.cbegin(), onSequences.cend(), std::back_inserter(qExpectedInput));
        VERIFY_SUCCEEDED(engine->_UpdateExtendedAttrs(desiredAttrs));
    });

    VerifyExpectedInputsDrained();
}

void VtRendererTest::Xterm256TestAttributesAcrossReset()
{
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:renditionAttribute", L"{1, 2, 3, 4, 5, 7, 8, 9, 21, 53}")
    END_TEST_METHOD_PROPERTIES()

    int renditionAttribute;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"renditionAttribute", renditionAttribute));

    std::stringstream renditionSequence;
    // test underline with curly underlined
    if (renditionAttribute == 4)
    {
        renditionSequence << "\x1b[4:3m";
    }
    else
    {
        renditionSequence << "\x1b[" << renditionAttribute << "m";
    }

    auto hFile = wil::unique_hfile(INVALID_HANDLE_VALUE);
    auto engine = std::make_unique<Xterm256Engine>(std::move(hFile), SetUpViewport());
    auto pfn = std::bind(&VtRendererTest::WriteCallback, this, std::placeholders::_1, std::placeholders::_2);
    engine->SetTestCallback(pfn);
    RenderSettings renderSettings;
    RenderData renderData;

    Log::Comment(L"Make sure rendition attributes are retained when colors are reset");

    Log::Comment(L"----Start With All Attributes Reset----");
    TextAttribute textAttributes = {};
    qExpectedInput.push_back("\x1b[m");
    VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes, renderSettings, &renderData, false, false));

    switch (renditionAttribute)
    {
    case GraphicsOptions::Intense:
        Log::Comment(L"----Set Intense Attribute----");
        textAttributes.SetIntense(true);
        break;
    case GraphicsOptions::RGBColorOrFaint:
        Log::Comment(L"----Set Faint Attribute----");
        textAttributes.SetFaint(true);
        break;
    case GraphicsOptions::Italics:
        Log::Comment(L"----Set Italics Attribute----");
        textAttributes.SetItalic(true);
        break;
    case GraphicsOptions::Underline:
        Log::Comment(L"----Set Underline Attribute----");
        textAttributes.SetUnderlineStyle(UnderlineStyle::CurlyUnderlined);
        break;
    case GraphicsOptions::DoublyUnderlined:
        Log::Comment(L"----Set Doubly Underlined Attribute----");
        textAttributes.SetUnderlineStyle(UnderlineStyle::DoublyUnderlined);
        break;
    case GraphicsOptions::Overline:
        Log::Comment(L"----Set Overline Attribute----");
        textAttributes.SetOverlined(true);
        break;
    case GraphicsOptions::BlinkOrXterm256Index:
        Log::Comment(L"----Set Blink Attribute----");
        textAttributes.SetBlinking(true);
        break;
    case GraphicsOptions::Negative:
        Log::Comment(L"----Set Negative Attribute----");
        textAttributes.SetReverseVideo(true);
        break;
    case GraphicsOptions::Invisible:
        Log::Comment(L"----Set Invisible Attribute----");
        textAttributes.SetInvisible(true);
        break;
    case GraphicsOptions::CrossedOut:
        Log::Comment(L"----Set Crossed Out Attribute----");
        textAttributes.SetCrossedOut(true);
        break;
    }
    qExpectedInput.push_back(renditionSequence.str());
    VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes, renderSettings, &renderData, false, false));

    Log::Comment(L"----Set Green Foreground----");
    textAttributes.SetIndexedForeground(TextColor::DARK_GREEN);
    qExpectedInput.push_back("\x1b[32m");
    VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes, renderSettings, &renderData, false, false));

    Log::Comment(L"----Reset Default Foreground and Retain Rendition----");
    textAttributes.SetDefaultForeground();
    qExpectedInput.push_back("\x1b[m");
    qExpectedInput.push_back(renditionSequence.str());
    VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes, renderSettings, &renderData, false, false));

    Log::Comment(L"----Set Green Background----");
    textAttributes.SetIndexedBackground(TextColor::DARK_GREEN);
    qExpectedInput.push_back("\x1b[42m");
    VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes, renderSettings, &renderData, false, false));

    Log::Comment(L"----Reset Default Background and Retain Rendition----");
    textAttributes.SetDefaultBackground();
    qExpectedInput.push_back("\x1b[m");
    qExpectedInput.push_back(renditionSequence.str());
    VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes, renderSettings, &renderData, false, false));

    VerifyExpectedInputsDrained();
}

void VtRendererTest::Xterm256TestDoublyUnderlinedResetBeforeSettingStyle()
{
    auto hFile = wil::unique_hfile(INVALID_HANDLE_VALUE);
    auto engine = std::make_unique<Xterm256Engine>(std::move(hFile), SetUpViewport());
    auto pfn = std::bind(&VtRendererTest::WriteCallback, this, std::placeholders::_1, std::placeholders::_2);
    engine->SetTestCallback(pfn);

    VerifyFirstPaint(*engine);

    auto attrs = TextAttribute{};

    Log::Comment(NoThrowString().Format(
        L"----Testing Doubly underlined is properly reset before applying the new underline style----"));

    Log::Comment(NoThrowString().Format(
        L"----Set Doubly Underlined----"));
    TestPaint(*engine, [&]() {
        attrs.SetUnderlineStyle(UnderlineStyle::DoublyUnderlined);
        qExpectedInput.push_back("\x1b[21m");
        VERIFY_SUCCEEDED(engine->_UpdateExtendedAttrs(attrs));
    });

    Log::Comment(NoThrowString().Format(
        L"----Set Underline To Any Other Style----"));
    TestPaint(*engine, [&]() {
        attrs.SetUnderlineStyle(UnderlineStyle::CurlyUnderlined);
        qExpectedInput.push_back("\x1b[24m");
        qExpectedInput.push_back("\x1b[4:3m");
        VERIFY_SUCCEEDED(engine->_UpdateExtendedAttrs(attrs));
    });

    Log::Comment(NoThrowString().Format(
        L"----Remove The Underline----"));
    TestPaint(*engine, [&]() {
        attrs.SetUnderlineStyle(UnderlineStyle::NoUnderline);
        qExpectedInput.push_back("\x1b[24m");
        VERIFY_SUCCEEDED(engine->_UpdateExtendedAttrs(attrs));
    });

    VerifyExpectedInputsDrained();
}

void VtRendererTest::XtermTestInvalidate()
{
    auto hFile = wil::unique_hfile(INVALID_HANDLE_VALUE);
    auto engine = std::make_unique<XtermEngine>(std::move(hFile), SetUpViewport(), false);
    auto pfn = std::bind(&VtRendererTest::WriteCallback, this, std::placeholders::_1, std::placeholders::_2);
    engine->SetTestCallback(pfn);

    VerifyFirstPaint(*engine);

    auto view = SetUpViewport();

    Log::Comment(NoThrowString().Format(
        L"Make sure that invalidating all invalidates the whole viewport."));
    VERIFY_SUCCEEDED(engine->InvalidateAll());
    TestPaint(*engine, [&]() {
        VERIFY_IS_TRUE(engine->_invalidMap.all());
    });

    Log::Comment(NoThrowString().Format(
        L"Make sure that invalidating anything only invalidates that portion"));
    til::rect invalid = { 1, 1, 2, 2 };
    VERIFY_SUCCEEDED(engine->Invalidate(&invalid));
    TestPaint(*engine, [&]() {
        VERIFY_IS_TRUE(engine->_invalidMap.one());
        VERIFY_ARE_EQUAL(invalid, *(engine->_invalidMap.begin()));
    });

    Log::Comment(NoThrowString().Format(
        L"Make sure that scrolling only invalidates part of the viewport, and sends the right sequences"));
    til::point scrollDelta = { 0, 1 };
    VERIFY_SUCCEEDED(engine->InvalidateScroll(&scrollDelta));
    TestPaint(*engine, [&]() {
        Log::Comment(NoThrowString().Format(
            L"---- Scrolled one down, only top line is invalid. ----"));
        invalid = view.ToExclusive();
        invalid.bottom = 1;

        const auto runs = engine->_invalidMap.runs();
        VERIFY_ARE_EQUAL(1u, runs.size());
        VERIFY_ARE_EQUAL(invalid, runs.front());

        qExpectedInput.push_back("\x1b[H"); // Go Home
        qExpectedInput.push_back("\x1b[L"); // insert a line
        VERIFY_SUCCEEDED(engine->ScrollFrame());
    });

    scrollDelta = { 0, 3 };
    VERIFY_SUCCEEDED(engine->InvalidateScroll(&scrollDelta));
    TestPaint(*engine, [&]() {
        Log::Comment(NoThrowString().Format(
            L"---- Scrolled three down, only top 3 lines are invalid. ----"));
        invalid = view.ToExclusive();
        invalid.bottom = 3;

        // we should have 3 runs and build a rectangle out of them
        const auto runs = engine->_invalidMap.runs();
        VERIFY_ARE_EQUAL(3u, runs.size());
        auto invalidRect = runs.front();
        for (size_t i = 1; i < runs.size(); ++i)
        {
            invalidRect |= runs[i];
        }

        // verify the rect matches the invalid one.
        VERIFY_ARE_EQUAL(invalid, invalidRect);
        // We would expect a CUP here, but the cursor is already at the home position
        qExpectedInput.push_back("\x1b[3L"); // insert 3 lines
        VERIFY_SUCCEEDED(engine->ScrollFrame());
    });

    scrollDelta = { 0, -1 };
    VERIFY_SUCCEEDED(engine->InvalidateScroll(&scrollDelta));
    TestPaint(*engine, [&]() {
        Log::Comment(NoThrowString().Format(
            L"---- Scrolled one up, only bottom line is invalid. ----"));
        invalid = view.ToExclusive();
        invalid.top = invalid.bottom - 1;

        const auto runs = engine->_invalidMap.runs();
        VERIFY_ARE_EQUAL(1u, runs.size());
        VERIFY_ARE_EQUAL(invalid, runs.front());

        qExpectedInput.push_back("\x1b[32;1H"); // Bottom of buffer
        qExpectedInput.push_back("\n"); // Scroll down once
        VERIFY_SUCCEEDED(engine->ScrollFrame());
    });

    scrollDelta = { 0, -3 };
    VERIFY_SUCCEEDED(engine->InvalidateScroll(&scrollDelta));
    TestPaint(*engine, [&]() {
        Log::Comment(NoThrowString().Format(
            L"---- Scrolled three up, only bottom 3 lines are invalid. ----"));
        invalid = view.ToExclusive();
        invalid.top = invalid.bottom - 3;

        // we should have 3 runs and build a rectangle out of them
        const auto runs = engine->_invalidMap.runs();
        VERIFY_ARE_EQUAL(3u, runs.size());
        auto invalidRect = runs.front();
        for (size_t i = 1; i < runs.size(); ++i)
        {
            invalidRect |= runs[i];
        }

        // verify the rect matches the invalid one.
        VERIFY_ARE_EQUAL(invalid, invalidRect);

        // We would expect a CUP here, but we're already at the bottom from the last call.
        qExpectedInput.push_back("\n\n\n"); // Scroll down three times
        VERIFY_SUCCEEDED(engine->ScrollFrame());
    });

    Log::Comment(NoThrowString().Format(
        L"Multiple scrolls are coalesced"));

    scrollDelta = { 0, 1 };
    VERIFY_SUCCEEDED(engine->InvalidateScroll(&scrollDelta));
    scrollDelta = { 0, 2 };
    VERIFY_SUCCEEDED(engine->InvalidateScroll(&scrollDelta));
    TestPaint(*engine, [&]() {
        Log::Comment(NoThrowString().Format(
            L"---- Scrolled three down, only top 3 lines are invalid. ----"));
        invalid = view.ToExclusive();
        invalid.bottom = 3;

        // we should have 3 runs and build a rectangle out of them
        const auto runs = engine->_invalidMap.runs();
        VERIFY_ARE_EQUAL(3u, runs.size());
        auto invalidRect = runs.front();
        for (size_t i = 1; i < runs.size(); ++i)
        {
            invalidRect |= runs[i];
        }

        // verify the rect matches the invalid one.
        VERIFY_ARE_EQUAL(invalid, invalidRect);

        qExpectedInput.push_back("\x1b[H"); // Go to home
        qExpectedInput.push_back("\x1b[3L"); // insert 3 lines
        VERIFY_SUCCEEDED(engine->ScrollFrame());
    });

    scrollDelta = { 0, 1 };
    VERIFY_SUCCEEDED(engine->InvalidateScroll(&scrollDelta));
    Log::Comment(engine->_invalidMap.to_string().c_str());

    scrollDelta = { 0, -1 };
    VERIFY_SUCCEEDED(engine->InvalidateScroll(&scrollDelta));
    Log::Comment(engine->_invalidMap.to_string().c_str());

    qExpectedInput.push_back("\x1b[2J");
    TestPaint(*engine, [&]() {
        Log::Comment(NoThrowString().Format(
            L"---- Scrolled one down and one up, nothing should change ----"
            L" But it still does for now MSFT:14169294"));

        const auto runs = engine->_invalidMap.runs();
        auto invalidRect = runs.front();
        for (size_t i = 1; i < runs.size(); ++i)
        {
            invalidRect |= runs[i];
        }

        // only the bottom line should be dirty.
        // When we scrolled down, the bitmap looked like this:
        // 1111
        // 0000
        // 0000
        // 0000
        // And then we scrolled up and the top line fell off and a bottom
        // line was filled in like this:
        // 0000
        // 0000
        // 0000
        // 1111
        const til::rect expected{ til::point{ view.Left(), view.BottomInclusive() }, til::size{ view.Width(), 1 } };
        VERIFY_ARE_EQUAL(expected, invalidRect);

        VERIFY_SUCCEEDED(engine->ScrollFrame());
    });
}

void VtRendererTest::XtermTestColors()
{
    auto hFile = wil::unique_hfile(INVALID_HANDLE_VALUE);
    auto engine = std::make_unique<XtermEngine>(std::move(hFile), SetUpViewport(), false);
    auto pfn = std::bind(&VtRendererTest::WriteCallback, this, std::placeholders::_1, std::placeholders::_2);
    engine->SetTestCallback(pfn);
    RenderSettings renderSettings;
    RenderData renderData;

    VerifyFirstPaint(*engine);

    auto view = SetUpViewport();

    Log::Comment(NoThrowString().Format(
        L"Test changing the text attributes"));

    Log::Comment(NoThrowString().Format(
        L"Begin by setting the default colors"));

    qExpectedInput.push_back("\x1b[m");
    VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes({},
                                                  renderSettings,
                                                  &renderData,
                                                  false,
                                                  false));

    TestPaint(*engine, [&]() {
        TextAttribute textAttributes;

        Log::Comment(NoThrowString().Format(
            L"----Change only the BG----"));
        textAttributes.SetIndexedBackground(TextColor::DARK_RED);
        qExpectedInput.push_back("\x1b[41m"); // Background DARK_RED
        VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes,
                                                      renderSettings,
                                                      &renderData,
                                                      false,
                                                      false));

        Log::Comment(NoThrowString().Format(
            L"----Change only the FG----"));
        textAttributes.SetIndexedForeground(TextColor::DARK_WHITE);
        qExpectedInput.push_back("\x1b[37m"); // Foreground DARK_WHITE
        VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes,
                                                      renderSettings,
                                                      &renderData,
                                                      false,
                                                      false));

        Log::Comment(NoThrowString().Format(
            L"----Change only the BG to an RGB value----"));
        textAttributes.SetBackground(RGB(19, 161, 14));
        qExpectedInput.push_back("\x1b[42m"); // Background DARK_GREEN
        VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes,
                                                      renderSettings,
                                                      &renderData,
                                                      false,
                                                      false));

        Log::Comment(NoThrowString().Format(
            L"----Change only the FG to an RGB value----"));
        textAttributes.SetForeground(RGB(193, 156, 0));
        qExpectedInput.push_back("\x1b[33m"); // Foreground DARK_YELLOW
        VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes,
                                                      renderSettings,
                                                      &renderData,
                                                      false,
                                                      false));

        Log::Comment(NoThrowString().Format(
            L"----Change only the BG to the 'Default' background----"));
        textAttributes.SetDefaultBackground();
        qExpectedInput.push_back("\x1b[m"); // Both foreground and background default
        qExpectedInput.push_back("\x1b[33m"); // Reapply foreground DARK_YELLOW
        VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes,
                                                      renderSettings,
                                                      &renderData,
                                                      false,
                                                      false));

        Log::Comment(NoThrowString().Format(
            L"----Change only the FG to a 256-color index----"));
        textAttributes.SetIndexedForeground256(TextColor::DARK_WHITE);
        qExpectedInput.push_back("\x1b[37m"); // Foreground DARK_WHITE
        VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes,
                                                      renderSettings,
                                                      &renderData,
                                                      false,
                                                      false));

        Log::Comment(NoThrowString().Format(
            L"----Change only the BG to a 256-color index----"));
        textAttributes.SetIndexedBackground256(TextColor::DARK_RED);
        qExpectedInput.push_back("\x1b[41m"); // Background DARK_RED
        VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes,
                                                      renderSettings,
                                                      &renderData,
                                                      false,
                                                      false));

        Log::Comment(NoThrowString().Format(
            L"----Change only the FG to the 'Default' foreground----"));
        textAttributes.SetDefaultForeground();
        qExpectedInput.push_back("\x1b[m"); // Both foreground and background default
        qExpectedInput.push_back("\x1b[41m"); // Reapply background DARK_RED
        VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes,
                                                      renderSettings,
                                                      &renderData,
                                                      false,
                                                      false));

        Log::Comment(NoThrowString().Format(
            L"----Back to defaults----"));
        textAttributes = {};
        qExpectedInput.push_back("\x1b[m");
        VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes,
                                                      renderSettings,
                                                      &renderData,
                                                      false,
                                                      false));
    });

    TestPaint(*engine, [&]() {
        Log::Comment(NoThrowString().Format(
            L"Make sure that color setting persists across EndPaint/StartPaint"));
        qExpectedInput.push_back(EMPTY_CALLBACK_SENTINEL);
        VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes({},
                                                      renderSettings,
                                                      &renderData,
                                                      false,
                                                      false));
        WriteCallback(EMPTY_CALLBACK_SENTINEL, 1); // This will make sure nothing was written to the callback
    });
}

void VtRendererTest::XtermTestCursor()
{
    auto hFile = wil::unique_hfile(INVALID_HANDLE_VALUE);
    auto engine = std::make_unique<XtermEngine>(std::move(hFile), SetUpViewport(), false);
    auto pfn = std::bind(&VtRendererTest::WriteCallback, this, std::placeholders::_1, std::placeholders::_2);
    engine->SetTestCallback(pfn);

    VerifyFirstPaint(*engine);

    auto view = SetUpViewport();

    Log::Comment(NoThrowString().Format(
        L"Test moving the cursor around. Every sequence should have both params to CUP explicitly."));
    TestPaint(*engine, [&]() {
        qExpectedInput.push_back("\x1b[2;2H");
        VERIFY_SUCCEEDED(engine->_MoveCursor({ 1, 1 }));

        Log::Comment(NoThrowString().Format(
            L"----Only move Y coord----"));
        qExpectedInput.push_back("\x1b[31;2H");
        VERIFY_SUCCEEDED(engine->_MoveCursor({ 1, 30 }));

        Log::Comment(NoThrowString().Format(
            L"----Only move X coord----"));
        qExpectedInput.push_back("\x1b[29C");
        VERIFY_SUCCEEDED(engine->_MoveCursor({ 30, 30 }));

        Log::Comment(NoThrowString().Format(
            L"----Sending the same move sends nothing----"));
        qExpectedInput.push_back(EMPTY_CALLBACK_SENTINEL);
        VERIFY_SUCCEEDED(engine->_MoveCursor({ 30, 30 }));
        WriteCallback(EMPTY_CALLBACK_SENTINEL, 1);

        Log::Comment(NoThrowString().Format(
            L"----moving home sends a simple sequence----"));
        qExpectedInput.push_back("\x1b[H");
        VERIFY_SUCCEEDED(engine->_MoveCursor({ 0, 0 }));

        Log::Comment(NoThrowString().Format(
            L"----move into the line to test some other sequences----"));
        qExpectedInput.push_back("\x1b[7C");
        VERIFY_SUCCEEDED(engine->_MoveCursor({ 7, 0 }));

        Log::Comment(NoThrowString().Format(
            L"----move down one line (x stays the same)----"));
        qExpectedInput.push_back("\n");
        VERIFY_SUCCEEDED(engine->_MoveCursor({ 7, 1 }));

        Log::Comment(NoThrowString().Format(
            L"----move to the start of the next line----"));
        qExpectedInput.push_back("\r\n");
        VERIFY_SUCCEEDED(engine->_MoveCursor({ 0, 2 }));

        Log::Comment(NoThrowString().Format(
            L"----move into the line to test some other sequences----"));
        qExpectedInput.push_back("\x1b[2;8H");
        VERIFY_SUCCEEDED(engine->_MoveCursor({ 7, 1 }));

        Log::Comment(NoThrowString().Format(
            L"----move to the start of this line (y stays the same)----"));
        qExpectedInput.push_back("\r");
        VERIFY_SUCCEEDED(engine->_MoveCursor({ 0, 1 }));
    });

    TestPaint(*engine, [&]() {
        Log::Comment(NoThrowString().Format(
            L"Sending the same move across paint calls sends nothing."
            L"The cursor's last \"real\" position was 0,0"));
        qExpectedInput.push_back(EMPTY_CALLBACK_SENTINEL);
        VERIFY_SUCCEEDED(engine->_MoveCursor({ 0, 1 }));
        WriteCallback(EMPTY_CALLBACK_SENTINEL, 1);

        Log::Comment(NoThrowString().Format(
            L"Paint some text at 0,0, then try moving the cursor to where it currently is."));
        qExpectedInput.push_back("\x1b[1C");
        qExpectedInput.push_back("asdfghjkl");

        const auto line = L"asdfghjkl";
        const unsigned char rgWidths[] = { 1, 1, 1, 1, 1, 1, 1, 1, 1 };

        std::vector<Cluster> clusters;
        for (size_t i = 0; i < wcslen(line); i++)
        {
            clusters.emplace_back(std::wstring_view{ &line[i], 1 }, static_cast<til::CoordType>(rgWidths[i]));
        }

        VERIFY_SUCCEEDED(engine->PaintBufferLine({ clusters.data(), clusters.size() }, { 1, 1 }, false, false));

        qExpectedInput.push_back(EMPTY_CALLBACK_SENTINEL);
        VERIFY_SUCCEEDED(engine->_MoveCursor({ 10, 1 }));
        WriteCallback(EMPTY_CALLBACK_SENTINEL, 1);
    });

    // Note that only PaintBufferLine updates the "Real" cursor position, which
    //  the cursor is moved back to at the end of each paint
    TestPaint(*engine, [&]() {
        Log::Comment(NoThrowString().Format(
            L"Sending the same move across paint calls sends nothing."));
        qExpectedInput.push_back(EMPTY_CALLBACK_SENTINEL);
        VERIFY_SUCCEEDED(engine->_MoveCursor({ 10, 1 }));
        WriteCallback(EMPTY_CALLBACK_SENTINEL, 1);
    });
}

void VtRendererTest::XtermTestAttributesAcrossReset()
{
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:renditionAttribute", L"{1, 4, 7}")
    END_TEST_METHOD_PROPERTIES()

    int renditionAttribute;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"renditionAttribute", renditionAttribute));

    std::stringstream renditionSequence;
    renditionSequence << "\x1b[" << renditionAttribute << "m";

    auto hFile = wil::unique_hfile(INVALID_HANDLE_VALUE);
    auto engine = std::make_unique<XtermEngine>(std::move(hFile), SetUpViewport(), false);
    auto pfn = std::bind(&VtRendererTest::WriteCallback, this, std::placeholders::_1, std::placeholders::_2);
    engine->SetTestCallback(pfn);
    RenderSettings renderSettings;
    RenderData renderData;

    Log::Comment(L"Make sure rendition attributes are retained when colors are reset");

    Log::Comment(L"----Start With All Attributes Reset----");
    TextAttribute textAttributes = {};
    qExpectedInput.push_back("\x1b[m");
    VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes, renderSettings, &renderData, false, false));

    switch (renditionAttribute)
    {
    case GraphicsOptions::Intense:
        Log::Comment(L"----Set Intense Attribute----");
        textAttributes.SetIntense(true);
        break;
    case GraphicsOptions::Underline:
        Log::Comment(L"----Set Underline Attribute----");
        textAttributes.SetUnderlineStyle(UnderlineStyle::SinglyUnderlined);
        break;
    case GraphicsOptions::Negative:
        Log::Comment(L"----Set Negative Attribute----");
        textAttributes.SetReverseVideo(true);
        break;
    }
    qExpectedInput.push_back(renditionSequence.str());
    VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes, renderSettings, &renderData, false, false));

    Log::Comment(L"----Set Green Foreground----");
    textAttributes.SetIndexedForeground(TextColor::DARK_GREEN);
    qExpectedInput.push_back("\x1b[32m");
    VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes, renderSettings, &renderData, false, false));

    Log::Comment(L"----Reset Default Foreground and Retain Rendition----");
    textAttributes.SetDefaultForeground();
    qExpectedInput.push_back("\x1b[m");
    qExpectedInput.push_back(renditionSequence.str());
    VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes, renderSettings, &renderData, false, false));

    Log::Comment(L"----Set Green Background----");
    textAttributes.SetIndexedBackground(TextColor::DARK_GREEN);
    qExpectedInput.push_back("\x1b[42m");
    VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes, renderSettings, &renderData, false, false));

    Log::Comment(L"----Reset Default Background and Retain Rendition----");
    textAttributes.SetDefaultBackground();
    qExpectedInput.push_back("\x1b[m");
    qExpectedInput.push_back(renditionSequence.str());
    VERIFY_SUCCEEDED(engine->UpdateDrawingBrushes(textAttributes, renderSettings, &renderData, false, false));

    VerifyExpectedInputsDrained();
}

void VtRendererTest::TestWrapping()
{
    auto hFile = wil::unique_hfile(INVALID_HANDLE_VALUE);
    auto engine = std::make_unique<Xterm256Engine>(std::move(hFile), SetUpViewport());
    auto pfn = std::bind(&VtRendererTest::WriteCallback, this, std::placeholders::_1, std::placeholders::_2);
    engine->SetTestCallback(pfn);

    VerifyFirstPaint(*engine);

    auto view = SetUpViewport();

    TestPaint(*engine, [&]() {
        Log::Comment(NoThrowString().Format(
            L"Make sure the cursor is at 0,0"));
        qExpectedInput.push_back("\x1b[H");
        VERIFY_SUCCEEDED(engine->_MoveCursor({ 0, 0 }));
    });

    TestPaint(*engine, [&]() {
        Log::Comment(NoThrowString().Format(
            L"Painting a line that wrapped, then painting another line, and "
            L"making sure we don't manually move the cursor between those paints."));
        qExpectedInput.push_back("asdfghjkl");
        // TODO: Undoing this behavior due to 18123777. Will come back in MSFT:16485846
        qExpectedInput.push_back("\r\n");
        qExpectedInput.push_back("zxcvbnm,.");

        const auto line1 = L"asdfghjkl";
        const auto line2 = L"zxcvbnm,.";
        const unsigned char rgWidths[] = { 1, 1, 1, 1, 1, 1, 1, 1, 1 };

        std::vector<Cluster> clusters1;
        for (size_t i = 0; i < wcslen(line1); i++)
        {
            clusters1.emplace_back(std::wstring_view{ &line1[i], 1 }, static_cast<til::CoordType>(rgWidths[i]));
        }
        std::vector<Cluster> clusters2;
        for (size_t i = 0; i < wcslen(line2); i++)
        {
            clusters2.emplace_back(std::wstring_view{ &line2[i], 1 }, static_cast<til::CoordType>(rgWidths[i]));
        }

        VERIFY_SUCCEEDED(engine->PaintBufferLine({ clusters1.data(), clusters1.size() }, { 0, 0 }, false, false));
        VERIFY_SUCCEEDED(engine->PaintBufferLine({ clusters2.data(), clusters2.size() }, { 0, 1 }, false, false));
    });
}

void VtRendererTest::TestResize()
{
    auto view = SetUpViewport();
    auto hFile = wil::unique_hfile(INVALID_HANDLE_VALUE);
    auto engine = std::make_unique<Xterm256Engine>(std::move(hFile), view);
    auto pfn = std::bind(&VtRendererTest::WriteCallback, this, std::placeholders::_1, std::placeholders::_2);
    engine->SetTestCallback(pfn);

    // Verify the first BeginPaint emits a clear and go home
    qExpectedInput.push_back("\x1b[2J");
    // Verify the first EndPaint sets the cursor state
    qExpectedInput.push_back("\x1b[?25l");
    VERIFY_IS_TRUE(engine->_firstPaint);
    VERIFY_IS_TRUE(engine->_suppressResizeRepaint);

    // The renderer (in Renderer@_PaintFrameForEngine..._CheckViewportAndScroll)
    //      will manually call UpdateViewport once before actually painting the
    //      first frame. Replicate that behavior here
    VERIFY_SUCCEEDED(engine->UpdateViewport(view.ToInclusive()));

    TestPaint(*engine, [&]() {
        VERIFY_IS_FALSE(engine->_firstPaint);
        VERIFY_IS_FALSE(engine->_suppressResizeRepaint);
    });

    // Resize the viewport to 120x30
    // Everything should be invalidated, and a resize message sent.
    const auto newView = Viewport::FromDimensions({ 0, 0 }, { 120, 30 });
    qExpectedInput.push_back("\x1b[8;30;120t");

    VERIFY_SUCCEEDED(engine->UpdateViewport(newView.ToInclusive()));

    TestPaint(*engine, [&]() {
        VERIFY_IS_TRUE(engine->_invalidMap.all());
        VERIFY_IS_FALSE(engine->_firstPaint);
        VERIFY_IS_FALSE(engine->_suppressResizeRepaint);
    });
}

void VtRendererTest::TestCursorVisibility()
{
    auto view = SetUpViewport();
    auto hFile = wil::unique_hfile(INVALID_HANDLE_VALUE);
    auto engine = std::make_unique<Xterm256Engine>(std::move(hFile), view);
    auto pfn = std::bind(&VtRendererTest::WriteCallback, this, std::placeholders::_1, std::placeholders::_2);
    engine->SetTestCallback(pfn);

    til::point origin{ 0, 0 };

    VERIFY_ARE_NOT_EQUAL(origin, engine->_lastText);

    CursorOptions options{};
    options.coordCursor = origin;

    // Frame 1: Paint the cursor at the home position. At the end of the frame,
    // the cursor should be on. Because we're moving the cursor with CUP, we
    // need to disable the cursor during this frame.
    qExpectedInput.push_back("\x1b[2J");
    TestPaint(*engine, [&]() {
        VERIFY_IS_FALSE(engine->_nextCursorIsVisible);
        VERIFY_IS_FALSE(engine->_needToDisableCursor);

        Log::Comment(NoThrowString().Format(L"Make sure the cursor is at 0,0"));
        qExpectedInput.push_back("\x1b[H");
        VERIFY_SUCCEEDED(engine->PaintCursor(options));

        VERIFY_IS_TRUE(engine->_nextCursorIsVisible);
        VERIFY_IS_TRUE(engine->_needToDisableCursor);

        // GH#12401:
        // The other tests verify that the cursor is explicitly hidden on the
        // first frame (VerifyFirstPaint). This test on the other hand does
        // the opposite by calling PaintCursor() during the first paint cycle.
        qExpectedInput.push_back("\x1b[?25h");
    });

    VERIFY_IS_TRUE(engine->_nextCursorIsVisible);
    VERIFY_IS_FALSE(engine->_needToDisableCursor);

    // Frame 2: Paint the cursor again at the home position. At the end of the
    // frame, the cursor should be on, the same as before. We aren't moving the
    // cursor during this frame, so _needToDisableCursor will stay false.
    TestPaint(*engine, [&]() {
        VERIFY_IS_FALSE(engine->_nextCursorIsVisible);
        VERIFY_IS_FALSE(engine->_needToDisableCursor);

        Log::Comment(NoThrowString().Format(L"If we just paint the cursor again at the same position, the cursor should not need to be disabled"));
        VERIFY_SUCCEEDED(engine->PaintCursor(options));

        VERIFY_IS_TRUE(engine->_nextCursorIsVisible);
        VERIFY_IS_FALSE(engine->_needToDisableCursor);
    });

    VERIFY_IS_TRUE(engine->_nextCursorIsVisible);
    VERIFY_IS_FALSE(engine->_needToDisableCursor);

    // Frame 3: Paint the cursor at 2,2. At the end of the frame, the cursor
    // should be on, the same as before. Because we're moving the cursor with
    // CUP, we need to disable the cursor during this frame.
    TestPaint(*engine, [&]() {
        VERIFY_IS_FALSE(engine->_nextCursorIsVisible);
        VERIFY_IS_FALSE(engine->_needToDisableCursor);

        Log::Comment(NoThrowString().Format(L"Move the cursor to 2,2"));
        qExpectedInput.push_back("\x1b[3;3H");

        options.coordCursor = { 2, 2 };

        VERIFY_SUCCEEDED(engine->PaintCursor(options));

        VERIFY_IS_TRUE(engine->_nextCursorIsVisible);
        VERIFY_IS_TRUE(engine->_needToDisableCursor);

        // Because _needToDisableCursor is true, we'll insert a ?25l at the
        // start of the frame. Unfortunately, we can't test to make sure that
        // it's there, but we can ensure that the matching ?25h is printed:
        qExpectedInput.push_back("\x1b[?25h");
    });

    VERIFY_IS_TRUE(engine->_nextCursorIsVisible);
    VERIFY_IS_FALSE(engine->_needToDisableCursor);

    // Frame 4: Don't paint the cursor. At the end of the frame, the cursor
    // should be off.
    Log::Comment(NoThrowString().Format(L"Painting without calling PaintCursor will hide the cursor"));
    TestPaint(*engine, [&]() {
        VERIFY_IS_FALSE(engine->_nextCursorIsVisible);
        VERIFY_IS_FALSE(engine->_needToDisableCursor);

        qExpectedInput.push_back("\x1b[?25l");
    });

    VERIFY_IS_FALSE(engine->_nextCursorIsVisible);
    VERIFY_IS_FALSE(engine->_needToDisableCursor);
}

void VtRendererTest::FormattedString()
{
    // This test works with a static cache variable that
    // can be affected by other tests
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
    END_TEST_METHOD_PROPERTIES();

    static const auto format = FMT_COMPILE("\x1b[{}m");
    const auto value = 12;

    auto view = SetUpViewport();
    auto hFile = wil::unique_hfile(INVALID_HANDLE_VALUE);
    auto engine = std::make_unique<Xterm256Engine>(std::move(hFile), view);
    auto pfn = std::bind(&VtRendererTest::WriteCallback, this, std::placeholders::_1, std::placeholders::_2);
    engine->SetTestCallback(pfn);

    Log::Comment(L"1.) Write it once. It should resize itself.");
    qExpectedInput.push_back("\x1b[12m");
    VERIFY_SUCCEEDED(engine->_WriteFormatted(format, value));

    Log::Comment(L"2.) Write the same thing again, should be fine.");
    qExpectedInput.push_back("\x1b[12m");
    VERIFY_SUCCEEDED(engine->_WriteFormatted(format, value));

    Log::Comment(L"3.) Now write something huge. Should resize itself and still be fine.");
    static const auto bigFormat = FMT_COMPILE("\x1b[28;3;{};{};{}m");
    const auto bigValue = 500;
    qExpectedInput.push_back("\x1b[28;3;500;500;500m");
    VERIFY_SUCCEEDED(engine->_WriteFormatted(bigFormat, bigValue, bigValue, bigValue));
}
