/*++
Copyright (c) Microsoft Corporation

Module Name:
- utils.hpp

Abstract:
- Helpful cross-lib utilities

Author(s):
- Mike Griese (migrie) 12-Jun-2018
--*/

#pragma once

namespace Microsoft::Console::Utils
{
    // Function Description:
    // - Returns -1, 0 or +1 to indicate the sign of the passed-in value.
    template<typename T>
    constexpr int Sign(T val) noexcept
    {
        return (T{ 0 } < val) - (val < T{ 0 });
    }

    bool IsValidHandle(const HANDLE handle) noexcept;

    // Function Description:
    // - Clamps a long in between `min` and `SHRT_MAX`
    // Arguments:
    // - value: the value to clamp
    // - min: the minimum value to clamp to
    // Return Value:
    // - The clamped value as a short.
    constexpr short ClampToShortMax(const long value, const short min) noexcept
    {
        return static_cast<short>(std::clamp(value,
                                             static_cast<long>(min),
                                             static_cast<long>(SHRT_MAX)));
    }

    std::wstring GuidToString(const GUID guid);
    GUID GuidFromString(_Null_terminated_ const wchar_t* str);
    GUID CreateGuid();

    std::string ColorToHexString(const til::color color);
    til::color ColorFromHexString(const std::string_view wstr);
    std::optional<til::color> ColorFromXTermColor(const std::wstring_view wstr) noexcept;
    std::optional<til::color> ColorFromXParseColorSpec(const std::wstring_view wstr) noexcept;

    bool HexToUint(const wchar_t wch, unsigned int& value) noexcept;
    bool StringToUint(const std::wstring_view wstr, unsigned int& value);
    std::vector<std::wstring_view> SplitString(const std::wstring_view wstr, const wchar_t delimiter) noexcept;

    enum FilterOption
    {
        None = 0,
        // Convert CR+LF and LF-only line endings to CR-only.
        CarriageReturnNewline = 1u << 0,
        // For security reasons, remove most control characters.
        ControlCodes = 1u << 1,
    };

    DEFINE_ENUM_FLAG_OPERATORS(FilterOption)

    std::wstring FilterStringForPaste(const std::wstring_view wstr, const FilterOption option);

    constexpr uint16_t EndianSwap(uint16_t value)
    {
        return (value & 0xFF00) >> 8 |
               (value & 0x00FF) << 8;
    }

    constexpr uint32_t EndianSwap(uint32_t value)
    {
        return (value & 0xFF000000) >> 24 |
               (value & 0x00FF0000) >> 8 |
               (value & 0x0000FF00) << 8 |
               (value & 0x000000FF) << 24;
    }

    constexpr unsigned long EndianSwap(unsigned long value)
    {
        return gsl::narrow_cast<unsigned long>(EndianSwap(gsl::narrow_cast<uint32_t>(value)));
    }

    constexpr GUID EndianSwap(GUID value)
    {
        value.Data1 = EndianSwap(value.Data1);
        value.Data2 = EndianSwap(value.Data2);
        value.Data3 = EndianSwap(value.Data3);
        return value;
    }

    GUID CreateV5Uuid(const GUID& namespaceGuid, const gsl::span<const gsl::byte> name);

}
