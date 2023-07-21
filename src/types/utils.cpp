// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/utils.hpp"
#include "inc/colorTable.hpp"

#include <wil/token_helpers.h>
#include <til/string.h>

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
    THROW_HR_IF(E_INVALIDARG, str.size() != 9 && str.size() != 7 && str.size() != 4);
    THROW_HR_IF(E_INVALIDARG, str.at(0) != '#');

    std::string rStr;
    std::string gStr;
    std::string bStr;
    std::string aStr;

    if (str.size() == 4)
    {
        rStr = std::string(2, str.at(1));
        gStr = std::string(2, str.at(2));
        bStr = std::string(2, str.at(3));
        aStr = "ff";
    }
    else if (str.size() == 7)
    {
        rStr = std::string(&str.at(1), 2);
        gStr = std::string(&str.at(3), 2);
        bStr = std::string(&str.at(5), 2);
        aStr = "ff";
    }
    else if (str.size() == 9)
    {
        // #rrggbbaa
        rStr = std::string(&str.at(1), 2);
        gStr = std::string(&str.at(3), 2);
        bStr = std::string(&str.at(5), 2);
        aStr = std::string(&str.at(7), 2);
    }

    const auto r = gsl::narrow_cast<BYTE>(std::stoul(rStr, nullptr, 16));
    const auto g = gsl::narrow_cast<BYTE>(std::stoul(gStr, nullptr, 16));
    const auto b = gsl::narrow_cast<BYTE>(std::stoul(bStr, nullptr, 16));
    const auto a = gsl::narrow_cast<BYTE>(std::stoul(aStr, nullptr, 16));

    return til::color{ r, g, b, a };
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
    auto foundXParseColorSpec = false;
    auto foundValidColorSpec = false;

    auto isSharpSignFormat = false;
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
        auto foundColor = false;
        auto& parameterValue = til::at(parameterValues, component);
        // For "sharp sign" format, the rgbHexDigitCount is known.
        // For "rgb:" format, colorspecs are up to hhhh/hhhh/hhhh, for 1-4 h's
        const auto iteration = isSharpSignFormat ? rgbHexDigitCount : 4;
        for (size_t i = 0; i < iteration && curr < string.cend(); i++)
        {
            const auto wch = *curr++;

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
// - Constructs a til::color value from RGB percentage components.
// Arguments:
// - r - The red component of the color (0-100%).
// - g - The green component of the color (0-100%).
// - b - The blue component of the color (0-100%).
// Return Value:
// - The color defined by the given components.
til::color Utils::ColorFromRGB100(const int r, const int g, const int b) noexcept
{
    // The color class is expecting components in the range 0 to 255,
    // so we need to scale our percentage values by 255/100. We can
    // optimise this conversion with a pre-created lookup table.
    static constexpr auto scale100To255 = [] {
        std::array<uint8_t, 101> lut{};
        for (size_t i = 0; i < std::size(lut); i++)
        {
            lut.at(i) = gsl::narrow_cast<uint8_t>((i * 255 + 50) / 100);
        }
        return lut;
    }();

    const auto red = til::at(scale100To255, std::min<unsigned>(r, 100u));
    const auto green = til::at(scale100To255, std::min<unsigned>(g, 100u));
    const auto blue = til::at(scale100To255, std::min<unsigned>(b, 100u));
    return { red, green, blue };
}

// Routine Description:
// - Constructs a til::color value from HLS components.
// Arguments:
// - h - The hue component of the color (0-360°).
// - l - The luminosity component of the color (0-100%).
// - s - The saturation component of the color (0-100%).
// Return Value:
// - The color defined by the given components.
til::color Utils::ColorFromHLS(const int h, const int l, const int s) noexcept
{
    const auto hue = h % 360;
    const auto lum = gsl::narrow_cast<float>(std::min(l, 100));
    const auto sat = gsl::narrow_cast<float>(std::min(s, 100));

    // This calculation is based on the HSL to RGB algorithm described in
    // Wikipedia: https://en.wikipedia.org/wiki/HSL_and_HSV#HSL_to_RGB
    // We start by calculating the chroma value, and the point along the bottom
    // faces of the RGB cube with the same hue and chroma as our color (x).
    const auto chroma = (50.f - abs(lum - 50.f)) * sat / 50.f;
    const auto x = chroma * (60 - abs(hue % 120 - 60)) / 60.f;

    // We'll also need an offset added to each component to match lightness.
    const auto lightness = lum - chroma / 2.0f;

    // We use the chroma value for the brightest component, x for the second
    // brightest, and 0 for the last. The values  are scaled by 255/100 to get
    // them in the range 0 to 255, as required by the color class.
    constexpr auto scale = 255.f / 100.f;
    const auto comp1 = gsl::narrow_cast<uint8_t>((chroma + lightness) * scale + 0.5f);
    const auto comp2 = gsl::narrow_cast<uint8_t>((x + lightness) * scale + 0.5f);
    const auto comp3 = gsl::narrow_cast<uint8_t>((0 + lightness) * scale + 0.5f);

    // Finally we order the components based on the given hue. But note that the
    // DEC terminals used a different mapping for hue than is typical for modern
    // color models. Blue is at 0°, red is at 120°, and green is at 240°.
    // See DEC STD 070, ReGIS Graphics Extension, § 8.6.2.2.2, Color by Value.
    if (hue < 60)
        return { comp2, comp3, comp1 }; // blue to magenta
    else if (hue < 120)
        return { comp1, comp3, comp2 }; // magenta to red
    else if (hue < 180)
        return { comp1, comp2, comp3 }; // red to yellow
    else if (hue < 240)
        return { comp2, comp1, comp3 }; // yellow to green
    else if (hue < 300)
        return { comp3, comp1, comp2 }; // green to cyan
    else
        return { comp3, comp2, comp1 }; // cyan to blue
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
    auto success = false;
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
        const auto wch = wstr.at(current);
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
        const auto c = til::at(wstr, pos);

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
GUID Utils::CreateV5Uuid(const GUID& namespaceGuid, const std::span<const std::byte> name)
{
    // v5 uuid generation happens over values in network byte order, so let's enforce that
    auto correctEndianNamespaceGuid{ EndianSwap(namespaceGuid) };

    wil::unique_bcrypt_hash hash;
    THROW_IF_NTSTATUS_FAILED(BCryptCreateHash(BCRYPT_SHA1_ALG_HANDLE, &hash, nullptr, 0, nullptr, 0, 0));

    // According to N4713 8.2.1.11 [basic.lval], accessing the bytes underlying an object
    // through unsigned char or char pointer *is defined*.
    THROW_IF_NTSTATUS_FAILED(BCryptHashData(hash.get(), reinterpret_cast<PUCHAR>(&correctEndianNamespaceGuid), sizeof(GUID), 0));
    // BCryptHashData is ill-specified in that it leaves off "const" qualification for pbInput
    THROW_IF_NTSTATUS_FAILED(BCryptHashData(hash.get(), reinterpret_cast<PUCHAR>(const_cast<std::byte*>(name.data())), gsl::narrow<ULONG>(name.size()), 0));

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

// * Elevated users cannot use the modern drag drop experience. This is
//   specifically normal users running the Terminal as admin
// * The Default Administrator, who does not have a split token, CAN drag drop
//   perfectly fine. So in that case, we want to return false.
// * This has to be kept separate from IsRunningElevated, which is exclusively
//   used for "is this instance running as admin".
bool Utils::CanUwpDragDrop()
{
    // There's a lot of wacky double negatives here so that the logic is
    // basically the same as IsRunningElevated, but the end result semantically
    // makes sense as "CanDragDrop".
    static auto isDragDropBroken = []() {
        try
        {
            wil::unique_handle processToken{ GetCurrentProcessToken() };
            const auto elevationType = wil::get_token_information<TOKEN_ELEVATION_TYPE>(processToken.get());
            const auto elevationState = wil::get_token_information<TOKEN_ELEVATION>(processToken.get());
            if (elevationType == TokenElevationTypeDefault && elevationState.TokenIsElevated)
            {
                // In this case, the user has UAC entirely disabled. This is sort of
                // weird, we treat this like the user isn't an admin at all. There's no
                // separation of powers, so the things we normally want to gate on
                // "having special powers" doesn't apply.
                //
                // See GH#7754, GH#11096
                return false;
                // drag drop is _not_ broken -> they _can_ drag drop
            }

            // If they are running admin, they cannot drag drop.
            return wil::test_token_membership(nullptr, SECURITY_NT_AUTHORITY, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            // This failed? That's very peculiar indeed. Let's err on the side
            // of "drag drop is broken", just in case.
            return true;
        }
    }();

    return !isDragDropBroken;
}

// See CanUwpDragDrop, GH#13928 for why this is different.
bool Utils::IsRunningElevated()
{
    static auto isElevated = []() {
        try
        {
            return wil::test_token_membership(nullptr, SECURITY_NT_AUTHORITY, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return false;
        }
    }();
    return isElevated;
}

// Function Description:
// - Promotes a starting directory provided to a WSL invocation to a commandline argument.
//   This is necessary because WSL has some modicum of support for linux-side directories (!) which
//   CreateProcess never will.
std::tuple<std::wstring, std::wstring> Utils::MangleStartingDirectoryForWSL(std::wstring_view commandLine,
                                                                            std::wstring_view startingDirectory)
{
    do
    {
        if (startingDirectory.size() > 0 && commandLine.size() >= 3)
        { // "wsl" is three characters; this is a safe bet. no point in doing it if there's no starting directory though!
            // Find the first space, quote or the end of the string -- we'll look for wsl before that.
            const auto terminator{ commandLine.find_first_of(LR"(" )", 1) }; // look past the first character in case it starts with "
            const auto start{ til::at(commandLine, 0) == L'"' ? 1 : 0 };
            const std::filesystem::path executablePath{ commandLine.substr(start, terminator - start) };
            const auto executableFilename{ executablePath.filename() };
            if (executableFilename == L"wsl" || executableFilename == L"wsl.exe")
            {
                // We've got a WSL -- let's just make sure it's the right one.
                if (executablePath.has_parent_path())
                {
                    std::wstring systemDirectory{};
                    if (FAILED(wil::GetSystemDirectoryW(systemDirectory)))
                    {
                        break; // just bail out.
                    }

                    if (!til::equals_insensitive_ascii(executablePath.parent_path().c_str(), systemDirectory))
                    {
                        break; // it wasn't in system32!
                    }
                }
                else
                {
                    // assume that unqualified WSL is the one in system32 (minor danger)
                }

                const auto arguments{ terminator == std::wstring_view::npos ? std::wstring_view{} : commandLine.substr(terminator + 1) };
                if (arguments.find(L"--cd") != std::wstring_view::npos)
                {
                    break; // they've already got a --cd!
                }

                const auto tilde{ arguments.find_first_of(L'~') };
                if (tilde != std::wstring_view::npos)
                {
                    if (tilde + 1 == arguments.size() || til::at(arguments, tilde + 1) == L' ')
                    {
                        // We want to suppress --cd if they have added a bare ~ to their commandline (they conflict).
                        break;
                    }
                    // Tilde followed by non-space should be okay (like, wsl -d Debian ~/blah.sh)
                }

                // GH#11994 - If the path starts with //wsl$, then the user is
                // likely passing a Windows-style path to the WSL filesystem,
                // but with forward slashes instead of backslashes.
                // Unfortunately, `wsl --cd` will try to treat this as a
                // linux-relative path, which will fail to do the expected
                // thing.
                //
                // In that case, manually mangle the startingDirectory to use
                // backslashes as the path separator instead.
                std::wstring mangledDirectory{ startingDirectory };
                if (til::starts_with(mangledDirectory, L"//wsl$") || til::starts_with(mangledDirectory, L"//wsl.localhost"))
                {
                    mangledDirectory = std::filesystem::path{ startingDirectory }.make_preferred().wstring();
                }

                return {
                    fmt::format(LR"("{}" --cd "{}" {})", executablePath.native(), mangledDirectory, arguments),
                    std::wstring{}
                };
            }
        }
    } while (false);

    // GH #12353: `~` is never a valid windows path. We can only accept that as
    // a startingDirectory when the exe is specifically wsl.exe, because that
    // can override the real startingDirectory. If the user set the
    // startingDirectory to ~, but the commandline to something like pwsh.exe,
    // that won't actually work. In that case, mangle the startingDirectory to
    // %userprofile%, so it's at least something reasonable.
    return {
        std::wstring{ commandLine },
        startingDirectory == L"~" ? wil::ExpandEnvironmentStringsW<std::wstring>(L"%USERPROFILE%") :
                                    std::wstring{ startingDirectory }
    };
}

std::wstring_view Utils::TrimPaste(std::wstring_view textView) noexcept
{
    const auto lastNonSpace = textView.find_last_not_of(L"\t\n\v\f\r ");
    const auto firstNewline = textView.find_first_of(L"\n\v\f\r");

    const bool isOnlyWhitespace = lastNonSpace == textView.npos;
    const bool isMultiline = firstNewline < lastNonSpace;

    if (isOnlyWhitespace)
    {
        // Text is all white space, nothing to paste
        return L"";
    }

    if (isMultiline)
    {
        // In this case, the user totally wanted to paste multiple lines of text,
        // and that likely includes the trailing newline.
        // DON'T trim it in this case.
        return textView;
    }

    return textView.substr(0, lastNonSpace + 1);
}

std::wstring Utils::EvaluateStartingDirectory(
    std::wstring_view currentDirectory,
    std::wstring_view startingDirectory)
{
    std::wstring resultPath{ startingDirectory };

    // We only want to resolve the new WD against the CWD if it doesn't look
    // like a Linux path (see GH#592)

    // Append only if it DOESN'T look like a linux-y path. A linux-y path starts
    // with `~` or `/`.
    const bool looksLikeLinux =
        resultPath.size() >= 1 &&
        (til::at(resultPath, 0) == L'~' || til::at(resultPath, 0) == L'/');

    if (!looksLikeLinux)
    {
        std::filesystem::path cwd{ currentDirectory };
        cwd /= startingDirectory;
        resultPath = cwd.wstring();
    }
    return resultPath;
}

bool Utils::IsWindows11() noexcept
{
    static const bool isWindows11 = []() noexcept {
        OSVERSIONINFOEXW osver{};
        osver.dwOSVersionInfoSize = sizeof(osver);
        osver.dwBuildNumber = 22000;

        DWORDLONG dwlConditionMask = 0;
        VER_SET_CONDITION(dwlConditionMask, VER_BUILDNUMBER, VER_GREATER_EQUAL);

        if (VerifyVersionInfoW(&osver, VER_BUILDNUMBER, dwlConditionMask) != FALSE)
        {
            return true;
        }
        return false;
    }();
    return isWindows11;
}
