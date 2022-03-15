/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ITerminalApi.hpp

Abstract:
- This serves as an abstraction layer for the dispatch class to connect to conhost/terminal API functions.

Author(s):
- Michael Niksa (MiNiksa) 30-July-2015
--*/

#pragma once

#include "../parser/stateMachine.hpp"
#include "../../types/inc/IInputEvent.hpp"
#include "../../buffer/out/LineRendition.hpp"
#include "../../buffer/out/textBuffer.hpp"
#include "../../renderer/inc/RenderSettings.hpp"

#include <deque>
#include <memory>

namespace Microsoft::Console::VirtualTerminal
{
    class ITerminalApi
    {
        using RenderSettings = Microsoft::Console::Render::RenderSettings;

    public:
        virtual ~ITerminalApi() = default;

        virtual void PrintString(const std::wstring_view string) = 0;

        virtual StateMachine& GetStateMachine() = 0;
        virtual TextBuffer& GetTextBuffer() = 0;
        virtual til::rect GetViewport() const = 0;
        virtual void SetViewportPosition(const til::point position) = 0;

        virtual bool IsVtInputEnabled() const = 0;

        virtual void SetTextAttributes(const TextAttribute& attrs) = 0;

        virtual void WriteInput(std::deque<std::unique_ptr<IInputEvent>>& events, size_t& eventsWritten) = 0;

        virtual void SetAutoWrapMode(const bool wrapAtEOL) = 0;

        virtual void SetScrollingRegion(const til::inclusive_rect& scrollMargins) = 0;
        virtual void WarningBell() = 0;
        virtual bool GetLineFeedMode() const = 0;
        virtual void LineFeed(const bool withReturn) = 0;
        virtual void SetWindowTitle(const std::wstring_view title) = 0;
        virtual void UseAlternateScreenBuffer() = 0;
        virtual void UseMainScreenBuffer() = 0;

        virtual CursorType GetUserDefaultCursorStyle() const = 0;

        virtual void ShowWindow(bool showOrHide) = 0;

        virtual void SetConsoleOutputCP(const unsigned int codepage) = 0;
        virtual unsigned int GetConsoleOutputCP() const = 0;

        virtual bool ResizeWindow(const size_t width, const size_t height) = 0;
        virtual bool IsConsolePty() const = 0;

        virtual void NotifyAccessibilityChange(const til::rect& changedRect) = 0;

        virtual void ReparentWindow(const uint64_t handle) = 0;
    };
}
