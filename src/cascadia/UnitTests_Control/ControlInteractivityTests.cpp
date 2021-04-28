// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "../TerminalControl/EventArgs.h"
#include "../TerminalControl/ControlInteractivity.h"
#include "MockControlSettings.h"
#include "MockConnection.h"

using namespace ::Microsoft::Console;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

using namespace winrt;
using namespace winrt::Microsoft::Terminal;
using namespace ::Microsoft::Terminal::Core;
using namespace ::Microsoft::Console::VirtualTerminal;

namespace ControlUnitTests
{
    class ControlInteractivityTests
    {
        BEGIN_TEST_CLASS(ControlInteractivityTests)
            TEST_CLASS_PROPERTY(L"TestTimeout", L"0:0:10") // 10s timeout
        END_TEST_CLASS()

        TEST_METHOD(TestAdjustAcrylic);
        TEST_METHOD(TestScrollWithMouse);

        TEST_METHOD(CreateSubsequentSelectionWithDragging);
        TEST_METHOD(ScrollWithSelection);
        TEST_METHOD(TestScrollWithTrackpad);
        TEST_METHOD(TestQuickDragOnSelect);

        TEST_CLASS_SETUP(ClassSetup)
        {
            winrt::init_apartment(winrt::apartment_type::single_threaded);

            return true;
        }
        TEST_CLASS_CLEANUP(ClassCleanup)
        {
            winrt::uninit_apartment();
            return true;
        }

        std::tuple<winrt::com_ptr<MockControlSettings>,
                   winrt::com_ptr<MockConnection>>
        _createSettingsAndConnection()
        {
            Log::Comment(L"Create settings object");
            auto settings = winrt::make_self<MockControlSettings>();
            VERIFY_IS_NOT_NULL(settings);

            Log::Comment(L"Create connection object");
            auto conn = winrt::make_self<MockConnection>();
            VERIFY_IS_NOT_NULL(conn);

            return { settings, conn };
        }

        std::tuple<winrt::com_ptr<Control::implementation::ControlCore>,
                   winrt::com_ptr<Control::implementation::ControlInteractivity>>
        _createCoreAndInteractivity(Control::IControlSettings settings,
                                    TerminalConnection::ITerminalConnection conn)
        {
            Log::Comment(L"Create ControlInteractivity object");
            auto interactivity = winrt::make_self<Control::implementation::ControlInteractivity>(settings, conn);
            VERIFY_IS_NOT_NULL(interactivity);
            auto core = interactivity->_core;
            VERIFY_IS_NOT_NULL(core);

            return { core, interactivity };
        }

        void _standardInit(winrt::com_ptr<Control::implementation::ControlCore> core,
                           winrt::com_ptr<Control::implementation::ControlInteractivity> interactivity)
        {
            // "Consolas" ends up with an actual size of 9x21 at 96DPI. So
            // let's just arbitrarily start with a 270x420px (30x20 chars) window
            core->Initialize(270, 420, 1.0);
            VERIFY_IS_TRUE(core->_initializedTerminal);
            VERIFY_ARE_EQUAL(20, core->_terminal->GetViewport().Height());
            interactivity->Initialize();
        }
    };

    void ControlInteractivityTests::TestAdjustAcrylic()
    {
        Log::Comment(L"Test that scrolling the mouse wheel with Ctrl+Shift changes opacity");
        Log::Comment(L"(This test won't log as it goes, because it does some 200 verifications.)");

        WEX::TestExecution::SetVerifyOutput verifyOutputScope{ WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures };

        auto [settings, conn] = _createSettingsAndConnection();

        settings->UseAcrylic(true);
        settings->TintOpacity(0.5f);

        auto [core, interactivity] = _createCoreAndInteractivity(*settings, *conn);

        // A callback to make sure that we're raising TransparencyChanged events
        double expectedOpacity = 0.5;
        auto opacityCallback = [&](auto&&, Control::TransparencyChangedEventArgs args) mutable {
            VERIFY_ARE_EQUAL(expectedOpacity, args.Opacity());
            VERIFY_ARE_EQUAL(expectedOpacity, settings->TintOpacity());
            VERIFY_ARE_EQUAL(expectedOpacity, core->_settings.TintOpacity());

            if (expectedOpacity < 1.0)
            {
                VERIFY_IS_TRUE(settings->UseAcrylic());
                VERIFY_IS_TRUE(core->_settings.UseAcrylic());
            }
            VERIFY_ARE_EQUAL(expectedOpacity < 1.0, settings->UseAcrylic());
            VERIFY_ARE_EQUAL(expectedOpacity < 1.0, core->_settings.UseAcrylic());
        };
        core->TransparencyChanged(opacityCallback);

        const auto modifiers = ControlKeyStates(ControlKeyStates::RightCtrlPressed | ControlKeyStates::ShiftPressed);

        Log::Comment(L"Scroll in the positive direction, increasing opacity");
        // Scroll more than enough times to get to 1.0 from .5.
        for (int i = 0; i < 55; i++)
        {
            // each mouse wheel only adjusts opacity by .01
            expectedOpacity += 0.01;
            if (expectedOpacity >= 1.0)
            {
                expectedOpacity = 1.0;
            }

            // The mouse location and buttons don't matter here.
            interactivity->MouseWheel(modifiers,
                                      30,
                                      til::point{ 0, 0 },
                                      { false, false, false });
        }

        Log::Comment(L"Scroll in the negative direction, decreasing opacity");
        // Scroll more than enough times to get to 0.0 from 1.0
        for (int i = 0; i < 105; i++)
        {
            // each mouse wheel only adjusts opacity by .01
            expectedOpacity -= 0.01;
            if (expectedOpacity <= 0.0)
            {
                expectedOpacity = 0.0;
            }

            // The mouse location and buttons don't matter here.
            interactivity->MouseWheel(modifiers,
                                      30,
                                      til::point{ 0, 0 },
                                      { false, false, false });
        }
    }

    void ControlInteractivityTests::TestScrollWithMouse()
    {
        WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};

        auto [settings, conn] = _createSettingsAndConnection();
        auto [core, interactivity] = _createCoreAndInteractivity(*settings, *conn);
        _standardInit(core, interactivity);
        // For the sake of this test, scroll one line at a time
        interactivity->_rowsToScroll = 1;

        int expectedTop = 0;
        int expectedViewHeight = 20;
        int expectedBufferHeight = 20;

        auto scrollChangedHandler = [&](auto&&, const Control::ScrollPositionChangedArgs& args) mutable {
            VERIFY_ARE_EQUAL(expectedTop, args.ViewTop());
            VERIFY_ARE_EQUAL(expectedViewHeight, args.ViewHeight());
            VERIFY_ARE_EQUAL(expectedBufferHeight, args.BufferSize());
        };
        core->ScrollPositionChanged(scrollChangedHandler);
        interactivity->ScrollPositionChanged(scrollChangedHandler);

        for (int i = 0; i < 40; ++i)
        {
            Log::Comment(NoThrowString().Format(L"Writing line #%d", i));
            // The \r\n in the 19th loop will cause the view to start moving
            if (i >= 19)
            {
                expectedTop++;
                expectedBufferHeight++;
            }

            conn->WriteInput(L"Foo\r\n");
        }
        // We printed that 40 times, but the final \r\n bumped the view down one MORE row.
        VERIFY_ARE_EQUAL(20, core->_terminal->GetViewport().Height());
        VERIFY_ARE_EQUAL(21, core->ScrollOffset());
        VERIFY_ARE_EQUAL(20, core->ViewHeight());
        VERIFY_ARE_EQUAL(41, core->BufferHeight());

        Log::Comment(L"Scroll up a line");
        const auto modifiers = ControlKeyStates();
        expectedBufferHeight = 41;
        expectedTop = 20;

        interactivity->MouseWheel(modifiers,
                                  WHEEL_DELTA,
                                  til::point{ 0, 0 },
                                  { false, false, false });

        Log::Comment(L"Scroll up 19 more times, to the top");
        for (int i = 0; i < 20; ++i)
        {
            expectedTop--;
            interactivity->MouseWheel(modifiers,
                                      WHEEL_DELTA,
                                      til::point{ 0, 0 },
                                      { false, false, false });
        }
        Log::Comment(L"Scrolling up more should do nothing");
        expectedTop = 0;
        interactivity->MouseWheel(modifiers,
                                  WHEEL_DELTA,
                                  til::point{ 0, 0 },
                                  { false, false, false });
        interactivity->MouseWheel(modifiers,
                                  WHEEL_DELTA,
                                  til::point{ 0, 0 },
                                  { false, false, false });

        Log::Comment(L"Scroll down 21 more times, to the bottom");
        for (int i = 0; i < 21; ++i)
        {
            Log::Comment(NoThrowString().Format(L"---scroll down #%d---", i));
            expectedTop++;
            interactivity->MouseWheel(modifiers,
                                      -WHEEL_DELTA,
                                      til::point{ 0, 0 },
                                      { false, false, false });
            Log::Comment(NoThrowString().Format(L"internal scrollbar pos:%f", interactivity->_internalScrollbarPosition));
        }
        Log::Comment(L"Scrolling up more should do nothing");
        expectedTop = 21;
        interactivity->MouseWheel(modifiers,
                                  -WHEEL_DELTA,
                                  til::point{ 0, 0 },
                                  { false, false, false });
        interactivity->MouseWheel(modifiers,
                                  -WHEEL_DELTA,
                                  til::point{ 0, 0 },
                                  { false, false, false });
    }

    void ControlInteractivityTests::CreateSubsequentSelectionWithDragging()
    {
        // This is a test for GH#9725
        WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};

        auto [settings, conn] = _createSettingsAndConnection();
        auto [core, interactivity] = _createCoreAndInteractivity(*settings, *conn);
        _standardInit(core, interactivity);

        // For this test, don't use any modifiers
        const auto modifiers = ControlKeyStates();
        const TerminalInput::MouseButtonState leftMouseDown{ true, false, false };
        const TerminalInput::MouseButtonState noMouseDown{ false, false, false };

        const til::size fontSize{ 9, 21 };

        Log::Comment(L"Click on the terminal");
        const til::point terminalPosition0{ 0, 0 };
        const til::point cursorPosition0 = terminalPosition0 * fontSize;
        interactivity->PointerPressed(leftMouseDown,
                                      WM_LBUTTONDOWN, //pointerUpdateKind
                                      0, // timestamp
                                      modifiers,
                                      cursorPosition0);
        Log::Comment(L"Verify that there's not yet a selection");

        VERIFY_IS_FALSE(core->HasSelection());

        Log::Comment(L"Drag the mouse just a little");
        // move not quite a whole cell, but enough to start a selection
        const til::point terminalPosition1{ 0, 0 };
        const til::point cursorPosition1{ 6, 0 };
        interactivity->PointerMoved(leftMouseDown,
                                    WM_LBUTTONDOWN, //pointerUpdateKind
                                    modifiers,
                                    true, // focused,
                                    cursorPosition1);
        Log::Comment(L"Verify that there's one selection");
        VERIFY_IS_TRUE(core->HasSelection());
        VERIFY_ARE_EQUAL(1u, core->_terminal->GetSelectionRects().size());

        Log::Comment(L"Drag the mouse down a whole row");
        const til::point terminalPosition2{ 1, 1 };
        const til::point cursorPosition2 = terminalPosition2 * fontSize;
        interactivity->PointerMoved(leftMouseDown,
                                    WM_LBUTTONDOWN, //pointerUpdateKind
                                    modifiers,
                                    true, // focused,
                                    cursorPosition2);
        Log::Comment(L"Verify that there's now two selections (one on each row)");
        VERIFY_IS_TRUE(core->HasSelection());
        VERIFY_ARE_EQUAL(2u, core->_terminal->GetSelectionRects().size());

        Log::Comment(L"Release the mouse");
        interactivity->PointerReleased(noMouseDown,
                                       WM_LBUTTONUP, //pointerUpdateKind
                                       modifiers,
                                       cursorPosition2);
        Log::Comment(L"Verify that there's still two selections");
        VERIFY_IS_TRUE(core->HasSelection());
        VERIFY_ARE_EQUAL(2u, core->_terminal->GetSelectionRects().size());

        Log::Comment(L"click outside the current selection");
        const til::point terminalPosition3{ 2, 2 };
        const til::point cursorPosition3 = terminalPosition3 * fontSize;
        interactivity->PointerPressed(leftMouseDown,
                                      WM_LBUTTONDOWN, //pointerUpdateKind
                                      0, // timestamp
                                      modifiers,
                                      cursorPosition3);
        Log::Comment(L"Verify that there's now no selection");
        VERIFY_IS_FALSE(core->HasSelection());
        VERIFY_ARE_EQUAL(0u, core->_terminal->GetSelectionRects().size());

        Log::Comment(L"Drag the mouse");
        const til::point terminalPosition4{ 3, 2 };
        const til::point cursorPosition4 = terminalPosition4 * fontSize;
        interactivity->PointerMoved(leftMouseDown,
                                    WM_LBUTTONDOWN, //pointerUpdateKind
                                    modifiers,
                                    true, // focused,
                                    cursorPosition4);
        Log::Comment(L"Verify that there's now one selection");
        VERIFY_IS_TRUE(core->HasSelection());
        VERIFY_ARE_EQUAL(1u, core->_terminal->GetSelectionRects().size());
    }

    void ControlInteractivityTests::ScrollWithSelection()
    {
        // This is a test for GH#9955.a
        auto [settings, conn] = _createSettingsAndConnection();
        auto [core, interactivity] = _createCoreAndInteractivity(*settings, *conn);
        _standardInit(core, interactivity);
        // For the sake of this test, scroll one line at a time
        interactivity->_rowsToScroll = 1;

        Log::Comment(L"Add some test to the terminal so we can scroll");
        for (int i = 0; i < 40; ++i)
        {
            conn->WriteInput(L"Foo\r\n");
        }
        // We printed that 40 times, but the final \r\n bumped the view down one MORE row.
        VERIFY_ARE_EQUAL(20, core->_terminal->GetViewport().Height());
        VERIFY_ARE_EQUAL(21, core->ScrollOffset());
        VERIFY_ARE_EQUAL(20, core->ViewHeight());
        VERIFY_ARE_EQUAL(41, core->BufferHeight());

        // For this test, don't use any modifiers
        const auto modifiers = ControlKeyStates();
        const TerminalInput::MouseButtonState leftMouseDown{ true, false, false };
        const TerminalInput::MouseButtonState noMouseDown{ false, false, false };

        const til::size fontSize{ 9, 21 };

        Log::Comment(L"Click on the terminal");
        const til::point terminalPosition0{ 5, 5 };
        const til::point cursorPosition0{ terminalPosition0 * fontSize };
        interactivity->PointerPressed(leftMouseDown,
                                      WM_LBUTTONDOWN, //pointerUpdateKind
                                      0, // timestamp
                                      modifiers,
                                      cursorPosition0);

        Log::Comment(L"Verify that there's not yet a selection");
        VERIFY_IS_FALSE(core->HasSelection());

        VERIFY_IS_TRUE(interactivity->_singleClickTouchdownPos.has_value());
        VERIFY_ARE_EQUAL(cursorPosition0, interactivity->_singleClickTouchdownPos.value());

        Log::Comment(L"Drag the mouse just a little");
        // move not quite a whole cell, but enough to start a selection
        const til::point cursorPosition1{ cursorPosition0 + til::point{ 6, 0 } };
        interactivity->PointerMoved(leftMouseDown,
                                    WM_LBUTTONDOWN, //pointerUpdateKind
                                    modifiers,
                                    true, // focused,
                                    cursorPosition1);
        Log::Comment(L"Verify that there's one selection");
        VERIFY_IS_TRUE(core->HasSelection());
        VERIFY_ARE_EQUAL(1u, core->_terminal->GetSelectionRects().size());

        Log::Comment(L"Verify the location of the selection");
        // The viewport is on row 21, so the selection will be on:
        // {(5, 5)+(0, 21)} to {(5, 5)+(0, 21)}
        COORD expectedAnchor{ 5, 26 };
        VERIFY_ARE_EQUAL(expectedAnchor, core->_terminal->GetSelectionAnchor());
        VERIFY_ARE_EQUAL(expectedAnchor, core->_terminal->GetSelectionEnd());

        Log::Comment(L"Scroll up a line, with the left mouse button selected");
        interactivity->MouseWheel(modifiers,
                                  WHEEL_DELTA,
                                  cursorPosition1,
                                  { true, false, false });

        Log::Comment(L"Verify the location of the selection");
        // The viewport is now on row 20, so the selection will be on:
        // {(5, 5)+(0, 20)} to {(5, 5)+(0, 21)}
        COORD newExpectedAnchor{ 5, 25 };
        // Remember, the anchor is always before the end in the buffer. So yes,
        // se started the selection on 5,26, but now that's the end.
        VERIFY_ARE_EQUAL(newExpectedAnchor, core->_terminal->GetSelectionAnchor());
        VERIFY_ARE_EQUAL(expectedAnchor, core->_terminal->GetSelectionEnd());
    }

    void ControlInteractivityTests::TestScrollWithTrackpad()
    {
        WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};

        auto [settings, conn] = _createSettingsAndConnection();
        auto [core, interactivity] = _createCoreAndInteractivity(*settings, *conn);
        _standardInit(core, interactivity);
        // For the sake of this test, scroll one line at a time
        interactivity->_rowsToScroll = 1;

        for (int i = 0; i < 40; ++i)
        {
            conn->WriteInput(L"Foo\r\n");
        }
        // We printed that 40 times, but the final \r\n bumped the view down one MORE row.
        VERIFY_ARE_EQUAL(20, core->_terminal->GetViewport().Height());
        VERIFY_ARE_EQUAL(21, core->ScrollOffset());
        VERIFY_ARE_EQUAL(20, core->ViewHeight());
        VERIFY_ARE_EQUAL(41, core->BufferHeight());

        Log::Comment(L"Scroll up a line");
        const auto modifiers = ControlKeyStates();

        // Deltas that I saw while scrolling with the surface laptop trackpad
        // were on the range [-22, 7], though I'm sure they could be greater in
        // magnitude.
        //
        // WHEEL_DELTA is 120, so we'll use 24 for now as the delta, just so the tests don't take forever.

        const int delta = WHEEL_DELTA / 5;
        const til::point mousePos{ 0, 0 };
        TerminalInput::MouseButtonState state{ false, false, false };

        interactivity->MouseWheel(modifiers, delta, mousePos, state); // 1/5
        VERIFY_ARE_EQUAL(21, core->ScrollOffset());

        Log::Comment(L"Scroll up 4 more times. Once we're at 3/5 scrolls, "
                     L"we'll round the internal scrollbar position to scrolling to the next row.");
        interactivity->MouseWheel(modifiers, delta, mousePos, state); // 2/5
        VERIFY_ARE_EQUAL(21, core->ScrollOffset());
        interactivity->MouseWheel(modifiers, delta, mousePos, state); // 3/5
        VERIFY_ARE_EQUAL(20, core->ScrollOffset());
        interactivity->MouseWheel(modifiers, delta, mousePos, state); // 4/5
        VERIFY_ARE_EQUAL(20, core->ScrollOffset());
        interactivity->MouseWheel(modifiers, delta, mousePos, state); // 5/5
        VERIFY_ARE_EQUAL(20, core->ScrollOffset());

        Log::Comment(L"Jump to line 5, so we can scroll down from there.");
        interactivity->UpdateScrollbar(5);
        VERIFY_ARE_EQUAL(5, core->ScrollOffset());
        Log::Comment(L"Scroll down 5 times, at which point we should accumulate a whole row of delta.");
        interactivity->MouseWheel(modifiers, -delta, mousePos, state); // 1/5
        VERIFY_ARE_EQUAL(5, core->ScrollOffset());
        interactivity->MouseWheel(modifiers, -delta, mousePos, state); // 2/5
        VERIFY_ARE_EQUAL(5, core->ScrollOffset());
        interactivity->MouseWheel(modifiers, -delta, mousePos, state); // 3/5
        VERIFY_ARE_EQUAL(6, core->ScrollOffset());
        interactivity->MouseWheel(modifiers, -delta, mousePos, state); // 4/5
        VERIFY_ARE_EQUAL(6, core->ScrollOffset());
        interactivity->MouseWheel(modifiers, -delta, mousePos, state); // 5/5
        VERIFY_ARE_EQUAL(6, core->ScrollOffset());

        Log::Comment(L"Jump to the bottom.");
        interactivity->UpdateScrollbar(21);
        VERIFY_ARE_EQUAL(21, core->ScrollOffset());
        Log::Comment(L"Scroll a bit, then emit a line of text. We should reset our internal scroll position.");
        interactivity->MouseWheel(modifiers, delta, mousePos, state); // 1/5
        VERIFY_ARE_EQUAL(21, core->ScrollOffset());
        interactivity->MouseWheel(modifiers, delta, mousePos, state); // 2/5
        VERIFY_ARE_EQUAL(21, core->ScrollOffset());

        conn->WriteInput(L"Foo\r\n");
        VERIFY_ARE_EQUAL(22, core->ScrollOffset());
        interactivity->MouseWheel(modifiers, delta, mousePos, state); // 1/5
        VERIFY_ARE_EQUAL(22, core->ScrollOffset());
        interactivity->MouseWheel(modifiers, delta, mousePos, state); // 2/5
        VERIFY_ARE_EQUAL(22, core->ScrollOffset());
        interactivity->MouseWheel(modifiers, delta, mousePos, state); // 3/5
        VERIFY_ARE_EQUAL(21, core->ScrollOffset());
        interactivity->MouseWheel(modifiers, delta, mousePos, state); // 4/5
        VERIFY_ARE_EQUAL(21, core->ScrollOffset());
        interactivity->MouseWheel(modifiers, delta, mousePos, state); // 5/5
        VERIFY_ARE_EQUAL(21, core->ScrollOffset());
    }

    void ControlInteractivityTests::TestQuickDragOnSelect()
    {
        // This is a test for GH#9955.c

        auto [settings, conn] = _createSettingsAndConnection();
        auto [core, interactivity] = _createCoreAndInteractivity(*settings, *conn);
        _standardInit(core, interactivity);

        // For this test, don't use any modifiers
        const auto modifiers = ControlKeyStates();
        const TerminalInput::MouseButtonState leftMouseDown{ true, false, false };
        const TerminalInput::MouseButtonState noMouseDown{ false, false, false };

        const til::size fontSize{ 9, 21 };

        Log::Comment(L"Click on the terminal");
        const til::point cursorPosition0{ 6, 0 };
        interactivity->PointerPressed(leftMouseDown,
                                      WM_LBUTTONDOWN, //pointerUpdateKind
                                      0, // timestamp
                                      modifiers,
                                      cursorPosition0);

        Log::Comment(L"Verify that there's not yet a selection");
        VERIFY_IS_FALSE(core->HasSelection());

        VERIFY_IS_TRUE(interactivity->_singleClickTouchdownPos.has_value());
        VERIFY_ARE_EQUAL(cursorPosition0, interactivity->_singleClickTouchdownPos.value());

        Log::Comment(L"Drag the mouse a lot. This simulates dragging the mouse real fast.");
        const til::point cursorPosition1{ 6 + fontSize.width<int>() * 2, 0 };
        interactivity->PointerMoved(leftMouseDown,
                                    WM_LBUTTONDOWN, //pointerUpdateKind
                                    modifiers,
                                    true, // focused,
                                    cursorPosition1);
        Log::Comment(L"Verify that there's one selection");
        VERIFY_IS_TRUE(core->HasSelection());
        VERIFY_ARE_EQUAL(1u, core->_terminal->GetSelectionRects().size());

        Log::Comment(L"Verify that it started on the first cell we clicked on, not the one we dragged to");
        COORD expectedAnchor{ 0, 0 };
        VERIFY_ARE_EQUAL(expectedAnchor, core->_terminal->GetSelectionAnchor());
    }
}
