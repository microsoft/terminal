// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TerminalDispatch.hpp"
using namespace ::Microsoft::Terminal::Core;
using namespace ::Microsoft::Console::VirtualTerminal;

// NOTE:
// Functions related to Set Graphics Renditions (SGR) are in
//      TerminalDispatchGraphics.cpp, not this file

TerminalDispatch::TerminalDispatch(ITerminalApi& terminalApi) noexcept :
    _terminalApi{ terminalApi }
{
}

void TerminalDispatch::Execute(const wchar_t wchControl)
{
    _terminalApi.ExecuteChar(wchControl);
}

void TerminalDispatch::Print(const wchar_t wchPrintable)
{
    _terminalApi.PrintString({ &wchPrintable, 1 });
}

void TerminalDispatch::PrintString(const std::wstring_view string)
{
    _terminalApi.PrintString(string);
}

bool TerminalDispatch::CursorPosition(const size_t line,
                                      const size_t column) noexcept
try
{
    SHORT x{ 0 };
    SHORT y{ 0 };

    if (FAILED(SizeTToShort(column, &x)) ||
        FAILED(SizeTToShort(line, &y)))
    {
        return false;
    }

    if (FAILED(ShortSub(x, 1, &x)) ||
        FAILED(ShortSub(y, 1, &y)))
    {
        return false;
    }

    return _terminalApi.SetCursorPosition(x, y);
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return false;
}

bool TerminalDispatch::CursorForward(const size_t distance) noexcept
try
{
    const auto cursorPos = _terminalApi.GetCursorPosition();
    const COORD newCursorPos{ cursorPos.X + gsl::narrow<short>(distance), cursorPos.Y };
    return _terminalApi.SetCursorPosition(newCursorPos.X, newCursorPos.Y);
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return false;
}

bool TerminalDispatch::CursorBackward(const size_t distance) noexcept
try
{
    const auto cursorPos = _terminalApi.GetCursorPosition();
    const COORD newCursorPos{ cursorPos.X - gsl::narrow<short>(distance), cursorPos.Y };
    return _terminalApi.SetCursorPosition(newCursorPos.X, newCursorPos.Y);
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return false;
}

bool TerminalDispatch::CursorUp(const size_t distance) noexcept
try
{
    const auto cursorPos = _terminalApi.GetCursorPosition();
    const COORD newCursorPos{ cursorPos.X, cursorPos.Y + gsl::narrow<short>(distance) };
    return _terminalApi.SetCursorPosition(newCursorPos.X, newCursorPos.Y);
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return false;
}

bool TerminalDispatch::EraseCharacters(const size_t numChars) noexcept
try
{
    return _terminalApi.EraseCharacters(numChars);
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return false;
}

bool TerminalDispatch::SetWindowTitle(std::wstring_view title) noexcept
try
{
    return _terminalApi.SetWindowTitle(title);
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return false;
}

// Method Description:
// - Sets a single entry of the colortable to a new value
// Arguments:
// - tableIndex: The VT color table index
// - color: The new RGB color value to use.
// Return Value:
// True if handled successfully. False otherwise.
bool TerminalDispatch::SetColorTableEntry(const size_t tableIndex,
                                          const DWORD color) noexcept
try
{
    return _terminalApi.SetColorTableEntry(tableIndex, color);
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return false;
}

bool TerminalDispatch::SetCursorStyle(const DispatchTypes::CursorStyle cursorStyle) noexcept
try
{
    return _terminalApi.SetCursorStyle(cursorStyle);
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return false;
}

// Method Description:
// - Sets the default foreground color to a new value
// Arguments:
// - color: The new RGB color value to use, in 0x00BBGGRR form
// Return Value:
// True if handled successfully. False otherwise.
bool TerminalDispatch::SetDefaultForeground(const DWORD color) noexcept
try
{
    return _terminalApi.SetDefaultForeground(color);
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return false;
}

// Method Description:
// - Sets the default background color to a new value
// Arguments:
// - color: The new RGB color value to use, in 0x00BBGGRR form
// Return Value:
// True if handled successfully. False otherwise.
bool TerminalDispatch::SetDefaultBackground(const DWORD color) noexcept
try
{
    return _terminalApi.SetDefaultBackground(color);
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return false;
}

// Method Description:
// - Erases characters in the buffer depending on the erase type
// Arguments:
// - eraseType: the erase type (from beginning, to end, or all)
// Return Value:
// True if handled successfully. False otherwise.
bool TerminalDispatch::EraseInLine(const DispatchTypes::EraseType eraseType) noexcept
try
{
    return _terminalApi.EraseInLine(eraseType);
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return false;
}

// Method Description:
// - Deletes count number of characters starting from where the cursor is currently
// Arguments:
// - count, the number of characters to delete
// Return Value:
// True if handled successfully. False otherwise.
bool TerminalDispatch::DeleteCharacter(const size_t count) noexcept
try
{
    return _terminalApi.DeleteCharacter(count);
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return false;
}

// Method Description:
// - Adds count number of spaces starting from where the cursor is currently
// Arguments:
// - count, the number of spaces to add
// Return Value:
// True if handled successfully, false otherwise
bool TerminalDispatch::InsertCharacter(const size_t count) noexcept
try
{
    return _terminalApi.InsertCharacter(count);
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return false;
}

// Method Description:
// - Moves the viewport and erases text from the buffer depending on the eraseType
// Arguments:
// - eraseType: the desired erase type
// Return Value:
// True if handled successfully. False otherwise
bool TerminalDispatch::EraseInDisplay(const DispatchTypes::EraseType eraseType) noexcept
try
{
    return _terminalApi.EraseInDisplay(eraseType);
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return false;
}
