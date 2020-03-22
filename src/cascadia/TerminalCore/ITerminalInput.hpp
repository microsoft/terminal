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

        virtual bool SendKeyEvent(const WORD vkey, const WORD scanCode, const ControlKeyStates states) = 0;
        virtual bool SendMouseEvent(const COORD viewportPos, const unsigned int uiButton, const ControlKeyStates states, const short wheelDelta) = 0;
        virtual bool SendCharEvent(const wchar_t ch) = 0;

        // void SendMouseEvent(uint row, uint col, KeyModifiers modifiers);
        [[nodiscard]] virtual HRESULT UserResize(const COORD size) noexcept = 0;
        virtual void UserScrollViewport(const int viewTop) = 0;
        virtual int GetScrollOffset() = 0;

        virtual void TrySnapOnInput() = 0;

    protected:
        ITerminalInput() = default;
    };
}
