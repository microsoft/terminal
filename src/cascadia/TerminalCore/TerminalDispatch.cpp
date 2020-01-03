// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TerminalDispatch.hpp"
using namespace ::Microsoft::Terminal::Core;
using namespace ::Microsoft::Console::VirtualTerminal;

// NOTE:
// Functions related to Set Graphics Renditions (SGR) are in
//      TerminalDispatchGraphics.cpp, not this file

TerminalDispatch::TerminalDispatch(ITerminalApi& terminalApi) :
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
{
    const auto columnInBufferSpace = column - 1;
    const auto lineInBufferSpace = line - 1;
    short x = static_cast<short>(column - 1);
    short y = static_cast<short>(line - 1);
    return _terminalApi.SetCursorPosition(x, y);
}

bool TerminalDispatch::CursorForward(const size_t distance) noexcept
{
    const auto cursorPos = _terminalApi.GetCursorPosition();
    const COORD newCursorPos{ cursorPos.X + gsl::narrow<short>(distance), cursorPos.Y };
    return _terminalApi.SetCursorPosition(newCursorPos.X, newCursorPos.Y);
}

bool TerminalDispatch::CursorBackward(const size_t distance) noexcept
{
    const auto cursorPos = _terminalApi.GetCursorPosition();
    const COORD newCursorPos{ cursorPos.X - gsl::narrow<short>(distance), cursorPos.Y };
    return _terminalApi.SetCursorPosition(newCursorPos.X, newCursorPos.Y);
}

bool TerminalDispatch::CursorUp(const size_t distance) noexcept
{
    const auto cursorPos = _terminalApi.GetCursorPosition();
    const COORD newCursorPos{ cursorPos.X, cursorPos.Y + gsl::narrow<short>(distance) };
    return _terminalApi.SetCursorPosition(newCursorPos.X, newCursorPos.Y);
}

bool TerminalDispatch::EraseCharacters(const size_t numChars) noexcept
{
    return _terminalApi.EraseCharacters(numChars);
}

bool TerminalDispatch::SetWindowTitle(std::wstring_view title) noexcept
{
    return _terminalApi.SetWindowTitle(title);
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
{
    return _terminalApi.SetColorTableEntry(tableIndex, color);
}

bool TerminalDispatch::SetCursorStyle(const DispatchTypes::CursorStyle cursorStyle) noexcept
{
    return _terminalApi.SetCursorStyle(cursorStyle);
}

// Method Description:
// - Sets the default foreground color to a new value
// Arguments:
// - color: The new RGB color value to use, in 0x00BBGGRR form
// Return Value:
// True if handled successfully. False otherwise.
bool TerminalDispatch::SetDefaultForeground(const DWORD color) noexcept
{
    return _terminalApi.SetDefaultForeground(color);
}

// Method Description:
// - Sets the default background color to a new value
// Arguments:
// - color: The new RGB color value to use, in 0x00BBGGRR form
// Return Value:
// True if handled successfully. False otherwise.
bool TerminalDispatch::SetDefaultBackground(const DWORD color) noexcept
{
    return _terminalApi.SetDefaultBackground(color);
}

// Method Description:
// - Erases characters in the buffer depending on the erase type
// Arguments:
// - eraseType: the erase type (from beginning, to end, or all)
// Return Value:
// True if handled successfully. False otherwise.
bool TerminalDispatch::EraseInLine(const DispatchTypes::EraseType eraseType) noexcept
{
    return _terminalApi.EraseInLine(eraseType);
}

// Method Description:
// - Deletes count number of characters starting from where the cursor is currently
// Arguments:
// - count, the number of characters to delete
// Return Value:
// True if handled successfully. False otherwise.
bool TerminalDispatch::DeleteCharacter(const size_t count) noexcept
{
    return _terminalApi.DeleteCharacter(count);
}

// Method Description:
// - Adds count number of spaces starting from where the cursor is currently
// Arguments:
// - count, the number of spaces to add
// Return Value:
// True if handled successfully, false otherwise
bool TerminalDispatch::InsertCharacter(const size_t count) noexcept
{
    return _terminalApi.InsertCharacter(count);
}

// Method Description:
// - Moves the viewport and erases text from the buffer depending on the eraseType
// Arguments:
// - eraseType: the desired erase type
// Return Value:
// True if handled successfully. False otherwise
bool TerminalDispatch::EraseInDisplay(const DispatchTypes::EraseType eraseType) noexcept
{
    return _terminalApi.EraseInDisplay(eraseType);
}
