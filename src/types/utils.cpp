// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/utils.hpp"

using namespace Microsoft::Console;

// Function Description:
// - Creates a String representation of a guid, in the format
//      "{12345678-ABCD-EF12-3456-7890ABCDEF12}"
// Arguments:
// - guid: the GUID to create the string for
// Return Value:
// - a string representation of the GUID. On failure, throws E_INVALIDARG.
std::wstring Utils::GuidToString(const GUID guid)
{
    return wil::str_printf<std::wstring>(L"{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}", guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
}

// Method Description:
// - Parses a GUID from a string representation of the GUID. Throws an exception
//      if it fails to parse the GUID. See documentation of IIDFromString for
//      details.
// Arguments:
// - wstr: a string representation of the GUID to parse
// Return Value:
// - A GUID if the string could successfully be parsed. On failure, throws the
//      failing HRESULT.
GUID Utils::GuidFromString(const std::wstring wstr)
{
    GUID result{};
    THROW_IF_FAILED(IIDFromString(wstr.c_str(), &result));
    return result;
}

// Method Description:
// - Creates a GUID, but not via an out parameter.
// Return Value:
// - A GUID if there's enough randomness; otherwise, an exception.
GUID Utils::CreateGuid()
{
    GUID result{};
    THROW_IF_FAILED(::CoCreateGuid(&result));
    return result;
}

// Function Description:
// - Creates a String representation of a color, in the format "#RRGGBB"
// Arguments:
// - color: the COLORREF to create the string for
// Return Value:
// - a string representation of the color
std::string Utils::ColorToHexString(const til::color color)
{
    std::stringstream ss;
    ss << "#" << std::uppercase << std::setfill('0') << std::hex;
    // Force the compiler to promote from byte to int. Without it, the
    // stringstream will try to write the components as chars
    ss << std::setw(2) << static_cast<int>(color.r);
    ss << std::setw(2) << static_cast<int>(color.g);
    ss << std::setw(2) << static_cast<int>(color.b);
    return ss.str();
}

// Function Description:
// - Parses a color from a string. The string should be in the format "#RRGGBB" or "#RGB"
// Arguments:
// - str: a string representation of the COLORREF to parse
// Return Value:
// - A COLORREF if the string could successfully be parsed. If the string is not
//      the correct format, throws E_INVALIDARG
til::color Utils::ColorFromHexString(const std::string_view str)
{
    THROW_HR_IF(E_INVALIDARG, str.size() != 7 && str.size() != 4);
    THROW_HR_IF(E_INVALIDARG, str.at(0) != '#');

    std::string rStr;
    std::string gStr;
    std::string bStr;

    if (str.size() == 4)
    {
        rStr = std::string(2, str.at(1));
        gStr = std::string(2, str.at(2));
        bStr = std::string(2, str.at(3));
    }
    else
    {
        rStr = std::string(&str.at(1), 2);
        gStr = std::string(&str.at(3), 2);
        bStr = std::string(&str.at(5), 2);
    }

    const BYTE r = gsl::narrow_cast<BYTE>(std::stoul(rStr, nullptr, 16));
    const BYTE g = gsl::narrow_cast<BYTE>(std::stoul(gStr, nullptr, 16));
    const BYTE b = gsl::narrow_cast<BYTE>(std::stoul(bStr, nullptr, 16));

    return til::color{ r, g, b };
}

// Routine Description:
// - Shorthand check if a handle value is null or invalid.
// Arguments:
// - Handle
// Return Value:
// - True if non zero and not set to invalid magic value. False otherwise.
bool Utils::IsValidHandle(const HANDLE handle) noexcept
{
    return handle != nullptr && handle != INVALID_HANDLE_VALUE;
}

// Function Description:
// - Fill the first 16 entries of a given color table with the Campbell color
//   scheme, in the ANSI/VT RGB order.
// Arguments:
// - table: a color table with at least 16 entries
// Return Value:
// - <none>, throws if the table has less that 16 entries
void Utils::InitializeCampbellColorTable(const gsl::span<COLORREF> table)
{
    THROW_HR_IF(E_INVALIDARG, table.size() < 16);

    // clang-format off
    gsl::at(table, 0)   = RGB(12,   12,   12);
    gsl::at(table, 1)   = RGB(197,  15,   31);
    gsl::at(table, 2)   = RGB(19,   161,  14);
    gsl::at(table, 3)   = RGB(193,  156,  0);
    gsl::at(table, 4)   = RGB(0,    55,   218);
    gsl::at(table, 5)   = RGB(136,  23,   152);
    gsl::at(table, 6)   = RGB(58,   150,  221);
    gsl::at(table, 7)   = RGB(204,  204,  204);
    gsl::at(table, 8)   = RGB(118,  118,  118);
    gsl::at(table, 9)   = RGB(231,  72,   86);
    gsl::at(table, 10)  = RGB(22,   198,  12);
    gsl::at(table, 11)  = RGB(249,  241,  165);
    gsl::at(table, 12)  = RGB(59,   120,  255);
    gsl::at(table, 13)  = RGB(180,  0,    158);
    gsl::at(table, 14)  = RGB(97,   214,  214);
    gsl::at(table, 15)  = RGB(242,  242,  242);
    // clang-format on
}

// Function Description:
// - Fill the first 16 entries of a given color table with the Campbell color
//   scheme, in the Windows BGR order.
// Arguments:
// - table: a color table with at least 16 entries
// Return Value:
// - <none>, throws if the table has less that 16 entries
void Utils::InitializeCampbellColorTableForConhost(const gsl::span<COLORREF> table)
{
    THROW_HR_IF(E_INVALIDARG, table.size() < 16);
    InitializeCampbellColorTable(table);
    SwapANSIColorOrderForConhost(table);
}

// Function Description:
// - modifies in-place the given color table from ANSI (RGB) order to Console order (BRG).
// Arguments:
// - table: a color table with at least 16 entries
// Return Value:
// - <none>, throws if the table has less that 16 entries
void Utils::SwapANSIColorOrderForConhost(const gsl::span<COLORREF> table)
{
    THROW_HR_IF(E_INVALIDARG, table.size() < 16);
    std::swap(gsl::at(table, 1), gsl::at(table, 4));
    std::swap(gsl::at(table, 3), gsl::at(table, 6));
    std::swap(gsl::at(table, 9), gsl::at(table, 12));
    std::swap(gsl::at(table, 11), gsl::at(table, 14));
}

// Function Description:
// - Fill the first 255 entries of a given color table with the default values
//      of a full 256-color table
// Arguments:
// - table: a color table with at least 256 entries
// Return Value:
// - <none>, throws if the table has less that 256 entries
void Utils::Initialize256ColorTable(const gsl::span<COLORREF> table)
{
    THROW_HR_IF(E_INVALIDARG, table.size() < 256);

    // clang-format off
    gsl::at(table, 0)   = RGB(0x00, 0x00, 0x00);
    gsl::at(table, 1)   = RGB(0x80, 0x00, 0x00);
    gsl::at(table, 2)   = RGB(0x00, 0x80, 0x00);
    gsl::at(table, 3)   = RGB(0x80, 0x80, 0x00);
    gsl::at(table, 4)   = RGB(0x00, 0x00, 0x80);
    gsl::at(table, 5)   = RGB(0x80, 0x00, 0x80);
    gsl::at(table, 6)   = RGB(0x00, 0x80, 0x80);
    gsl::at(table, 7)   = RGB(0xc0, 0xc0, 0xc0);
    gsl::at(table, 8)   = RGB(0x80, 0x80, 0x80);
    gsl::at(table, 9)   = RGB(0xff, 0x00, 0x00);
    gsl::at(table, 10)  = RGB(0x00, 0xff, 0x00);
    gsl::at(table, 11)  = RGB(0xff, 0xff, 0x00);
    gsl::at(table, 12)  = RGB(0x00, 0x00, 0xff);
    gsl::at(table, 13)  = RGB(0xff, 0x00, 0xff);
    gsl::at(table, 14)  = RGB(0x00, 0xff, 0xff);
    gsl::at(table, 15)  = RGB(0xff, 0xff, 0xff);
    gsl::at(table, 16)  = RGB(0x00, 0x00, 0x00);
    gsl::at(table, 17)  = RGB(0x00, 0x00, 0x5f);
    gsl::at(table, 18)  = RGB(0x00, 0x00, 0x87);
    gsl::at(table, 19)  = RGB(0x00, 0x00, 0xaf);
    gsl::at(table, 20)  = RGB(0x00, 0x00, 0xd7);
    gsl::at(table, 21)  = RGB(0x00, 0x00, 0xff);
    gsl::at(table, 22)  = RGB(0x00, 0x5f, 0x00);
    gsl::at(table, 23)  = RGB(0x00, 0x5f, 0x5f);
    gsl::at(table, 24)  = RGB(0x00, 0x5f, 0x87);
    gsl::at(table, 25)  = RGB(0x00, 0x5f, 0xaf);
    gsl::at(table, 26)  = RGB(0x00, 0x5f, 0xd7);
    gsl::at(table, 27)  = RGB(0x00, 0x5f, 0xff);
    gsl::at(table, 28)  = RGB(0x00, 0x87, 0x00);
    gsl::at(table, 29)  = RGB(0x00, 0x87, 0x5f);
    gsl::at(table, 30)  = RGB(0x00, 0x87, 0x87);
    gsl::at(table, 31)  = RGB(0x00, 0x87, 0xaf);
    gsl::at(table, 32)  = RGB(0x00, 0x87, 0xd7);
    gsl::at(table, 33)  = RGB(0x00, 0x87, 0xff);
    gsl::at(table, 34)  = RGB(0x00, 0xaf, 0x00);
    gsl::at(table, 35)  = RGB(0x00, 0xaf, 0x5f);
    gsl::at(table, 36)  = RGB(0x00, 0xaf, 0x87);
    gsl::at(table, 37)  = RGB(0x00, 0xaf, 0xaf);
    gsl::at(table, 38)  = RGB(0x00, 0xaf, 0xd7);
    gsl::at(table, 39)  = RGB(0x00, 0xaf, 0xff);
    gsl::at(table, 40)  = RGB(0x00, 0xd7, 0x00);
    gsl::at(table, 41)  = RGB(0x00, 0xd7, 0x5f);
    gsl::at(table, 42)  = RGB(0x00, 0xd7, 0x87);
    gsl::at(table, 43)  = RGB(0x00, 0xd7, 0xaf);
    gsl::at(table, 44)  = RGB(0x00, 0xd7, 0xd7);
    gsl::at(table, 45)  = RGB(0x00, 0xd7, 0xff);
    gsl::at(table, 46)  = RGB(0x00, 0xff, 0x00);
    gsl::at(table, 47)  = RGB(0x00, 0xff, 0x5f);
    gsl::at(table, 48)  = RGB(0x00, 0xff, 0x87);
    gsl::at(table, 49)  = RGB(0x00, 0xff, 0xaf);
    gsl::at(table, 50)  = RGB(0x00, 0xff, 0xd7);
    gsl::at(table, 51)  = RGB(0x00, 0xff, 0xff);
    gsl::at(table, 52)  = RGB(0x5f, 0x00, 0x00);
    gsl::at(table, 53)  = RGB(0x5f, 0x00, 0x5f);
    gsl::at(table, 54)  = RGB(0x5f, 0x00, 0x87);
    gsl::at(table, 55)  = RGB(0x5f, 0x00, 0xaf);
    gsl::at(table, 56)  = RGB(0x5f, 0x00, 0xd7);
    gsl::at(table, 57)  = RGB(0x5f, 0x00, 0xff);
    gsl::at(table, 58)  = RGB(0x5f, 0x5f, 0x00);
    gsl::at(table, 59)  = RGB(0x5f, 0x5f, 0x5f);
    gsl::at(table, 60)  = RGB(0x5f, 0x5f, 0x87);
    gsl::at(table, 61)  = RGB(0x5f, 0x5f, 0xaf);
    gsl::at(table, 62)  = RGB(0x5f, 0x5f, 0xd7);
    gsl::at(table, 63)  = RGB(0x5f, 0x5f, 0xff);
    gsl::at(table, 64)  = RGB(0x5f, 0x87, 0x00);
    gsl::at(table, 65)  = RGB(0x5f, 0x87, 0x5f);
    gsl::at(table, 66)  = RGB(0x5f, 0x87, 0x87);
    gsl::at(table, 67)  = RGB(0x5f, 0x87, 0xaf);
    gsl::at(table, 68)  = RGB(0x5f, 0x87, 0xd7);
    gsl::at(table, 69)  = RGB(0x5f, 0x87, 0xff);
    gsl::at(table, 70)  = RGB(0x5f, 0xaf, 0x00);
    gsl::at(table, 71)  = RGB(0x5f, 0xaf, 0x5f);
    gsl::at(table, 72)  = RGB(0x5f, 0xaf, 0x87);
    gsl::at(table, 73)  = RGB(0x5f, 0xaf, 0xaf);
    gsl::at(table, 74)  = RGB(0x5f, 0xaf, 0xd7);
    gsl::at(table, 75)  = RGB(0x5f, 0xaf, 0xff);
    gsl::at(table, 76)  = RGB(0x5f, 0xd7, 0x00);
    gsl::at(table, 77)  = RGB(0x5f, 0xd7, 0x5f);
    gsl::at(table, 78)  = RGB(0x5f, 0xd7, 0x87);
    gsl::at(table, 79)  = RGB(0x5f, 0xd7, 0xaf);
    gsl::at(table, 80)  = RGB(0x5f, 0xd7, 0xd7);
    gsl::at(table, 81)  = RGB(0x5f, 0xd7, 0xff);
    gsl::at(table, 82)  = RGB(0x5f, 0xff, 0x00);
    gsl::at(table, 83)  = RGB(0x5f, 0xff, 0x5f);
    gsl::at(table, 84)  = RGB(0x5f, 0xff, 0x87);
    gsl::at(table, 85)  = RGB(0x5f, 0xff, 0xaf);
    gsl::at(table, 86)  = RGB(0x5f, 0xff, 0xd7);
    gsl::at(table, 87)  = RGB(0x5f, 0xff, 0xff);
    gsl::at(table, 88)  = RGB(0x87, 0x00, 0x00);
    gsl::at(table, 89)  = RGB(0x87, 0x00, 0x5f);
    gsl::at(table, 90)  = RGB(0x87, 0x00, 0x87);
    gsl::at(table, 91)  = RGB(0x87, 0x00, 0xaf);
    gsl::at(table, 92)  = RGB(0x87, 0x00, 0xd7);
    gsl::at(table, 93)  = RGB(0x87, 0x00, 0xff);
    gsl::at(table, 94)  = RGB(0x87, 0x5f, 0x00);
    gsl::at(table, 95)  = RGB(0x87, 0x5f, 0x5f);
    gsl::at(table, 96)  = RGB(0x87, 0x5f, 0x87);
    gsl::at(table, 97)  = RGB(0x87, 0x5f, 0xaf);
    gsl::at(table, 98)  = RGB(0x87, 0x5f, 0xd7);
    gsl::at(table, 99)  = RGB(0x87, 0x5f, 0xff);
    gsl::at(table, 100) = RGB(0x87, 0x87, 0x00);
    gsl::at(table, 101) = RGB(0x87, 0x87, 0x5f);
    gsl::at(table, 102) = RGB(0x87, 0x87, 0x87);
    gsl::at(table, 103) = RGB(0x87, 0x87, 0xaf);
    gsl::at(table, 104) = RGB(0x87, 0x87, 0xd7);
    gsl::at(table, 105) = RGB(0x87, 0x87, 0xff);
    gsl::at(table, 106) = RGB(0x87, 0xaf, 0x00);
    gsl::at(table, 107) = RGB(0x87, 0xaf, 0x5f);
    gsl::at(table, 108) = RGB(0x87, 0xaf, 0x87);
    gsl::at(table, 109) = RGB(0x87, 0xaf, 0xaf);
    gsl::at(table, 110) = RGB(0x87, 0xaf, 0xd7);
    gsl::at(table, 111) = RGB(0x87, 0xaf, 0xff);
    gsl::at(table, 112) = RGB(0x87, 0xd7, 0x00);
    gsl::at(table, 113) = RGB(0x87, 0xd7, 0x5f);
    gsl::at(table, 114) = RGB(0x87, 0xd7, 0x87);
    gsl::at(table, 115) = RGB(0x87, 0xd7, 0xaf);
    gsl::at(table, 116) = RGB(0x87, 0xd7, 0xd7);
    gsl::at(table, 117) = RGB(0x87, 0xd7, 0xff);
    gsl::at(table, 118) = RGB(0x87, 0xff, 0x00);
    gsl::at(table, 119) = RGB(0x87, 0xff, 0x5f);
    gsl::at(table, 120) = RGB(0x87, 0xff, 0x87);
    gsl::at(table, 121) = RGB(0x87, 0xff, 0xaf);
    gsl::at(table, 122) = RGB(0x87, 0xff, 0xd7);
    gsl::at(table, 123) = RGB(0x87, 0xff, 0xff);
    gsl::at(table, 124) = RGB(0xaf, 0x00, 0x00);
    gsl::at(table, 125) = RGB(0xaf, 0x00, 0x5f);
    gsl::at(table, 126) = RGB(0xaf, 0x00, 0x87);
    gsl::at(table, 127) = RGB(0xaf, 0x00, 0xaf);
    gsl::at(table, 128) = RGB(0xaf, 0x00, 0xd7);
    gsl::at(table, 129) = RGB(0xaf, 0x00, 0xff);
    gsl::at(table, 130) = RGB(0xaf, 0x5f, 0x00);
    gsl::at(table, 131) = RGB(0xaf, 0x5f, 0x5f);
    gsl::at(table, 132) = RGB(0xaf, 0x5f, 0x87);
    gsl::at(table, 133) = RGB(0xaf, 0x5f, 0xaf);
    gsl::at(table, 134) = RGB(0xaf, 0x5f, 0xd7);
    gsl::at(table, 135) = RGB(0xaf, 0x5f, 0xff);
    gsl::at(table, 136) = RGB(0xaf, 0x87, 0x00);
    gsl::at(table, 137) = RGB(0xaf, 0x87, 0x5f);
    gsl::at(table, 138) = RGB(0xaf, 0x87, 0x87);
    gsl::at(table, 139) = RGB(0xaf, 0x87, 0xaf);
    gsl::at(table, 140) = RGB(0xaf, 0x87, 0xd7);
    gsl::at(table, 141) = RGB(0xaf, 0x87, 0xff);
    gsl::at(table, 142) = RGB(0xaf, 0xaf, 0x00);
    gsl::at(table, 143) = RGB(0xaf, 0xaf, 0x5f);
    gsl::at(table, 144) = RGB(0xaf, 0xaf, 0x87);
    gsl::at(table, 145) = RGB(0xaf, 0xaf, 0xaf);
    gsl::at(table, 146) = RGB(0xaf, 0xaf, 0xd7);
    gsl::at(table, 147) = RGB(0xaf, 0xaf, 0xff);
    gsl::at(table, 148) = RGB(0xaf, 0xd7, 0x00);
    gsl::at(table, 149) = RGB(0xaf, 0xd7, 0x5f);
    gsl::at(table, 150) = RGB(0xaf, 0xd7, 0x87);
    gsl::at(table, 151) = RGB(0xaf, 0xd7, 0xaf);
    gsl::at(table, 152) = RGB(0xaf, 0xd7, 0xd7);
    gsl::at(table, 153) = RGB(0xaf, 0xd7, 0xff);
    gsl::at(table, 154) = RGB(0xaf, 0xff, 0x00);
    gsl::at(table, 155) = RGB(0xaf, 0xff, 0x5f);
    gsl::at(table, 156) = RGB(0xaf, 0xff, 0x87);
    gsl::at(table, 157) = RGB(0xaf, 0xff, 0xaf);
    gsl::at(table, 158) = RGB(0xaf, 0xff, 0xd7);
    gsl::at(table, 159) = RGB(0xaf, 0xff, 0xff);
    gsl::at(table, 160) = RGB(0xd7, 0x00, 0x00);
    gsl::at(table, 161) = RGB(0xd7, 0x00, 0x5f);
    gsl::at(table, 162) = RGB(0xd7, 0x00, 0x87);
    gsl::at(table, 163) = RGB(0xd7, 0x00, 0xaf);
    gsl::at(table, 164) = RGB(0xd7, 0x00, 0xd7);
    gsl::at(table, 165) = RGB(0xd7, 0x00, 0xff);
    gsl::at(table, 166) = RGB(0xd7, 0x5f, 0x00);
    gsl::at(table, 167) = RGB(0xd7, 0x5f, 0x5f);
    gsl::at(table, 168) = RGB(0xd7, 0x5f, 0x87);
    gsl::at(table, 169) = RGB(0xd7, 0x5f, 0xaf);
    gsl::at(table, 170) = RGB(0xd7, 0x5f, 0xd7);
    gsl::at(table, 171) = RGB(0xd7, 0x5f, 0xff);
    gsl::at(table, 172) = RGB(0xd7, 0x87, 0x00);
    gsl::at(table, 173) = RGB(0xd7, 0x87, 0x5f);
    gsl::at(table, 174) = RGB(0xd7, 0x87, 0x87);
    gsl::at(table, 175) = RGB(0xd7, 0x87, 0xaf);
    gsl::at(table, 176) = RGB(0xd7, 0x87, 0xd7);
    gsl::at(table, 177) = RGB(0xd7, 0x87, 0xff);
    gsl::at(table, 178) = RGB(0xd7, 0xaf, 0x00);
    gsl::at(table, 179) = RGB(0xd7, 0xaf, 0x5f);
    gsl::at(table, 180) = RGB(0xd7, 0xaf, 0x87);
    gsl::at(table, 181) = RGB(0xd7, 0xaf, 0xaf);
    gsl::at(table, 182) = RGB(0xd7, 0xaf, 0xd7);
    gsl::at(table, 183) = RGB(0xd7, 0xaf, 0xff);
    gsl::at(table, 184) = RGB(0xd7, 0xd7, 0x00);
    gsl::at(table, 185) = RGB(0xd7, 0xd7, 0x5f);
    gsl::at(table, 186) = RGB(0xd7, 0xd7, 0x87);
    gsl::at(table, 187) = RGB(0xd7, 0xd7, 0xaf);
    gsl::at(table, 188) = RGB(0xd7, 0xd7, 0xd7);
    gsl::at(table, 189) = RGB(0xd7, 0xd7, 0xff);
    gsl::at(table, 190) = RGB(0xd7, 0xff, 0x00);
    gsl::at(table, 191) = RGB(0xd7, 0xff, 0x5f);
    gsl::at(table, 192) = RGB(0xd7, 0xff, 0x87);
    gsl::at(table, 193) = RGB(0xd7, 0xff, 0xaf);
    gsl::at(table, 194) = RGB(0xd7, 0xff, 0xd7);
    gsl::at(table, 195) = RGB(0xd7, 0xff, 0xff);
    gsl::at(table, 196) = RGB(0xff, 0x00, 0x00);
    gsl::at(table, 197) = RGB(0xff, 0x00, 0x5f);
    gsl::at(table, 198) = RGB(0xff, 0x00, 0x87);
    gsl::at(table, 199) = RGB(0xff, 0x00, 0xaf);
    gsl::at(table, 200) = RGB(0xff, 0x00, 0xd7);
    gsl::at(table, 201) = RGB(0xff, 0x00, 0xff);
    gsl::at(table, 202) = RGB(0xff, 0x5f, 0x00);
    gsl::at(table, 203) = RGB(0xff, 0x5f, 0x5f);
    gsl::at(table, 204) = RGB(0xff, 0x5f, 0x87);
    gsl::at(table, 205) = RGB(0xff, 0x5f, 0xaf);
    gsl::at(table, 206) = RGB(0xff, 0x5f, 0xd7);
    gsl::at(table, 207) = RGB(0xff, 0x5f, 0xff);
    gsl::at(table, 208) = RGB(0xff, 0x87, 0x00);
    gsl::at(table, 209) = RGB(0xff, 0x87, 0x5f);
    gsl::at(table, 210) = RGB(0xff, 0x87, 0x87);
    gsl::at(table, 211) = RGB(0xff, 0x87, 0xaf);
    gsl::at(table, 212) = RGB(0xff, 0x87, 0xd7);
    gsl::at(table, 213) = RGB(0xff, 0x87, 0xff);
    gsl::at(table, 214) = RGB(0xff, 0xaf, 0x00);
    gsl::at(table, 215) = RGB(0xff, 0xaf, 0x5f);
    gsl::at(table, 216) = RGB(0xff, 0xaf, 0x87);
    gsl::at(table, 217) = RGB(0xff, 0xaf, 0xaf);
    gsl::at(table, 218) = RGB(0xff, 0xaf, 0xd7);
    gsl::at(table, 219) = RGB(0xff, 0xaf, 0xff);
    gsl::at(table, 220) = RGB(0xff, 0xd7, 0x00);
    gsl::at(table, 221) = RGB(0xff, 0xd7, 0x5f);
    gsl::at(table, 222) = RGB(0xff, 0xd7, 0x87);
    gsl::at(table, 223) = RGB(0xff, 0xd7, 0xaf);
    gsl::at(table, 224) = RGB(0xff, 0xd7, 0xd7);
    gsl::at(table, 225) = RGB(0xff, 0xd7, 0xff);
    gsl::at(table, 226) = RGB(0xff, 0xff, 0x00);
    gsl::at(table, 227) = RGB(0xff, 0xff, 0x5f);
    gsl::at(table, 228) = RGB(0xff, 0xff, 0x87);
    gsl::at(table, 229) = RGB(0xff, 0xff, 0xaf);
    gsl::at(table, 230) = RGB(0xff, 0xff, 0xd7);
    gsl::at(table, 231) = RGB(0xff, 0xff, 0xff);
    gsl::at(table, 232) = RGB(0x08, 0x08, 0x08);
    gsl::at(table, 233) = RGB(0x12, 0x12, 0x12);
    gsl::at(table, 234) = RGB(0x1c, 0x1c, 0x1c);
    gsl::at(table, 235) = RGB(0x26, 0x26, 0x26);
    gsl::at(table, 236) = RGB(0x30, 0x30, 0x30);
    gsl::at(table, 237) = RGB(0x3a, 0x3a, 0x3a);
    gsl::at(table, 238) = RGB(0x44, 0x44, 0x44);
    gsl::at(table, 239) = RGB(0x4e, 0x4e, 0x4e);
    gsl::at(table, 240) = RGB(0x58, 0x58, 0x58);
    gsl::at(table, 241) = RGB(0x62, 0x62, 0x62);
    gsl::at(table, 242) = RGB(0x6c, 0x6c, 0x6c);
    gsl::at(table, 243) = RGB(0x76, 0x76, 0x76);
    gsl::at(table, 244) = RGB(0x80, 0x80, 0x80);
    gsl::at(table, 245) = RGB(0x8a, 0x8a, 0x8a);
    gsl::at(table, 246) = RGB(0x94, 0x94, 0x94);
    gsl::at(table, 247) = RGB(0x9e, 0x9e, 0x9e);
    gsl::at(table, 248) = RGB(0xa8, 0xa8, 0xa8);
    gsl::at(table, 249) = RGB(0xb2, 0xb2, 0xb2);
    gsl::at(table, 250) = RGB(0xbc, 0xbc, 0xbc);
    gsl::at(table, 251) = RGB(0xc6, 0xc6, 0xc6);
    gsl::at(table, 252) = RGB(0xd0, 0xd0, 0xd0);
    gsl::at(table, 253) = RGB(0xda, 0xda, 0xda);
    gsl::at(table, 254) = RGB(0xe4, 0xe4, 0xe4);
    gsl::at(table, 255) = RGB(0xee, 0xee, 0xee);
    // clang-format on
}

// Function Description:
// - Generate a Version 5 UUID (specified in RFC4122 4.3)
//   v5 UUIDs are stable given the same namespace and "name".
// Arguments:
// - namespaceGuid: The GUID of the v5 UUID namespace, which provides both
//                  a seed and a tacit agreement that all UUIDs generated
//                  with it will follow the same data format.
// - name: Bytes comprising the name (in a namespace-specific format)
// Return Value:
// - a new stable v5 UUID
GUID Utils::CreateV5Uuid(const GUID& namespaceGuid, const gsl::span<const gsl::byte> name)
{
    // v5 uuid generation happens over values in network byte order, so let's enforce that
    auto correctEndianNamespaceGuid{ EndianSwap(namespaceGuid) };

    wil::unique_bcrypt_hash hash;
    THROW_IF_NTSTATUS_FAILED(BCryptCreateHash(BCRYPT_SHA1_ALG_HANDLE, &hash, nullptr, 0, nullptr, 0, 0));

    // According to N4713 8.2.1.11 [basic.lval], accessing the bytes underlying an object
    // through unsigned char or char pointer *is defined*.
    THROW_IF_NTSTATUS_FAILED(BCryptHashData(hash.get(), reinterpret_cast<PUCHAR>(&correctEndianNamespaceGuid), sizeof(GUID), 0));
    // BCryptHashData is ill-specified in that it leaves off "const" qualification for pbInput
    THROW_IF_NTSTATUS_FAILED(BCryptHashData(hash.get(), reinterpret_cast<PUCHAR>(const_cast<gsl::byte*>(name.data())), gsl::narrow<ULONG>(name.size()), 0));

    std::array<uint8_t, 20> buffer;
    THROW_IF_NTSTATUS_FAILED(BCryptFinishHash(hash.get(), buffer.data(), gsl::narrow<ULONG>(buffer.size()), 0));

    buffer.at(6) = (buffer.at(6) & 0x0F) | 0x50; // set the uuid version to 5
    buffer.at(8) = (buffer.at(8) & 0x3F) | 0x80; // set the variant to 2 (RFC4122)

    // We're using memcpy here pursuant to N4713 6.7.2/3 [basic.types],
    // "...the underlying bytes making up the object can be copied into an array
    // of char or unsigned char...array is copied back into the object..."
    // std::copy may compile down to ::memcpy for these types, but using it might
    // contravene the standard and nobody's got time for that.
    GUID newGuid{ 0 };
    ::memcpy_s(&newGuid, sizeof(GUID), buffer.data(), sizeof(GUID));
    return EndianSwap(newGuid);
}
