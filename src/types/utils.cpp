// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/utils.hpp"

#include <til/string.h>
#include <wil/token_helpers.h>

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

GSL_SUPPRESS(bounds)
static std::wstring guidToStringCommon(const GUID& guid, size_t offset, size_t length)
{
    // This is just like StringFromGUID2 but with lowercase hexadecimal.
    wchar_t buffer[39];
    swprintf_s(&buffer[0], 39, L"{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}", guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    return { &buffer[offset], length };
}

// Creates a string from the given GUID in the format "{12345678-abcd-ef12-3456-7890abcdef12}".
std::wstring Utils::GuidToString(const GUID& guid)
{
    return guidToStringCommon(guid, 0, 38);
}

// Creates a string from the given GUID in the format "12345678-abcd-ef12-3456-7890abcdef12".
std::wstring Utils::GuidToPlainString(const GUID& guid)
{
    return guidToStringCommon(guid, 1, 36);
}

// Creates a GUID from a string in the format "{12345678-abcd-ef12-3456-7890abcdef12}".
// Throws if the conversion failed.
GUID Utils::GuidFromString(_Null_terminated_ const wchar_t* str)
{
    GUID result;
    THROW_IF_FAILED(IIDFromString(str, &result));
    return result;
}

// Creates a GUID from a string in the format "12345678-abcd-ef12-3456-7890abcdef12".
// Throws if the conversion failed.
//
// Side-note: An interesting quirk of this method is that the given string doesn't need to be null-terminated.
// This method could be combined with GuidFromString() so that it also doesn't require null-termination.
GSL_SUPPRESS(bounds)
GUID Utils::GuidFromPlainString(_Null_terminated_ const wchar_t* str)
{
    // Add "{}" brackets around our string, as required by IIDFromString().
    wchar_t buffer[39];
    buffer[0] = L'{';
    // This wcscpy_s() copies 36 characters and 1 terminating null.
    // The latter forces us to call this method before filling buffer[37] with '}'.
    THROW_HR_IF(CO_E_CLASSSTRING, wcscpy_s(&buffer[1], 37, str));
    buffer[37] = L'}';
    buffer[38] = L'\0';

    return GuidFromString(&buffer[0]);
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
    return fmt::format(FMT_COMPILE("#{:02X}{:02X}{:02X}"), color.r, color.g, color.b);
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

        // The "rgb:" indicator should be case-insensitive. To prevent possible issues under
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

// Function Description:
// - Returns the RGB percentage components of a given til::color value.
// Arguments:
// - color: the color being queried
// Return Value:
// - a tuple containing the three components
std::tuple<int, int, int> Utils::ColorToRGB100(const til::color color) noexcept
{
    // The color class components are in the range 0 to 255, so we
    // need to scale them by 100/255 to obtain percentage values. We
    // can optimise this conversion with a pre-created lookup table.
    static constexpr auto scale255To100 = [] {
        std::array<int8_t, 256> lut{};
        for (size_t i = 0; i < std::size(lut); i++)
        {
            lut.at(i) = gsl::narrow_cast<uint8_t>((i * 100 + 128) / 255);
        }
        return lut;
    }();

    const auto red = til::at(scale255To100, color.r);
    const auto green = til::at(scale255To100, color.g);
    const auto blue = til::at(scale255To100, color.b);
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

// Function Description:
// - Returns the HLS components of a given til::color value.
// Arguments:
// - color: the color being queried
// Return Value:
// - a tuple containing the three components
std::tuple<int, int, int> Utils::ColorToHLS(const til::color color) noexcept
{
    const auto red = color.r / 255.f;
    const auto green = color.g / 255.f;
    const auto blue = color.b / 255.f;

    // This calculation is based on the RGB to HSL algorithm described in
    // Wikipedia: https://en.wikipedia.org/wiki/HSL_and_HSV#From_RGB
    // We start by calculating the maximum and minimum component values.
    const auto maxComp = std::max({ red, green, blue });
    const auto minComp = std::min({ red, green, blue });

    // The chroma value is the range of those components.
    const auto chroma = maxComp - minComp;

    // And the luma is the middle of the range. But we're actually calculating
    // double that value here to save on a division.
    const auto luma2 = (maxComp + minComp);

    // The saturation is half the chroma value divided by min(luma, 1-luma),
    // but since the luma is already doubled, we can use the chroma as is.
    const auto divisor = std::min(luma2, 2.f - luma2);
    const auto sat = divisor > 0 ? chroma / divisor : 0.f;

    // Finally we calculate the hue, which is represented by the angle of a
    // vector to a point in a color hexagon with blue, magenta, red, yellow,
    // green, and cyan at its corners. As noted above, the DEC standard has
    // blue at 0°, red at 120°, and green at 240°, which is slightly different
    // from the way that hue is typically mapped in modern color models.
    auto hue = 0.f;
    if (chroma != 0)
    {
        if (maxComp == red)
            hue = (green - blue) / chroma + 2.f; // magenta to yellow
        else if (maxComp == green)
            hue = (blue - red) / chroma + 4.f; // yellow to cyan
        else if (maxComp == blue)
            hue = (red - green) / chroma + 6.f; // cyan to magenta
    }

    // The hue value calculated above is essentially a fractional offset from the
    // six hexagon corners, so it has to be scaled by 60 to get the angle value.
    // Luma and saturation are percentages so must be scaled by 100, but our luma
    // value is already doubled, so only needs to be scaled by 50.
    const auto h = static_cast<int>(hue * 60.f + 0.5f) % 360;
    const auto l = static_cast<int>(luma2 * 50.f + 0.5f);
    const auto s = static_cast<int>(sat * 100.f + 0.5f);
    return { h, l, s };
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

#define FileModeInformation (FILE_INFORMATION_CLASS)16

#define FILE_PIPE_BYTE_STREAM_TYPE 0x00000000
#define FILE_PIPE_BYTE_STREAM_MODE 0x00000000
#define FILE_PIPE_QUEUE_OPERATION 0x00000000

typedef struct _FILE_MODE_INFORMATION
{
    ULONG Mode;
} FILE_MODE_INFORMATION, *PFILE_MODE_INFORMATION;

extern "C" NTSTATUS NTAPI NtQueryInformationFile(
    HANDLE FileHandle,
    PIO_STATUS_BLOCK IoStatusBlock,
    PVOID FileInformation,
    ULONG Length,
    FILE_INFORMATION_CLASS FileInformationClass);

extern "C" NTSTATUS NTAPI NtCreateNamedPipeFile(
    PHANDLE FileHandle,
    ULONG DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    PIO_STATUS_BLOCK IoStatusBlock,
    ULONG ShareAccess,
    ULONG CreateDisposition,
    ULONG CreateOptions,
    ULONG NamedPipeType,
    ULONG ReadMode,
    ULONG CompletionMode,
    ULONG MaximumInstances,
    ULONG InboundQuota,
    ULONG OutboundQuota,
    PLARGE_INTEGER DefaultTimeout);

bool Utils::HandleWantsOverlappedIo(HANDLE handle) noexcept
{
    IO_STATUS_BLOCK statusBlock;
    FILE_MODE_INFORMATION modeInfo;
    const auto status = NtQueryInformationFile(handle, &statusBlock, &modeInfo, sizeof(modeInfo), FileModeInformation);
    return status == 0 && WI_AreAllFlagsClear(modeInfo.Mode, FILE_SYNCHRONOUS_IO_ALERT | FILE_SYNCHRONOUS_IO_NONALERT);
}

// Creates an anonymous pipe. Behaves like PIPE_ACCESS_INBOUND,
// meaning the .server is for reading and the .client is for writing.
Utils::Pipe Utils::CreatePipe(DWORD bufferSize)
{
    wil::unique_hfile rx, tx;
    THROW_IF_WIN32_BOOL_FALSE(::CreatePipe(rx.addressof(), tx.addressof(), nullptr, bufferSize));
    return { std::move(rx), std::move(tx) };
}

// Creates an overlapped anonymous pipe. openMode should be either:
// * PIPE_ACCESS_INBOUND
// * PIPE_ACCESS_OUTBOUND
// * PIPE_ACCESS_DUPLEX
//
// I know, I know. MSDN infamously says
// > Asynchronous (overlapped) read and write operations are not supported by anonymous pipes.
// but that's a lie. The only reason they're not supported is because the Win32
// API doesn't have a parameter where you could pass FILE_FLAG_OVERLAPPED!
// So, we'll simply use the underlying NT APIs instead.
//
// Most code on the internet suggests creating named pipes with a random name,
// but usually conveniently forgets to mention that named pipes require strict ACLs.
// https://stackoverflow.com/q/60645 for instance contains a lot of poor advice.
// Anonymous pipes also cannot be discovered via NtQueryDirectoryFile inside the NPFS driver,
// whereas running a tool like Sysinternals' PipeList will return all those semi-named pipes.
//
// The code below contains comments to create unidirectional pipes.
Utils::Pipe Utils::CreateOverlappedPipe(DWORD openMode, DWORD bufferSize)
{
    LARGE_INTEGER timeout = { .QuadPart = -10'0000'0000 }; // 1 second
    UNICODE_STRING emptyPath{};
    IO_STATUS_BLOCK statusBlock;
    OBJECT_ATTRIBUTES objectAttributes{
        .Length = sizeof(OBJECT_ATTRIBUTES),
        .ObjectName = &emptyPath,
        .Attributes = OBJ_CASE_INSENSITIVE,
    };
    DWORD serverDesiredAccess = 0;
    DWORD clientDesiredAccess = 0;
    DWORD serverShareAccess = 0;
    DWORD clientShareAccess = 0;

    switch (openMode)
    {
    case PIPE_ACCESS_INBOUND:
        serverDesiredAccess = SYNCHRONIZE | GENERIC_READ | FILE_WRITE_ATTRIBUTES;
        clientDesiredAccess = SYNCHRONIZE | GENERIC_WRITE | FILE_READ_ATTRIBUTES;
        serverShareAccess = FILE_SHARE_WRITE;
        clientShareAccess = FILE_SHARE_READ;
        break;
    case PIPE_ACCESS_OUTBOUND:
        serverDesiredAccess = SYNCHRONIZE | GENERIC_WRITE | FILE_READ_ATTRIBUTES;
        clientDesiredAccess = SYNCHRONIZE | GENERIC_READ | FILE_WRITE_ATTRIBUTES;
        serverShareAccess = FILE_SHARE_READ;
        clientShareAccess = FILE_SHARE_WRITE;
        break;
    case PIPE_ACCESS_DUPLEX:
        serverDesiredAccess = SYNCHRONIZE | GENERIC_READ | GENERIC_WRITE;
        clientDesiredAccess = SYNCHRONIZE | GENERIC_READ | GENERIC_WRITE;
        serverShareAccess = FILE_SHARE_READ | FILE_SHARE_WRITE;
        clientShareAccess = FILE_SHARE_READ | FILE_SHARE_WRITE;
        break;
    default:
        THROW_HR(E_UNEXPECTED);
    }

    // Cache a handle to the pipe driver.
    static const auto pipeDirectory = []() {
        UNICODE_STRING path = RTL_CONSTANT_STRING(L"\\Device\\NamedPipe\\");

        OBJECT_ATTRIBUTES objectAttributes{
            .Length = sizeof(OBJECT_ATTRIBUTES),
            .ObjectName = &path,
        };

        wil::unique_hfile dir;
        IO_STATUS_BLOCK statusBlock;
        THROW_IF_NTSTATUS_FAILED(NtCreateFile(
            /* FileHandle        */ dir.addressof(),
            /* DesiredAccess     */ SYNCHRONIZE | GENERIC_READ,
            /* ObjectAttributes  */ &objectAttributes,
            /* IoStatusBlock     */ &statusBlock,
            /* AllocationSize    */ nullptr,
            /* FileAttributes    */ 0,
            /* ShareAccess       */ FILE_SHARE_READ | FILE_SHARE_WRITE,
            /* CreateDisposition */ FILE_OPEN,
            /* CreateOptions     */ FILE_SYNCHRONOUS_IO_NONALERT,
            /* EaBuffer          */ nullptr,
            /* EaLength          */ 0));

        return dir;
    }();

    wil::unique_hfile server;
    objectAttributes.RootDirectory = pipeDirectory.get();
    THROW_IF_NTSTATUS_FAILED(NtCreateNamedPipeFile(
        /* FileHandle        */ server.addressof(),
        /* DesiredAccess     */ serverDesiredAccess,
        /* ObjectAttributes  */ &objectAttributes,
        /* IoStatusBlock     */ &statusBlock,
        /* ShareAccess       */ serverShareAccess,
        /* CreateDisposition */ FILE_CREATE,
        /* CreateOptions     */ 0, // would be FILE_SYNCHRONOUS_IO_NONALERT for a synchronous pipe
        /* NamedPipeType     */ FILE_PIPE_BYTE_STREAM_TYPE,
        /* ReadMode          */ FILE_PIPE_BYTE_STREAM_MODE,
        /* CompletionMode    */ FILE_PIPE_QUEUE_OPERATION, // would be FILE_PIPE_COMPLETE_OPERATION for PIPE_NOWAIT
        /* MaximumInstances  */ 1,
        /* InboundQuota      */ bufferSize,
        /* OutboundQuota     */ bufferSize,
        /* DefaultTimeout    */ &timeout));

    wil::unique_hfile client;
    objectAttributes.RootDirectory = server.get();
    THROW_IF_NTSTATUS_FAILED(NtCreateFile(
        /* FileHandle        */ client.addressof(),
        /* DesiredAccess     */ clientDesiredAccess,
        /* ObjectAttributes  */ &objectAttributes,
        /* IoStatusBlock     */ &statusBlock,
        /* AllocationSize    */ nullptr,
        /* FileAttributes    */ 0,
        /* ShareAccess       */ clientShareAccess,
        /* CreateDisposition */ FILE_OPEN,
        /* CreateOptions     */ FILE_NON_DIRECTORY_FILE, // would include FILE_SYNCHRONOUS_IO_NONALERT for a synchronous pipe
        /* EaBuffer          */ nullptr,
        /* EaLength          */ 0));

    return { std::move(server), std::move(client) };
}

// GetOverlappedResult() for professionals! Only for single-threaded use.
//
// GetOverlappedResult() used to have a neat optimization where it would only call WaitForSingleObject() if the state was STATUS_PENDING.
// That got removed in Windows 7, because people kept starting a read/write on one thread and called GetOverlappedResult() on another.
// When the OS sets Internal from STATUS_PENDING to 0 (= done) and then flags the hEvent, that doesn't happen atomically.
// This results in a race condition if a OVERLAPPED is used across threads.
HRESULT Utils::GetOverlappedResultSameThread(const OVERLAPPED* overlapped, DWORD* bytesTransferred) noexcept
{
    assert(overlapped != nullptr);
    assert(overlapped->hEvent != nullptr);
    assert(bytesTransferred != nullptr);

    __assume(overlapped != nullptr);
    __assume(overlapped->hEvent != nullptr);
    __assume(bytesTransferred != nullptr);

    if (overlapped->Internal == STATUS_PENDING)
    {
        if (WaitForSingleObjectEx(overlapped->hEvent, INFINITE, FALSE) != WAIT_OBJECT_0)
        {
            return HRESULT_FROM_WIN32(GetLastError());
        }
    }

    // Assuming no multi-threading as per the function contract and
    // now that we ensured that hEvent is set (= read/write done),
    // we can safely read whatever want because nothing will set these concurrently.
    *bytesTransferred = gsl::narrow_cast<DWORD>(overlapped->InternalHigh);
    return HRESULT_FROM_NT(overlapped->Internal);
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

                    if (!til::equals_insensitive_ascii(executablePath.parent_path().native(), systemDirectory))
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
                    fmt::format(FMT_COMPILE(LR"("{}" --cd "{}" {})"), executablePath.native(), mangledDirectory, arguments),
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

// Disable vectorization-unfriendly warnings.
#pragma warning(push)
#pragma warning(disable : 26429) // Symbol '...' is never tested for nullness, it can be marked as not_null (f.23).
#pragma warning(disable : 26472) // Don't use a static_cast for arithmetic conversions. Use brace initialization, gsl::narrow_cast or gsl::narrow (type.1).
#pragma warning(disable : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
#pragma warning(disable : 26490) // Don't use reinterpret_cast (type.1).

// Returns true for C0 characters and C1 [single-character] CSI.
constexpr bool isActionableFromGround(const wchar_t wch) noexcept
{
    // This is equivalent to:
    //   return (wch <= 0x1f) || (wch >= 0x7f && wch <= 0x9f);
    // It's written like this to get MSVC to emit optimal assembly for findActionableFromGround.
    // It lacks the ability to turn boolean operators into binary operations and also happens
    // to fail to optimize the printable-ASCII range check into a subtraction & comparison.
    return (wch <= 0x1f) | (static_cast<wchar_t>(wch - 0x7f) <= 0x20);
}

const wchar_t* Utils::FindActionableControlCharacter(const wchar_t* beg, const size_t len) noexcept
{
    auto it = beg;

    // The following vectorized code replicates isActionableFromGround which is equivalent to:
    //   (wch <= 0x1f) || (wch >= 0x7f && wch <= 0x9f)
    // or rather its more machine friendly equivalent:
    //   (wch <= 0x1f) | ((wch - 0x7f) <= 0x20)
#if defined(TIL_SSE_INTRINSICS)

    for (const auto end = beg + (len & ~size_t{ 7 }); it < end; it += 8)
    {
        const auto wch = _mm_loadu_si128(reinterpret_cast<const __m128i*>(it));
        const auto z = _mm_setzero_si128();

        // Dealing with unsigned numbers in SSE2 is annoying because it has poor support for that.
        // We'll use subtractions with saturation ("SubS") to work around that. A check like
        // a < b can be implemented as "max(0, a - b) == 0" and "max(0, a - b)" is what "SubS" is.

        // Check for (wch < 0x20)
        auto a = _mm_subs_epu16(wch, _mm_set1_epi16(0x1f));
        // Check for "((wch - 0x7f) <= 0x20)" by adding 0x10000-0x7f, which overflows to a
        // negative number if "wch >= 0x7f" and then subtracting 0x9f-0x7f with saturation to an
        // unsigned number (= can't go lower than 0), which results in all numbers up to 0x9f to be 0.
        auto b = _mm_subs_epu16(_mm_add_epi16(wch, _mm_set1_epi16(static_cast<short>(0xff81))), _mm_set1_epi16(0x20));
        a = _mm_cmpeq_epi16(a, z);
        b = _mm_cmpeq_epi16(b, z);

        const auto c = _mm_or_si128(a, b);
        const auto mask = _mm_movemask_epi8(c);

        if (mask)
        {
            unsigned long offset;
            _BitScanForward(&offset, mask);
            it += offset / 2;
            return it;
        }
    }

#elif defined(TIL_ARM_NEON_INTRINSICS)

    uint64_t mask;

    for (const auto end = beg + (len & ~size_t{ 7 });;)
    {
        if (it >= end)
        {
            goto plainSearch;
        }

        const auto wch = vld1q_u16(it);
        const auto a = vcleq_u16(wch, vdupq_n_u16(0x1f));
        const auto b = vcleq_u16(vsubq_u16(wch, vdupq_n_u16(0x7f)), vdupq_n_u16(0x20));
        const auto c = vorrq_u16(a, b);

        mask = vgetq_lane_u64(c, 0);
        if (mask)
        {
            break;
        }
        it += 4;

        mask = vgetq_lane_u64(c, 1);
        if (mask)
        {
            break;
        }
        it += 4;
    }

    unsigned long offset;
    _BitScanForward64(&offset, mask);
    it += offset / 16;
    return it;

plainSearch:

#endif

#pragma loop(no_vector)
    for (const auto end = beg + len; it < end && !isActionableFromGround(*it); ++it)
    {
    }

    return it;
}

#pragma warning(pop)

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
