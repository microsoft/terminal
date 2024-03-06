// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "../types/inc/IInputEvent.hpp"
#include "../terminal/input/terminalInput.hpp"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace Microsoft
{
    namespace Console
    {
        namespace VirtualTerminal
        {
            class MouseInputTest;
        };
    };
};
using namespace Microsoft::Console::VirtualTerminal;

static til::point s_rgTestCoords[] = {
    { 0, 0 },
    { 0, 1 },
    { 1, 1 },
    { 2, 2 },
    { 94, 94 }, // 94+1+32 = 127
    { 95, 95 }, // 95+1+32 = 128, this is the ascii boundary
    { 96, 96 },
    { 127, 127 },
    { 128, 128 },
    { SHORT_MAX - 33, SHORT_MAX - 33 },
    { SHORT_MAX - 32, SHORT_MAX - 32 },
};

// Note: We're going to be changing the value of the third char (the space) of
//      these strings as we test things with this array, to alter the expected button value.
// The default value is the button=WM_LBUTTONDOWN case, which is element[3]=' '
static const wchar_t* s_rgDefaultTestOutput[] = {
    L"\x1b[M !!",
    L"\x1b[M !\"",
    L"\x1b[M \"\"",
    L"\x1b[M ##",
    L"\x1b[M \x7f\x7f",
    L"\x1b[M \x80\x80", // 95 - This and below will always fail for default (non utf8)
    L"\x1b[M \x81\x81",
    L"\x1b[M \x00A0\x00A0", //127
    L"\x1b[M \x00A1\x00A1",
    L"\x1b[M \x7FFF\x7FFF", // FFDE
    L"\x1b[M \x8000\x8000", // This one will always fail for Default and UTF8
};

// Note: We're going to be changing the value of the third char (the space) of
//      these strings as we test things with this array, to alter the expected button value.
// The default value is the button=WM_LBUTTONDOWN case, which is element[3]='0'
// We're also going to change the last element, for button-down (M) vs button-up (m)
static const wchar_t* s_rgSgrTestOutput[] = {
    L"\x1b[<%d;1;1M",
    L"\x1b[<%d;1;2M",
    L"\x1b[<%d;2;2M",
    L"\x1b[<%d;3;3M",
    L"\x1b[<%d;95;95M",
    L"\x1b[<%d;96;96M", // 95 - This and below will always fail for default (non utf8)
    L"\x1b[<%d;97;97M",
    L"\x1b[<%d;128;128M", //127
    L"\x1b[<%d;129;129M",
    L"\x1b[<%d;32735;32735M", // FFDE
    L"\x1b[<%d;32736;32736M",
};

static int s_iTestCoordsLength = ARRAYSIZE(s_rgTestCoords);

class MouseInputTest
{
public:
    TEST_CLASS(MouseInputTest);

    // Routine Description:
    // Constructs a string from s_rgDefaultTestOutput with the third char
    //      correctly filled in to match uiButton.
    TerminalInput::OutputType BuildDefaultTestOutput(const std::wstring_view& pwchTestOutput, unsigned int uiButton, short sModifierKeystate, short sScrollDelta)
    {
        TerminalInput::StringType str;
        str.append(pwchTestOutput);
        str[3] = GetDefaultCharFromButton(uiButton, sModifierKeystate, sScrollDelta);
        return str;
    }

    // Routine Description:
    // Constructs a string from s_rgSgrTestOutput with the third and last chars
    //      correctly filled in to match uiButton.
    TerminalInput::OutputType BuildSGRTestOutput(const wchar_t* pwchTestOutput, unsigned int uiButton, short sModifierKeystate, short sScrollDelta)
    {
        wchar_t buffer[256];
        swprintf_s(buffer, pwchTestOutput, GetSgrCharFromButton(uiButton, sModifierKeystate, sScrollDelta));

        TerminalInput::StringType str;
        str.append(std::wstring_view{ buffer });
        str[str.size() - 1] = IsButtonDown(uiButton) ? L'M' : L'm';
        return str;
    }

    wchar_t GetDefaultCharFromButton(unsigned int uiButton, short sModifierKeystate, short sScrollDelta)
    {
        auto wch = L'\x0';
        Log::Comment(NoThrowString().Format(L"uiButton '%d'", uiButton));
        switch (uiButton)
        {
        case WM_LBUTTONDBLCLK:
        case WM_LBUTTONDOWN:
            wch = L' ';
            break;
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP:
            wch = L'#';
            break;
        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
            wch = L'\"';
            break;
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK:
            wch = L'!';
            break;
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
            Log::Comment(NoThrowString().Format(L"MOUSEWHEEL"));
            wch = L'`' + (sScrollDelta > 0 ? 0 : 1);
            break;
        case WM_MOUSEMOVE:
        default:
            Log::Comment(NoThrowString().Format(L"DEFAULT"));
            wch = L'\x0';
            break;
        }
        // Use Any here with the multi-flag constants -- they capture left/right key state
        WI_UpdateFlag(wch, 0x04, WI_IsAnyFlagSet(sModifierKeystate, SHIFT_PRESSED));
        WI_UpdateFlag(wch, 0x08, WI_IsAnyFlagSet(sModifierKeystate, ALT_PRESSED));
        WI_UpdateFlag(wch, 0x10, WI_IsAnyFlagSet(sModifierKeystate, CTRL_PRESSED));
        return wch;
    }

    int GetSgrCharFromButton(unsigned int uiButton, short sModifierKeystate, short sScrollDelta)
    {
        auto result = 0;
        switch (uiButton)
        {
        case WM_LBUTTONDBLCLK:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
            result = 0;
            break;
        case WM_MBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK:
            result = 1;
            break;
        case WM_RBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
            result = 2;
            break;
        case WM_MOUSEMOVE:
            result = 3 + 0x20; // we add 0x20 to hover events, which are all encoded as WM_MOUSEMOVE events
            break;
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
            result = (sScrollDelta > 0 ? 64 : 65);
            break;
        default:
            result = 0;
            break;
        }
        // Use Any here with the multi-flag constants -- they capture left/right key state
        WI_UpdateFlag(result, 0x04, WI_IsAnyFlagSet(sModifierKeystate, SHIFT_PRESSED));
        WI_UpdateFlag(result, 0x08, WI_IsAnyFlagSet(sModifierKeystate, ALT_PRESSED));
        WI_UpdateFlag(result, 0x10, WI_IsAnyFlagSet(sModifierKeystate, CTRL_PRESSED));
        return result;
    }

    bool IsButtonDown(unsigned int uiButton)
    {
        auto fIsDown = false;
        switch (uiButton)
        {
        case WM_LBUTTONDBLCLK:
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK:
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
            fIsDown = true;
            break;
        }
        return fIsDown;
    }

    /* From winuser.h - Needed to manually specify the test properties
        #define WM_MOUSEFIRST                   0x0200
        #define WM_MOUSEMOVE                    0x0200
        #define WM_LBUTTONDOWN                  0x0201
        #define WM_LBUTTONUP                    0x0202
        #define WM_LBUTTONDBLCLK                0x0203
        #define WM_RBUTTONDOWN                  0x0204
        #define WM_RBUTTONUP                    0x0205
        #define WM_RBUTTONDBLCLK                0x0206
        #define WM_MBUTTONDOWN                  0x0207
        #define WM_MBUTTONUP                    0x0208
        #define WM_MBUTTONDBLCLK                0x0209
        #if (_WIN32_WINNT >= 0x0400) || (_WIN32_WINDOWS > 0x0400)
        #define WM_MOUSEWHEEL                   0x020A
        #endif
        #if (_WIN32_WINNT >= 0x0500)
        #define WM_XBUTTONDOWN                  0x020B
        #define WM_XBUTTONUP                    0x020C
        #define WM_XBUTTONDBLCLK                0x020D
        #endif
        #if (_WIN32_WINNT >= 0x0600)
        #define WM_MOUSEHWHEEL                  0x020E
    */
    TEST_METHOD(DefaultModeTests)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            // TEST_METHOD_PROPERTY(L"Data:uiButton", L"{WM_LBUTTONDOWN, WM_LBUTTONUP, WM_MBUTTONDOWN, WM_MBUTTONUP, WM_RBUTTONDOWN, WM_RBUTTONUP}")
            TEST_METHOD_PROPERTY(L"Data:uiButton", L"{0x0201, 0x0202, 0x0207, 0x0208, 0x0204, 0x0205}")
            // None, SHIFT, LEFT_CONTROL, RIGHT_ALT, RIGHT_ALT | LEFT_CONTROL
            TEST_METHOD_PROPERTY(L"Data:uiModifierKeystate", L"{0x0000, 0x0010, 0x0008, 0x0001, 0x0009}")
        END_TEST_METHOD_PROPERTIES()

        Log::Comment(L"Starting test...");

        TerminalInput mouseInput;

        unsigned int uiModifierKeystate = 0;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiModifierKeystate", uiModifierKeystate));
        auto sModifierKeystate = (SHORT)uiModifierKeystate;
        short sScrollDelta = 0;

        unsigned int uiButton;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiButton", uiButton));

        VERIFY_ARE_EQUAL(TerminalInput::MakeUnhandled(), mouseInput.HandleMouse({ 0, 0 }, uiButton, sModifierKeystate, sScrollDelta, {}));

        mouseInput.SetInputMode(TerminalInput::Mode::DefaultMouseTracking, true);

        for (auto i = 0; i < s_iTestCoordsLength; i++)
        {
            const auto Coord = s_rgTestCoords[i];

            TerminalInput::OutputType expected;
            if (Coord.x <= 94 && Coord.y <= 94)
            {
                expected = BuildDefaultTestOutput(s_rgDefaultTestOutput[i], uiButton, sModifierKeystate, sScrollDelta);
            }

            // validate translation
            VERIFY_ARE_EQUAL(expected,
                             mouseInput.HandleMouse(Coord,
                                                    uiButton,
                                                    sModifierKeystate,
                                                    sScrollDelta,
                                                    {}),
                             NoThrowString().Format(L"(x,y)=(%d,%d)", Coord.x, Coord.y));
        }

        mouseInput.SetInputMode(TerminalInput::Mode::ButtonEventMouseTracking, true);
        for (auto i = 0; i < s_iTestCoordsLength; i++)
        {
            const auto Coord = s_rgTestCoords[i];

            TerminalInput::OutputType expected;
            if (Coord.x <= 94 && Coord.y <= 94)
            {
                expected = BuildDefaultTestOutput(s_rgDefaultTestOutput[i], uiButton, sModifierKeystate, sScrollDelta);
            }

            // validate translation
            VERIFY_ARE_EQUAL(expected,
                             mouseInput.HandleMouse(Coord,
                                                    uiButton,
                                                    sModifierKeystate,
                                                    sScrollDelta,
                                                    {}),
                             NoThrowString().Format(L"(x,y)=(%d,%d)", Coord.x, Coord.y));
        }

        mouseInput.SetInputMode(TerminalInput::Mode::AnyEventMouseTracking, true);
        for (auto i = 0; i < s_iTestCoordsLength; i++)
        {
            const auto Coord = s_rgTestCoords[i];

            TerminalInput::OutputType expected;
            if (Coord.x <= 94 && Coord.y <= 94)
            {
                expected = BuildDefaultTestOutput(s_rgDefaultTestOutput[i], uiButton, sModifierKeystate, sScrollDelta);
            }

            // validate translation
            VERIFY_ARE_EQUAL(expected,
                             mouseInput.HandleMouse(Coord,
                                                    uiButton,
                                                    sModifierKeystate,
                                                    sScrollDelta,
                                                    {}),
                             NoThrowString().Format(L"(x,y)=(%d,%d)", Coord.x, Coord.y));
        }
    }
    TEST_METHOD(Utf8ModeTests)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            // TEST_METHOD_PROPERTY(L"Data:uiButton", L"{WM_LBUTTONDOWN, WM_LBUTTONUP, WM_MBUTTONDOWN, WM_MBUTTONUP, WM_RBUTTONDOWN, WM_RBUTTONUP}")
            TEST_METHOD_PROPERTY(L"Data:uiButton", L"{0x0201, 0x0202, 0x0207, 0x0208, 0x0204, 0x0205}")
            // None, SHIFT, LEFT_CONTROL, RIGHT_ALT, RIGHT_ALT | LEFT_CONTROL
            TEST_METHOD_PROPERTY(L"Data:uiModifierKeystate", L"{0x0000, 0x0010, 0x0008, 0x0001, 0x0009}")
        END_TEST_METHOD_PROPERTIES()

        Log::Comment(L"Starting test...");

        TerminalInput mouseInput;

        unsigned int uiModifierKeystate = 0;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiModifierKeystate", uiModifierKeystate));
        auto sModifierKeystate = (SHORT)uiModifierKeystate;
        short sScrollDelta = 0;

        unsigned int uiButton;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiButton", uiButton));

        VERIFY_ARE_EQUAL(TerminalInput::MakeUnhandled(), mouseInput.HandleMouse({ 0, 0 }, uiButton, sModifierKeystate, sScrollDelta, {}));

        mouseInput.SetInputMode(TerminalInput::Mode::Utf8MouseEncoding, true);

        auto MaxCoord = SHORT_MAX - 33;

        mouseInput.SetInputMode(TerminalInput::Mode::DefaultMouseTracking, true);
        for (auto i = 0; i < s_iTestCoordsLength; i++)
        {
            const auto Coord = s_rgTestCoords[i];

            TerminalInput::OutputType expected;
            if (Coord.x <= MaxCoord && Coord.y <= MaxCoord)
            {
                expected = BuildDefaultTestOutput(s_rgDefaultTestOutput[i], uiButton, sModifierKeystate, sScrollDelta);
            }

            // validate translation
            VERIFY_ARE_EQUAL(expected,
                             mouseInput.HandleMouse(Coord,
                                                    uiButton,
                                                    sModifierKeystate,
                                                    sScrollDelta,
                                                    {}),
                             NoThrowString().Format(L"(x,y)=(%d,%d)", Coord.x, Coord.y));
        }

        mouseInput.SetInputMode(TerminalInput::Mode::ButtonEventMouseTracking, true);
        for (auto i = 0; i < s_iTestCoordsLength; i++)
        {
            const auto Coord = s_rgTestCoords[i];

            TerminalInput::OutputType expected;
            if (Coord.x <= MaxCoord && Coord.y <= MaxCoord)
            {
                expected = BuildDefaultTestOutput(s_rgDefaultTestOutput[i], uiButton, sModifierKeystate, sScrollDelta);
            }

            // validate translation
            VERIFY_ARE_EQUAL(expected,
                             mouseInput.HandleMouse(Coord,
                                                    uiButton,
                                                    sModifierKeystate,
                                                    sScrollDelta,
                                                    {}),
                             NoThrowString().Format(L"(x,y)=(%d,%d)", Coord.x, Coord.y));
        }

        mouseInput.SetInputMode(TerminalInput::Mode::AnyEventMouseTracking, true);
        for (auto i = 0; i < s_iTestCoordsLength; i++)
        {
            const auto Coord = s_rgTestCoords[i];

            TerminalInput::OutputType expected;
            if (Coord.x <= MaxCoord && Coord.y <= MaxCoord)
            {
                expected = BuildDefaultTestOutput(s_rgDefaultTestOutput[i], uiButton, sModifierKeystate, sScrollDelta);
            }

            // validate translation
            VERIFY_ARE_EQUAL(expected,
                             mouseInput.HandleMouse(Coord,
                                                    uiButton,
                                                    sModifierKeystate,
                                                    sScrollDelta,
                                                    {}),
                             NoThrowString().Format(L"(x,y)=(%d,%d)", Coord.x, Coord.y));
        }
    }

    TEST_METHOD(SgrModeTests)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            // TEST_METHOD_PROPERTY(L"Data:uiButton", L"{WM_LBUTTONDOWN, WM_LBUTTONUP, WM_MBUTTONDOWN, WM_MBUTTONUP, WM_RBUTTONDOWN, WM_RBUTTONUP, WM_MOUSEMOVE}")
            TEST_METHOD_PROPERTY(L"Data:uiButton", L"{0x0201, 0x0202, 0x0207, 0x0208, 0x0204, 0x0205, 0x0200}")
            // None, SHIFT, LEFT_CONTROL, RIGHT_ALT, RIGHT_ALT | LEFT_CONTROL
            TEST_METHOD_PROPERTY(L"Data:uiModifierKeystate", L"{0x0000, 0x0010, 0x0008, 0x0001, 0x0009}")
        END_TEST_METHOD_PROPERTIES()

        Log::Comment(L"Starting test...");

        TerminalInput mouseInput;
        unsigned int uiModifierKeystate = 0;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiModifierKeystate", uiModifierKeystate));
        auto sModifierKeystate = (SHORT)uiModifierKeystate;
        short sScrollDelta = 0;

        unsigned int uiButton;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiButton", uiButton));

        VERIFY_ARE_EQUAL(TerminalInput::MakeUnhandled(), mouseInput.HandleMouse({ 0, 0 }, uiButton, sModifierKeystate, sScrollDelta, {}));

        mouseInput.SetInputMode(TerminalInput::Mode::SgrMouseEncoding, true);

        mouseInput.SetInputMode(TerminalInput::Mode::DefaultMouseTracking, true);
        for (auto i = 0; i < s_iTestCoordsLength; i++)
        {
            const auto Coord = s_rgTestCoords[i];

            // SGR Mode should be able to handle any arbitrary coords.
            // However, mouse moves are only handled in Any Event mode
            TerminalInput::OutputType expected;
            if (uiButton != WM_MOUSEMOVE)
            {
                expected = BuildSGRTestOutput(s_rgSgrTestOutput[i], uiButton, sModifierKeystate, sScrollDelta);
            }

            // validate translation
            VERIFY_ARE_EQUAL(expected,
                             mouseInput.HandleMouse(Coord, uiButton, sModifierKeystate, sScrollDelta, {}),
                             NoThrowString().Format(L"(x,y)=(%d,%d)", Coord.x, Coord.y));
        }

        mouseInput.SetInputMode(TerminalInput::Mode::ButtonEventMouseTracking, true);
        for (auto i = 0; i < s_iTestCoordsLength; i++)
        {
            const auto Coord = s_rgTestCoords[i];

            // SGR Mode should be able to handle any arbitrary coords.
            // However, mouse moves are only handled in Any Event mode
            TerminalInput::OutputType expected;
            if (uiButton != WM_MOUSEMOVE)
            {
                expected = BuildSGRTestOutput(s_rgSgrTestOutput[i], uiButton, sModifierKeystate, sScrollDelta);
            }

            // validate translation
            VERIFY_ARE_EQUAL(expected,
                             mouseInput.HandleMouse(Coord,
                                                    uiButton,
                                                    sModifierKeystate,
                                                    sScrollDelta,
                                                    {}),
                             NoThrowString().Format(L"(x,y)=(%d,%d)", Coord.x, Coord.y));
        }

        mouseInput.SetInputMode(TerminalInput::Mode::AnyEventMouseTracking, true);
        for (auto i = 0; i < s_iTestCoordsLength; i++)
        {
            const auto Coord = s_rgTestCoords[i];
            const auto expected = BuildSGRTestOutput(s_rgSgrTestOutput[i], uiButton, sModifierKeystate, sScrollDelta);

            // validate translation
            VERIFY_ARE_EQUAL(expected,
                             mouseInput.HandleMouse(Coord,
                                                    uiButton,
                                                    sModifierKeystate,
                                                    sScrollDelta,
                                                    {}),
                             NoThrowString().Format(L"(x,y)=(%d,%d)", Coord.x, Coord.y));
        }
    }

    TEST_METHOD(ScrollWheelTests)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:sScrollDelta", L"{-120, 120, -10000, 32736}")
            // None, SHIFT, LEFT_CONTROL, RIGHT_ALT, RIGHT_ALT | LEFT_CONTROL
            TEST_METHOD_PROPERTY(L"Data:uiModifierKeystate", L"{0x0000, 0x0010, 0x0008, 0x0001, 0x0009}")
        END_TEST_METHOD_PROPERTIES()

        Log::Comment(L"Starting test...");

        TerminalInput mouseInput;
        unsigned int uiModifierKeystate = 0;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiModifierKeystate", uiModifierKeystate));
        auto sModifierKeystate = (SHORT)uiModifierKeystate;

        unsigned int uiButton = WM_MOUSEWHEEL;
        auto iScrollDelta = 0;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"sScrollDelta", iScrollDelta));
        auto sScrollDelta = (short)(iScrollDelta);

        VERIFY_ARE_EQUAL(TerminalInput::MakeUnhandled(), mouseInput.HandleMouse({ 0, 0 }, uiButton, sModifierKeystate, sScrollDelta, {}));

        // Default Tracking, Default Encoding
        mouseInput.SetInputMode(TerminalInput::Mode::DefaultMouseTracking, true);

        for (auto i = 0; i < s_iTestCoordsLength; i++)
        {
            const auto Coord = s_rgTestCoords[i];

            TerminalInput::OutputType expected;
            if (Coord.x <= 94 && Coord.y <= 94)
            {
                expected = BuildDefaultTestOutput(s_rgDefaultTestOutput[i], uiButton, sModifierKeystate, sScrollDelta);
            }

            // validate translation
            VERIFY_ARE_EQUAL(expected,
                             mouseInput.HandleMouse(Coord,
                                                    uiButton,
                                                    sModifierKeystate,
                                                    sScrollDelta,
                                                    {}),
                             NoThrowString().Format(L"(x,y)=(%d,%d)", Coord.x, Coord.y));
        }

        // Default Tracking, UTF8 Encoding
        mouseInput.SetInputMode(TerminalInput::Mode::Utf8MouseEncoding, true);
        auto MaxCoord = SHORT_MAX - 33;
        for (auto i = 0; i < s_iTestCoordsLength; i++)
        {
            const auto Coord = s_rgTestCoords[i];

            TerminalInput::OutputType expected;
            if (Coord.x <= MaxCoord && Coord.y <= MaxCoord)
            {
                expected = BuildDefaultTestOutput(s_rgDefaultTestOutput[i], uiButton, sModifierKeystate, sScrollDelta);
            }

            // validate translation
            VERIFY_ARE_EQUAL(expected,
                             mouseInput.HandleMouse(Coord,
                                                    uiButton,
                                                    sModifierKeystate,
                                                    sScrollDelta,
                                                    {}),
                             NoThrowString().Format(L"(x,y)=(%d,%d)", Coord.x, Coord.y));
        }

        // Default Tracking, SGR Encoding
        mouseInput.SetInputMode(TerminalInput::Mode::SgrMouseEncoding, true);
        for (auto i = 0; i < s_iTestCoordsLength; i++)
        {
            const auto Coord = s_rgTestCoords[i];
            const auto expected = BuildSGRTestOutput(s_rgSgrTestOutput[i], uiButton, sModifierKeystate, sScrollDelta);

            // validate translation
            VERIFY_ARE_EQUAL(expected,
                             mouseInput.HandleMouse(Coord,
                                                    uiButton,
                                                    sModifierKeystate,
                                                    sScrollDelta,
                                                    {}),
                             NoThrowString().Format(L"(x,y)=(%d,%d)", Coord.x, Coord.y));
        }
    }

    TEST_METHOD(AlternateScrollModeTests)
    {
        Log::Comment(L"Starting test...");
        TerminalInput mouseInput;
        const short noModifierKeys = 0;

        Log::Comment(L"Enable alternate scroll mode in the alt screen buffer");
        mouseInput.UseAlternateScreenBuffer();
        mouseInput.SetInputMode(TerminalInput::Mode::AlternateScroll, true);

        Log::Comment(L"Test mouse wheel scrolling up");
        VERIFY_ARE_EQUAL(TerminalInput::MakeOutput(L"\x1B[A"), mouseInput.HandleMouse({ 0, 0 }, WM_MOUSEWHEEL, noModifierKeys, WHEEL_DELTA, {}));

        Log::Comment(L"Test mouse wheel scrolling down");
        VERIFY_ARE_EQUAL(TerminalInput::MakeOutput(L"\x1B[B"), mouseInput.HandleMouse({ 0, 0 }, WM_MOUSEWHEEL, noModifierKeys, -WHEEL_DELTA, {}));

        Log::Comment(L"Enable cursor keys mode");
        mouseInput.SetInputMode(TerminalInput::Mode::CursorKey, true);

        Log::Comment(L"Test mouse wheel scrolling up");
        VERIFY_ARE_EQUAL(TerminalInput::MakeOutput(L"\x1BOA"), mouseInput.HandleMouse({ 0, 0 }, WM_MOUSEWHEEL, noModifierKeys, WHEEL_DELTA, {}));

        Log::Comment(L"Test mouse wheel scrolling down");
        VERIFY_ARE_EQUAL(TerminalInput::MakeOutput(L"\x1BOB"), mouseInput.HandleMouse({ 0, 0 }, WM_MOUSEWHEEL, noModifierKeys, -WHEEL_DELTA, {}));

        Log::Comment(L"Confirm no effect when scroll mode is disabled");
        mouseInput.UseAlternateScreenBuffer();
        mouseInput.SetInputMode(TerminalInput::Mode::AlternateScroll, false);
        VERIFY_ARE_EQUAL(TerminalInput::MakeUnhandled(), mouseInput.HandleMouse({ 0, 0 }, WM_MOUSEWHEEL, noModifierKeys, WHEEL_DELTA, {}));

        Log::Comment(L"Confirm no effect when using the main buffer");
        mouseInput.UseMainScreenBuffer();
        mouseInput.SetInputMode(TerminalInput::Mode::AlternateScroll, true);
        VERIFY_ARE_EQUAL(TerminalInput::MakeUnhandled(), mouseInput.HandleMouse({ 0, 0 }, WM_MOUSEWHEEL, noModifierKeys, WHEEL_DELTA, {}));
    }
};
