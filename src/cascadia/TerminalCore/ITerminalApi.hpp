// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#pragma once

#include "../../terminal/adapter/DispatchTypes.hpp"
#include "../../buffer/out/TextAttribute.hpp"
#include "../../types/inc/Viewport.hpp"

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

        virtual TextAttribute GetTextAttributes() const noexcept = 0;
        virtual void SetTextAttributes(const TextAttribute& attrs) noexcept = 0;

        virtual Microsoft::Console::Types::Viewport GetBufferSize() noexcept = 0;
        virtual bool SetCursorPosition(short x, short y) noexcept = 0;
        virtual COORD GetCursorPosition() noexcept = 0;
        virtual bool SetCursorVisibility(const bool visible) noexcept = 0;
        virtual bool CursorLineFeed(const bool withReturn) noexcept = 0;
        virtual bool EnableCursorBlinking(const bool enable) noexcept = 0;

        virtual bool DeleteCharacter(const size_t count) noexcept = 0;
        virtual bool InsertCharacter(const size_t count) noexcept = 0;
        virtual bool EraseCharacters(const size_t numChars) noexcept = 0;
        virtual bool EraseInLine(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::EraseType eraseType) noexcept = 0;
        virtual bool EraseInDisplay(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::EraseType eraseType) noexcept = 0;

        virtual bool WarningBell() noexcept = 0;
        virtual bool SetWindowTitle(std::wstring_view title) noexcept = 0;

        virtual bool SetColorTableEntry(const size_t tableIndex, const DWORD color) noexcept = 0;

        virtual bool SetCursorStyle(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::CursorStyle cursorStyle) noexcept = 0;
        virtual bool SetCursorColor(const DWORD color) noexcept = 0;

        virtual bool SetDefaultForeground(const DWORD color) noexcept = 0;
        virtual bool SetDefaultBackground(const DWORD color) noexcept = 0;

        virtual bool EnableWin32InputMode(const bool win32InputMode) noexcept = 0;
        virtual bool SetCursorKeysMode(const bool applicationMode) noexcept = 0;
        virtual bool SetKeypadMode(const bool applicationMode) noexcept = 0;
        virtual bool SetScreenMode(const bool reverseMode) noexcept = 0;
        virtual bool EnableVT200MouseMode(const bool enabled) noexcept = 0;
        virtual bool EnableUTF8ExtendedMouseMode(const bool enabled) noexcept = 0;
        virtual bool EnableSGRExtendedMouseMode(const bool enabled) noexcept = 0;
        virtual bool EnableButtonEventMouseMode(const bool enabled) noexcept = 0;
        virtual bool EnableAnyEventMouseMode(const bool enabled) noexcept = 0;
        virtual bool EnableAlternateScrollMode(const bool enabled) noexcept = 0;
        virtual bool EnableXtermBracketedPasteMode(const bool enabled) noexcept = 0;
        virtual bool IsXtermBracketedPasteModeEnabled() const = 0;

        virtual bool IsVtInputEnabled() const = 0;

        virtual bool CopyToClipboard(std::wstring_view content) noexcept = 0;

        virtual bool AddHyperlink(std::wstring_view uri, std::wstring_view params) noexcept = 0;
        virtual bool EndHyperlink() noexcept = 0;

        virtual bool SetTaskbarProgress(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::TaskbarState state, const size_t progress) noexcept = 0;

        virtual bool SetWorkingDirectory(std::wstring_view uri) noexcept = 0;
        virtual std::wstring_view GetWorkingDirectory() noexcept = 0;

        virtual bool PushGraphicsRendition(const ::Microsoft::Console::VirtualTerminal::VTParameters options) noexcept = 0;
        virtual bool PopGraphicsRendition() noexcept = 0;

    protected:
        ITerminalApi() = default;
    };
}
