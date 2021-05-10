/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ConsoleTAEFTemplates.hpp

Abstract:
- This module contains common TAEF templates for console structures

Author:
- Michael Niksa (MiNiksa)          2015
- Paul Campbell (PaulCam)          2015

Revision History:
--*/

#pragma once

// Helper for declaring a variable to store a TEST_METHOD_PROPERTY and get it's value from the test metadata
#define INIT_TEST_PROPERTY(type, identifer, description) \
    type identifer;                                      \
    VERIFY_SUCCEEDED(TestData::TryGetValue(L#identifer, identifer), description);

// Thinking of adding a new VerifyOutputTraits for a new type? MAKE SURE that
// you include this header (or at least the relevant definition) before _every_
// Verify for that type.
//
// From a thread on the matter in 2018:
// > my best guess is that one of your cpp files uses a COORD in a Verify macro
// > without including consoletaeftemplates.hpp. This caused the
// > VerifyOutputTraits<COORD> symbol to be used with the default
// > implementation. In other of your cpp files, you did include
// > consoletaeftemplates.hpp properly and they would have compiled the actual
// > specialization from consoletaeftemplates.hpp into the corresponding obj
// > file for that cpp file. When the test DLL was linked, the linker picks one
// > of the multiple definitions available from the different obj files for
// > VerifyOutputTraits<COORD>. The linker happened to pick the one from the cpp
// > file where consoletaeftemplates.hpp was not included properly. I’ve
// > encountered a similar situation before and it was baffling because the
// > compiled code was obviously doing different behavior than what the source
// > code said. This can happen when you violate the one-definition rule.

namespace WEX::TestExecution
{
    template<>
    class VerifyOutputTraits<SMALL_RECT>
    {
    public:
        static WEX::Common::NoThrowString ToString(const SMALL_RECT& sr)
        {
            return WEX::Common::NoThrowString().Format(L"(L:%d, R:%d, T:%d, B:%d)", sr.Left, sr.Right, sr.Top, sr.Bottom);
        }
    };

    template<>
    class VerifyCompareTraits<SMALL_RECT, SMALL_RECT>
    {
    public:
        static bool AreEqual(const SMALL_RECT& expected, const SMALL_RECT& actual)
        {
            return expected.Left == actual.Left &&
                   expected.Right == actual.Right &&
                   expected.Top == actual.Top &&
                   expected.Bottom == actual.Bottom;
        }

        static bool AreSame(const SMALL_RECT& expected, const SMALL_RECT& actual)
        {
            return &expected == &actual;
        }

        static bool IsLessThan(const SMALL_RECT& expectedLess, const SMALL_RECT& expectedGreater) = delete;

        static bool IsGreaterThan(const SMALL_RECT& expectedGreater, const SMALL_RECT& expectedLess) = delete;

        static bool IsNull(const SMALL_RECT& object)
        {
            return object.Left == 0 && object.Right == 0 && object.Top == 0 && object.Bottom == 0;
        }
    };

    template<>
    class VerifyOutputTraits<COORD>
    {
    public:
        static WEX::Common::NoThrowString ToString(const COORD& coord)
        {
            return WEX::Common::NoThrowString().Format(L"(X:%d, Y:%d)", coord.X, coord.Y);
        }
    };

    template<>
    class VerifyCompareTraits<COORD, COORD>
    {
    public:
        static bool AreEqual(const COORD& expected, const COORD& actual)
        {
            return expected.X == actual.X &&
                   expected.Y == actual.Y;
        }

        static bool AreSame(const COORD& expected, const COORD& actual)
        {
            return &expected == &actual;
        }

        static bool IsLessThan(const COORD& expectedLess, const COORD& expectedGreater)
        {
            // less is on a line above greater (Y values less than)
            return (expectedLess.Y < expectedGreater.Y) ||
                   // or on the same lines and less is left of greater (X values less than)
                   ((expectedLess.Y == expectedGreater.Y) && (expectedLess.X < expectedGreater.X));
        }

        static bool IsGreaterThan(const COORD& expectedGreater, const COORD& expectedLess)
        {
            // greater is on a line below less (Y value greater than)
            return (expectedGreater.Y > expectedLess.Y) ||
                   // or on the same lines and greater is right of less (X values greater than)
                   ((expectedGreater.Y == expectedLess.Y) && (expectedGreater.X > expectedLess.X));
        }

        static bool IsNull(const COORD& object)
        {
            return object.X == 0 && object.Y == 0;
        }
    };

    template<>
    class VerifyOutputTraits<CONSOLE_CURSOR_INFO>
    {
    public:
        static WEX::Common::NoThrowString ToString(const CONSOLE_CURSOR_INFO& cci)
        {
            return WEX::Common::NoThrowString().Format(L"(Vis:%s, Size:%d)", cci.bVisible ? L"True" : L"False", cci.dwSize);
        }
    };

    template<>
    class VerifyCompareTraits<CONSOLE_CURSOR_INFO, CONSOLE_CURSOR_INFO>
    {
    public:
        static bool AreEqual(const CONSOLE_CURSOR_INFO& expected, const CONSOLE_CURSOR_INFO& actual)
        {
            return expected.bVisible == actual.bVisible &&
                   expected.dwSize == actual.dwSize;
        }

        static bool AreSame(const CONSOLE_CURSOR_INFO& expected, const CONSOLE_CURSOR_INFO& actual)
        {
            return &expected == &actual;
        }

        static bool IsLessThan(const CONSOLE_CURSOR_INFO& expectedLess, const CONSOLE_CURSOR_INFO& expectedGreater) = delete;

        static bool IsGreaterThan(const CONSOLE_CURSOR_INFO& expectedGreater, const CONSOLE_CURSOR_INFO& expectedLess) = delete;

        static bool IsNull(const CONSOLE_CURSOR_INFO& object)
        {
            return object.bVisible == 0 && object.dwSize == 0;
        }
    };

    template<>
    class VerifyOutputTraits<CONSOLE_SCREEN_BUFFER_INFOEX>
    {
    public:
        static WEX::Common::NoThrowString ToString(const CONSOLE_SCREEN_BUFFER_INFOEX& sbiex)
        {
            return WEX::Common::NoThrowString().Format(L"(Full:%s Attrs:0x%x PopupAttrs:0x%x CursorPos:%s Size:%s MaxSize:%s Viewport:%s)\r\nColors:\r\n(0:0x%x)\r\n(1:0x%x)\r\n(2:0x%x)\r\n(3:0x%x)\r\n(4:0x%x)\r\n(5:0x%x)\r\n(6:0x%x)\r\n(7:0x%x)\r\n(8:0x%x)\r\n(9:0x%x)\r\n(A:0x%x)\r\n(B:0x%x)\r\n(C:0x%x)\r\n(D:0x%x)\r\n(E:0x%x)\r\n(F:0x%x)\r\n",
                                                       sbiex.bFullscreenSupported ? L"True" : L"False",
                                                       sbiex.wAttributes,
                                                       sbiex.wPopupAttributes,
                                                       VerifyOutputTraits<COORD>::ToString(sbiex.dwCursorPosition).ToCStrWithFallbackTo(L"Fail"),
                                                       VerifyOutputTraits<COORD>::ToString(sbiex.dwSize).ToCStrWithFallbackTo(L"Fail"),
                                                       VerifyOutputTraits<COORD>::ToString(sbiex.dwMaximumWindowSize).ToCStrWithFallbackTo(L"Fail"),
                                                       VerifyOutputTraits<SMALL_RECT>::ToString(sbiex.srWindow).ToCStrWithFallbackTo(L"Fail"),
                                                       sbiex.ColorTable[0],
                                                       sbiex.ColorTable[1],
                                                       sbiex.ColorTable[2],
                                                       sbiex.ColorTable[3],
                                                       sbiex.ColorTable[4],
                                                       sbiex.ColorTable[5],
                                                       sbiex.ColorTable[6],
                                                       sbiex.ColorTable[7],
                                                       sbiex.ColorTable[8],
                                                       sbiex.ColorTable[9],
                                                       sbiex.ColorTable[10],
                                                       sbiex.ColorTable[11],
                                                       sbiex.ColorTable[12],
                                                       sbiex.ColorTable[13],
                                                       sbiex.ColorTable[14],
                                                       sbiex.ColorTable[15]);
        }
    };

    template<>
    class VerifyCompareTraits<CONSOLE_SCREEN_BUFFER_INFOEX, CONSOLE_SCREEN_BUFFER_INFOEX>
    {
    public:
        static bool AreEqual(const CONSOLE_SCREEN_BUFFER_INFOEX& expected, const CONSOLE_SCREEN_BUFFER_INFOEX& actual)
        {
            return expected.bFullscreenSupported == actual.bFullscreenSupported &&
                   expected.wAttributes == actual.wAttributes &&
                   expected.wPopupAttributes == actual.wPopupAttributes &&
                   VerifyCompareTraits<COORD>::AreEqual(expected.dwCursorPosition, actual.dwCursorPosition) &&
                   VerifyCompareTraits<COORD>::AreEqual(expected.dwSize, actual.dwSize) &&
                   VerifyCompareTraits<COORD>::AreEqual(expected.dwMaximumWindowSize, actual.dwMaximumWindowSize) &&
                   VerifyCompareTraits<SMALL_RECT>::AreEqual(expected.srWindow, actual.srWindow) &&
                   expected.ColorTable[0] == actual.ColorTable[0] &&
                   expected.ColorTable[1] == actual.ColorTable[1] &&
                   expected.ColorTable[2] == actual.ColorTable[2] &&
                   expected.ColorTable[3] == actual.ColorTable[3] &&
                   expected.ColorTable[4] == actual.ColorTable[4] &&
                   expected.ColorTable[5] == actual.ColorTable[5] &&
                   expected.ColorTable[6] == actual.ColorTable[6] &&
                   expected.ColorTable[7] == actual.ColorTable[7] &&
                   expected.ColorTable[8] == actual.ColorTable[8] &&
                   expected.ColorTable[9] == actual.ColorTable[9] &&
                   expected.ColorTable[10] == actual.ColorTable[10] &&
                   expected.ColorTable[11] == actual.ColorTable[11] &&
                   expected.ColorTable[12] == actual.ColorTable[12] &&
                   expected.ColorTable[13] == actual.ColorTable[13] &&
                   expected.ColorTable[14] == actual.ColorTable[14] &&
                   expected.ColorTable[15] == actual.ColorTable[15];
        }

        static bool AreSame(const CONSOLE_SCREEN_BUFFER_INFOEX& expected, const CONSOLE_SCREEN_BUFFER_INFOEX& actual)
        {
            return &expected == &actual;
        }

        static bool IsLessThan(const CONSOLE_SCREEN_BUFFER_INFOEX& expectedLess, const CONSOLE_SCREEN_BUFFER_INFOEX& expectedGreater) = delete;

        static bool IsGreaterThan(const CONSOLE_SCREEN_BUFFER_INFOEX& expectedGreater, const CONSOLE_SCREEN_BUFFER_INFOEX& expectedLess) = delete;

        static bool IsNull(const CONSOLE_SCREEN_BUFFER_INFOEX& object)
        {
            return object.bFullscreenSupported == 0 &&
                   object.wAttributes == 0 &&
                   object.wPopupAttributes == 0 &&
                   VerifyCompareTraits<COORD>::IsNull(object.dwCursorPosition) &&
                   VerifyCompareTraits<COORD>::IsNull(object.dwSize) &&
                   VerifyCompareTraits<COORD>::IsNull(object.dwMaximumWindowSize) &&
                   VerifyCompareTraits<SMALL_RECT>::IsNull(object.srWindow) &&
                   object.ColorTable[0] == 0x0 &&
                   object.ColorTable[1] == 0x0 &&
                   object.ColorTable[2] == 0x0 &&
                   object.ColorTable[3] == 0x0 &&
                   object.ColorTable[4] == 0x0 &&
                   object.ColorTable[5] == 0x0 &&
                   object.ColorTable[6] == 0x0 &&
                   object.ColorTable[7] == 0x0 &&
                   object.ColorTable[8] == 0x0 &&
                   object.ColorTable[9] == 0x0 &&
                   object.ColorTable[10] == 0x0 &&
                   object.ColorTable[11] == 0x0 &&
                   object.ColorTable[12] == 0x0 &&
                   object.ColorTable[13] == 0x0 &&
                   object.ColorTable[14] == 0x0 &&
                   object.ColorTable[15] == 0x0;
        }
    };

    template<>
    class VerifyOutputTraits<INPUT_RECORD>
    {
    public:
        static WEX::Common::NoThrowString ToString(const INPUT_RECORD& ir)
        {
            SetVerifyOutput verifySettings(VerifyOutputSettings::LogOnlyFailures);
            WCHAR szBuf[1024];
            VERIFY_SUCCEEDED(StringCchCopy(szBuf, ARRAYSIZE(szBuf), L"(ev: "));
            switch (ir.EventType)
            {
            case FOCUS_EVENT:
            {
                WCHAR szFocus[512];
                VERIFY_SUCCEEDED(StringCchPrintf(szFocus,
                                                 ARRAYSIZE(szFocus),
                                                 L"FOCUS set: %s)",
                                                 ir.Event.FocusEvent.bSetFocus ? L"T" : L"F"));
                VERIFY_SUCCEEDED(StringCchCat(szBuf, ARRAYSIZE(szBuf), szFocus));
                break;
            }

            case KEY_EVENT:
            {
                WCHAR szKey[512];
                VERIFY_SUCCEEDED(StringCchPrintf(szKey,
                                                 ARRAYSIZE(szKey),
                                                 L"KEY down: %s reps: %d kc: 0x%x sc: 0x%x uc: %d ctl: 0x%x)",
                                                 ir.Event.KeyEvent.bKeyDown ? L"T" : L"F",
                                                 ir.Event.KeyEvent.wRepeatCount,
                                                 ir.Event.KeyEvent.wVirtualKeyCode,
                                                 ir.Event.KeyEvent.wVirtualScanCode,
                                                 ir.Event.KeyEvent.uChar.UnicodeChar,
                                                 ir.Event.KeyEvent.dwControlKeyState));
                VERIFY_SUCCEEDED(StringCchCat(szBuf, ARRAYSIZE(szBuf), szKey));
                break;
            }

            case MENU_EVENT:
            {
                WCHAR szMenu[512];
                VERIFY_SUCCEEDED(StringCchPrintf(szMenu,
                                                 ARRAYSIZE(szMenu),
                                                 L"MENU cmd: %d (0x%x))",
                                                 ir.Event.MenuEvent.dwCommandId,
                                                 ir.Event.MenuEvent.dwCommandId));
                VERIFY_SUCCEEDED(StringCchCat(szBuf, ARRAYSIZE(szBuf), szMenu));
                break;
            }

            case MOUSE_EVENT:
            {
                WCHAR szMouse[512];
                VERIFY_SUCCEEDED(StringCchPrintf(szMouse,
                                                 ARRAYSIZE(szMouse),
                                                 L"MOUSE pos: (%d, %d) buttons: 0x%x ctl: 0x%x evflags: 0x%x)",
                                                 ir.Event.MouseEvent.dwMousePosition.X,
                                                 ir.Event.MouseEvent.dwMousePosition.Y,
                                                 ir.Event.MouseEvent.dwButtonState,
                                                 ir.Event.MouseEvent.dwControlKeyState,
                                                 ir.Event.MouseEvent.dwEventFlags));
                VERIFY_SUCCEEDED(StringCchCat(szBuf, ARRAYSIZE(szBuf), szMouse));
                break;
            }

            case WINDOW_BUFFER_SIZE_EVENT:
            {
                WCHAR szBufferSize[512];
                VERIFY_SUCCEEDED(StringCchPrintf(szBufferSize,
                                                 ARRAYSIZE(szBufferSize),
                                                 L"WINDOW_BUFFER_SIZE (%d, %d)",
                                                 ir.Event.WindowBufferSizeEvent.dwSize.X,
                                                 ir.Event.WindowBufferSizeEvent.dwSize.Y));
                VERIFY_SUCCEEDED(StringCchCat(szBuf, ARRAYSIZE(szBuf), szBufferSize));
                break;
            }

            default:
                VERIFY_FAIL(L"ERROR: unknown input event type encountered");
            }

            return WEX::Common::NoThrowString(szBuf);
        }
    };

    template<>
    class VerifyCompareTraits<INPUT_RECORD, INPUT_RECORD>
    {
    public:
        static bool AreEqual(const INPUT_RECORD& expected, const INPUT_RECORD& actual)
        {
            bool fEqual = false;
            if (expected.EventType == actual.EventType)
            {
                switch (expected.EventType)
                {
                case FOCUS_EVENT:
                {
                    fEqual = expected.Event.FocusEvent.bSetFocus == actual.Event.FocusEvent.bSetFocus;
                    break;
                }

                case KEY_EVENT:
                {
                    fEqual = (expected.Event.KeyEvent.bKeyDown == actual.Event.KeyEvent.bKeyDown &&
                              expected.Event.KeyEvent.wRepeatCount == actual.Event.KeyEvent.wRepeatCount &&
                              expected.Event.KeyEvent.wVirtualKeyCode == actual.Event.KeyEvent.wVirtualKeyCode &&
                              expected.Event.KeyEvent.wVirtualScanCode == actual.Event.KeyEvent.wVirtualScanCode &&
                              expected.Event.KeyEvent.uChar.UnicodeChar == actual.Event.KeyEvent.uChar.UnicodeChar &&
                              expected.Event.KeyEvent.dwControlKeyState == actual.Event.KeyEvent.dwControlKeyState);
                    break;
                }

                case MENU_EVENT:
                {
                    fEqual = expected.Event.MenuEvent.dwCommandId == actual.Event.MenuEvent.dwCommandId;
                    break;
                }

                case MOUSE_EVENT:
                {
                    fEqual = (expected.Event.MouseEvent.dwMousePosition.X == actual.Event.MouseEvent.dwMousePosition.X &&
                              expected.Event.MouseEvent.dwMousePosition.Y == actual.Event.MouseEvent.dwMousePosition.Y &&
                              expected.Event.MouseEvent.dwButtonState == actual.Event.MouseEvent.dwButtonState &&
                              expected.Event.MouseEvent.dwControlKeyState == actual.Event.MouseEvent.dwControlKeyState &&
                              expected.Event.MouseEvent.dwEventFlags == actual.Event.MouseEvent.dwEventFlags);
                    break;
                }

                case WINDOW_BUFFER_SIZE_EVENT:
                {
                    fEqual = (expected.Event.WindowBufferSizeEvent.dwSize.X == actual.Event.WindowBufferSizeEvent.dwSize.X &&
                              expected.Event.WindowBufferSizeEvent.dwSize.Y == actual.Event.WindowBufferSizeEvent.dwSize.Y);
                    break;
                }

                default:
                    VERIFY_FAIL(L"ERROR: unknown input event type encountered");
                }
            }

            return fEqual;
        }

        static bool AreSame(const INPUT_RECORD& expected, const INPUT_RECORD& actual)
        {
            return &expected == &actual;
        }

        static bool IsLessThan(const INPUT_RECORD& expectedLess, const INPUT_RECORD& expectedGreater) = delete;

        static bool IsGreaterThan(const INPUT_RECORD& expectedGreater, const INPUT_RECORD& expectedLess) = delete;

        static bool IsNull(const INPUT_RECORD& object)
        {
            return object.EventType == 0;
        }
    };

    template<>
    class VerifyOutputTraits<CONSOLE_FONT_INFO>
    {
    public:
        static WEX::Common::NoThrowString ToString(const CONSOLE_FONT_INFO& cfi)
        {
            return WEX::Common::NoThrowString().Format(L"Index: %n  Size: (X:%d, Y:%d)", cfi.nFont, cfi.dwFontSize.X, cfi.dwFontSize.Y);
        }
    };

    template<>
    class VerifyCompareTraits<CONSOLE_FONT_INFO, CONSOLE_FONT_INFO>
    {
    public:
        static bool AreEqual(const CONSOLE_FONT_INFO& expected, const CONSOLE_FONT_INFO& actual)
        {
            return expected.nFont == actual.nFont &&
                   expected.dwFontSize.X == actual.dwFontSize.X &&
                   expected.dwFontSize.Y == actual.dwFontSize.Y;
        }

        static bool AreSame(const CONSOLE_FONT_INFO& expected, const CONSOLE_FONT_INFO& actual)
        {
            return &expected == &actual;
        }

        static bool IsLessThan(const CONSOLE_FONT_INFO& expectedLess, const CONSOLE_FONT_INFO& expectedGreater)
        {
            return expectedLess.dwFontSize.X < expectedGreater.dwFontSize.X &&
                   expectedLess.dwFontSize.Y < expectedGreater.dwFontSize.Y;
        }

        static bool IsGreaterThan(const CONSOLE_FONT_INFO& expectedGreater, const CONSOLE_FONT_INFO& expectedLess)
        {
            return expectedLess.dwFontSize.X < expectedGreater.dwFontSize.X &&
                   expectedLess.dwFontSize.Y < expectedGreater.dwFontSize.Y;
        }

        static bool IsNull(const CONSOLE_FONT_INFO& object)
        {
            return object.nFont == 0 && object.dwFontSize.X == 0 && object.dwFontSize.Y == 0;
        }
    };

    template<>
    class VerifyOutputTraits<CONSOLE_FONT_INFOEX>
    {
    public:
        static WEX::Common::NoThrowString ToString(const CONSOLE_FONT_INFOEX& cfiex)
        {
            return WEX::Common::NoThrowString().Format(L"Index: %d  Size: (X:%d, Y:%d)  Family: 0x%x (%d)  Weight: 0x%x (%d)  Name: %ls",
                                                       cfiex.nFont,
                                                       cfiex.dwFontSize.X,
                                                       cfiex.dwFontSize.Y,
                                                       cfiex.FontFamily,
                                                       cfiex.FontFamily,
                                                       cfiex.FontWeight,
                                                       cfiex.FontWeight,
                                                       cfiex.FaceName);
        }
    };

    template<>
    class VerifyCompareTraits<CONSOLE_FONT_INFOEX, CONSOLE_FONT_INFOEX>
    {
    public:
        static bool AreEqual(const CONSOLE_FONT_INFOEX& expected, const CONSOLE_FONT_INFOEX& actual)
        {
            return expected.nFont == actual.nFont &&
                   expected.dwFontSize.X == actual.dwFontSize.X &&
                   expected.dwFontSize.Y == actual.dwFontSize.Y &&
                   expected.FontFamily == actual.FontFamily &&
                   expected.FontWeight == actual.FontWeight &&
                   0 == wcscmp(expected.FaceName, actual.FaceName);
        }

        static bool AreSame(const CONSOLE_FONT_INFOEX& expected, const CONSOLE_FONT_INFOEX& actual)
        {
            return &expected == &actual;
        }

        static bool IsLessThan(const CONSOLE_FONT_INFOEX& expectedLess, const CONSOLE_FONT_INFOEX& expectedGreater)
        {
            return expectedLess.dwFontSize.X < expectedGreater.dwFontSize.X &&
                   expectedLess.dwFontSize.Y < expectedGreater.dwFontSize.Y;
        }

        static bool IsGreaterThan(const CONSOLE_FONT_INFOEX& expectedGreater, const CONSOLE_FONT_INFOEX& expectedLess)
        {
            return expectedLess.dwFontSize.X < expectedGreater.dwFontSize.X &&
                   expectedLess.dwFontSize.Y < expectedGreater.dwFontSize.Y;
        }

        static bool IsNull(const CONSOLE_FONT_INFOEX& object)
        {
            return object.nFont == 0 && object.dwFontSize.X == 0 && object.dwFontSize.Y == 0 &&
                   object.FontFamily == 0 && object.FontWeight == 0 && object.FaceName[0] == L'\0';
        }
    };

    template<>
    class VerifyOutputTraits<CHAR_INFO>
    {
    public:
        static WEX::Common::NoThrowString ToString(const CHAR_INFO& ci)
        {
            // 0x2400 is the Unicode symbol for a printable 'NUL' inscribed in a 1 column block. It's for communicating NUL without printing 0x0.
            wchar_t const wch = ci.Char.UnicodeChar != L'\0' ? ci.Char.UnicodeChar : 0x2400;

            // 0x20 is a standard space character.
            char const ch = ci.Char.AsciiChar != '\0' ? ci.Char.AsciiChar : 0x20;

            return WEX::Common::NoThrowString().Format(L"Unicode Char: %lc (0x%x),  Attributes: 0x%x,  [Ascii Char: %c (0x%hhx)]",
                                                       wch,
                                                       ci.Char.UnicodeChar,
                                                       ci.Attributes,
                                                       ch,
                                                       ci.Char.AsciiChar);
        }
    };

    template<>
    class VerifyCompareTraits<CHAR_INFO, CHAR_INFO>
    {
    public:
        static bool AreEqual(const CHAR_INFO& expected, const CHAR_INFO& actual)
        {
            return expected.Attributes == actual.Attributes &&
                   expected.Char.UnicodeChar == actual.Char.UnicodeChar;
        }

        static bool AreSame(const CHAR_INFO& expected, const CHAR_INFO& actual)
        {
            return &expected == &actual;
        }

        static bool IsLessThan(const CHAR_INFO&, const CHAR_INFO&) = delete;

        static bool IsGreaterThan(const CHAR_INFO&, const CHAR_INFO&) = delete;

        static bool IsNull(const CHAR_INFO& object)
        {
            return object.Attributes == 0 && object.Char.UnicodeChar == 0;
        }
    };

    template<>
    class VerifyOutputTraits<std::string_view>
    {
    public:
        static WEX::Common::NoThrowString ToString(const std::string_view& view)
        {
            if (view.empty())
            {
                return L"<empty>";
            }

            WEX::Common::NoThrowString s;
            s.AppendFormat(L"%.*hs", gsl::narrow_cast<unsigned int>(view.size()), view.data());
            return s;
        }
    };

    template<>
    class VerifyOutputTraits<std::wstring_view>
    {
    public:
        static WEX::Common::NoThrowString ToString(const std::wstring_view& view)
        {
            if (view.empty())
            {
                return L"<empty>";
            }

            return WEX::Common::NoThrowString(view.data(), gsl::narrow<int>(view.size()));
        }
    };

    template<typename Elem>
    class VerifyCompareTraits<std::basic_string_view<Elem>, std::basic_string_view<Elem>>
    {
    public:
        static bool AreEqual(const std::basic_string_view<Elem>& expected, const std::basic_string_view<Elem>& actual)
        {
            return expected == actual;
        }

        static bool AreSame(const std::basic_string_view<Elem>& expected, const std::basic_string_view<Elem>& actual)
        {
            return expected.data() == actual.data();
        }

        static bool IsLessThan(const std::basic_string_view<Elem>&, const std::basic_string_view<Elem>&) = delete;

        static bool IsGreaterThan(const std::basic_string_view<Elem>&, const std::basic_string_view<Elem>&) = delete;

        static bool IsNull(const std::basic_string_view<Elem>& object)
        {
            return object.empty();
        }
    };
}
