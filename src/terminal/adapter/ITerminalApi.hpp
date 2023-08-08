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
        ITerminalApi() = default;
        ITerminalApi(const ITerminalApi&) = delete;
        ITerminalApi(ITerminalApi&&) = delete;
        ITerminalApi& operator=(const ITerminalApi&) = delete;
        ITerminalApi& operator=(ITerminalApi&&) = delete;

        virtual void ReturnResponse(const std::wstring_view response) = 0;

        virtual StateMachine& GetStateMachine() = 0;
        virtual TextBuffer& GetTextBuffer() = 0;
        virtual til::rect GetViewport() const = 0;
        virtual void SetViewportPosition(const til::point position) = 0;

        virtual bool IsVtInputEnabled() const = 0;

        virtual void SetTextAttributes(const TextAttribute& attrs) = 0;

        enum class Mode : size_t
        {
            AutoWrap,
            LineFeed,
            BracketedPaste
        };

        virtual void SetSystemMode(const Mode mode, const bool enabled) = 0;
        virtual bool GetSystemMode(const Mode mode) const = 0;

        virtual void WarningBell() = 0;
        virtual void SetWindowTitle(const std::wstring_view title) = 0;
        virtual void UseAlternateScreenBuffer(const TextAttribute& attrs) = 0;
        virtual void UseMainScreenBuffer() = 0;

        virtual CursorType GetUserDefaultCursorStyle() const = 0;

        virtual void ShowWindow(bool showOrHide) = 0;

        virtual void SetConsoleOutputCP(const unsigned int codepage) = 0;
        virtual unsigned int GetConsoleOutputCP() const = 0;

        virtual void CopyToClipboard(const std::wstring_view content) = 0;
        virtual void SetTaskbarProgress(const DispatchTypes::TaskbarState state, const size_t progress) = 0;
        virtual void SetWorkingDirectory(const std::wstring_view uri) = 0;
        virtual void PlayMidiNote(const int noteNumber, const int velocity, const std::chrono::microseconds duration) = 0;

        virtual bool ResizeWindow(const til::CoordType width, const til::CoordType height) = 0;
        virtual bool IsConsolePty() const = 0;

        virtual void NotifyAccessibilityChange(const til::rect& changedRect) = 0;
        virtual void NotifyBufferRotation(const int delta) = 0;

        virtual void MarkPrompt(const ScrollMark& mark) = 0;
        virtual void MarkCommandStart() = 0;
        virtual void MarkOutputStart() = 0;
        virtual void MarkCommandFinish(std::optional<unsigned int> error) = 0;

        virtual void InvokeCompletions(std::wstring_view menuJson, unsigned int replaceLength) = 0;
    };
}
