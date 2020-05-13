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
std::string Utils::ColorToHexString(const COLORREF color)
{
    std::stringstream ss;
    ss << "#" << std::uppercase << std::setfill('0') << std::hex;
    // Force the compiler to promote from byte to int. Without it, the
    // stringstream will try to write the components as chars
    ss << std::setw(2) << static_cast<int>(GetRValue(color));
    ss << std::setw(2) << static_cast<int>(GetGValue(color));
    ss << std::setw(2) << static_cast<int>(GetBValue(color));
    return ss.str();
}

// Function Description:
// - Parses a color from a string. The string should be in the format "#RRGGBB" or "#RGB"
// Arguments:
// - str: a string representation of the COLORREF to parse
// Return Value:
// - A COLORREF if the string could successfully be parsed. If the string is not
//      the correct format, throws E_INVALIDARG
COLORREF Utils::ColorFromHexString(const std::string str)
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

    return RGB(r, g, b);
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
    table[0]   = RGB( 12,   12,   12);
    table[1]   = RGB( 197,  15,   31);
    table[2]   = RGB( 19,   161,  14);
    table[3]   = RGB( 193,  156,  0);
    table[4]   = RGB( 0,    55,   218);
    table[5]   = RGB( 136,  23,   152);
    table[6]   = RGB( 58,   150,  221);
    table[7]   = RGB( 204,  204,  204);
    table[8]   = RGB( 118,  118,  118);
    table[9]   = RGB( 231,  72,   86);
    table[10]  = RGB( 22,   198,  12);
    table[11]  = RGB( 249,  241,  165);
    table[12]  = RGB( 59,   120,  255);
    table[13]  = RGB( 180,  0,    158);
    table[14]  = RGB( 97,   214,  214);
    table[15]  = RGB( 242,  242,  242);
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
    std::swap(table[1], table[4]);
    std::swap(table[3], table[6]);
    std::swap(table[9], table[12]);
    std::swap(table[11], table[14]);
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
    table[0]   = RGB( 0x00, 0x00, 0x00);
    table[1]   = RGB( 0x80, 0x00, 0x00);
    table[2]   = RGB( 0x00, 0x80, 0x00);
    table[3]   = RGB( 0x80, 0x80, 0x00);
    table[4]   = RGB( 0x00, 0x00, 0x80);
    table[5]   = RGB( 0x80, 0x00, 0x80);
    table[6]   = RGB( 0x00, 0x80, 0x80);
    table[7]   = RGB( 0xc0, 0xc0, 0xc0);
    table[8]   = RGB( 0x80, 0x80, 0x80);
    table[9]   = RGB( 0xff, 0x00, 0x00);
    table[10]  = RGB( 0x00, 0xff, 0x00);
    table[11]  = RGB( 0xff, 0xff, 0x00);
    table[12]  = RGB( 0x00, 0x00, 0xff);
    table[13]  = RGB( 0xff, 0x00, 0xff);
    table[14]  = RGB( 0x00, 0xff, 0xff);
    table[15]  = RGB( 0xff, 0xff, 0xff);
    table[16]  = RGB( 0x00, 0x00, 0x00);
    table[17]  = RGB( 0x00, 0x00, 0x5f);
    table[18]  = RGB( 0x00, 0x00, 0x87);
    table[19]  = RGB( 0x00, 0x00, 0xaf);
    table[20]  = RGB( 0x00, 0x00, 0xd7);
    table[21]  = RGB( 0x00, 0x00, 0xff);
    table[22]  = RGB( 0x00, 0x5f, 0x00);
    table[23]  = RGB( 0x00, 0x5f, 0x5f);
    table[24]  = RGB( 0x00, 0x5f, 0x87);
    table[25]  = RGB( 0x00, 0x5f, 0xaf);
    table[26]  = RGB( 0x00, 0x5f, 0xd7);
    table[27]  = RGB( 0x00, 0x5f, 0xff);
    table[28]  = RGB( 0x00, 0x87, 0x00);
    table[29]  = RGB( 0x00, 0x87, 0x5f);
    table[30]  = RGB( 0x00, 0x87, 0x87);
    table[31]  = RGB( 0x00, 0x87, 0xaf);
    table[32]  = RGB( 0x00, 0x87, 0xd7);
    table[33]  = RGB( 0x00, 0x87, 0xff);
    table[34]  = RGB( 0x00, 0xaf, 0x00);
    table[35]  = RGB( 0x00, 0xaf, 0x5f);
    table[36]  = RGB( 0x00, 0xaf, 0x87);
    table[37]  = RGB( 0x00, 0xaf, 0xaf);
    table[38]  = RGB( 0x00, 0xaf, 0xd7);
    table[39]  = RGB( 0x00, 0xaf, 0xff);
    table[40]  = RGB( 0x00, 0xd7, 0x00);
    table[41]  = RGB( 0x00, 0xd7, 0x5f);
    table[42]  = RGB( 0x00, 0xd7, 0x87);
    table[43]  = RGB( 0x00, 0xd7, 0xaf);
    table[44]  = RGB( 0x00, 0xd7, 0xd7);
    table[45]  = RGB( 0x00, 0xd7, 0xff);
    table[46]  = RGB( 0x00, 0xff, 0x00);
    table[47]  = RGB( 0x00, 0xff, 0x5f);
    table[48]  = RGB( 0x00, 0xff, 0x87);
    table[49]  = RGB( 0x00, 0xff, 0xaf);
    table[50]  = RGB( 0x00, 0xff, 0xd7);
    table[51]  = RGB( 0x00, 0xff, 0xff);
    table[52]  = RGB( 0x5f, 0x00, 0x00);
    table[53]  = RGB( 0x5f, 0x00, 0x5f);
    table[54]  = RGB( 0x5f, 0x00, 0x87);
    table[55]  = RGB( 0x5f, 0x00, 0xaf);
    table[56]  = RGB( 0x5f, 0x00, 0xd7);
    table[57]  = RGB( 0x5f, 0x00, 0xff);
    table[58]  = RGB( 0x5f, 0x5f, 0x00);
    table[59]  = RGB( 0x5f, 0x5f, 0x5f);
    table[60]  = RGB( 0x5f, 0x5f, 0x87);
    table[61]  = RGB( 0x5f, 0x5f, 0xaf);
    table[62]  = RGB( 0x5f, 0x5f, 0xd7);
    table[63]  = RGB( 0x5f, 0x5f, 0xff);
    table[64]  = RGB( 0x5f, 0x87, 0x00);
    table[65]  = RGB( 0x5f, 0x87, 0x5f);
    table[66]  = RGB( 0x5f, 0x87, 0x87);
    table[67]  = RGB( 0x5f, 0x87, 0xaf);
    table[68]  = RGB( 0x5f, 0x87, 0xd7);
    table[69]  = RGB( 0x5f, 0x87, 0xff);
    table[70]  = RGB( 0x5f, 0xaf, 0x00);
    table[71]  = RGB( 0x5f, 0xaf, 0x5f);
    table[72]  = RGB( 0x5f, 0xaf, 0x87);
    table[73]  = RGB( 0x5f, 0xaf, 0xaf);
    table[74]  = RGB( 0x5f, 0xaf, 0xd7);
    table[75]  = RGB( 0x5f, 0xaf, 0xff);
    table[76]  = RGB( 0x5f, 0xd7, 0x00);
    table[77]  = RGB( 0x5f, 0xd7, 0x5f);
    table[78]  = RGB( 0x5f, 0xd7, 0x87);
    table[79]  = RGB( 0x5f, 0xd7, 0xaf);
    table[80]  = RGB( 0x5f, 0xd7, 0xd7);
    table[81]  = RGB( 0x5f, 0xd7, 0xff);
    table[82]  = RGB( 0x5f, 0xff, 0x00);
    table[83]  = RGB( 0x5f, 0xff, 0x5f);
    table[84]  = RGB( 0x5f, 0xff, 0x87);
    table[85]  = RGB( 0x5f, 0xff, 0xaf);
    table[86]  = RGB( 0x5f, 0xff, 0xd7);
    table[87]  = RGB( 0x5f, 0xff, 0xff);
    table[88]  = RGB( 0x87, 0x00, 0x00);
    table[89]  = RGB( 0x87, 0x00, 0x5f);
    table[90]  = RGB( 0x87, 0x00, 0x87);
    table[91]  = RGB( 0x87, 0x00, 0xaf);
    table[92]  = RGB( 0x87, 0x00, 0xd7);
    table[93]  = RGB( 0x87, 0x00, 0xff);
    table[94]  = RGB( 0x87, 0x5f, 0x00);
    table[95]  = RGB( 0x87, 0x5f, 0x5f);
    table[96]  = RGB( 0x87, 0x5f, 0x87);
    table[97]  = RGB( 0x87, 0x5f, 0xaf);
    table[98]  = RGB( 0x87, 0x5f, 0xd7);
    table[99]  = RGB( 0x87, 0x5f, 0xff);
    table[100] = RGB(0x87, 0x87, 0x00);
    table[101] = RGB(0x87, 0x87, 0x5f);
    table[102] = RGB(0x87, 0x87, 0x87);
    table[103] = RGB(0x87, 0x87, 0xaf);
    table[104] = RGB(0x87, 0x87, 0xd7);
    table[105] = RGB(0x87, 0x87, 0xff);
    table[106] = RGB(0x87, 0xaf, 0x00);
    table[107] = RGB(0x87, 0xaf, 0x5f);
    table[108] = RGB(0x87, 0xaf, 0x87);
    table[109] = RGB(0x87, 0xaf, 0xaf);
    table[110] = RGB(0x87, 0xaf, 0xd7);
    table[111] = RGB(0x87, 0xaf, 0xff);
    table[112] = RGB(0x87, 0xd7, 0x00);
    table[113] = RGB(0x87, 0xd7, 0x5f);
    table[114] = RGB(0x87, 0xd7, 0x87);
    table[115] = RGB(0x87, 0xd7, 0xaf);
    table[116] = RGB(0x87, 0xd7, 0xd7);
    table[117] = RGB(0x87, 0xd7, 0xff);
    table[118] = RGB(0x87, 0xff, 0x00);
    table[119] = RGB(0x87, 0xff, 0x5f);
    table[120] = RGB(0x87, 0xff, 0x87);
    table[121] = RGB(0x87, 0xff, 0xaf);
    table[122] = RGB(0x87, 0xff, 0xd7);
    table[123] = RGB(0x87, 0xff, 0xff);
    table[124] = RGB(0xaf, 0x00, 0x00);
    table[125] = RGB(0xaf, 0x00, 0x5f);
    table[126] = RGB(0xaf, 0x00, 0x87);
    table[127] = RGB(0xaf, 0x00, 0xaf);
    table[128] = RGB(0xaf, 0x00, 0xd7);
    table[129] = RGB(0xaf, 0x00, 0xff);
    table[130] = RGB(0xaf, 0x5f, 0x00);
    table[131] = RGB(0xaf, 0x5f, 0x5f);
    table[132] = RGB(0xaf, 0x5f, 0x87);
    table[133] = RGB(0xaf, 0x5f, 0xaf);
    table[134] = RGB(0xaf, 0x5f, 0xd7);
    table[135] = RGB(0xaf, 0x5f, 0xff);
    table[136] = RGB(0xaf, 0x87, 0x00);
    table[137] = RGB(0xaf, 0x87, 0x5f);
    table[138] = RGB(0xaf, 0x87, 0x87);
    table[139] = RGB(0xaf, 0x87, 0xaf);
    table[140] = RGB(0xaf, 0x87, 0xd7);
    table[141] = RGB(0xaf, 0x87, 0xff);
    table[142] = RGB(0xaf, 0xaf, 0x00);
    table[143] = RGB(0xaf, 0xaf, 0x5f);
    table[144] = RGB(0xaf, 0xaf, 0x87);
    table[145] = RGB(0xaf, 0xaf, 0xaf);
    table[146] = RGB(0xaf, 0xaf, 0xd7);
    table[147] = RGB(0xaf, 0xaf, 0xff);
    table[148] = RGB(0xaf, 0xd7, 0x00);
    table[149] = RGB(0xaf, 0xd7, 0x5f);
    table[150] = RGB(0xaf, 0xd7, 0x87);
    table[151] = RGB(0xaf, 0xd7, 0xaf);
    table[152] = RGB(0xaf, 0xd7, 0xd7);
    table[153] = RGB(0xaf, 0xd7, 0xff);
    table[154] = RGB(0xaf, 0xff, 0x00);
    table[155] = RGB(0xaf, 0xff, 0x5f);
    table[156] = RGB(0xaf, 0xff, 0x87);
    table[157] = RGB(0xaf, 0xff, 0xaf);
    table[158] = RGB(0xaf, 0xff, 0xd7);
    table[159] = RGB(0xaf, 0xff, 0xff);
    table[160] = RGB(0xd7, 0x00, 0x00);
    table[161] = RGB(0xd7, 0x00, 0x5f);
    table[162] = RGB(0xd7, 0x00, 0x87);
    table[163] = RGB(0xd7, 0x00, 0xaf);
    table[164] = RGB(0xd7, 0x00, 0xd7);
    table[165] = RGB(0xd7, 0x00, 0xff);
    table[166] = RGB(0xd7, 0x5f, 0x00);
    table[167] = RGB(0xd7, 0x5f, 0x5f);
    table[168] = RGB(0xd7, 0x5f, 0x87);
    table[169] = RGB(0xd7, 0x5f, 0xaf);
    table[170] = RGB(0xd7, 0x5f, 0xd7);
    table[171] = RGB(0xd7, 0x5f, 0xff);
    table[172] = RGB(0xd7, 0x87, 0x00);
    table[173] = RGB(0xd7, 0x87, 0x5f);
    table[174] = RGB(0xd7, 0x87, 0x87);
    table[175] = RGB(0xd7, 0x87, 0xaf);
    table[176] = RGB(0xd7, 0x87, 0xd7);
    table[177] = RGB(0xd7, 0x87, 0xff);
    table[178] = RGB(0xdf, 0xaf, 0x00);
    table[179] = RGB(0xdf, 0xaf, 0x5f);
    table[180] = RGB(0xdf, 0xaf, 0x87);
    table[181] = RGB(0xdf, 0xaf, 0xaf);
    table[182] = RGB(0xdf, 0xaf, 0xd7);
    table[183] = RGB(0xdf, 0xaf, 0xff);
    table[184] = RGB(0xdf, 0xd7, 0x00);
    table[185] = RGB(0xdf, 0xd7, 0x5f);
    table[186] = RGB(0xdf, 0xd7, 0x87);
    table[187] = RGB(0xdf, 0xd7, 0xaf);
    table[188] = RGB(0xdf, 0xd7, 0xd7);
    table[189] = RGB(0xdf, 0xd7, 0xff);
    table[190] = RGB(0xdf, 0xff, 0x00);
    table[191] = RGB(0xdf, 0xff, 0x5f);
    table[192] = RGB(0xdf, 0xff, 0x87);
    table[193] = RGB(0xdf, 0xff, 0xaf);
    table[194] = RGB(0xdf, 0xff, 0xd7);
    table[195] = RGB(0xdf, 0xff, 0xff);
    table[196] = RGB(0xff, 0x00, 0x00);
    table[197] = RGB(0xff, 0x00, 0x5f);
    table[198] = RGB(0xff, 0x00, 0x87);
    table[199] = RGB(0xff, 0x00, 0xaf);
    table[200] = RGB(0xff, 0x00, 0xd7);
    table[201] = RGB(0xff, 0x00, 0xff);
    table[202] = RGB(0xff, 0x5f, 0x00);
    table[203] = RGB(0xff, 0x5f, 0x5f);
    table[204] = RGB(0xff, 0x5f, 0x87);
    table[205] = RGB(0xff, 0x5f, 0xaf);
    table[206] = RGB(0xff, 0x5f, 0xd7);
    table[207] = RGB(0xff, 0x5f, 0xff);
    table[208] = RGB(0xff, 0x87, 0x00);
    table[209] = RGB(0xff, 0x87, 0x5f);
    table[210] = RGB(0xff, 0x87, 0x87);
    table[211] = RGB(0xff, 0x87, 0xaf);
    table[212] = RGB(0xff, 0x87, 0xd7);
    table[213] = RGB(0xff, 0x87, 0xff);
    table[214] = RGB(0xff, 0xaf, 0x00);
    table[215] = RGB(0xff, 0xaf, 0x5f);
    table[216] = RGB(0xff, 0xaf, 0x87);
    table[217] = RGB(0xff, 0xaf, 0xaf);
    table[218] = RGB(0xff, 0xaf, 0xd7);
    table[219] = RGB(0xff, 0xaf, 0xff);
    table[220] = RGB(0xff, 0xd7, 0x00);
    table[221] = RGB(0xff, 0xd7, 0x5f);
    table[222] = RGB(0xff, 0xd7, 0x87);
    table[223] = RGB(0xff, 0xd7, 0xaf);
    table[224] = RGB(0xff, 0xd7, 0xd7);
    table[225] = RGB(0xff, 0xd7, 0xff);
    table[226] = RGB(0xff, 0xff, 0x00);
    table[227] = RGB(0xff, 0xff, 0x5f);
    table[228] = RGB(0xff, 0xff, 0x87);
    table[229] = RGB(0xff, 0xff, 0xaf);
    table[230] = RGB(0xff, 0xff, 0xd7);
    table[231] = RGB(0xff, 0xff, 0xff);
    table[232] = RGB(0x08, 0x08, 0x08);
    table[233] = RGB(0x12, 0x12, 0x12);
    table[234] = RGB(0x1c, 0x1c, 0x1c);
    table[235] = RGB(0x26, 0x26, 0x26);
    table[236] = RGB(0x30, 0x30, 0x30);
    table[237] = RGB(0x3a, 0x3a, 0x3a);
    table[238] = RGB(0x44, 0x44, 0x44);
    table[239] = RGB(0x4e, 0x4e, 0x4e);
    table[240] = RGB(0x58, 0x58, 0x58);
    table[241] = RGB(0x62, 0x62, 0x62);
    table[242] = RGB(0x6c, 0x6c, 0x6c);
    table[243] = RGB(0x76, 0x76, 0x76);
    table[244] = RGB(0x80, 0x80, 0x80);
    table[245] = RGB(0x8a, 0x8a, 0x8a);
    table[246] = RGB(0x94, 0x94, 0x94);
    table[247] = RGB(0x9e, 0x9e, 0x9e);
    table[248] = RGB(0xa8, 0xa8, 0xa8);
    table[249] = RGB(0xb2, 0xb2, 0xb2);
    table[250] = RGB(0xbc, 0xbc, 0xbc);
    table[251] = RGB(0xc6, 0xc6, 0xc6);
    table[252] = RGB(0xd0, 0xd0, 0xd0);
    table[253] = RGB(0xda, 0xda, 0xda);
    table[254] = RGB(0xe4, 0xe4, 0xe4);
    table[255] = RGB(0xee, 0xee, 0xee);
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
