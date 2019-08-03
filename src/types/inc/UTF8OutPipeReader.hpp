/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- UTF8OutPipeReader.hpp

Abstract:
- This class provides methods to read a UTF-8 stream and gives back a buffer that contains only
  complete code points and complete composite characters.
- Partial UTF-8 code points at the end of the buffer read are cached as well as the last
  non-combining character of an entirely filled buffer. They are prepended to the next chunk read.
- In case of populated UTF-16, composite characters are converted to their canonical precomposed
  equivalents.

Author(s):
- Steffen Illhardt (german-one) 2019
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
    UTF8OutPipeReader(HANDLE outPipe); // Constructor that takes the read handle of the pipe
    [[nodiscard]] HRESULT Read(_Out_ std::string_view& utf8StrView); // Overload to get UTF-8 chunks
    [[nodiscard]] HRESULT Read(_Out_ std::wstring_view& utf16StrView); // Overload to get UTF-16 chunks

private:
    [[nodiscard]] HRESULT _UTF8ToUtf16Precomposed(_In_ const std::string_view utf8StrView, _Out_ std::wstring_view& utf16StrView);

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
    constexpr const static BYTE _cmpMasks[]{
        0, // unused
        _Utf8BitMasks::MaskContinuationByte,
        _Utf8BitMasks::MaskLeadByteTwoByteSequence,
        _Utf8BitMasks::MaskLeadByteThreeByteSequence,
    };

    // array of values for the comparisons
    constexpr const static BYTE _cmpOperands[]{
        0, // unused
        _Utf8BitMasks::IsAsciiByte, // intentionally conflicts with MaskContinuationByte
        _Utf8BitMasks::IsLeadByteTwoByteSequence,
        _Utf8BitMasks::IsLeadByteThreeByteSequence,
    };

    // structure of boundaries of Combining Marks
    const static struct tagCombiningMarks
    {
        const static struct tagDiacriticalBasic // U+0300 - U+036F Combining Diacritical Marks
        {
            constexpr const static BYTE first[]{ 0xCC, 0x80 };
            constexpr const static BYTE last[]{ 0xCD, 0xAF };
        } diacriticalBasic;

        const static struct tagDiacriticalExtended // U+1AB0 - U+1AFF Combining Diacritical Marks Extended
        {
            constexpr const static BYTE first[]{ 0xE1, 0xAA, 0xB0 };
            constexpr const static BYTE last[]{ 0xE1, 0xAB, 0xBF };
        } diacriticalExtended;

        const static struct tagDiacriticalSupplement // U+1DC0 - U+1DFF Combining Diacritical Marks Supplement
        {
            constexpr const static BYTE first[]{ 0xE1, 0xB7, 0x80 };
            constexpr const static BYTE last[]{ 0xE1, 0xB7, 0xBF };
        } diacriticalSupplement;

        const static struct tagDiacriticalForSymbols // U+20D0 - U+20FF Combining Diacritical Marks for Symbols
        {
            constexpr const static BYTE first[]{ 0xE2, 0x83, 0x90 };
            constexpr const static BYTE last[]{ 0xE2, 0x83, 0xBF };
        } diacriticalForSymbols;

        const static struct tagHalfMarks // U+FE20 - U+FE2F Combining Half Marks
        {
            constexpr const static BYTE first[]{ 0xEF, 0xB8, 0xA0 };
            constexpr const static BYTE last[]{ 0xEF, 0xB8, 0xAF };
        } halfMarks;
    } _combiningMarks;

    HANDLE _outPipe; // non-owning reference to a pipe.
    BYTE _buffer[4096]{ 0 }; // buffer for the chunk read
    BYTE _utf8Partials[4]{ 0 }; // buffer for code units of a partial UTF-8 code point that have to be cached
    DWORD _dwPartialsLen{}; // number of cached partial code units
    BYTE _utf8NonCombining[4]{ 0 }; // buffer for code units of the last code point
    DWORD _dwNonCombiningLen{}; // number of cached code units of the last code point
    std::unique_ptr<wchar_t[]> _convertedBuffer{}; // holds the buffer for converted UTF-16 text
    size_t _convertedCapacity{}; // current capacity of _convertedBuffer
    std::unique_ptr<wchar_t[]> _precomposedBuffer{}; // holds the buffer for precomposed UTF-16 text
    size_t _precomposedCapacity{}; // current capacity of _precomposedBuffer

#ifdef UNIT_TESTING
    friend class UTF8OutPipeReaderTests; // Make _UTF8ToUtf16Precomposed() accessible for the unit test
#endif
};
