// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/utils.hpp"
#include "inc/colorTable.hpp"

using namespace Microsoft::Console;

// Routine Description:
// - Determines if a character is a valid number character, 0-9.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
static constexpr bool _isNumber(const wchar_t wch) noexcept
{
    return wch >= L'0' && wch <= L'9'; // 0x30 - 0x39
}

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
GUID Utils::GuidFromString(_Null_terminated_ const wchar_t* str)
{
    GUID result;
    THROW_IF_FAILED(IIDFromString(str, &result));
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
// - Given a color string, attempts to parse the color.
//   The color are specified by name or RGB specification as per XParseColor.
// Arguments:
// - string - The string containing the color spec string to parse.
// Return Value:
// - An optional color which contains value if a color was successfully parsed
std::optional<til::color> Utils::ColorFromXTermColor(const std::wstring_view string) noexcept
{
    auto color = ColorFromXParseColorSpec(string);
    if (!color.has_value())
    {
        // Try again, but use the app color name parser
        color = ColorFromXOrgAppColorName(string);
    }

    return color;
}

// Routine Description:
// - Given a color spec string, attempts to parse the color that's encoded.
//
//   Based on the XParseColor documentation, the supported specs currently are the following:
//      spec1: a color in the following format:
//          "rgb:<red>/<green>/<blue>"
//      spec2: a color in the following format:
//          "#<red><green><blue>"
//
//   In both specs, <color> is a value contains up to 4 hex digits, upper or lower case.
// Arguments:
// - string - The string containing the color spec string to parse.
// Return Value:
// - An optional color which contains value if a color was successfully parsed
std::optional<til::color> Utils::ColorFromXParseColorSpec(const std::wstring_view string) noexcept
try
{
    bool foundXParseColorSpec = false;
    bool foundValidColorSpec = false;

    bool isSharpSignFormat = false;
    size_t rgbHexDigitCount = 0;
    std::array<unsigned int, 3> colorValues = { 0 };
    std::array<unsigned int, 3> parameterValues = { 0 };
    const auto stringSize = string.size();

    // First we look for "rgb:"
    // Other colorspaces are theoretically possible, but we don't support them.
    auto curr = string.cbegin();
    if (stringSize > 4)
    {
        auto prefix = std::wstring(string.substr(0, 4));

        // The "rgb:" indicator should be case insensitive. To prevent possible issues under
        // different locales, transform only ASCII range latin characters.
        std::transform(prefix.begin(), prefix.end(), prefix.begin(), [](const auto x) {
            return x >= L'A' && x <= L'Z' ? static_cast<wchar_t>(std::towlower(x)) : x;
        });

        if (prefix.compare(L"rgb:") == 0)
        {
            // If all the components have the same digit count, we can have one of the following formats:
            // 9 "rgb:h/h/h"
            // 12 "rgb:hh/hh/hh"
            // 15 "rgb:hhh/hhh/hhh"
            // 18 "rgb:hhhh/hhhh/hhhh"
            // Note that the component sizes aren't required to be the same.
            // Anything in between is also valid, e.g. "rgb:h/hh/h" and "rgb:h/hh/hhh".
            // Any fewer cannot be valid, and any more will be too many. Return early in this case.
            if (stringSize < 9 || stringSize > 18)
            {
                return std::nullopt;
            }

            foundXParseColorSpec = true;

            std::advance(curr, 4);
        }
    }

    // Try the sharp sign format.
    if (!foundXParseColorSpec && stringSize > 1)
    {
        if (til::at(string, 0) == L'#')
        {
            // We can have one of the following formats:
            // 4 "#hhh"
            // 7 "#hhhhhh"
            // 10 "#hhhhhhhhh"
            // 13 "#hhhhhhhhhhhh"
            // Any other cases will be invalid. Return early in this case.
            if (!(stringSize == 4 || stringSize == 7 || stringSize == 10 || stringSize == 13))
            {
                return std::nullopt;
            }

            isSharpSignFormat = true;
            foundXParseColorSpec = true;
            rgbHexDigitCount = (stringSize - 1) / 3;

            std::advance(curr, 1);
        }
    }

    // No valid spec is found. Return early.
    if (!foundXParseColorSpec)
    {
        return std::nullopt;
    }

    // Try to parse the actual color value of each component.
    for (size_t component = 0; component < 3; component++)
    {
        bool foundColor = false;
        auto& parameterValue = til::at(parameterValues, component);
        // For "sharp sign" format, the rgbHexDigitCount is known.
        // For "rgb:" format, colorspecs are up to hhhh/hhhh/hhhh, for 1-4 h's
        const auto iteration = isSharpSignFormat ? rgbHexDigitCount : 4;
        for (size_t i = 0; i < iteration && curr < string.cend(); i++)
        {
            const wchar_t wch = *curr++;

            parameterValue *= 16;
            unsigned int intVal = 0;
            const auto ret = HexToUint(wch, intVal);
            if (!ret)
            {
                // Encountered something weird oh no
                return std::nullopt;
            }

            parameterValue += intVal;

            if (isSharpSignFormat)
            {
                // If we get this far, any number can be seen as a valid part
                // of this component.
                foundColor = true;

                if (i >= rgbHexDigitCount)
                {
                    // Successfully parsed this component. Start the next one.
                    break;
                }
            }
            else
            {
                // Record the hex digit count of the current component.
                rgbHexDigitCount = i + 1;

                // If this is the first 2 component...
                if (component < 2 && curr < string.cend() && *curr == L'/')
                {
                    // ...and we have successfully parsed this component, we need
                    // to skip the delimiter before starting the next one.
                    curr++;
                    foundColor = true;
                    break;
                }
                // Or we have reached the end of the string...
                else if (curr >= string.cend())
                {
                    // ...meaning that this is the last component. We're not going to
                    // see any delimiter. We can just break out.
                    foundColor = true;
                    break;
                }
            }
        }

        if (!foundColor)
        {
            // Indicates there was some error parsing color.
            return std::nullopt;
        }

        // Calculate the actual color value based on the hex digit count.
        auto& colorValue = til::at(colorValues, component);
        const auto scaleMultiplier = isSharpSignFormat ? 0x10 : 0x11;
        const auto scaleDivisor = scaleMultiplier << 8 >> 4 * (4 - rgbHexDigitCount);
        colorValue = parameterValue * scaleMultiplier / scaleDivisor;
    }

    if (curr >= string.cend())
    {
        // We're at the end of the string and we have successfully parsed the color.
        foundValidColorSpec = true;
    }

    // Only if we find a valid colorspec can we pass it out successfully.
    if (foundValidColorSpec)
    {
        return til::color(LOBYTE(til::at(colorValues, 0)),
                          LOBYTE(til::at(colorValues, 1)),
                          LOBYTE(til::at(colorValues, 2)));
    }

    return std::nullopt;
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return std::nullopt;
}

// Routine Description:
// - Converts a hex character to its equivalent integer value.
// Arguments:
// - wch - Character to convert.
// - value - receives the int value of the char
// Return Value:
// - true iff the character is a hex character.
bool Utils::HexToUint(const wchar_t wch,
                      unsigned int& value) noexcept
{
    value = 0;
    bool success = false;
    if (wch >= L'0' && wch <= L'9')
    {
        value = wch - L'0';
        success = true;
    }
    else if (wch >= L'A' && wch <= L'F')
    {
        value = (wch - L'A') + 10;
        success = true;
    }
    else if (wch >= L'a' && wch <= L'f')
    {
        value = (wch - L'a') + 10;
        success = true;
    }
    return success;
}

// Routine Description:
// - Converts a number string to its equivalent unsigned integer value.
// Arguments:
// - wstr - String to convert.
// - value - receives the int value of the string
// Return Value:
// - true iff the string is a unsigned integer string.
bool Utils::StringToUint(const std::wstring_view wstr,
                         unsigned int& value)
{
    if (wstr.size() < 1)
    {
        return false;
    }

    unsigned int result = 0;
    size_t current = 0;
    while (current < wstr.size())
    {
        const wchar_t wch = wstr.at(current);
        if (_isNumber(wch))
        {
            result *= 10;
            result += wch - L'0';

            ++current;
        }
        else
        {
            return false;
        }
    }

    value = result;

    return true;
}

// Routine Description:
// - Split a string into different parts using the delimiter provided.
// Arguments:
// - wstr - String to split.
// - delimiter - delimiter to use.
// Return Value:
// - a vector containing the result string parts.
std::vector<std::wstring_view> Utils::SplitString(const std::wstring_view wstr,
                                                  const wchar_t delimiter) noexcept
try
{
    std::vector<std::wstring_view> result;
    size_t current = 0;
    while (current < wstr.size())
    {
        const auto nextDelimiter = wstr.find(delimiter, current);
        if (nextDelimiter == std::wstring::npos)
        {
            result.push_back(wstr.substr(current));
            break;
        }
        else
        {
            const auto length = nextDelimiter - current;
            result.push_back(wstr.substr(current, length));
            // Skip this part and the delimiter. Start the next one
            current += length + 1;
            // The next index is larger than string size, which means the string
            // is in the format of "part1;part2;" (assuming use ';' as delimiter).
            // Add the last part which is an empty string.
            if (current >= wstr.size())
            {
                result.push_back(L"");
            }
        }
    }

    return result;
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return {};
}

// Routine Description:
// - Pre-process text pasted (presumably from the clipboard) with provided option.
// Arguments:
// - wstr - String to process.
// - option - option to use.
// Return Value:
// - The result string.
std::wstring Utils::FilterStringForPaste(const std::wstring_view wstr, const FilterOption option)
{
    std::wstring filtered;
    filtered.reserve(wstr.length());

    const auto isControlCode = [](wchar_t c) {
        if (c >= L'\x20' && c < L'\x7f')
        {
            // Printable ASCII characters.
            return false;
        }

        if (c > L'\x9f')
        {
            // Not a control code.
            return false;
        }

        // All C0 & C1 control codes will be removed except HT(0x09), LF(0x0a) and CR(0x0d).
        return c != L'\x09' && c != L'\x0a' && c != L'\x0d';
    };

    std::wstring::size_type pos = 0;
    std::wstring::size_type begin = 0;

    while (pos < wstr.size())
    {
        const wchar_t c = til::at(wstr, pos);

        if (WI_IsFlagSet(option, FilterOption::CarriageReturnNewline) && c == L'\n')
        {
            // copy up to but not including the \n
            filtered.append(wstr.cbegin() + begin, wstr.cbegin() + pos);
            if (!(pos > 0 && (til::at(wstr, pos - 1) == L'\r')))
            {
                // there was no \r before the \n we did not copy,
                // so append our own \r (this effectively replaces the \n
                // with a \r)
                filtered.push_back(L'\r');
            }
            ++pos;
            begin = pos;
        }
        else if (WI_IsFlagSet(option, FilterOption::ControlCodes) && isControlCode(c))
        {
            // copy up to but not including the control code
            filtered.append(wstr.cbegin() + begin, wstr.cbegin() + pos);
            ++pos;
            begin = pos;
        }
        else
        {
            ++pos;
        }
    }

    // If we entered the while loop even once, begin would be non-zero
    // (because we set begin = pos right after incrementing pos)
    // So, if begin is still zero at this point it means we never found a newline
    // and we can just write the original string
    if (begin == 0)
    {
        return std::wstring{ wstr };
    }
    else
    {
        filtered.append(wstr.cbegin() + begin, wstr.cend());
        // we may have removed some characters, so we may not need as much space
        // as we reserved earlier
        filtered.shrink_to_fit();
        return filtered;
    }
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
