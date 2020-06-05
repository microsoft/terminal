/*++
Copyright (c) Microsoft Corporation

Module Name:
- utils.hpp

Abstract:
- Helpful cross-lib utilities

Author(s):
- Mike Griese (migrie) 12-Jun-2018
--*/

namespace Microsoft::Console::Utils
{
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
    GUID GuidFromString(const std::wstring wstr);
    GUID CreateGuid();

    std::string ColorToHexString(const til::color color);
    til::color ColorFromHexString(const std::string_view wstr);

    void InitializeCampbellColorTable(const gsl::span<COLORREF> table);
    void InitializeCampbellColorTableForConhost(const gsl::span<COLORREF> table);
    void SwapANSIColorOrderForConhost(const gsl::span<COLORREF> table);
    void Initialize256ColorTable(const gsl::span<COLORREF> table);

    // Function Description:
    // - Fill the alpha byte of the colors in a given color table with the given value.
    // Arguments:
    // - table: a color table
    // - newAlpha: the new value to use as the alpha for all the entries in that table.
    // Return Value:
    // - <none>
    constexpr void SetColorTableAlpha(const gsl::span<COLORREF> table, const BYTE newAlpha) noexcept
    {
        const auto shiftedAlpha = newAlpha << 24;
        for (auto& color : table)
        {
            WI_UpdateFlagsInMask(color, 0xff000000, shiftedAlpha);
        }
    }

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

    // Method Description:
    // - Base case provided to handle the last argument to CoalesceOptionals<T...>()
    template<typename T>
    T CoalesceOptionals(const T& base)
    {
        return base;
    }

    // Method Description:
    // - Base case provided to throw an assertion if you call CoalesceOptionals(opt, opt, opt)
    template<typename T>
    T CoalesceOptionals(const std::optional<T>& base)
    {
        static_assert(false, "CoalesceOptionals must be passed a base non-optional value to be used if all optionals are empty");
        return T{};
    }

    // Method Description:
    // - Returns the value from the first populated optional, or a base value if none were populated.
    template<typename T, typename... Ts>
    T CoalesceOptionals(const std::optional<T>& t1, Ts&&... t2)
    {
        // Initially, I wanted to check "has_value" and short-circuit out so that we didn't
        // evaluate value_or for every single optional, but has_value/value emits exception handling
        // code that value_or doesn't. Less exception handling is cheaper than calling value_or a
        // few more times.
        return t1.value_or(CoalesceOptionals(std::forward<Ts>(t2)...));
    }
}
