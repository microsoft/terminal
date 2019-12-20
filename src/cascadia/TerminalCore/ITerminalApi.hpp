// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#pragma once

#include "../../terminal/adapter/DispatchTypes.hpp"

namespace Microsoft::Terminal::Core
{
    class ITerminalApi
    {
    public:
        virtual ~ITerminalApi() {}
        ITerminalApi(const ITerminalApi&) = default;
        ITerminalApi(ITerminalApi&&) = default;
        ITerminalApi& operator=(const ITerminalApi&) = default;
        ITerminalApi& operator=(ITerminalApi&&) = default;

        virtual bool PrintString(std::wstring_view string) = 0;
        virtual bool ExecuteChar(wchar_t wch) = 0;

        virtual bool SetTextToDefaults(bool foreground, bool background) = 0;
        virtual bool SetTextForegroundIndex(BYTE colorIndex) = 0;
        virtual bool SetTextBackgroundIndex(BYTE colorIndex) = 0;
        virtual bool SetTextRgbColor(COLORREF color, bool foreground) = 0;
        virtual bool BoldText(bool boldOn) = 0;
        virtual bool UnderlineText(bool underlineOn) = 0;
        virtual bool ReverseText(bool reversed) = 0;

        virtual bool SetCursorPosition(short x, short y) = 0;
        virtual COORD GetCursorPosition() = 0;

        virtual bool DeleteCharacter(const size_t count) = 0;
        virtual bool InsertCharacter(const size_t count) = 0;
        virtual bool EraseCharacters(const size_t numChars) = 0;
        virtual bool EraseInLine(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::EraseType eraseType) = 0;
        virtual bool EraseInDisplay(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::EraseType eraseType) = 0;

        virtual bool SetWindowTitle(std::wstring_view title) = 0;

        virtual bool SetColorTableEntry(const size_t tableIndex, const DWORD color) = 0;

        virtual bool SetCursorStyle(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::CursorStyle cursorStyle) = 0;

        virtual bool SetDefaultForeground(const DWORD color) = 0;
        virtual bool SetDefaultBackground(const DWORD color) = 0;

    protected:
        ITerminalApi() = default;
    };
}
