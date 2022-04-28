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

        virtual void PrintString(std::wstring_view string) = 0;

        virtual bool ReturnResponse(std::wstring_view responseString) = 0;

        virtual TextAttribute GetTextAttributes() const = 0;
        virtual void SetTextAttributes(const TextAttribute& attrs) = 0;

        virtual Microsoft::Console::Types::Viewport GetBufferSize() = 0;
        virtual void SetCursorPosition(til::point pos) = 0;
        virtual til::point GetCursorPosition() = 0;
        virtual void SetCursorVisibility(const bool visible) = 0;
        virtual void CursorLineFeed(const bool withReturn) = 0;
        virtual void EnableCursorBlinking(const bool enable) = 0;

        virtual void DeleteCharacter(const til::CoordType count) = 0;
        virtual void InsertCharacter(const til::CoordType count) = 0;
        virtual void EraseCharacters(const til::CoordType numChars) = 0;
        virtual bool EraseInLine(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::EraseType eraseType) = 0;
        virtual bool EraseInDisplay(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::EraseType eraseType) = 0;

        virtual void WarningBell() = 0;
        virtual void SetWindowTitle(std::wstring_view title) = 0;

        virtual COLORREF GetColorTableEntry(const size_t tableIndex) const = 0;
        virtual void SetColorTableEntry(const size_t tableIndex, const COLORREF color) = 0;
        virtual void SetColorAliasIndex(const ColorAlias alias, const size_t tableIndex) = 0;

        virtual void SetCursorStyle(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::CursorStyle cursorStyle) = 0;

        virtual void SetInputMode(const ::Microsoft::Console::VirtualTerminal::TerminalInput::Mode mode, const bool enabled) = 0;
        virtual void SetRenderMode(const ::Microsoft::Console::Render::RenderSettings::Mode mode, const bool enabled) = 0;

        virtual void EnableXtermBracketedPasteMode(const bool enabled) = 0;
        virtual bool IsXtermBracketedPasteModeEnabled() const = 0;

        virtual bool IsVtInputEnabled() const = 0;

        virtual void CopyToClipboard(std::wstring_view content) = 0;

        virtual void AddHyperlink(std::wstring_view uri, std::wstring_view params) = 0;
        virtual void EndHyperlink() = 0;

        virtual void SetTaskbarProgress(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::TaskbarState state, const size_t progress) = 0;

        virtual void SetWorkingDirectory(std::wstring_view uri) = 0;
        virtual std::wstring_view GetWorkingDirectory() = 0;

        virtual void PushGraphicsRendition(const ::Microsoft::Console::VirtualTerminal::VTParameters options) = 0;
        virtual void PopGraphicsRendition() = 0;

        virtual void ShowWindow(bool showOrHide) = 0;

        virtual void UseAlternateScreenBuffer() = 0;
        virtual void UseMainScreenBuffer() = 0;

    protected:
        ITerminalApi() = default;
    };
}
