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

    short ClampToShortMax(const long value, const short min);

    std::wstring GuidToString(const GUID guid);
    GUID GuidFromString(const std::wstring wstr);
    GUID CreateGuid();

    std::string ColorToHexString(const COLORREF color);
    COLORREF ColorFromHexString(const std::string wstr);

    void InitializeCampbellColorTable(gsl::span<COLORREF>& table);
    void Initialize256ColorTable(gsl::span<COLORREF>& table);
    void SetColorTableAlpha(gsl::span<COLORREF>& table, const BYTE newAlpha);

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
        return static_cast<unsigned long>(EndianSwap(static_cast<uint32_t>(value)));
    }

    constexpr GUID EndianSwap(GUID value)
    {
        value.Data1 = EndianSwap(value.Data1);
        value.Data2 = EndianSwap(value.Data2);
        value.Data3 = EndianSwap(value.Data3);
        return value;
    }

    GUID CreateV5Uuid(const GUID& namespaceGuid, const gsl::span<const gsl::byte>& name);
}
