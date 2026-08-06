/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- FontResource.hpp

Abstract:
- This manages the construction of in-memory font resources for the VT soft fonts.
--*/

#pragma once

namespace wil
{
    typedef unique_any<HANDLE, decltype(&::RemoveFontMemResourceEx), ::RemoveFontMemResourceEx> unique_hfontresource;
}

namespace Microsoft::Console::Render
{
    class FontResource
    {
    public:
        FontResource(const std::span<const uint16_t> bitPattern,
                     const til::size sourceSize,
                     const til::size targetSize,
                     const size_t centeringHint);
        FontResource() = default;
        ~FontResource() = default;
        FontResource& operator=(FontResource&&) = default;
        void SetTargetSize(const til::size targetSize);
        operator HFONT();

    private:
        void _regenerateFont();
        void _resizeBitPattern(std::span<byte> targetBuffer);

        std::vector<uint16_t> _bitPattern;
        til::size _sourceSize;
        til::size _targetSize;
        size_t _centeringHint{ 0 };
        wil::unique_hfontresource _resourceHandle;
        wil::unique_hfont _fontHandle;
    };
}
