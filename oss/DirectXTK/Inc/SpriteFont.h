//--------------------------------------------------------------------------------------
// File: SpriteFont.h
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#pragma once

#include "SpriteBatch.h"

#include <cstddef>
#include <cstdint>
#include <memory>


namespace DirectX
{
    inline namespace DX11
    {
        class SpriteFont
        {
        public:
            struct Glyph;

            SpriteFont(_In_ ID3D11Device* device, _In_z_ wchar_t const* fileName, bool forceSRGB = false);
            SpriteFont(_In_ ID3D11Device* device, _In_reads_bytes_(dataSize) uint8_t const* dataBlob, _In_ size_t dataSize, bool forceSRGB = false);
            SpriteFont(_In_ ID3D11ShaderResourceView* texture, _In_reads_(glyphCount) Glyph const* glyphs, _In_ size_t glyphCount, _In_ float lineSpacing);

            SpriteFont(SpriteFont&&) noexcept;
            SpriteFont& operator= (SpriteFont&&) noexcept;

            SpriteFont(SpriteFont const&) = delete;
            SpriteFont& operator= (SpriteFont const&) = delete;

            virtual ~SpriteFont();

            // Wide-character / UTF-16LE
            void XM_CALLCONV DrawString(_In_ SpriteBatch* spriteBatch, _In_z_ wchar_t const* text, XMFLOAT2 const& position, FXMVECTOR color = Colors::White, float rotation = 0, XMFLOAT2 const& origin = Float2Zero, float scale = 1, SpriteEffects effects = SpriteEffects_None, float layerDepth = 0) const;
            void XM_CALLCONV DrawString(_In_ SpriteBatch* spriteBatch, _In_z_ wchar_t const* text, XMFLOAT2 const& position, FXMVECTOR color, float rotation, XMFLOAT2 const& origin, XMFLOAT2 const& scale, SpriteEffects effects = SpriteEffects_None, float layerDepth = 0) const;
            void XM_CALLCONV DrawString(_In_ SpriteBatch* spriteBatch, _In_z_ wchar_t const* text, FXMVECTOR position, FXMVECTOR color = Colors::White, float rotation = 0, FXMVECTOR origin = g_XMZero, float scale = 1, SpriteEffects effects = SpriteEffects_None, float layerDepth = 0) const;
            void XM_CALLCONV DrawString(_In_ SpriteBatch* spriteBatch, _In_z_ wchar_t const* text, FXMVECTOR position, FXMVECTOR color, float rotation, FXMVECTOR origin, GXMVECTOR scale, SpriteEffects effects = SpriteEffects_None, float layerDepth = 0) const;

            XMVECTOR XM_CALLCONV MeasureString(_In_z_ wchar_t const* text, bool ignoreWhitespace = true) const;

            RECT __cdecl MeasureDrawBounds(_In_z_ wchar_t const* text, XMFLOAT2 const& position, bool ignoreWhitespace = true) const;
            RECT XM_CALLCONV MeasureDrawBounds(_In_z_ wchar_t const* text, FXMVECTOR position, bool ignoreWhitespace = true) const;

            // UTF-8
            void XM_CALLCONV DrawString(_In_ SpriteBatch* spriteBatch, _In_z_ char const* text, XMFLOAT2 const& position, FXMVECTOR color = Colors::White, float rotation = 0, XMFLOAT2 const& origin = Float2Zero, float scale = 1, SpriteEffects effects = SpriteEffects_None, float layerDepth = 0) const;
            void XM_CALLCONV DrawString(_In_ SpriteBatch* spriteBatch, _In_z_ char const* text, XMFLOAT2 const& position, FXMVECTOR color, float rotation, XMFLOAT2 const& origin, XMFLOAT2 const& scale, SpriteEffects effects = SpriteEffects_None, float layerDepth = 0) const;
            void XM_CALLCONV DrawString(_In_ SpriteBatch* spriteBatch, _In_z_ char const* text, FXMVECTOR position, FXMVECTOR color = Colors::White, float rotation = 0, FXMVECTOR origin = g_XMZero, float scale = 1, SpriteEffects effects = SpriteEffects_None, float layerDepth = 0) const;
            void XM_CALLCONV DrawString(_In_ SpriteBatch* spriteBatch, _In_z_ char const* text, FXMVECTOR position, FXMVECTOR color, float rotation, FXMVECTOR origin, GXMVECTOR scale, SpriteEffects effects = SpriteEffects_None, float layerDepth = 0) const;

            XMVECTOR XM_CALLCONV MeasureString(_In_z_ char const* text, bool ignoreWhitespace = true) const;

            RECT __cdecl MeasureDrawBounds(_In_z_ char const* text, XMFLOAT2 const& position, bool ignoreWhitespace = true) const;
            RECT XM_CALLCONV MeasureDrawBounds(_In_z_ char const* text, FXMVECTOR position, bool ignoreWhitespace = true) const;

            // Spacing properties
            float __cdecl GetLineSpacing() const noexcept;
            void __cdecl SetLineSpacing(float spacing);

            // Font properties
            wchar_t __cdecl GetDefaultCharacter() const noexcept;
            void __cdecl SetDefaultCharacter(wchar_t character);

            bool __cdecl ContainsCharacter(wchar_t character) const;

            // Custom layout/rendering
            Glyph const* __cdecl FindGlyph(wchar_t character) const;
            void __cdecl GetSpriteSheet(ID3D11ShaderResourceView** texture) const;

            // Describes a single character glyph.
            struct Glyph
            {
                uint32_t Character;
                RECT Subrect;
                float XOffset;
                float YOffset;
                float XAdvance;
            };


        private:
            // Private implementation.
            class Impl;

            std::unique_ptr<Impl> pImpl;

            static const XMFLOAT2 Float2Zero;
        };
    }
}
