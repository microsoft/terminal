// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "../TerminalControl/EventArgs.h"
#include "../TerminalControl/ControlInteractivity.h"

#include "../../inc/TestUtils.h"
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

        TEST_METHOD(TestDragSelectOutsideBounds);

        TEST_METHOD(PointerClickOutsideActiveRegion);
        TEST_METHOD(IncrementCircularBufferWithSelection);

        TEST_METHOD(GetMouseEventsInTest);
        TEST_METHOD(AltBufferClampMouse);

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
            auto interactivity = winrt::make_self<Control::implementation::ControlInteractivity>(settings, settings, conn);
            VERIFY_IS_NOT_NULL(interactivity);
            auto core = interactivity->_core;
            core->_inUnitTests = true;
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

        // Returns a scope_exit callback that should be used to ensure all
        // output is drained.
        auto _addInputCallback(const winrt::com_ptr<MockConnection>& conn,
                               std::deque<std::wstring>& expectedOutput)
        {
            conn->TerminalOutput([&](const hstring& hstr) {
                VERIFY_IS_GREATER_THAN(expectedOutput.size(), 0u);
                const auto expected = expectedOutput.front();
                expectedOutput.pop_front();
                Log::Comment(fmt::format(L"Received: \"{}\"", TerminalCoreUnitTests::TestUtils::ReplaceEscapes(hstr.c_str())).c_str());
                Log::Comment(fmt::format(L"Expected: \"{}\"", TerminalCoreUnitTests::TestUtils::ReplaceEscapes(expected)).c_str());
                VERIFY_ARE_EQUAL(expected, hstr);
            });

            return std::move(wil::scope_exit([&]() {
                VERIFY_ARE_EQUAL(0u, expectedOutput.size(), L"Validate we drained all the expected output");
            }));
        }
    };

    void ControlInteractivityTests::TestAdjustAcrylic()
    {
        Log::Comment(L"Test that scrolling the mouse wheel with Ctrl+Shift changes opacity");
        Log::Comment(L"(This test won't log as it goes, because it does some 200 verifications.)");

        WEX::TestExecution::SetVerifyOutput verifyOutputScope{ WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures };

        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:useAcrylic", L"{true, false}")
        END_TEST_METHOD_PROPERTIES()
        bool useAcrylic;
        VERIFY_SUCCEEDED(TestData::TryGetValue(L"useAcrylic", useAcrylic), L"whether or not we should enable acrylic");

        auto [settings, conn] = _createSettingsAndConnection();

        settings->UseAcrylic(useAcrylic);
        settings->Opacity(0.5f);

        auto [core, interactivity] = _createCoreAndInteractivity(*settings, *conn);

        // A callback to make sure that we're raising TransparencyChanged events
        auto expectedOpacity = 0.5;
        auto opacityCallback = [&](auto&&, Control::TransparencyChangedEventArgs args) mutable {
            VERIFY_ARE_EQUAL(expectedOpacity, args.Opacity());
            VERIFY_ARE_EQUAL(expectedOpacity, core->Opacity());
            // The Settings object's opacity shouldn't be changed
            VERIFY_ARE_EQUAL(0.5, settings->Opacity());

            auto expectedUseAcrylic = expectedOpacity < 1.0 &&
                                      (useAcrylic);
            VERIFY_ARE_EQUAL(useAcrylic, settings->UseAcrylic());
            VERIFY_ARE_EQUAL(expectedUseAcrylic, core->UseAcrylic());
        };
        core->TransparencyChanged(opacityCallback);

        const auto modifiers = ControlKeyStates(ControlKeyStates::RightCtrlPressed | ControlKeyStates::ShiftPressed);
        const Control::MouseButtonState buttonState{};

        Log::Comment(L"Scroll in the positive direction, increasing opacity");
        // Scroll more than enough times to get to 1.0 from .5.
        for (auto i = 0; i < 55; i++)
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
                                      Core::Point{ 0, 0 },
                                      buttonState);
        }

        Log::Comment(L"Scroll in the negative direction, decreasing opacity");
        // Scroll more than enough times to get to 0.0 from 1.0
        for (auto i = 0; i < 105; i++)
        {
            // each mouse wheel only adjusts opacity by .01
            expectedOpacity -= 0.01;
            if (expectedOpacity <= 0.0)
            {
                expectedOpacity = 0.0;
            }

            // The mouse location and buttons don't matter here.
            interactivity->MouseWheel(modifiers,
                                      -30,
                                      Core::Point{ 0, 0 },
                                      buttonState);
        }
    }

    void ControlInteractivityTests::TestScrollWithMouse()
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
        END_TEST_METHOD_PROPERTIES()

        WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};

        auto [settings, conn] = _createSettingsAndConnection();
        auto [core, interactivity] = _createCoreAndInteractivity(*settings, *conn);
        _standardInit(core, interactivity);
        // For the sake of this test, scroll one line at a time
        interactivity->_rowsToScroll = 1;

        auto expectedTop = 0;
        auto expectedViewHeight = 20;
        auto expectedBufferHeight = 20;

        auto scrollChangedHandler = [&](auto&&, const Control::ScrollPositionChangedArgs& args) mutable {
            VERIFY_ARE_EQUAL(expectedTop, args.ViewTop());
            VERIFY_ARE_EQUAL(expectedViewHeight, args.ViewHeight());
            VERIFY_ARE_EQUAL(expectedBufferHeight, args.BufferSize());
        };
        core->ScrollPositionChanged(scrollChangedHandler);
        interactivity->ScrollPositionChanged(scrollChangedHandler);

        for (auto i = 0; i < 40; ++i)
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
        const Control::MouseButtonState buttonState{};
        const auto modifiers = ControlKeyStates();
        expectedBufferHeight = 41;
        expectedTop = 20;

        interactivity->MouseWheel(modifiers,
                                  WHEEL_DELTA,
                                  Core::Point{ 0, 0 },
                                  buttonState);

        Log::Comment(L"Scroll up 19 more times, to the top");
        for (auto i = 0; i < 20; ++i)
        {
            expectedTop--;
            interactivity->MouseWheel(modifiers,
                                      WHEEL_DELTA,
                                      Core::Point{ 0, 0 },
                                      buttonState);
        }
        Log::Comment(L"Scrolling up more should do nothing");
        expectedTop = 0;
        interactivity->MouseWheel(modifiers,
                                  WHEEL_DELTA,
                                  Core::Point{ 0, 0 },
                                  buttonState);
        interactivity->MouseWheel(modifiers,
                                  WHEEL_DELTA,
                                  Core::Point{ 0, 0 },
                                  buttonState);

        Log::Comment(L"Scroll down 21 more times, to the bottom");
        for (auto i = 0; i < 21; ++i)
        {
            Log::Comment(NoThrowString().Format(L"---scroll down #%d---", i));
            expectedTop++;
            interactivity->MouseWheel(modifiers,
                                      -WHEEL_DELTA,
                                      Core::Point{ 0, 0 },
                                      buttonState);
            Log::Comment(NoThrowString().Format(L"internal scrollbar pos:%f", interactivity->_internalScrollbarPosition));
        }
        Log::Comment(L"Scrolling down more should do nothing");
        expectedTop = 21;
        interactivity->MouseWheel(modifiers,
                                  -WHEEL_DELTA,
                                  Core::Point{ 0, 0 },
                                  buttonState);
        interactivity->MouseWheel(modifiers,
                                  -WHEEL_DELTA,
                                  Core::Point{ 0, 0 },
                                  buttonState);
    }

    void ControlInteractivityTests::CreateSubsequentSelectionWithDragging()
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
        END_TEST_METHOD_PROPERTIES()

        // This is a test for GH#9725
        WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};

        auto [settings, conn] = _createSettingsAndConnection();
        auto [core, interactivity] = _createCoreAndInteractivity(*settings, *conn);
        _standardInit(core, interactivity);

        // For this test, don't use any modifiers
        const auto modifiers = ControlKeyStates();
        const auto leftMouseDown{ Control::MouseButtonState::IsLeftButtonDown };
        const Control::MouseButtonState noMouseDown{};

        const til::size fontSize{ 9, 21 };

        Log::Comment(L"Click on the terminal");
        const til::point terminalPosition0{ 0, 0 };
        const auto cursorPosition0 = terminalPosition0 * fontSize;
        interactivity->PointerPressed(leftMouseDown,
                                      WM_LBUTTONDOWN, //pointerUpdateKind
                                      0, // timestamp
                                      modifiers,
                                      cursorPosition0.to_core_point());
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
                                    cursorPosition1.to_core_point(),
                                    true);
        Log::Comment(L"Verify that there's one selection");
        VERIFY_IS_TRUE(core->HasSelection());
        VERIFY_ARE_EQUAL(1u, core->_terminal->GetSelectionRects().size());

        Log::Comment(L"Drag the mouse down a whole row");
        const til::point terminalPosition2{ 1, 1 };
        const auto cursorPosition2 = terminalPosition2 * fontSize;
        interactivity->PointerMoved(leftMouseDown,
                                    WM_LBUTTONDOWN, //pointerUpdateKind
                                    modifiers,
                                    true, // focused,
                                    cursorPosition2.to_core_point(),
                                    true);
        Log::Comment(L"Verify that there's now two selections (one on each row)");
        VERIFY_IS_TRUE(core->HasSelection());
        VERIFY_ARE_EQUAL(2u, core->_terminal->GetSelectionRects().size());

        Log::Comment(L"Release the mouse");
        interactivity->PointerReleased(noMouseDown,
                                       WM_LBUTTONUP, //pointerUpdateKind
                                       modifiers,
                                       cursorPosition2.to_core_point());
        Log::Comment(L"Verify that there's still two selections");
        VERIFY_IS_TRUE(core->HasSelection());
        VERIFY_ARE_EQUAL(2u, core->_terminal->GetSelectionRects().size());

        Log::Comment(L"click outside the current selection");
        const til::point terminalPosition3{ 2, 2 };
        const auto cursorPosition3 = terminalPosition3 * fontSize;
        interactivity->PointerPressed(leftMouseDown,
                                      WM_LBUTTONDOWN, //pointerUpdateKind
                                      0, // timestamp
                                      modifiers,
                                      cursorPosition3.to_core_point());
        Log::Comment(L"Verify that there's now no selection");
        VERIFY_IS_FALSE(core->HasSelection());
        VERIFY_ARE_EQUAL(0u, core->_terminal->GetSelectionRects().size());

        Log::Comment(L"Drag the mouse");
        const til::point terminalPosition4{ 3, 2 };
        const auto cursorPosition4 = terminalPosition4 * fontSize;
        interactivity->PointerMoved(leftMouseDown,
                                    WM_LBUTTONDOWN, //pointerUpdateKind
                                    modifiers,
                                    true, // focused,
                                    cursorPosition4.to_core_point(),
                                    true);
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
        for (auto i = 0; i < 40; ++i)
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
        const auto leftMouseDown{ Control::MouseButtonState::IsLeftButtonDown };

        const til::size fontSize{ 9, 21 };

        Log::Comment(L"Click on the terminal");
        const til::point terminalPosition0{ 5, 5 };
        const auto cursorPosition0{ terminalPosition0 * fontSize };
        interactivity->PointerPressed(leftMouseDown,
                                      WM_LBUTTONDOWN, //pointerUpdateKind
                                      0, // timestamp
                                      modifiers,
                                      cursorPosition0.to_core_point());

        Log::Comment(L"Verify that there's not yet a selection");
        VERIFY_IS_FALSE(core->HasSelection());

        VERIFY_IS_TRUE(interactivity->_singleClickTouchdownPos.has_value());
        VERIFY_ARE_EQUAL(cursorPosition0.to_core_point(), interactivity->_singleClickTouchdownPos.value());

        Log::Comment(L"Drag the mouse just a little");
        // move not quite a whole cell, but enough to start a selection
        const auto cursorPosition1{ cursorPosition0 + til::point{ 6, 0 } };
        interactivity->PointerMoved(leftMouseDown,
                                    WM_LBUTTONDOWN, //pointerUpdateKind
                                    modifiers,
                                    true, // focused,
                                    cursorPosition1.to_core_point(),
                                    true);
        Log::Comment(L"Verify that there's one selection");
        VERIFY_IS_TRUE(core->HasSelection());
        VERIFY_ARE_EQUAL(1u, core->_terminal->GetSelectionRects().size());

        Log::Comment(L"Verify the location of the selection");
        // The viewport is on row 21, so the selection will be on:
        // {(5, 5)+(0, 21)} to {(5, 5)+(0, 21)}
        til::point expectedAnchor{ 5, 26 };
        VERIFY_ARE_EQUAL(expectedAnchor, core->_terminal->GetSelectionAnchor());
        VERIFY_ARE_EQUAL(expectedAnchor, core->_terminal->GetSelectionEnd());

        Log::Comment(L"Scroll up a line, with the left mouse button selected");
        interactivity->MouseWheel(modifiers,
                                  WHEEL_DELTA,
                                  cursorPosition1.to_core_point(),
                                  leftMouseDown);

        Log::Comment(L"Verify the location of the selection");
        // The viewport is now on row 20, so the selection will be on:
        // {(5, 5)+(0, 20)} to {(5, 5)+(0, 21)}
        til::point newExpectedAnchor{ 5, 25 };
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

        for (auto i = 0; i < 40; ++i)
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

        const auto delta = WHEEL_DELTA / 5;
        const Core::Point mousePos{ 0, 0 };
        Control::MouseButtonState state{};

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
        const auto leftMouseDown{ Control::MouseButtonState::IsLeftButtonDown };

        const til::size fontSize{ 9, 21 };

        Log::Comment(L"Click on the terminal");
        const til::point cursorPosition0{ 6, 0 };
        interactivity->PointerPressed(leftMouseDown,
                                      WM_LBUTTONDOWN, //pointerUpdateKind
                                      0, // timestamp
                                      modifiers,
                                      cursorPosition0.to_core_point());

        Log::Comment(L"Verify that there's not yet a selection");
        VERIFY_IS_FALSE(core->HasSelection());

        VERIFY_IS_TRUE(interactivity->_singleClickTouchdownPos.has_value());
        VERIFY_ARE_EQUAL(cursorPosition0.to_core_point(), interactivity->_singleClickTouchdownPos.value());

        Log::Comment(L"Drag the mouse a lot. This simulates dragging the mouse real fast.");
        const til::point cursorPosition1{ 6 + fontSize.width * 2, 0 };
        interactivity->PointerMoved(leftMouseDown,
                                    WM_LBUTTONDOWN, //pointerUpdateKind
                                    modifiers,
                                    true, // focused,
                                    cursorPosition1.to_core_point(),
                                    true);
        Log::Comment(L"Verify that there's one selection");
        VERIFY_IS_TRUE(core->HasSelection());
        VERIFY_ARE_EQUAL(1u, core->_terminal->GetSelectionRects().size());

        Log::Comment(L"Verify that it started on the first cell we clicked on, not the one we dragged to");
        til::point expectedAnchor{ 0, 0 };
        VERIFY_ARE_EQUAL(expectedAnchor, core->_terminal->GetSelectionAnchor());
    }

    void ControlInteractivityTests::TestDragSelectOutsideBounds()
    {
        // This is a test for GH#4603

        auto [settings, conn] = _createSettingsAndConnection();
        auto [core, interactivity] = _createCoreAndInteractivity(*settings, *conn);
        _standardInit(core, interactivity);

        // For this test, don't use any modifiers
        const auto modifiers = ControlKeyStates();
        const auto leftMouseDown{ Control::MouseButtonState::IsLeftButtonDown };
        const Control::MouseButtonState noMouseDown{};

        const til::size fontSize{ 9, 21 };
        Log::Comment(L"Click on the terminal");
        const til::point cursorPosition0{ 6, 0 };
        interactivity->PointerPressed(leftMouseDown,
                                      WM_LBUTTONDOWN, //pointerUpdateKind
                                      0, // timestamp
                                      modifiers,
                                      cursorPosition0.to_core_point());

        Log::Comment(L"Verify that there's not yet a selection");
        VERIFY_IS_FALSE(core->HasSelection());

        VERIFY_IS_TRUE(interactivity->_singleClickTouchdownPos.has_value());
        VERIFY_ARE_EQUAL(cursorPosition0.to_core_point(), interactivity->_singleClickTouchdownPos.value());

        Log::Comment(L"Drag the mouse a lot. This simulates dragging the mouse real fast.");
        const til::point cursorPosition1{ 6 + fontSize.width * 2, 0 };
        interactivity->PointerMoved(leftMouseDown,
                                    WM_LBUTTONDOWN, //pointerUpdateKind
                                    modifiers,
                                    true, // focused,
                                    cursorPosition1.to_core_point(),
                                    true);
        Log::Comment(L"Verify that there's one selection");
        VERIFY_IS_TRUE(core->HasSelection());
        VERIFY_ARE_EQUAL(1u, core->_terminal->GetSelectionRects().size());

        Log::Comment(L"Verify that it started on the first cell we clicked on, not the one we dragged to");
        til::point expectedAnchor{ 0, 0 };
        VERIFY_ARE_EQUAL(expectedAnchor, core->_terminal->GetSelectionAnchor());
        til::point expectedEnd{ 2, 0 };
        VERIFY_ARE_EQUAL(expectedEnd, core->_terminal->GetSelectionEnd());

        interactivity->PointerReleased(noMouseDown,
                                       WM_LBUTTONUP,
                                       modifiers,
                                       cursorPosition1.to_core_point());

        VERIFY_ARE_EQUAL(expectedAnchor, core->_terminal->GetSelectionAnchor());
        VERIFY_ARE_EQUAL(expectedEnd, core->_terminal->GetSelectionEnd());

        Log::Comment(L"Simulate dragging the mouse into the control, without first clicking into the control");
        const til::point cursorPosition2{ fontSize.width * 10, 0 };
        interactivity->PointerMoved(leftMouseDown,
                                    WM_LBUTTONDOWN, //pointerUpdateKind
                                    modifiers,
                                    true, // focused,
                                    cursorPosition2.to_core_point(),
                                    false);

        Log::Comment(L"The selection should be unchanged.");
        VERIFY_ARE_EQUAL(expectedAnchor, core->_terminal->GetSelectionAnchor());
        VERIFY_ARE_EQUAL(expectedEnd, core->_terminal->GetSelectionEnd());
    }

    void ControlInteractivityTests::PointerClickOutsideActiveRegion()
    {
        // This is a test for GH#10642
        WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};

        auto [settings, conn] = _createSettingsAndConnection();
        auto [core, interactivity] = _createCoreAndInteractivity(*settings, *conn);
        _standardInit(core, interactivity);

        // For this test, don't use any modifiers
        const auto modifiers = ControlKeyStates();
        const auto leftMouseDown{ Control::MouseButtonState::IsLeftButtonDown };
        const Control::MouseButtonState noMouseDown{};

        const til::size fontSize{ 9, 21 };
        interactivity->_rowsToScroll = 1;
        auto expectedTop = 0;
        auto expectedViewHeight = 20;
        auto expectedBufferHeight = 20;

        auto scrollChangedHandler = [&](auto&&, const Control::ScrollPositionChangedArgs& args) mutable {
            VERIFY_ARE_EQUAL(expectedTop, args.ViewTop());
            VERIFY_ARE_EQUAL(expectedViewHeight, args.ViewHeight());
            VERIFY_ARE_EQUAL(expectedBufferHeight, args.BufferSize());
        };
        core->ScrollPositionChanged(scrollChangedHandler);
        interactivity->ScrollPositionChanged(scrollChangedHandler);

        for (auto i = 0; i < 40; ++i)
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

        expectedBufferHeight = 41;
        expectedTop = 21;

        Log::Comment(L"Scroll up 10 times");
        for (auto i = 0; i < 11; ++i)
        {
            expectedTop--;
            interactivity->MouseWheel(modifiers,
                                      WHEEL_DELTA,
                                      Core::Point{ 0, 0 },
                                      noMouseDown);
        }

        // Enable VT mouse event tracking
        conn->WriteInput(L"\x1b[?1003;1006h");

        // Mouse clicks in the inactive region (i.e. the top 10 rows in this case) should not register
        Log::Comment(L"Click on the terminal");
        const til::point terminalPosition0{ 4, 4 };
        const auto cursorPosition0 = terminalPosition0 * fontSize;
        interactivity->PointerPressed(leftMouseDown,
                                      WM_LBUTTONDOWN, //pointerUpdateKind
                                      0, // timestamp
                                      modifiers,
                                      cursorPosition0.to_core_point());
        Log::Comment(L"Verify that there's not yet a selection");

        VERIFY_IS_FALSE(core->HasSelection());

        Log::Comment(L"Drag the mouse");
        // move the mouse as if to make a selection
        const til::point terminalPosition1{ 10, 4 };
        const auto cursorPosition1 = terminalPosition1 * fontSize;
        interactivity->PointerMoved(leftMouseDown,
                                    WM_LBUTTONDOWN, //pointerUpdateKind
                                    modifiers,
                                    true, // focused,
                                    cursorPosition1.to_core_point(),
                                    true);
        Log::Comment(L"Verify that there's still no selection");
        VERIFY_IS_FALSE(core->HasSelection());
    }

    void ControlInteractivityTests::IncrementCircularBufferWithSelection()
    {
        // This is a test for GH#10749
        WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};

        auto [settings, conn] = _createSettingsAndConnection();
        auto [core, interactivity] = _createCoreAndInteractivity(*settings, *conn);
        _standardInit(core, interactivity);

        Log::Comment(L"Fill up the history buffer");
        const auto scrollbackLength = settings->HistorySize();
        // Output lines equal to history size + viewport height to make sure we're
        // at the point where outputting more lines causes circular incrementing
        for (auto i = 0; i < settings->HistorySize() + core->ViewHeight(); ++i)
        {
            conn->WriteInput(L"Foo\r\n");
        }
        VERIFY_ARE_EQUAL(scrollbackLength, core->_terminal->GetScrollOffset());

        // For this test, don't use any modifiers
        const auto modifiers = ControlKeyStates();
        const auto leftMouseDown{ Control::MouseButtonState::IsLeftButtonDown };

        const til::size fontSize{ 9, 21 };

        Log::Comment(L"Click on the terminal");
        const til::point terminalPosition0{ 5, 5 };
        const auto cursorPosition0{ terminalPosition0 * fontSize };
        interactivity->PointerPressed(leftMouseDown,
                                      WM_LBUTTONDOWN, //pointerUpdateKind
                                      0, // timestamp
                                      modifiers,
                                      cursorPosition0.to_core_point());

        Log::Comment(L"Verify that there's not yet a selection");
        VERIFY_IS_FALSE(core->HasSelection());

        VERIFY_IS_TRUE(interactivity->_singleClickTouchdownPos.has_value());
        VERIFY_ARE_EQUAL(cursorPosition0.to_core_point(), interactivity->_singleClickTouchdownPos.value());

        Log::Comment(L"Drag the mouse just a little");
        // move not quite a whole cell, but enough to start a selection
        const auto cursorPosition1{ cursorPosition0 + til::point{ 6, 0 } };
        interactivity->PointerMoved(leftMouseDown,
                                    WM_LBUTTONDOWN, //pointerUpdateKind
                                    modifiers,
                                    true, // focused,
                                    cursorPosition1.to_core_point(),
                                    true);
        Log::Comment(L"Verify that there's one selection");
        VERIFY_IS_TRUE(core->HasSelection());
        VERIFY_ARE_EQUAL(1u, core->_terminal->GetSelectionRects().size());

        Log::Comment(L"Verify the location of the selection");
        // The viewport is on row (historySize + 5), so the selection will be on:
        // {(5, (historySize+5))+(0, 21)} to {(5, (historySize+5))+(0, 21)}
        til::point expectedAnchor{ 5, settings->HistorySize() + 5 };
        VERIFY_ARE_EQUAL(expectedAnchor, core->_terminal->GetSelectionAnchor());
        VERIFY_ARE_EQUAL(expectedAnchor, core->_terminal->GetSelectionEnd());

        Log::Comment(L"Output a line of text");
        conn->WriteInput(L"Foo\r\n");

        Log::Comment(L"Verify the location of the selection");
        // The selection should now be 1 row lower
        expectedAnchor.y -= 1;
        {
            const auto anchor{ core->_terminal->GetSelectionAnchor() };
            const auto end{ core->_terminal->GetSelectionEnd() };
            Log::Comment(fmt::format(L"expectedAnchor:({},{})", expectedAnchor.x, expectedAnchor.y).c_str());
            Log::Comment(fmt::format(L"anchor:({},{})", anchor.x, anchor.y).c_str());
            Log::Comment(fmt::format(L"end:({},{})", end.x, end.y).c_str());

            VERIFY_ARE_EQUAL(expectedAnchor, anchor);
            VERIFY_ARE_EQUAL(expectedAnchor, end);
        }
        VERIFY_ARE_EQUAL(scrollbackLength - 1, core->_terminal->GetScrollOffset());

        Log::Comment(L"Output a line of text");
        conn->WriteInput(L"Foo\r\n");

        Log::Comment(L"Verify the location of the selection");
        // The selection should now be 1 row lower
        expectedAnchor.y -= 1;
        {
            const auto anchor{ core->_terminal->GetSelectionAnchor() };
            const auto end{ core->_terminal->GetSelectionEnd() };
            Log::Comment(fmt::format(L"expectedAnchor:({},{})", expectedAnchor.x, expectedAnchor.y).c_str());
            Log::Comment(fmt::format(L"anchor:({},{})", anchor.x, anchor.y).c_str());
            Log::Comment(fmt::format(L"end:({},{})", end.x, end.y).c_str());

            VERIFY_ARE_EQUAL(expectedAnchor, anchor);
            VERIFY_ARE_EQUAL(expectedAnchor, end);
        }
        VERIFY_ARE_EQUAL(scrollbackLength - 2, core->_terminal->GetScrollOffset());

        Log::Comment(L"Move the mouse a little, to update the selection");
        // At this point, there should only be one selection region! The
        // viewport moved up to keep the selection at the same relative spot. So
        // wiggling the cursor should continue to select only the same
        // character in the buffer (if, albeit in a new location).
        //
        // This helps test GH #14462, a regression from #10749.
        interactivity->PointerMoved(leftMouseDown,
                                    WM_LBUTTONDOWN, //pointerUpdateKind
                                    modifiers,
                                    true, // focused,
                                    cursorPosition0.to_core_point(),
                                    true);
        VERIFY_IS_TRUE(core->HasSelection());
        VERIFY_ARE_EQUAL(1u, core->_terminal->GetSelectionRects().size());
        {
            const auto anchor{ core->_terminal->GetSelectionAnchor() };
            const auto end{ core->_terminal->GetSelectionEnd() };
            Log::Comment(fmt::format(L"expectedAnchor:({},{})", expectedAnchor.x, expectedAnchor.y).c_str());
            Log::Comment(fmt::format(L"anchor:({},{})", anchor.x, anchor.y).c_str());
            Log::Comment(fmt::format(L"end:({},{})", end.x, end.y).c_str());

            VERIFY_ARE_EQUAL(expectedAnchor, anchor);
            VERIFY_ARE_EQUAL(expectedAnchor, end);
        }

        Log::Comment(L"Output a line ant move the mouse a little to update the selection, all at once");
        // Same as above. The viewport has moved, so the mouse is still over the
        // same character, even though it's at a new offset.
        conn->WriteInput(L"Foo\r\n");
        expectedAnchor.y -= 1;
        VERIFY_ARE_EQUAL(scrollbackLength - 3, core->_terminal->GetScrollOffset());
        interactivity->PointerMoved(leftMouseDown,
                                    WM_LBUTTONDOWN, //pointerUpdateKind
                                    modifiers,
                                    true, // focused,
                                    cursorPosition1.to_core_point(),
                                    true);
        VERIFY_IS_TRUE(core->HasSelection());
        VERIFY_ARE_EQUAL(1u, core->_terminal->GetSelectionRects().size());
        {
            const auto anchor{ core->_terminal->GetSelectionAnchor() };
            const auto end{ core->_terminal->GetSelectionEnd() };
            Log::Comment(fmt::format(L"expectedAnchor:({},{})", expectedAnchor.x, expectedAnchor.y).c_str());
            Log::Comment(fmt::format(L"anchor:({},{})", anchor.x, anchor.y).c_str());
            Log::Comment(fmt::format(L"end:({},{})", end.x, end.y).c_str());

            VERIFY_ARE_EQUAL(expectedAnchor, anchor);
            VERIFY_ARE_EQUAL(expectedAnchor, end);
        }

        // Output enough text for the selection to get pushed off the buffer
        for (auto i = 0; i < settings->HistorySize() + core->ViewHeight(); ++i)
        {
            conn->WriteInput(L"Foo\r\n");
        }
        // Verify that the selection got reset
        VERIFY_IS_FALSE(core->HasSelection());
    }

    void ControlInteractivityTests::GetMouseEventsInTest()
    {
        // This is just a simple case that proves you can test mouse events
        // generated by the terminal
        WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};

        auto [settings, conn] = _createSettingsAndConnection();
        auto [core, interactivity] = _createCoreAndInteractivity(*settings, *conn);
        _standardInit(core, interactivity);

        std::deque<std::wstring> expectedOutput{};
        auto validateDrained = _addInputCallback(conn, expectedOutput);

        Log::Comment(L"Enable mouse mode");
        auto& term{ *core->_terminal };
        term.Write(L"\x1b[?1000h");

        Log::Comment(L"Click on the terminal");

        expectedOutput.push_back(L"\x1b[M &&");
        // For this test, don't use any modifiers
        const auto modifiers = ControlKeyStates();
        const auto leftMouseDown{ Control::MouseButtonState::IsLeftButtonDown };
        const til::size fontSize{ 9, 21 };
        const til::point terminalPosition0{ 5, 5 };
        const auto cursorPosition0{ terminalPosition0 * fontSize };
        interactivity->PointerPressed(leftMouseDown,
                                      WM_LBUTTONDOWN, //pointerUpdateKind
                                      0, // timestamp
                                      modifiers,
                                      cursorPosition0.to_core_point());
    }

    void ControlInteractivityTests::AltBufferClampMouse()
    {
        // This is a test for
        // * GH#10642
        // * a comment in GH#12719
        WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};

        auto [settings, conn] = _createSettingsAndConnection();
        auto [core, interactivity] = _createCoreAndInteractivity(*settings, *conn);
        _standardInit(core, interactivity);
        auto& term{ *core->_terminal };

        // Output enough text for view to start scrolling
        for (auto i = 0; i < core->ViewHeight() * 2; ++i)
        {
            conn->WriteInput(L"Foo\r\n");
        }

        // Start checking output
        std::deque<std::wstring> expectedOutput{};
        auto validateDrained = _addInputCallback(conn, expectedOutput);

        const auto originalViewport{ term.GetViewport() };
        VERIFY_ARE_EQUAL(originalViewport.Width(), 30);

        Log::Comment(L" --- Enable mouse mode ---");
        term.Write(L"\x1b[?1000h");

        Log::Comment(L" --- Click on the terminal ---");
        // Recall:
        //
        // >  !  specifies the value 1.  The upper left character position on
        // >  the terminal is denoted as 1,1
        //
        // So 5 in our buffer is 32+5+1 = '&'
        expectedOutput.push_back(L"\x1b[M &&");
        // For this test, don't use any modifiers
        const auto modifiers = ControlKeyStates();
        const auto leftMouseDown{ Control::MouseButtonState::IsLeftButtonDown };
        const til::size fontSize{ 9, 21 };
        const til::point terminalPosition0{ 5, 5 };
        const auto cursorPosition0{ terminalPosition0 * fontSize };
        interactivity->PointerPressed(leftMouseDown,
                                      WM_LBUTTONDOWN, //pointerUpdateKind
                                      0, // timestamp
                                      modifiers,
                                      cursorPosition0.to_core_point());
        VERIFY_ARE_EQUAL(0u, expectedOutput.size(), L"Validate we drained all the expected output");

        // These first two bits are a test for GH#10642
        Log::Comment(L" --- Click on the terminal outside the width of the mutable viewport, see that it's clamped to the viewport ---");
        // Not actually possible, but for validation.
        const til::point terminalPosition1{ originalViewport.Width() + 5, 5 };
        const auto cursorPosition1{ terminalPosition1 * fontSize };

        // The viewport is only 30 wide, so clamping 35 to the buffer size gets
        // us 29, which converted is (32 + 29 + 1) = 62 = '>'
        expectedOutput.push_back(L"\x1b[M >&");
        interactivity->PointerPressed(leftMouseDown,
                                      WM_LBUTTONDOWN, //pointerUpdateKind
                                      0, // timestamp
                                      modifiers,
                                      cursorPosition1.to_core_point());
        VERIFY_ARE_EQUAL(0u, expectedOutput.size(), L"Validate we drained all the expected output");

        Log::Comment(L" --- Scroll up, click the terminal. We shouldn't get any event. ---");
        core->UserScrollViewport(10);
        VERIFY_IS_GREATER_THAN(core->ScrollOffset(), 0);

        // Viewport is now above the mutable viewport, so the mouse event
        // straight up won't be sent to the terminal.

        expectedOutput.push_back(L"sentinel"); // Clearly, it won't be this string
        interactivity->PointerPressed(leftMouseDown,
                                      WM_LBUTTONDOWN, //pointerUpdateKind
                                      0, // timestamp
                                      modifiers,
                                      cursorPosition0.to_core_point());
        // Flush it out.
        conn->WriteInput(L"sentinel");
        VERIFY_ARE_EQUAL(0u, expectedOutput.size(), L"Validate we drained all the expected output");

        // This is the part as mentioned in GH#12719
        Log::Comment(L" --- Switch to alt buffer ---");
        term.Write(L"\x1b[?1049h");
        auto returnToMain = wil::scope_exit([&]() { term.Write(L"\x1b[?1049h"); });

        VERIFY_ARE_EQUAL(0, core->ScrollOffset());
        Log::Comment(L" --- Click on a spot that's still outside the buffer ---");
        expectedOutput.push_back(L"\x1b[M >&");
        interactivity->PointerPressed(leftMouseDown,
                                      WM_LBUTTONDOWN, //pointerUpdateKind
                                      0, // timestamp
                                      modifiers,
                                      cursorPosition1.to_core_point());

        Log::Comment(L" --- Resize the terminal to be 10 columns wider ---");
        const auto newWidth = 40.0f * fontSize.width;
        const auto newHeight = 20.0f * fontSize.height;
        core->SizeChanged(newWidth, newHeight);

        Log::Comment(L" --- Click on a spot that's NOW INSIDE the buffer ---");
        // (32 + 35 + 1) = 68 = 'D'
        expectedOutput.push_back(L"\x1b[M D&");
        interactivity->PointerPressed(leftMouseDown,
                                      WM_LBUTTONDOWN, //pointerUpdateKind
                                      0, // timestamp
                                      modifiers,
                                      cursorPosition1.to_core_point());
        VERIFY_ARE_EQUAL(0u, expectedOutput.size(), L"Validate we drained all the expected output");
    }
}
