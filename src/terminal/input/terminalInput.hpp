/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- terminalInput.hpp

Abstract:
- This serves as an adapter between virtual key input from a user and the virtual terminal sequences that are
  typically emitted by an xterm-compatible console.

Author(s):
- Michael Niksa (MiNiksa) 30-Oct-2015
--*/

#include <functional>
#include "../../types/inc/IInputEvent.hpp"
#pragma once

namespace Microsoft::Console::VirtualTerminal
{
    class TerminalInput final
    {
    public:
        TerminalInput(_In_ std::function<void(std::deque<std::unique_ptr<IInputEvent>>&)> pfn);

        TerminalInput() = delete;
        TerminalInput(const TerminalInput& old) = default;
        TerminalInput(TerminalInput&& moved) = default;

        TerminalInput& operator=(const TerminalInput& old) = default;
        TerminalInput& operator=(TerminalInput&& moved) = default;

        ~TerminalInput() = default;

        bool HandleKey(const IInputEvent* const pInEvent) const;
        bool HandleChar(const wchar_t ch);
        void ChangeKeypadMode(const bool applicationMode) noexcept;
        void ChangeCursorKeysMode(const bool applicationMode) noexcept;

    private:
        std::function<void(std::deque<std::unique_ptr<IInputEvent>>&)> _pfnWriteEvents;

        // storage location for the leading surrogate of a utf-16 surrogate pair
        std::optional<wchar_t> _leadingSurrogate;

        bool _keypadApplicationMode = false;
        bool _cursorApplicationMode = false;

        void _SendNullInputSequence(const DWORD dwControlKeyState) const;
        void _SendInputSequence(const std::wstring_view sequence) const;
        void _SendEscapedInputSequence(const wchar_t wch) const;
    };
}
