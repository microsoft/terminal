// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "MacroBuffer.hpp"
#include "../parser/ascii.hpp"
#include "../parser/stateMachine.hpp"

using namespace Microsoft::Console::VirtualTerminal;

size_t MacroBuffer::GetSpaceAvailable() const noexcept
{
    return MAX_SPACE - _spaceUsed;
}

uint16_t MacroBuffer::CalculateChecksum() const noexcept
{
    // The algorithm that we're using here is intended to match the checksums
    // produced by the original DEC VT420 terminal. Although note that a real
    // VT420 would have included the entire macro memory area in the checksum,
    // which could still contain remnants of previous macro definitions that
    // are no longer active. We don't replicate that behavior, since that's of
    // no benefit to applications that might want to use the checksum.
    uint16_t checksum = 0;
    for (auto& macro : _macros)
    {
        for (auto ch : macro)
        {
            checksum -= ch;
        }
    }
    return checksum;
}

void MacroBuffer::InvokeMacro(const size_t macroId, StateMachine& stateMachine)
{
    if (macroId < _macros.size())
    {
        const auto& macroSequence = til::at(_macros, macroId);
        // Macros can invoke other macros up to a depth of 16, but we don't allow
        // the total sequence length to exceed the maximum buffer size, since that's
        // likely to facilitate a denial-of-service attack.
        const auto allowedLength = MAX_SPACE - _invokedSequenceLength;
        if (_invokedDepth < 16 && macroSequence.length() < allowedLength)
        {
            _invokedSequenceLength += macroSequence.length();
            _invokedDepth++;
            auto resetInvokeDepth = wil::scope_exit([&] {
                // Once the invoke depth reaches zero, we know we've reached the end
                // of the root invoke, so we can reset the sequence length tracker.
                if (--_invokedDepth == 0)
                {
                    _invokedSequenceLength = 0;
                }
            });
            stateMachine.ProcessString(macroSequence);
        }
    }
}

void MacroBuffer::ClearMacrosIfInUse()
{
    // If we receive an RIS from within a macro invocation, we can't release the
    // buffer because it's still being used. Instead we'll just replace all the
    // macro definitions with NUL characters to prevent any further output. The
    // buffer will eventually be released once the invocation finishes.
    if (_invokedDepth > 0)
    {
        for (auto& macro : _macros)
        {
            std::fill(macro.begin(), macro.end(), AsciiChars::NUL);
        }
    }
}

bool MacroBuffer::InitParser(const size_t macroId, const DispatchTypes::MacroDeleteControl deleteControl, const DispatchTypes::MacroEncoding encoding)
{
    // We're checking the invoked depth here to make sure we aren't defining
    // a macro from within a macro invocation.
    if (macroId < _macros.size() && _invokedDepth == 0)
    {
        _activeMacroId = macroId;
        _decodedChar = 0;
        _repeatPending = false;

        switch (encoding)
        {
        case DispatchTypes::MacroEncoding::HexPair:
            _parseState = State::ExpectingHexDigit;
            break;
        case DispatchTypes::MacroEncoding::Text:
            _parseState = State::ExpectingText;
            break;
        default:
            return false;
        }

        switch (deleteControl)
        {
        case DispatchTypes::MacroDeleteControl::DeleteId:
            _deleteMacro(_activeMacro());
            return true;
        case DispatchTypes::MacroDeleteControl::DeleteAll:
            for (auto& macro : _macros)
            {
                _deleteMacro(macro);
            }
            return true;
        default:
            return false;
        }
    }
    return false;
}

bool MacroBuffer::ParseDefinition(const wchar_t ch)
{
    // Once we receive an ESC, that marks the end of the definition, but if
    // an unterminated repeat is still pending, we should apply that now.
    if (ch == AsciiChars::ESC)
    {
        if (_repeatPending && !_applyPendingRepeat())
        {
            _deleteMacro(_activeMacro());
        }
        return false;
    }

    // Any other control characters are just ignored.
    if (ch < L' ')
    {
        return true;
    }

    // For "text encoded" macros, we'll always be in the ExpectingText state.
    // For "hex encoded" macros, we'll typically be alternating between the
    // ExpectingHexDigit and ExpectingSecondHexDigit states as we parse the two
    // digits of each hex pair. But we also need to deal with repeat sequences,
    // which start with `!`, followed by a numeric repeat count, and then a
    // range of hex pairs between two `;` characters. When parsing the repeat
    // count, we use the ExpectingRepeatCount state, but when parsing the hex
    // pairs of the repeat, we just use the regular ExpectingHexDigit states.

    auto success = true;
    switch (_parseState)
    {
    case State::ExpectingText:
        success = _appendToActiveMacro(ch);
        break;
    case State::ExpectingHexDigit:
        if (_decodeHexDigit(ch))
        {
            _parseState = State::ExpectingSecondHexDigit;
        }
        else if (ch == L'!' && !_repeatPending)
        {
            _parseState = State::ExpectingRepeatCount;
            _repeatCount = 0;
        }
        else if (ch == L';' && _repeatPending)
        {
            success = _applyPendingRepeat();
        }
        else
        {
            success = false;
        }
        break;
    case State::ExpectingSecondHexDigit:
        success = _decodeHexDigit(ch) && _appendToActiveMacro(_decodedChar);
        _decodedChar = 0;
        _parseState = State::ExpectingHexDigit;
        break;
    case State::ExpectingRepeatCount:
        if (ch >= L'0' && ch <= L'9')
        {
            _repeatCount = _repeatCount * 10 + (ch - L'0');
            _repeatCount = std::min<size_t>(_repeatCount, MAX_PARAMETER_VALUE);
        }
        else if (ch == L';')
        {
            _repeatPending = true;
            _repeatStart = _activeMacro().length();
            _parseState = State::ExpectingHexDigit;
        }
        else
        {
            success = false;
        }
        break;
    default:
        success = false;
        break;
    }

    // If there is an error in the definition, clear everything received so far.
    if (!success)
    {
        _deleteMacro(_activeMacro());
    }
    return success;
}

bool MacroBuffer::_decodeHexDigit(const wchar_t ch) noexcept
{
    _decodedChar <<= 4;
    if (ch >= L'0' && ch <= L'9')
    {
        _decodedChar += (ch - L'0');
        return true;
    }
    else if (ch >= L'A' && ch <= L'F')
    {
        _decodedChar += (ch - L'A' + 10);
        return true;
    }
    else if (ch >= L'a' && ch <= L'f')
    {
        _decodedChar += (ch - L'a' + 10);
        return true;
    }
    return false;
}

bool MacroBuffer::_appendToActiveMacro(const wchar_t ch)
{
    if (GetSpaceAvailable() > 0)
    {
        _activeMacro().push_back(ch);
        _spaceUsed++;
        return true;
    }
    return false;
}

std::wstring& MacroBuffer::_activeMacro()
{
    return _macros.at(_activeMacroId);
}

void MacroBuffer::_deleteMacro(std::wstring& macro) noexcept
{
    _spaceUsed -= macro.length();
    std::wstring{}.swap(macro);
}

bool MacroBuffer::_applyPendingRepeat()
{
    if (_repeatCount > 1)
    {
        auto& activeMacro = _activeMacro();
        const auto sequenceLength = activeMacro.length() - _repeatStart;
        // Note that the repeat sequence has already been written to the buffer
        // once while it was being parsed, so we only need to append additional
        // copies for repeat counts that are greater than one. If there is not
        // enough space for the additional content, we'll just abort the macro.
        const auto spaceRequired = (_repeatCount - 1) * sequenceLength;
        if (spaceRequired > GetSpaceAvailable())
        {
            return false;
        }
        for (size_t i = 1; i < _repeatCount; i++)
        {
            activeMacro.append(activeMacro.substr(_repeatStart, sequenceLength));
            _spaceUsed += sequenceLength;
        }
    }
    _repeatPending = false;
    return true;
}
