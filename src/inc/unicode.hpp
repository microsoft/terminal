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
const wchar_t UNICODE_BACKSPACE = 0x8;
const wchar_t UNICODE_ESC = 0x1b;
const wchar_t UNICODE_DEL = 0x7f;
// NOTE: This isn't actually a backspace. It's a graphical block. But
// I believe it's emitted by one of our ANSI/OEM --> Unicode conversions.
// We should dig further into this in the future.
const wchar_t UNICODE_BEL = 0x7;
const wchar_t UNICODE_BACKSPACE2 = 0x25d8;
const wchar_t UNICODE_CARRIAGERETURN = 0x0d;
const wchar_t UNICODE_LINEFEED = 0x0a;
const wchar_t UNICODE_BELL = 0x07;
const wchar_t UNICODE_TAB = 0x09;
const wchar_t UNICODE_SPACE = 0x20;
const wchar_t UNICODE_LEFT_SMARTQUOTE = 0x201c;
const wchar_t UNICODE_RIGHT_SMARTQUOTE = 0x201d;
const wchar_t UNICODE_EM_DASH = 0x2014;
const wchar_t UNICODE_EN_DASH = 0x2013;
const wchar_t UNICODE_NBSP = 0xa0;
const wchar_t UNICODE_NARROW_NBSP = 0x202f;
const wchar_t UNICODE_QUOTE = L'\"';
const wchar_t UNICODE_HYPHEN = L'-';
const wchar_t UNICODE_BOX_DRAW_LIGHT_DOWN_AND_RIGHT = 0x250c;
const wchar_t UNICODE_BOX_DRAW_LIGHT_DOWN_AND_LEFT = 0x2510;
const wchar_t UNICODE_BOX_DRAW_LIGHT_HORIZONTAL = 0x2500;
const wchar_t UNICODE_BOX_DRAW_LIGHT_VERTICAL = 0x2502;
const wchar_t UNICODE_BOX_DRAW_LIGHT_UP_AND_RIGHT = 0x2514;
const wchar_t UNICODE_BOX_DRAW_LIGHT_UP_AND_LEFT = 0x2518;
const wchar_t UNICODE_INVALID = 0xFFFF;

// This is the "Ctrl+C" character.
//      With VKey='C', it generates a CTRL_C_EVENT
//      With VKey=VK_CANCEL (0x3), it generates a CTRL_BREAK_EVENT
const wchar_t UNICODE_ETX = L'\x3';
const wchar_t UNICODE_REPLACEMENT = 0xFFFD;
