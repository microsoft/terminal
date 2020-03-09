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

void TerminalDispatch::Execute(const wchar_t wchControl) noexcept
{
    _terminalApi.ExecuteChar(wchControl);
}

void TerminalDispatch::Print(const wchar_t wchPrintable) noexcept
{
    _terminalApi.PrintString({ &wchPrintable, 1 });
}

void TerminalDispatch::PrintString(const std::wstring_view string) noexcept
{
    _terminalApi.PrintString(string);
}

bool TerminalDispatch::CursorPosition(const size_t line,
                                      const size_t column) noexcept
try
{
    SHORT x{ 0 };
    SHORT y{ 0 };

    RETURN_BOOL_IF_FALSE(SUCCEEDED(SizeTToShort(column, &x)) &&
                         SUCCEEDED(SizeTToShort(line, &y)));

    RETURN_BOOL_IF_FALSE(SUCCEEDED(ShortSub(x, 1, &x)) &&
                         SUCCEEDED(ShortSub(y, 1, &y)));

    return _terminalApi.SetCursorPosition(x, y);
}
CATCH_LOG_RETURN_FALSE()

bool TerminalDispatch::CursorForward(const size_t distance) noexcept
try
{
    const auto cursorPos = _terminalApi.GetCursorPosition();
    const COORD newCursorPos{ cursorPos.X + gsl::narrow<short>(distance), cursorPos.Y };
    return _terminalApi.SetCursorPosition(newCursorPos.X, newCursorPos.Y);
}
CATCH_LOG_RETURN_FALSE()

bool TerminalDispatch::CursorBackward(const size_t distance) noexcept
try
{
    const auto cursorPos = _terminalApi.GetCursorPosition();
    const COORD newCursorPos{ cursorPos.X - gsl::narrow<short>(distance), cursorPos.Y };
    return _terminalApi.SetCursorPosition(newCursorPos.X, newCursorPos.Y);
}
CATCH_LOG_RETURN_FALSE()

bool TerminalDispatch::CursorUp(const size_t distance) noexcept
try
{
    const auto cursorPos = _terminalApi.GetCursorPosition();
    const COORD newCursorPos{ cursorPos.X, cursorPos.Y + gsl::narrow<short>(distance) };
    return _terminalApi.SetCursorPosition(newCursorPos.X, newCursorPos.Y);
}
CATCH_LOG_RETURN_FALSE()

bool TerminalDispatch::LineFeed(const DispatchTypes::LineFeedType lineFeedType) noexcept
try
{
    switch (lineFeedType)
    {
    case DispatchTypes::LineFeedType::DependsOnMode:
        // There is currently no need for mode-specific line feeds in the Terminal,
        // so for now we just treat them as a line feed without carriage return.
    case DispatchTypes::LineFeedType::WithoutReturn:
        return _terminalApi.CursorLineFeed(false);
    case DispatchTypes::LineFeedType::WithReturn:
        return _terminalApi.CursorLineFeed(true);
    default:
        return false;
    }
}
CATCH_LOG_RETURN_FALSE()

bool TerminalDispatch::EraseCharacters(const size_t numChars) noexcept
try
{
    return _terminalApi.EraseCharacters(numChars);
}
CATCH_LOG_RETURN_FALSE()

bool TerminalDispatch::CarriageReturn() noexcept
try
{
    const auto cursorPos = _terminalApi.GetCursorPosition();
    return _terminalApi.SetCursorPosition(0, cursorPos.Y);
}
CATCH_LOG_RETURN_FALSE()

bool TerminalDispatch::SetWindowTitle(std::wstring_view title) noexcept
try
{
    return _terminalApi.SetWindowTitle(title);
}
CATCH_LOG_RETURN_FALSE()

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
CATCH_LOG_RETURN_FALSE()

bool TerminalDispatch::SetCursorStyle(const DispatchTypes::CursorStyle cursorStyle) noexcept
try
{
    return _terminalApi.SetCursorStyle(cursorStyle);
}
CATCH_LOG_RETURN_FALSE()

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
CATCH_LOG_RETURN_FALSE()

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
CATCH_LOG_RETURN_FALSE()

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
CATCH_LOG_RETURN_FALSE()

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
CATCH_LOG_RETURN_FALSE()

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
CATCH_LOG_RETURN_FALSE()

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
CATCH_LOG_RETURN_FALSE()
