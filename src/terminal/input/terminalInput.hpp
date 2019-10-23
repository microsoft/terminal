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
        void ChangeKeypadMode(const bool fApplicationMode);
        void ChangeCursorKeysMode(const bool fApplicationMode);

    private:
        std::function<void(std::deque<std::unique_ptr<IInputEvent>>&)> _pfnWriteEvents;

        // storage location for the leading surrogate of a utf-16 surrogate pair
        std::optional<wchar_t> _leadingSurrogate;

        bool _fKeypadApplicationMode = false;
        bool _fCursorApplicationMode = false;

        void _SendNullInputSequence(const DWORD dwControlKeyState) const;
        void _SendInputSequence(_In_ const std::wstring_view sequence) const;
        void _SendEscapedInputSequence(const wchar_t wch) const;

        struct _TermKeyMap
        {
            WORD const wVirtualKey;
            PCWSTR const pwszSequence;
            DWORD const dwModifiers;

            static const size_t s_cchMaxSequenceLength;

            _TermKeyMap(const WORD wVirtualKey, _In_ PCWSTR const pwszSequence) noexcept :
                wVirtualKey(wVirtualKey),
                pwszSequence(pwszSequence),
                dwModifiers(0){};

            _TermKeyMap(const WORD wVirtualKey, const DWORD dwModifiers, _In_ PCWSTR const pwszSequence) noexcept :
                wVirtualKey(wVirtualKey),
                pwszSequence(pwszSequence),
                dwModifiers(dwModifiers){};

            // C++11 syntax for prohibiting assignment
            // We can't assign, everything here is const.
            // We also shouldn't need to, this is only for a specific table.
            _TermKeyMap(const _TermKeyMap&) = delete;
            _TermKeyMap& operator=(const _TermKeyMap&) = delete;

            _TermKeyMap(_TermKeyMap&&) = delete;
            _TermKeyMap& operator=(_TermKeyMap&&) = delete;

            _TermKeyMap() = delete;
            ~_TermKeyMap() = default;
        };

        static const _TermKeyMap s_rgCursorKeysNormalMapping[];
        static const _TermKeyMap s_rgCursorKeysApplicationMapping[];
        static const _TermKeyMap s_rgKeypadNumericMapping[];
        static const _TermKeyMap s_rgKeypadApplicationMapping[];
        static const _TermKeyMap s_rgModifierKeyMapping[];
        static const _TermKeyMap s_rgSimpleModifedKeyMapping[];

        static const size_t s_cCursorKeysNormalMapping;
        static const size_t s_cCursorKeysApplicationMapping;
        static const size_t s_cKeypadNumericMapping;
        static const size_t s_cKeypadApplicationMapping;
        static const size_t s_cModifierKeyMapping;
        static const size_t s_cSimpleModifedKeyMapping;

        bool _SearchKeyMapping(const KeyEvent& keyEvent,
                               _In_reads_(cKeyMapping) const TerminalInput::_TermKeyMap* keyMapping,
                               const size_t cKeyMapping,
                               _Out_ const TerminalInput::_TermKeyMap** pMatchingMapping) const;

        bool _TranslateDefaultMapping(const KeyEvent& keyEvent,
                                      _In_reads_(cKeyMapping) const TerminalInput::_TermKeyMap* keyMapping,
                                      const size_t cKeyMapping) const;

        bool _SearchWithModifier(const KeyEvent& keyEvent) const;

        const size_t GetKeyMappingLength(const KeyEvent& keyEvent) const;
        const _TermKeyMap* GetKeyMapping(const KeyEvent& keyEvent) const;
    };
}
