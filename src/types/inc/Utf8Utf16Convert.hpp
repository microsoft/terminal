/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- Utf8Utf16Convert.hpp

Abstract:
- Defines functions for converting between UTF-8 and UTF-16 strings.
- Defines classes to complement partial code points at the begin of a string and cache partials from the end of a string.
- Defines classes to do both the complement/cache task and the conversion task at once.

Author(s):
- Steffen Illhardt (german-one) 2019
--*/

#pragma once

[[nodiscard]] HRESULT U8ToU16(_In_ const std::string_view u8Str, _Out_ std::wstring& u16Str, bool discardInvalids = false) noexcept;

[[nodiscard]] HRESULT U16ToU8(_In_ const std::wstring_view u16Str, _Out_ std::string& u8Str, bool discardInvalids = false) noexcept;

class UTF8PartialHandler final
{
public:
    UTF8PartialHandler() noexcept;
    [[nodiscard]] HRESULT operator()(_Inout_ std::string_view& u8Str) noexcept;

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

    std::string _buffer;
    std::array<char, 4> _utf8Partials; // buffer for code units of a partial UTF-8 code point that have to be cached
    size_t _partialsLen{}; // number of cached UTF-8 code units
};

class UTF16PartialHandler final
{
public:
    UTF16PartialHandler() noexcept;
    [[nodiscard]] HRESULT operator()(_Inout_ std::wstring_view& u16Str) noexcept;

private:
    std::wstring _buffer;
    wchar_t _highSurrogate{}; // UTF-16 high surrogate that has to be cached
    size_t _cached{}; // 1 if a high surrogate has been cached, 0 otherwise
};

class UTF8ChunkToUTF16Converter final
{
public:
    UTF8ChunkToUTF16Converter() noexcept;
    [[nodiscard]] HRESULT operator()(_In_ std::string_view u8Str, _Out_ std::wstring_view& u16Str, bool discardInvalids = false) noexcept;

private:
    UTF8PartialHandler _handleU8Partials;
    std::wstring _buffer;
};

class UTF16ChunkToUTF8Converter final
{
public:
    UTF16ChunkToUTF8Converter() noexcept;
    [[nodiscard]] HRESULT operator()(_In_ std::wstring_view u16Str, _Out_ std::string_view& u8Str, bool discardInvalids = false) noexcept;

private:
    UTF16PartialHandler _handleU16Partials;
    std::string _buffer;
};
