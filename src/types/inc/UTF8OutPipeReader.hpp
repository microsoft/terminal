/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- UTF8OutPipeReader.hpp

Abstract:
- This reads a UTF-8 stream and gives back a buffer that contains complete code points only
- Partial UTF-8 code points at the end of the buffer read are cached and prepended to the next chunk read

Author(s):
- Steffen Illhardt (german-one) 12-July-2019
--*/

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <wil\common.h>
#include <wil\resource.h>
#include <string_view>

class UTF8OutPipeReader final
{
public:
    UTF8OutPipeReader(HANDLE outPipe) noexcept;
    [[nodiscard]] HRESULT Read(_Out_ std::string_view& strView);

private:
    enum _Utf8BitMasks : BYTE
    {
        IsAsciiByte = 0b0'0000000, // Any byte representing an ASCII character has the MSB set to 0
        MaskAsciiByte = 0b1'0000000, // Bit mask to be used in a bitwise AND operation to find out whether or not a byte match the IsAsciiByte pattern
        IsContinuationByte = 0b10'000000, // Continuation bytes of any UTF-8 non-ASCII character have the MSB set to 1 and the adjacent bit set to 0
        MaskContinuationByte = 0b11'000000, // Bit mask to be used in a bitwise AND operation to find out whether or not a byte match the IsContinuationByte pattern
        IsLeadByteTwoByteSequence = 0b110'00000, // A lead byte that indicates a UTF-8 non-ASCII character consisting of two bytes has the two highest bits set to 1 and the adjacent bit set to 0
        MaskLeadByteTwoByteSequence = 0b111'00000, // Bit mask to be used in a bitwise AND operation to find out whether or not a lead byte match the IsLeadByteTwoByteSequence pattern
        IsLeadByteThreeByteSequence = 0b1110'0000, // A lead byte that indicates a UTF-8 non-ASCII character consisting of three bytes has the three highest bits set to 1 and the adjacent bit set to 0
        MaskLeadByteThreeByteSequence = 0b1111'0000, // Bit mask to be used in a bitwise AND operation to find out whether or not a lead byte match the IsLeadByteThreeByteSequence pattern
        IsLeadByteFourByteSequence = 0b11110'000, // A lead byte that indicates a UTF-8 non-ASCII character consisting of four bytes has the four highest bits set to 1 and the adjacent bit set to 0
        MaskLeadByteFourByteSequence = 0b11111'000 // Bit mask to be used in a bitwise AND operation to find out whether or not a lead byte match the IsLeadByteFourByteSequence pattern
    };

    // array of bitmasks
    constexpr static std::array<BYTE, 4> _cmpMasks{
        0, // unused
        _Utf8BitMasks::MaskContinuationByte,
        _Utf8BitMasks::MaskLeadByteTwoByteSequence,
        _Utf8BitMasks::MaskLeadByteThreeByteSequence,
    };

    // array of values for the comparisons
    constexpr static std::array<BYTE, 4> _cmpOperands{
        0, // unused
        _Utf8BitMasks::IsAsciiByte, // intentionally conflicts with MaskContinuationByte
        _Utf8BitMasks::IsLeadByteTwoByteSequence,
        _Utf8BitMasks::IsLeadByteThreeByteSequence,
    };

    HANDLE _outPipe; // non-owning reference to a pipe.
    std::array<char, 4096> _buffer; // buffer for the chunk read.
    std::array<char, 4> _utf8Partials; // buffer for code units of a partial UTF-8 code point that have to be cached
    DWORD _dwPartialsLen{}; // number of cached UTF-8 code units
};
