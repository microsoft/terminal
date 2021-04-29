// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ControlKeyStates.hpp"

namespace Microsoft::Terminal::Core
{
    class ITerminalInput
    {
    public:
        virtual ~ITerminalInput() {}
        ITerminalInput(const ITerminalInput&) = default;
        ITerminalInput(ITerminalInput&&) = default;
        ITerminalInput& operator=(const ITerminalInput&) = default;
        ITerminalInput& operator=(ITerminalInput&&) = default;

        virtual bool SendKeyEvent(const WORD vkey, const WORD scanCode, const ControlKeyStates states, const bool keyDown) = 0;
        virtual bool SendMouseEvent(const COORD viewportPos, const unsigned int uiButton, const ControlKeyStates states, const short wheelDelta, const Microsoft::Console::VirtualTerminal::TerminalInput::MouseButtonState state) = 0;
        virtual bool SendCharEvent(const wchar_t ch, const WORD scanCode, const ControlKeyStates states) = 0;

        [[nodiscard]] virtual HRESULT UserResize(const COORD size) noexcept = 0;
        virtual void UserScrollViewport(const int viewTop) = 0;
        virtual int GetScrollOffset() = 0;

        virtual void TrySnapOnInput() = 0;

    protected:
        ITerminalInput() = default;
    };
}
