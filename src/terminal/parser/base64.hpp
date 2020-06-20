// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

/*
Module Name:
- base64.hpp

Abstract:
- This declares standard base64 encoding and decoding, with paddings when needed.
*/

#pragma once

namespace Microsoft::Console::VirtualTerminal
{
    class Base64
    {
    public:
        static std::wstring s_Encode(const std::wstring_view src) noexcept;
        static bool s_Decode(const std::wstring_view src, std::wstring& dst) noexcept;

    private:
        static constexpr bool s_IsSpace(const wchar_t ch) noexcept;
    };
}
