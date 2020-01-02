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
// - Formats and writes a sequence to erase the remainer of the line starting
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
// - Formats and writes a sequence to erase the remainer of the line starting
//      from the cursor position.
// Arguments:
// - <none>
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_ClearScreen() noexcept
{
    return _Write("\x1b[2J");
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
// - Formats and writes a sequence change the boldness of the following text.
// Arguments:
// - isBold: If true, we'll embolden the text. Otherwise we'll debolden the text.
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_SetGraphicsBoldness(const bool isBold) noexcept
{
    const std::string fmt = isBold ? "\x1b[1m" : "\x1b[22m";
    return _Write(fmt);
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
    // By specifying the boldness and brightness seperately, we'll make sure the
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
    const std::string fmt = fIsForeground ? ("\x1b[39m") : ("\x1b[49m");

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
// - Writes a sequence to tell the terminal to start underlining text
// Arguments:
// - <none>
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_BeginUnderline() noexcept
{
    return _Write("\x1b[4m");
}

// Method Description:
// - Writes a sequence to tell the terminal to stop underlining text
// Arguments:
// - <none>
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_EndUnderline() noexcept
{
    return _Write("\x1b[24m");
}

// Method Description:
// - Writes a sequence to tell the terminal to start italicizing text
// Arguments:
// - <none>
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_BeginItalics() noexcept
{
    return _Write("\x1b[3m");
}

// Method Description:
// - Writes a sequence to tell the terminal to stop italicizing text
// Arguments:
// - <none>
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_EndItalics() noexcept
{
    return _Write("\x1b[23m");
}

// Method Description:
// - Writes a sequence to tell the terminal to start blinking text
// Arguments:
// - <none>
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_BeginBlink() noexcept
{
    return _Write("\x1b[5m");
}

// Method Description:
// - Writes a sequence to tell the terminal to stop blinking text
// Arguments:
// - <none>
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_EndBlink() noexcept
{
    return _Write("\x1b[25m");
}

// Method Description:
// - Writes a sequence to tell the terminal to start marking text as invisible
// Arguments:
// - <none>
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_BeginInvisible() noexcept
{
    return _Write("\x1b[8m");
}

// Method Description:
// - Writes a sequence to tell the terminal to stop marking text as invisible
// Arguments:
// - <none>
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_EndInvisible() noexcept
{
    return _Write("\x1b[28m");
}

// Method Description:
// - Writes a sequence to tell the terminal to start crossing-out text
// Arguments:
// - <none>
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_BeginCrossedOut() noexcept
{
    return _Write("\x1b[9m");
}

// Method Description:
// - Writes a sequence to tell the terminal to stop crossing-out text
// Arguments:
// - <none>
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_EndCrossedOut() noexcept
{
    return _Write("\x1b[29m");
}
