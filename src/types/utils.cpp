// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/utils.hpp"

using namespace Microsoft::Console;

static constexpr std::array<til::color, 16> campbellColorTable{
    til::color{ 0x0C, 0x0C, 0x0C },
    til::color{ 0xC5, 0x0F, 0x1F },
    til::color{ 0x13, 0xA1, 0x0E },
    til::color{ 0xC1, 0x9C, 0x00 },
    til::color{ 0x00, 0x37, 0xDA },
    til::color{ 0x88, 0x17, 0x98 },
    til::color{ 0x3A, 0x96, 0xDD },
    til::color{ 0xCC, 0xCC, 0xCC },
    til::color{ 0x76, 0x76, 0x76 },
    til::color{ 0xE7, 0x48, 0x56 },
    til::color{ 0x16, 0xC6, 0x0C },
    til::color{ 0xF9, 0xF1, 0xA5 },
    til::color{ 0x3B, 0x78, 0xFF },
    til::color{ 0xB4, 0x00, 0x9E },
    til::color{ 0x61, 0xD6, 0xD6 },
    til::color{ 0xF2, 0xF2, 0xF2 },
};

static constexpr std::array<til::color, 256> standardXterm256ColorTable{
    til::color{ 0x00, 0x00, 0x00 },
    til::color{ 0x80, 0x00, 0x00 },
    til::color{ 0x00, 0x80, 0x00 },
    til::color{ 0x80, 0x80, 0x00 },
    til::color{ 0x00, 0x00, 0x80 },
    til::color{ 0x80, 0x00, 0x80 },
    til::color{ 0x00, 0x80, 0x80 },
    til::color{ 0xC0, 0xC0, 0xC0 },
    til::color{ 0x80, 0x80, 0x80 },
    til::color{ 0xFF, 0x00, 0x00 },
    til::color{ 0x00, 0xFF, 0x00 },
    til::color{ 0xFF, 0xFF, 0x00 },
    til::color{ 0x00, 0x00, 0xFF },
    til::color{ 0xFF, 0x00, 0xFF },
    til::color{ 0x00, 0xFF, 0xFF },
    til::color{ 0xFF, 0xFF, 0xFF },
    til::color{ 0x00, 0x00, 0x00 },
    til::color{ 0x00, 0x00, 0x5F },
    til::color{ 0x00, 0x00, 0x87 },
    til::color{ 0x00, 0x00, 0xAF },
    til::color{ 0x00, 0x00, 0xD7 },
    til::color{ 0x00, 0x00, 0xFF },
    til::color{ 0x00, 0x5F, 0x00 },
    til::color{ 0x00, 0x5F, 0x5F },
    til::color{ 0x00, 0x5F, 0x87 },
    til::color{ 0x00, 0x5F, 0xAF },
    til::color{ 0x00, 0x5F, 0xD7 },
    til::color{ 0x00, 0x5F, 0xFF },
    til::color{ 0x00, 0x87, 0x00 },
    til::color{ 0x00, 0x87, 0x5F },
    til::color{ 0x00, 0x87, 0x87 },
    til::color{ 0x00, 0x87, 0xAF },
    til::color{ 0x00, 0x87, 0xD7 },
    til::color{ 0x00, 0x87, 0xFF },
    til::color{ 0x00, 0xAF, 0x00 },
    til::color{ 0x00, 0xAF, 0x5F },
    til::color{ 0x00, 0xAF, 0x87 },
    til::color{ 0x00, 0xAF, 0xAF },
    til::color{ 0x00, 0xAF, 0xD7 },
    til::color{ 0x00, 0xAF, 0xFF },
    til::color{ 0x00, 0xD7, 0x00 },
    til::color{ 0x00, 0xD7, 0x5F },
    til::color{ 0x00, 0xD7, 0x87 },
    til::color{ 0x00, 0xD7, 0xAF },
    til::color{ 0x00, 0xD7, 0xD7 },
    til::color{ 0x00, 0xD7, 0xFF },
    til::color{ 0x00, 0xFF, 0x00 },
    til::color{ 0x00, 0xFF, 0x5F },
    til::color{ 0x00, 0xFF, 0x87 },
    til::color{ 0x00, 0xFF, 0xAF },
    til::color{ 0x00, 0xFF, 0xD7 },
    til::color{ 0x00, 0xFF, 0xFF },
    til::color{ 0x5F, 0x00, 0x00 },
    til::color{ 0x5F, 0x00, 0x5F },
    til::color{ 0x5F, 0x00, 0x87 },
    til::color{ 0x5F, 0x00, 0xAF },
    til::color{ 0x5F, 0x00, 0xD7 },
    til::color{ 0x5F, 0x00, 0xFF },
    til::color{ 0x5F, 0x5F, 0x00 },
    til::color{ 0x5F, 0x5F, 0x5F },
    til::color{ 0x5F, 0x5F, 0x87 },
    til::color{ 0x5F, 0x5F, 0xAF },
    til::color{ 0x5F, 0x5F, 0xD7 },
    til::color{ 0x5F, 0x5F, 0xFF },
    til::color{ 0x5F, 0x87, 0x00 },
    til::color{ 0x5F, 0x87, 0x5F },
    til::color{ 0x5F, 0x87, 0x87 },
    til::color{ 0x5F, 0x87, 0xAF },
    til::color{ 0x5F, 0x87, 0xD7 },
    til::color{ 0x5F, 0x87, 0xFF },
    til::color{ 0x5F, 0xAF, 0x00 },
    til::color{ 0x5F, 0xAF, 0x5F },
    til::color{ 0x5F, 0xAF, 0x87 },
    til::color{ 0x5F, 0xAF, 0xAF },
    til::color{ 0x5F, 0xAF, 0xD7 },
    til::color{ 0x5F, 0xAF, 0xFF },
    til::color{ 0x5F, 0xD7, 0x00 },
    til::color{ 0x5F, 0xD7, 0x5F },
    til::color{ 0x5F, 0xD7, 0x87 },
    til::color{ 0x5F, 0xD7, 0xAF },
    til::color{ 0x5F, 0xD7, 0xD7 },
    til::color{ 0x5F, 0xD7, 0xFF },
    til::color{ 0x5F, 0xFF, 0x00 },
    til::color{ 0x5F, 0xFF, 0x5F },
    til::color{ 0x5F, 0xFF, 0x87 },
    til::color{ 0x5F, 0xFF, 0xAF },
    til::color{ 0x5F, 0xFF, 0xD7 },
    til::color{ 0x5F, 0xFF, 0xFF },
    til::color{ 0x87, 0x00, 0x00 },
    til::color{ 0x87, 0x00, 0x5F },
    til::color{ 0x87, 0x00, 0x87 },
    til::color{ 0x87, 0x00, 0xAF },
    til::color{ 0x87, 0x00, 0xD7 },
    til::color{ 0x87, 0x00, 0xFF },
    til::color{ 0x87, 0x5F, 0x00 },
    til::color{ 0x87, 0x5F, 0x5F },
    til::color{ 0x87, 0x5F, 0x87 },
    til::color{ 0x87, 0x5F, 0xAF },
    til::color{ 0x87, 0x5F, 0xD7 },
    til::color{ 0x87, 0x5F, 0xFF },
    til::color{ 0x87, 0x87, 0x00 },
    til::color{ 0x87, 0x87, 0x5F },
    til::color{ 0x87, 0x87, 0x87 },
    til::color{ 0x87, 0x87, 0xAF },
    til::color{ 0x87, 0x87, 0xD7 },
    til::color{ 0x87, 0x87, 0xFF },
    til::color{ 0x87, 0xAF, 0x00 },
    til::color{ 0x87, 0xAF, 0x5F },
    til::color{ 0x87, 0xAF, 0x87 },
    til::color{ 0x87, 0xAF, 0xAF },
    til::color{ 0x87, 0xAF, 0xD7 },
    til::color{ 0x87, 0xAF, 0xFF },
    til::color{ 0x87, 0xD7, 0x00 },
    til::color{ 0x87, 0xD7, 0x5F },
    til::color{ 0x87, 0xD7, 0x87 },
    til::color{ 0x87, 0xD7, 0xAF },
    til::color{ 0x87, 0xD7, 0xD7 },
    til::color{ 0x87, 0xD7, 0xFF },
    til::color{ 0x87, 0xFF, 0x00 },
    til::color{ 0x87, 0xFF, 0x5F },
    til::color{ 0x87, 0xFF, 0x87 },
    til::color{ 0x87, 0xFF, 0xAF },
    til::color{ 0x87, 0xFF, 0xD7 },
    til::color{ 0x87, 0xFF, 0xFF },
    til::color{ 0xAF, 0x00, 0x00 },
    til::color{ 0xAF, 0x00, 0x5F },
    til::color{ 0xAF, 0x00, 0x87 },
    til::color{ 0xAF, 0x00, 0xAF },
    til::color{ 0xAF, 0x00, 0xD7 },
    til::color{ 0xAF, 0x00, 0xFF },
    til::color{ 0xAF, 0x5F, 0x00 },
    til::color{ 0xAF, 0x5F, 0x5F },
    til::color{ 0xAF, 0x5F, 0x87 },
    til::color{ 0xAF, 0x5F, 0xAF },
    til::color{ 0xAF, 0x5F, 0xD7 },
    til::color{ 0xAF, 0x5F, 0xFF },
    til::color{ 0xAF, 0x87, 0x00 },
    til::color{ 0xAF, 0x87, 0x5F },
    til::color{ 0xAF, 0x87, 0x87 },
    til::color{ 0xAF, 0x87, 0xAF },
    til::color{ 0xAF, 0x87, 0xD7 },
    til::color{ 0xAF, 0x87, 0xFF },
    til::color{ 0xAF, 0xAF, 0x00 },
    til::color{ 0xAF, 0xAF, 0x5F },
    til::color{ 0xAF, 0xAF, 0x87 },
    til::color{ 0xAF, 0xAF, 0xAF },
    til::color{ 0xAF, 0xAF, 0xD7 },
    til::color{ 0xAF, 0xAF, 0xFF },
    til::color{ 0xAF, 0xD7, 0x00 },
    til::color{ 0xAF, 0xD7, 0x5F },
    til::color{ 0xAF, 0xD7, 0x87 },
    til::color{ 0xAF, 0xD7, 0xAF },
    til::color{ 0xAF, 0xD7, 0xD7 },
    til::color{ 0xAF, 0xD7, 0xFF },
    til::color{ 0xAF, 0xFF, 0x00 },
    til::color{ 0xAF, 0xFF, 0x5F },
    til::color{ 0xAF, 0xFF, 0x87 },
    til::color{ 0xAF, 0xFF, 0xAF },
    til::color{ 0xAF, 0xFF, 0xD7 },
    til::color{ 0xAF, 0xFF, 0xFF },
    til::color{ 0xD7, 0x00, 0x00 },
    til::color{ 0xD7, 0x00, 0x5F },
    til::color{ 0xD7, 0x00, 0x87 },
    til::color{ 0xD7, 0x00, 0xAF },
    til::color{ 0xD7, 0x00, 0xD7 },
    til::color{ 0xD7, 0x00, 0xFF },
    til::color{ 0xD7, 0x5F, 0x00 },
    til::color{ 0xD7, 0x5F, 0x5F },
    til::color{ 0xD7, 0x5F, 0x87 },
    til::color{ 0xD7, 0x5F, 0xAF },
    til::color{ 0xD7, 0x5F, 0xD7 },
    til::color{ 0xD7, 0x5F, 0xFF },
    til::color{ 0xD7, 0x87, 0x00 },
    til::color{ 0xD7, 0x87, 0x5F },
    til::color{ 0xD7, 0x87, 0x87 },
    til::color{ 0xD7, 0x87, 0xAF },
    til::color{ 0xD7, 0x87, 0xD7 },
    til::color{ 0xD7, 0x87, 0xFF },
    til::color{ 0xD7, 0xAF, 0x00 },
    til::color{ 0xD7, 0xAF, 0x5F },
    til::color{ 0xD7, 0xAF, 0x87 },
    til::color{ 0xD7, 0xAF, 0xAF },
    til::color{ 0xD7, 0xAF, 0xD7 },
    til::color{ 0xD7, 0xAF, 0xFF },
    til::color{ 0xD7, 0xD7, 0x00 },
    til::color{ 0xD7, 0xD7, 0x5F },
    til::color{ 0xD7, 0xD7, 0x87 },
    til::color{ 0xD7, 0xD7, 0xAF },
    til::color{ 0xD7, 0xD7, 0xD7 },
    til::color{ 0xD7, 0xD7, 0xFF },
    til::color{ 0xD7, 0xFF, 0x00 },
    til::color{ 0xD7, 0xFF, 0x5F },
    til::color{ 0xD7, 0xFF, 0x87 },
    til::color{ 0xD7, 0xFF, 0xAF },
    til::color{ 0xD7, 0xFF, 0xD7 },
    til::color{ 0xD7, 0xFF, 0xFF },
    til::color{ 0xFF, 0x00, 0x00 },
    til::color{ 0xFF, 0x00, 0x5F },
    til::color{ 0xFF, 0x00, 0x87 },
    til::color{ 0xFF, 0x00, 0xAF },
    til::color{ 0xFF, 0x00, 0xD7 },
    til::color{ 0xFF, 0x00, 0xFF },
    til::color{ 0xFF, 0x5F, 0x00 },
    til::color{ 0xFF, 0x5F, 0x5F },
    til::color{ 0xFF, 0x5F, 0x87 },
    til::color{ 0xFF, 0x5F, 0xAF },
    til::color{ 0xFF, 0x5F, 0xD7 },
    til::color{ 0xFF, 0x5F, 0xFF },
    til::color{ 0xFF, 0x87, 0x00 },
    til::color{ 0xFF, 0x87, 0x5F },
    til::color{ 0xFF, 0x87, 0x87 },
    til::color{ 0xFF, 0x87, 0xAF },
    til::color{ 0xFF, 0x87, 0xD7 },
    til::color{ 0xFF, 0x87, 0xFF },
    til::color{ 0xFF, 0xAF, 0x00 },
    til::color{ 0xFF, 0xAF, 0x5F },
    til::color{ 0xFF, 0xAF, 0x87 },
    til::color{ 0xFF, 0xAF, 0xAF },
    til::color{ 0xFF, 0xAF, 0xD7 },
    til::color{ 0xFF, 0xAF, 0xFF },
    til::color{ 0xFF, 0xD7, 0x00 },
    til::color{ 0xFF, 0xD7, 0x5F },
    til::color{ 0xFF, 0xD7, 0x87 },
    til::color{ 0xFF, 0xD7, 0xAF },
    til::color{ 0xFF, 0xD7, 0xD7 },
    til::color{ 0xFF, 0xD7, 0xFF },
    til::color{ 0xFF, 0xFF, 0x00 },
    til::color{ 0xFF, 0xFF, 0x5F },
    til::color{ 0xFF, 0xFF, 0x87 },
    til::color{ 0xFF, 0xFF, 0xAF },
    til::color{ 0xFF, 0xFF, 0xD7 },
    til::color{ 0xFF, 0xFF, 0xFF },
    til::color{ 0x08, 0x08, 0x08 },
    til::color{ 0x12, 0x12, 0x12 },
    til::color{ 0x1C, 0x1C, 0x1C },
    til::color{ 0x26, 0x26, 0x26 },
    til::color{ 0x30, 0x30, 0x30 },
    til::color{ 0x3A, 0x3A, 0x3A },
    til::color{ 0x44, 0x44, 0x44 },
    til::color{ 0x4E, 0x4E, 0x4E },
    til::color{ 0x58, 0x58, 0x58 },
    til::color{ 0x62, 0x62, 0x62 },
    til::color{ 0x6C, 0x6C, 0x6C },
    til::color{ 0x76, 0x76, 0x76 },
    til::color{ 0x80, 0x80, 0x80 },
    til::color{ 0x8A, 0x8A, 0x8A },
    til::color{ 0x94, 0x94, 0x94 },
    til::color{ 0x9E, 0x9E, 0x9E },
    til::color{ 0xA8, 0xA8, 0xA8 },
    til::color{ 0xB2, 0xB2, 0xB2 },
    til::color{ 0xBC, 0xBC, 0xBC },
    til::color{ 0xC6, 0xC6, 0xC6 },
    til::color{ 0xD0, 0xD0, 0xD0 },
    til::color{ 0xDA, 0xDA, 0xDA },
    til::color{ 0xE4, 0xE4, 0xE4 },
    til::color{ 0xEE, 0xEE, 0xEE },
};

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

    std::copy(campbellColorTable.begin(), campbellColorTable.end(), table.begin());
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
    std::swap(til::at(table, 1), til::at(table, 4));
    std::swap(til::at(table, 3), til::at(table, 6));
    std::swap(til::at(table, 9), til::at(table, 12));
    std::swap(til::at(table, 11), til::at(table, 14));
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

    std::copy(standardXterm256ColorTable.begin(), standardXterm256ColorTable.end(), table.begin());
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
