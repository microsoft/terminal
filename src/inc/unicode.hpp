/*++
Copyright (c) Microsoft Corporation.
Licensed under the MIT license.

Module Name:
- unicode.hpp

Abstract:
- This file contains global vars for some common wchar values.
- taken from input.h

Author(s):
- Austin Diviness (AustDi) Oct 2017
--*/

#pragma once

#define CP_UTF8 65001
#define CP_USA 437
#define CP_KOREAN 949
#define CP_JAPANESE 932
#define CP_CHINESE_SIMPLIFIED 936
#define CP_CHINESE_TRADITIONAL 950

#define IsBilingualCP(cp) ((cp) == CP_JAPANESE || (cp) == CP_KOREAN)
#define IsEastAsianCP(cp) ((cp) == CP_JAPANESE || (cp) == CP_KOREAN || (cp) == CP_CHINESE_TRADITIONAL || (cp) == CP_CHINESE_SIMPLIFIED)

// UNICODE_NULL is a Windows macro definition
constexpr wchar_t UNICODE_BACKSPACE = 0x8;
constexpr wchar_t UNICODE_ESC = 0x1b;
constexpr wchar_t UNICODE_DEL = 0x7f;
// NOTE: This isn't actually a backspace. It's a graphical block. But
// I believe it's emitted by one of our ANSI/OEM --> Unicode conversions.
// We should dig further into this in the future.
constexpr wchar_t UNICODE_BACKSPACE2 = 0x25d8;
constexpr wchar_t UNICODE_CARRIAGERETURN = 0x0d;
constexpr wchar_t UNICODE_LINEFEED = 0x0a;
constexpr wchar_t UNICODE_BELL = 0x07;
constexpr wchar_t UNICODE_TAB = 0x09;
constexpr wchar_t UNICODE_SPACE = 0x20;
constexpr wchar_t UNICODE_LEFT_SMARTQUOTE = 0x201c;
constexpr wchar_t UNICODE_RIGHT_SMARTQUOTE = 0x201d;
constexpr wchar_t UNICODE_EM_DASH = 0x2014;
constexpr wchar_t UNICODE_EN_DASH = 0x2013;
constexpr wchar_t UNICODE_NBSP = 0xa0;
constexpr wchar_t UNICODE_NARROW_NBSP = 0x202f;
constexpr wchar_t UNICODE_QUOTE = L'\"';
constexpr wchar_t UNICODE_HYPHEN = L'-';
constexpr wchar_t UNICODE_BOX_DRAW_LIGHT_DOWN_AND_RIGHT = 0x250c;
constexpr wchar_t UNICODE_BOX_DRAW_LIGHT_DOWN_AND_LEFT = 0x2510;
constexpr wchar_t UNICODE_BOX_DRAW_LIGHT_HORIZONTAL = 0x2500;
constexpr wchar_t UNICODE_BOX_DRAW_LIGHT_VERTICAL = 0x2502;
constexpr wchar_t UNICODE_BOX_DRAW_LIGHT_UP_AND_RIGHT = 0x2514;
constexpr wchar_t UNICODE_BOX_DRAW_LIGHT_UP_AND_LEFT = 0x2518;
constexpr wchar_t UNICODE_INVALID = 0xFFFF;

// This is the "Ctrl+C" character.
//      With VKey='C', it generates a CTRL_C_EVENT
//      With VKey=VK_CANCEL (0x3), it generates a CTRL_BREAK_EVENT
constexpr wchar_t UNICODE_ETX = L'\x3';
constexpr wchar_t UNICODE_REPLACEMENT = 0xFFFD;
