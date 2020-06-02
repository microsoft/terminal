// TEST TOOL U8U16Test
// Performance tests for UTF-8 <--> UTF-16 conversions, related to PR #4093
// NOTE The functions u8u16 and u16u8 contain own algorithms. Tests have shown that they perform
// worse than the platform API functions.
// Thus, these functions are *unrelated* to the til::u8u16 and til::u16u8 implementation.

#pragma once

#undef WIN32_LEAN_AND_MEAN
#undef NOMINMAX
#define NOMINMAX
#include <stdexcept>
#include <string>
#include <string_view>
#include <array>
#include <algorithm>
#include <windows.h>
#include <intsafe.h>

class u8state final
{
public:
    u8state() noexcept;
    [[nodiscard]] HRESULT operator()(const std::string_view in, std::string_view& out) noexcept;
    void reset() noexcept;

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

    std::string _buffer8;
    std::array<char, 4> _utf8Partials; // buffer for code units of a partial UTF-8 code point that have to be cached
    size_t _partialsLen{}; // number of cached UTF-8 code units
};

class u16state final
{
public:
    u16state() noexcept;
    [[nodiscard]] HRESULT operator()(const std::wstring_view in, std::wstring_view& out) noexcept;
    void reset() noexcept;

private:
    std::wstring _buffer16;
    wchar_t _highSurrogate{}; // UTF-16 high surrogate that has to be cached
    size_t _cached{}; // 1 if a high surrogate has been cached, 0 otherwise
};

[[nodiscard]] HRESULT u8u16(const std::string_view in, std::wstring& out, bool discardInvalids = false) noexcept;
[[nodiscard]] HRESULT u8u16_ptr(const std::string_view in, std::wstring& out, bool discardInvalids = false) noexcept;
[[nodiscard]] HRESULT u8u16(const std::string_view in, std::wstring& out, u8state& state, bool discardInvalids = false) noexcept;
[[nodiscard]] HRESULT u16u8(const std::wstring_view in, std::string& out, bool discardInvalids = false) noexcept;
[[nodiscard]] HRESULT u16u8_ptr(const std::wstring_view in, std::string& out, bool discardInvalids = false) noexcept;
[[nodiscard]] HRESULT u16u8(const std::wstring_view in, std::string& out, u16state& state, bool discardInvalids = false) noexcept;

std::wstring u8u16(const std::string_view in, bool discardInvalids = false);
std::wstring u8u16(const std::string_view in, u8state& state, bool discardInvalids = false);
std::string u16u8(const std::wstring_view in, bool discardInvalids = false);
std::string u16u8(const std::wstring_view in, u16state& state, bool discardInvalids = false);
