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

        virtual bool PrintString(std::wstring_view string) noexcept = 0;
        virtual bool ExecuteChar(wchar_t wch) noexcept = 0;

        virtual bool SetTextToDefaults(bool foreground, bool background) noexcept = 0;
        virtual bool SetTextForegroundIndex(BYTE colorIndex) noexcept = 0;
        virtual bool SetTextBackgroundIndex(BYTE colorIndex) noexcept = 0;
        virtual bool SetTextRgbColor(COLORREF color, bool foreground) noexcept = 0;
        virtual bool BoldText(bool boldOn) noexcept = 0;
        virtual bool UnderlineText(bool underlineOn) noexcept = 0;
        virtual bool ReverseText(bool reversed) noexcept = 0;

        virtual bool SetCursorPosition(short x, short y) noexcept = 0;
        virtual COORD GetCursorPosition() noexcept = 0;
        virtual bool CursorLineFeed(const bool withReturn) noexcept = 0;

        virtual bool DeleteCharacter(const size_t count) noexcept = 0;
        virtual bool InsertCharacter(const size_t count) noexcept = 0;
        virtual bool EraseCharacters(const size_t numChars) noexcept = 0;
        virtual bool EraseInLine(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::EraseType eraseType) noexcept = 0;
        virtual bool EraseInDisplay(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::EraseType eraseType) noexcept = 0;

        virtual bool SetWindowTitle(std::wstring_view title) noexcept = 0;

        virtual bool SetColorTableEntry(const size_t tableIndex, const DWORD color) noexcept = 0;

        virtual bool SetCursorStyle(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::CursorStyle cursorStyle) noexcept = 0;

        virtual bool SetDefaultForeground(const DWORD color) noexcept = 0;
        virtual bool SetDefaultBackground(const DWORD color) noexcept = 0;

    protected:
        ITerminalApi() = default;
    };
}
