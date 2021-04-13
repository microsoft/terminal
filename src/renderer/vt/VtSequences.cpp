// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "vtrenderer.hpp"
#include "../../inc/conattrs.hpp"

#pragma hdrstop
using namespace Microsoft::Console::Render;

// Method Description:
// - Formats and writes a sequence to stop the cursor from blinking.
// Arguments:
// - <none>
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_StopCursorBlinking() noexcept
{
    return _Write("\x1b[?12l");
}

// Method Description:
// - Formats and writes a sequence to start the cursor blinking. If it's
//      hidden, this won't also show it.
// Arguments:
// - <none>
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_StartCursorBlinking() noexcept
{
    return _Write("\x1b[?12h");
}

// Method Description:
// - Formats and writes a sequence to hide the cursor.
// Arguments:
// - <none>
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_HideCursor() noexcept
{
    return _Write("\x1b[?25l");
}

// Method Description:
// - Formats and writes a sequence to show the cursor.
// Arguments:
// - <none>
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_ShowCursor() noexcept
{
    return _Write("\x1b[?25h");
}

// Method Description:
// - Formats and writes a sequence to erase the remainder of the line starting
//      from the cursor position.
// Arguments:
// - <none>
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_EraseLine() noexcept
{
    // The default no-param action of erase line is erase to the right.
    // telnet client doesn't understand the parameterized version,
    // so emit the implicit sequence instead.
    return _Write("\x1b[K");
}

// Method Description:
// - Formats and writes a sequence to either insert or delete a number of lines
//      into the buffer at the current cursor location.
//   Delete/insert Character removes/adds N characters from/to the buffer, and
//      shifts the remaining chars in the row to the left/right, while Erase
//      Character replaces N characters with spaces, and leaves the rest
//      untouched.
// Arguments:
// - chars: a number of characters to erase (by overwriting with space)
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_EraseCharacter(const short chars) noexcept
{
    static const std::string format = "\x1b[%dX";
    return _WriteFormattedString(&format, chars);
}

// Method Description:
// - Moves the cursor forward (right) a number of characters.
// Arguments:
// - chars: a number of characters to move cursor right by.
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_CursorForward(const short chars) noexcept
{
    static const std::string format = "\x1b[%dC";
    return _WriteFormattedString(&format, chars);
}

// Method Description:
// - Formats and writes a sequence to erase the remainder of the line starting
//      from the cursor position.
// Arguments:
// - <none>
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_ClearScreen() noexcept
{
    return _Write("\x1b[2J");
}

[[nodiscard]] HRESULT VtEngine::_ClearScrollback() noexcept
{
    return _Write("\x1b[3J");
}

// Method Description:
// - Formats and writes a sequence to either insert or delete a number of lines
//      into the buffer at the current cursor location.
// Arguments:
// - sLines: a number of lines to insert or delete
// - fInsertLine: true iff we should insert the lines, false to delete them.
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_InsertDeleteLine(const short sLines, const bool fInsertLine) noexcept
{
    if (sLines <= 0)
    {
        return S_OK;
    }
    if (sLines == 1)
    {
        return _Write(fInsertLine ? "\x1b[L" : "\x1b[M");
    }
    const std::string format = fInsertLine ? "\x1b[%dL" : "\x1b[%dM";

    return _WriteFormattedString(&format, sLines);
}

// Method Description:
// - Formats and writes a sequence to delete a number of lines into the buffer
//      at the current cursor location.
// Arguments:
// - sLines: a number of lines to insert
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_DeleteLine(const short sLines) noexcept
{
    return _InsertDeleteLine(sLines, false);
}

// Method Description:
// - Formats and writes a sequence to insert a number of lines into the buffer
//      at the current cursor location.
// Arguments:
// - sLines: a number of lines to insert
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_InsertLine(const short sLines) noexcept
{
    return _InsertDeleteLine(sLines, true);
}

// Method Description:
// - Formats and writes a sequence to move the cursor to the specified
//      coordinate position. The input coord should be in console coordinates,
//      where origin=(0,0).
// Arguments:
// - coord: Console coordinates to move the cursor to.
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_CursorPosition(const COORD coord) noexcept
{
    static const std::string cursorFormat = "\x1b[%d;%dH";

    // VT coords start at 1,1
    COORD coordVt = coord;
    coordVt.X++;
    coordVt.Y++;

    return _WriteFormattedString(&cursorFormat, coordVt.Y, coordVt.X);
}

// Method Description:
// - Formats and writes a sequence to move the cursor to the origin.
// Arguments:
// - <none>
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_CursorHome() noexcept
{
    return _Write("\x1b[H");
}

// Method Description:
// - Formats and writes a sequence to change the current text attributes to the default.
// Arguments:
// <none>
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_SetGraphicsDefault() noexcept
{
    return _Write("\x1b[m");
}

// Method Description:
// - Formats and writes a sequence to change the current text attributes.
// Arguments:
// - wAttr: Windows color table index to emit as a VT sequence
// - fIsForeground: true if we should emit the foreground sequence, false for background
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_SetGraphicsRendition16Color(const WORD wAttr,
                                                             const bool fIsForeground) noexcept
{
    static const std::string fmt = "\x1b[%dm";

    // Always check using the foreground flags, because the bg flags constants
    //  are a higher byte
    // Foreground sequences are in [30,37] U [90,97]
    // Background sequences are in [40,47] U [100,107]
    // The "dark" sequences are in the first 7 values, the bright sequences in the second set.
    // Note that text brightness and boldness are different in VT. Boldness is
    //      handled by _SetGraphicsBoldness. Here, we can emit either bright or
    //      dark colors. For conhost as a terminal, it can't draw bold
    //      characters, so it displays "bold" as bright, and in fact most
    //      terminals display the bright color when displaying bolded text.
    // By specifying the boldness and brightness separately, we'll make sure the
    //      terminal has an accurate representation of our buffer.
    const int vtIndex = 30 +
                        (fIsForeground ? 0 : 10) +
                        ((WI_IsFlagSet(wAttr, FOREGROUND_INTENSITY)) ? 60 : 0) +
                        (WI_IsFlagSet(wAttr, FOREGROUND_RED) ? 1 : 0) +
                        (WI_IsFlagSet(wAttr, FOREGROUND_GREEN) ? 2 : 0) +
                        (WI_IsFlagSet(wAttr, FOREGROUND_BLUE) ? 4 : 0);

    return _WriteFormattedString(&fmt, vtIndex);
}

// Method Description:
// - Formats and writes a sequence to change the current text attributes to an
//      indexed color from the 256-color table.
// Arguments:
// - wAttr: Windows color table index to emit as a VT sequence
// - fIsForeground: true if we should emit the foreground sequence, false for background
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_SetGraphicsRendition256Color(const WORD index,
                                                              const bool fIsForeground) noexcept
{
    const std::string fmt = fIsForeground ?
                                "\x1b[38;5;%dm" :
                                "\x1b[48;5;%dm";

    return _WriteFormattedString(&fmt, ::Xterm256ToWindowsIndex(index));
}

// Method Description:
// - Formats and writes a sequence to change the current text attributes to an
//      RGB color.
// Arguments:
// - color: The color to emit a VT sequence for
// - fIsForeground: true if we should emit the foreground sequence, false for background
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_SetGraphicsRenditionRGBColor(const COLORREF color,
                                                              const bool fIsForeground) noexcept
{
    const std::string fmt = fIsForeground ?
                                "\x1b[38;2;%d;%d;%dm" :
                                "\x1b[48;2;%d;%d;%dm";

    DWORD const r = GetRValue(color);
    DWORD const g = GetGValue(color);
    DWORD const b = GetBValue(color);

    return _WriteFormattedString(&fmt, r, g, b);
}

// Method Description:
// - Formats and writes a sequence to change the current text attributes to the
//      default foreground or background. Does not affect the boldness of text.
// Arguments:
// - fIsForeground: true if we should emit the foreground sequence, false for background
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_SetGraphicsRenditionDefaultColor(const bool fIsForeground) noexcept
{
    const std::string_view fmt = fIsForeground ? ("\x1b[39m") : ("\x1b[49m");

    return _Write(fmt);
}

// Method Description:
// - Formats and writes a sequence to change the terminal's window size.
// Arguments:
// - sWidth: number of columns the terminal should display
// - sHeight: number of rows the terminal should display
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_ResizeWindow(const short sWidth, const short sHeight) noexcept
{
    static const std::string resizeFormat = "\x1b[8;%d;%dt";
    if (sWidth < 0 || sHeight < 0)
    {
        return E_INVALIDARG;
    }

    return _WriteFormattedString(&resizeFormat, sHeight, sWidth);
}

// Method Description:
// - Formats and writes a sequence to request the end terminal to tell us the
//      cursor position. The terminal will reply back on the vt input handle.
// Arguments:
// - <none>
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_RequestCursor() noexcept
{
    return _Write("\x1b[6n");
}

// Method Description:
// - Formats and writes a sequence to change the terminal's title string
// Arguments:
// - title: string to use as the new title of the window.
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_ChangeTitle(_In_ const std::string& title) noexcept
{
    const std::string titleFormat = "\x1b]0;" + title + "\x7";
    return _Write(titleFormat);
}

// Method Description:
// - Formats and writes a sequence to change the boldness of the following text.
// Arguments:
// - isBold: If true, we'll embolden the text. Otherwise we'll debolden the text.
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_SetBold(const bool isBold) noexcept
{
    return _Write(isBold ? "\x1b[1m" : "\x1b[22m");
}

// Method Description:
// - Formats and writes a sequence to change the faintness of the following text.
// Arguments:
// - isFaint: If true, we'll make the text faint. Otherwise we'll remove the faintness.
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_SetFaint(const bool isFaint) noexcept
{
    return _Write(isFaint ? "\x1b[2m" : "\x1b[22m");
}

// Method Description:
// - Formats and writes a sequence to change the underline of the following text.
// Arguments:
// - isUnderlined: If true, we'll underline the text. Otherwise we'll remove the underline.
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_SetUnderlined(const bool isUnderlined) noexcept
{
    return _Write(isUnderlined ? "\x1b[4m" : "\x1b[24m");
}

// Method Description:
// - Formats and writes a sequence to change the double underline of the following text.
// Arguments:
// - isUnderlined: If true, we'll doubly underline the text. Otherwise we'll remove the underline.
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_SetDoublyUnderlined(const bool isUnderlined) noexcept
{
    return _Write(isUnderlined ? "\x1b[21m" : "\x1b[24m");
}

// Method Description:
// - Formats and writes a sequence to change the overline of the following text.
// Arguments:
// - isOverlined: If true, we'll overline the text. Otherwise we'll remove the overline.
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_SetOverlined(const bool isOverlined) noexcept
{
    return _Write(isOverlined ? "\x1b[53m" : "\x1b[55m");
}

// Method Description:
// - Formats and writes a sequence to change the italics of the following text.
// Arguments:
// - isItalic: If true, we'll italicize the text. Otherwise we'll remove the italics.
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_SetItalic(const bool isItalic) noexcept
{
    return _Write(isItalic ? "\x1b[3m" : "\x1b[23m");
}

// Method Description:
// - Formats and writes a sequence to change the blinking of the following text.
// Arguments:
// - isBlinking: If true, we'll start the text blinking. Otherwise we'll stop the blinking.
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_SetBlinking(const bool isBlinking) noexcept
{
    return _Write(isBlinking ? "\x1b[5m" : "\x1b[25m");
}

// Method Description:
// - Formats and writes a sequence to change the visibility of the following text.
// Arguments:
// - isInvisible: If true, we'll make the text invisible. Otherwise we'll make it visible.
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_SetInvisible(const bool isInvisible) noexcept
{
    return _Write(isInvisible ? "\x1b[8m" : "\x1b[28m");
}

// Method Description:
// - Formats and writes a sequence to change the crossed out state of the following text.
// Arguments:
// - isCrossedOut: If true, we'll cross out the text. Otherwise we'll stop crossing out.
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_SetCrossedOut(const bool isCrossedOut) noexcept
{
    return _Write(isCrossedOut ? "\x1b[9m" : "\x1b[29m");
}

// Method Description:
// - Formats and writes a sequence to change the reversed state of the following text.
// Arguments:
// - isReversed: If true, we'll reverse the text. Otherwise we'll remove the reversed state.
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_SetReverseVideo(const bool isReversed) noexcept
{
    return _Write(isReversed ? "\x1b[7m" : "\x1b[27m");
}

// Method Description:
// - Send a sequence to the connected terminal to request win32-input-mode from
//   them. This will enable the connected terminal to send us full INPUT_RECORDs
//   as input. If the terminal doesn't understand this sequence, it'll just
//   ignore it.
// Arguments:
// - <none>
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_RequestWin32Input() noexcept
{
    return _Write("\x1b[?9001h");
}

// Method Description:
// - Formats and writes a sequence to add a hyperlink to the terminal buffer
// Arguments:
// - The hyperlink URI
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_SetHyperlink(const std::wstring_view& uri, const std::wstring_view& customId, const uint16_t& numberId) noexcept
{
    // Opening OSC8 sequence
    if (customId.empty())
    {
        // This is the case of auto-assigned IDs:
        // send the auto-assigned ID, prefixed with the PID of this session
        // (we do this so different conpty sessions do not overwrite each other's hyperlinks)
        const auto sessionID = GetCurrentProcessId();
        const std::string uri_str{ til::u16u8(uri) };
        auto s = fmt::format(FMT_COMPILE("\x1b]8;id={}-{};{}\x1b\\"), sessionID, numberId, uri_str);
        return _Write(s);
    }
    else
    {
        // This is the case of user-defined IDs:
        // send the user-defined ID, prefixed with a "u"
        // (we do this so no application can accidentally override a user defined ID)
        const std::string uri_str{ til::u16u8(uri) };
        const std::string customId_str{ til::u16u8(customId) };
        auto s = fmt::format(FMT_COMPILE("\x1b]8;id=u-{};{}\x1b\\"), customId_str, uri_str);
        return _Write(s);
    }
}

// Method Description:
// - Formats and writes a sequence to end a hyperlink to the terminal buffer
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_EndHyperlink() noexcept
{
    // Closing OSC8 sequence
    return _Write("\x1b]8;;\x1b\\");
}
