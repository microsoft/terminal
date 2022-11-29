/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- MacrosBuffer.hpp

Abstract:
- This manages the parsing and storage of macros defined by the DECDMAC control sequence.
--*/

#pragma once

#include "DispatchTypes.hpp"
#include <array>
#include <bitset>
#include <string>

// fwdecl unittest classes
#ifdef UNIT_TESTING
class AdapterTest;
#endif

namespace Microsoft::Console::VirtualTerminal
{
    class StateMachine;

    class MacroBuffer
    {
    public:
        // The original DEC terminals only supported 6K of memory, which is
        // probably a bit low for modern usage. But we also don't want to make
        // this value too large, otherwise it could be used in a denial-of-
        // service attack. So for now this is probably a sufficient limit, but
        // we may need to increase it in the future if we intend to support
        // macros containing sixel sequences.
        static constexpr size_t MAX_SPACE = 0x40000;

        MacroBuffer() = default;
        ~MacroBuffer() = default;

        size_t GetSpaceAvailable() const noexcept;
        uint16_t CalculateChecksum() const noexcept;
        void InvokeMacro(const size_t macroId, StateMachine& stateMachine);
        void ClearMacrosIfInUse();
        bool InitParser(const size_t macroId, const DispatchTypes::MacroDeleteControl deleteControl, const DispatchTypes::MacroEncoding encoding);
        bool ParseDefinition(const wchar_t ch);

    private:
        bool _decodeHexDigit(const wchar_t ch) noexcept;
        bool _appendToActiveMacro(const wchar_t ch);
        std::wstring& _activeMacro();
        void _deleteMacro(std::wstring& macro) noexcept;
        bool _applyPendingRepeat();

        enum class State
        {
            ExpectingText,
            ExpectingHexDigit,
            ExpectingSecondHexDigit,
            ExpectingRepeatCount
        };

        State _parseState{ State::ExpectingText };
        wchar_t _decodedChar{ 0 };
        bool _repeatPending{ false };
        size_t _repeatCount{ 0 };
        size_t _repeatStart{ 0 };
        std::array<std::wstring, 64> _macros;
        size_t _activeMacroId{ 0 };
        size_t _spaceUsed{ 0 };
        size_t _invokedDepth{ 0 };
        size_t _invokedSequenceLength{ 0 };

#ifdef UNIT_TESTING
        friend class AdapterTest;
#endif
    };
}
