// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#pragma once

#include "../../terminal/adapter/DispatchTypes.hpp"
#include "../../terminal/input/terminalInput.hpp"
#include "../../buffer/out/TextAttribute.hpp"
#include "../../renderer/inc/RenderSettings.hpp"
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

        virtual bool PrintString(std::wstring_view string) = 0;
        virtual bool ExecuteChar(wchar_t wch) = 0;

        virtual TextAttribute GetTextAttributes() const = 0;
        virtual void SetTextAttributes(const TextAttribute& attrs) = 0;

        virtual Microsoft::Console::Types::Viewport GetBufferSize() = 0;
        virtual bool SetCursorPosition(short x, short y) = 0;
        virtual COORD GetCursorPosition() = 0;
        virtual bool SetCursorVisibility(const bool visible) = 0;
        virtual bool CursorLineFeed(const bool withReturn) = 0;
        virtual bool EnableCursorBlinking(const bool enable) = 0;

        virtual bool DeleteCharacter(const size_t count) = 0;
        virtual bool InsertCharacter(const size_t count) = 0;
        virtual bool EraseCharacters(const size_t numChars) = 0;
        virtual bool EraseInLine(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::EraseType eraseType) = 0;
        virtual bool EraseInDisplay(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::EraseType eraseType) = 0;

        virtual bool WarningBell() = 0;
        virtual bool SetWindowTitle(std::wstring_view title) = 0;

        virtual COLORREF GetColorTableEntry(const size_t tableIndex) const = 0;
        virtual bool SetColorTableEntry(const size_t tableIndex, const COLORREF color) = 0;
        virtual void SetColorAliasIndex(const ColorAlias alias, const size_t tableIndex) = 0;

        virtual bool SetCursorStyle(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::CursorStyle cursorStyle) = 0;

        virtual bool SetInputMode(const ::Microsoft::Console::VirtualTerminal::TerminalInput::Mode mode, const bool enabled) = 0;
        virtual bool SetRenderMode(const ::Microsoft::Console::Render::RenderSettings::Mode mode, const bool enabled) = 0;

        virtual bool EnableXtermBracketedPasteMode(const bool enabled) = 0;
        virtual bool IsXtermBracketedPasteModeEnabled() const = 0;

        virtual bool IsVtInputEnabled() const = 0;

        virtual bool CopyToClipboard(std::wstring_view content) = 0;

        virtual bool AddHyperlink(std::wstring_view uri, std::wstring_view params) = 0;
        virtual bool EndHyperlink() = 0;

        virtual bool SetTaskbarProgress(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::TaskbarState state, const size_t progress) = 0;

        virtual bool SetWorkingDirectory(std::wstring_view uri) = 0;
        virtual std::wstring_view GetWorkingDirectory() = 0;

        virtual bool PushGraphicsRendition(const ::Microsoft::Console::VirtualTerminal::VTParameters options) = 0;
        virtual bool PopGraphicsRendition() = 0;

    protected:
        ITerminalApi() = default;
    };
}
