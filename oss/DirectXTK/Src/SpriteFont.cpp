//--------------------------------------------------------------------------------------
// File: SpriteFont.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#include "pch.h"

#include <algorithm>
#include <vector>

#include "SpriteFont.h"
#include "DirectXHelpers.h"
#include "BinaryReader.h"
#include "LoaderHelpers.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;


// Internal SpriteFont implementation class.
class SpriteFont::Impl
{
public:
    Impl(_In_ ID3D11Device* device,
        _In_ BinaryReader* reader,
        bool forceSRGB) noexcept(false);
    Impl(_In_ ID3D11ShaderResourceView* texture,
        _In_reads_(glyphCount) Glyph const* glyphs,
        size_t glyphCount,
        float lineSpacing) noexcept(false);

    Glyph const* FindGlyph(wchar_t character) const;

    void SetDefaultCharacter(wchar_t character);

    template<typename TAction>
    void ForEachGlyph(_In_z_ wchar_t const* text, TAction action, bool ignoreWhitespace) const;

    void CreateTextureResource(_In_ ID3D11Device* device,
        uint32_t width, uint32_t height,
        DXGI_FORMAT format,
        uint32_t stride, uint32_t rows,
        _In_reads_(stride * rows) const uint8_t* data) noexcept(false);

    const wchar_t* ConvertUTF8(_In_z_ const char *text) noexcept(false);

    // Fields.
    ComPtr<ID3D11ShaderResourceView> texture;
    std::vector<Glyph> glyphs;
    std::vector<uint32_t> glyphsIndex;
    Glyph const* defaultGlyph;
    float lineSpacing;

private:
    size_t utfBufferSize;
    std::unique_ptr<wchar_t[]> utfBuffer;
};


// Constants.
const XMFLOAT2 SpriteFont::Float2Zero(0, 0);

static const char spriteFontMagic[] = "DXTKfont";


// Comparison operators make our sorted glyph vector work with std::binary_search and lower_bound.
namespace DirectX
{
    static inline bool operator< (SpriteFont::Glyph const& left, SpriteFont::Glyph const& right) noexcept
    {
        return left.Character < right.Character;
    }

    static inline bool operator< (wchar_t left, SpriteFont::Glyph const& right) noexcept
    {
        return left < right.Character;
    }

    static inline bool operator< (SpriteFont::Glyph const& left, wchar_t right) noexcept
    {
        return left.Character < right;
    }
}


// Reads a SpriteFont from the binary format created by the MakeSpriteFont utility.
_Use_decl_annotations_
SpriteFont::Impl::Impl(
    ID3D11Device* device,
    BinaryReader* reader,
    bool forceSRGB) noexcept(false) :
    defaultGlyph(nullptr),
    lineSpacing(0),
    utfBufferSize(0)
{
    // Validate the header.
    for (char const* magic = spriteFontMagic; *magic; magic++)
    {
        if (reader->Read<uint8_t>() != *magic)
        {
            DebugTrace("ERROR: SpriteFont provided with an invalid .spritefont file\n");
            throw std::runtime_error("Not a MakeSpriteFont output binary");
        }
    }

    // Read the glyph data.
    auto glyphCount = reader->Read<uint32_t>();
    auto glyphData = reader->ReadArray<Glyph>(glyphCount);

    glyphs.assign(glyphData, glyphData + glyphCount);
    glyphsIndex.reserve(glyphs.size());

    for (auto& glyph : glyphs)
    {
        glyphsIndex.emplace_back(glyph.Character);
    }

    // Read font properties.
    lineSpacing = reader->Read<float>();

    SetDefaultCharacter(static_cast<wchar_t>(reader->Read<uint32_t>()));

    // Read the texture data.
    auto textureWidth = reader->Read<uint32_t>();
    auto textureHeight = reader->Read<uint32_t>();
    auto textureFormat = reader->Read<DXGI_FORMAT>();
    auto textureStride = reader->Read<uint32_t>();
    auto textureRows = reader->Read<uint32_t>();

    const uint64_t dataSize = uint64_t(textureStride) * uint64_t(textureRows);
    if (dataSize > UINT32_MAX)
    {
        DebugTrace("ERROR: SpriteFont provided with an invalid .spritefont file\n");
        throw std::overflow_error("Invalid .spritefont file");
    }

    auto textureData = reader->ReadArray<uint8_t>(static_cast<size_t>(dataSize));

    if (forceSRGB)
    {
        textureFormat = LoaderHelpers::MakeSRGB(textureFormat);
    }

    // Create the D3D texture.
    CreateTextureResource(
        device,
        textureWidth, textureHeight,
        textureFormat,
        textureStride, textureRows,
        textureData);
}


// Constructs a SpriteFont from arbitrary user specified glyph data.
_Use_decl_annotations_
SpriteFont::Impl::Impl(
    ID3D11ShaderResourceView* itexture,
    Glyph const* iglyphs,
    size_t glyphCount,
    float ilineSpacing) noexcept(false) :
    texture(itexture),
    glyphs(iglyphs, iglyphs + glyphCount),
    defaultGlyph(nullptr),
    lineSpacing(ilineSpacing),
    utfBufferSize(0)
{
    if (!std::is_sorted(iglyphs, iglyphs + glyphCount))
    {
        throw std::runtime_error("Glyphs must be in ascending codepoint order");
    }

    glyphsIndex.reserve(glyphs.size());

    for (auto& glyph : glyphs)
    {
        glyphsIndex.emplace_back(glyph.Character);
    }
}


// Looks up the requested glyph, falling back to the default character if it is not in the font.
SpriteFont::Glyph const* SpriteFont::Impl::FindGlyph(wchar_t character) const
{
    // Rather than use std::lower_bound (which includes a slow debug path when built for _DEBUG),
    // we implement a binary search inline to ensure sufficient Debug build performance to be useful
    // for text-heavy applications.

    size_t lower = 0;
    size_t higher = glyphs.size() - 1;
    size_t index = higher / 2;
    const size_t size = glyphs.size();

    while (index < size)
    {
        const auto curChar = glyphsIndex[index];
        if (curChar == character) { return &glyphs[index]; }
        if (curChar < character)
        {
            lower = index + 1;
        }
        else
        {
            higher = index - 1;
        }
        if (higher < lower) { break; }
        else if (higher - lower <= 4)
        {
            for (index = lower; index <= higher; index++)
            {
                if (glyphsIndex[index] == character)
                {
                    return &glyphs[index];
                }
            }
        }
        index = lower + ((higher - lower) / 2);
    }

    if (defaultGlyph)
    {
        return defaultGlyph;
    }

    DebugTrace("ERROR: SpriteFont encountered a character not in the font (%u, %C), and no default glyph was provided\n", character, character);
    throw std::runtime_error("Character not in font");
}


// Sets the missing-character fallback glyph.
void SpriteFont::Impl::SetDefaultCharacter(wchar_t character)
{
    defaultGlyph = nullptr;

    if (character)
    {
        defaultGlyph = FindGlyph(character);
    }
}


// The core glyph layout algorithm, shared between DrawString and MeasureString.
template<typename TAction>
void SpriteFont::Impl::ForEachGlyph(_In_z_ wchar_t const* text, TAction action, bool ignoreWhitespace) const
{
    float x = 0;
    float y = 0;

    for (; *text; text++)
    {
        const wchar_t character = *text;

        switch (character)
        {
        case '\r':
            // Skip carriage returns.
            continue;

        case '\n':
            // New line.
            x = 0;
            y += lineSpacing;
            break;

        default:
            // Output this character.
            auto glyph = FindGlyph(character);

            x += glyph->XOffset;

            if (x < 0)
                x = 0;

            const float advance = float(glyph->Subrect.right) - float(glyph->Subrect.left) + glyph->XAdvance;

            if (!ignoreWhitespace
                || !iswspace(character)
                || ((glyph->Subrect.right - glyph->Subrect.left) > 1)
                || ((glyph->Subrect.bottom - glyph->Subrect.top) > 1))
            {
                action(glyph, x, y, advance);
            }

            x += advance;
            break;
        }
    }
}


_Use_decl_annotations_
void SpriteFont::Impl::CreateTextureResource(
    ID3D11Device* device,
    uint32_t width, uint32_t height,
    DXGI_FORMAT format,
    uint32_t stride, uint32_t rows,
    const uint8_t* data) noexcept(false)
{
    const uint64_t sliceBytes = uint64_t(stride) * uint64_t(rows);
    if (sliceBytes > UINT32_MAX)
    {
        DebugTrace("ERROR: SpriteFont provided with an invalid .spritefont file\n");
        throw std::overflow_error("Invalid .spritefont file");
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = { data, stride, static_cast<UINT>(sliceBytes) };

    ComPtr<ID3D11Texture2D> texture2D;
    ThrowIfFailed(
        device->CreateTexture2D(&desc, &initData, &texture2D)
    );

    CD3D11_SHADER_RESOURCE_VIEW_DESC viewDesc(D3D11_SRV_DIMENSION_TEXTURE2D, format);
    ThrowIfFailed(
        device->CreateShaderResourceView(texture2D.Get(), &viewDesc, texture.ReleaseAndGetAddressOf())
    );

    SetDebugObjectName(texture.Get(), "DirectXTK:SpriteFont");
    SetDebugObjectName(texture2D.Get(), "DirectXTK:SpriteFont");
}


const wchar_t* SpriteFont::Impl::ConvertUTF8(_In_z_ const char *text) noexcept(false)
{
    if (!utfBuffer)
    {
        utfBufferSize = 1024;
        utfBuffer.reset(new wchar_t[1024]);
    }

    int result = MultiByteToWideChar(CP_UTF8, 0, text, -1, utfBuffer.get(), static_cast<int>(utfBufferSize));
    if (!result && (GetLastError() == ERROR_INSUFFICIENT_BUFFER))
    {
        // Compute required buffer size
        result = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
        utfBufferSize = AlignUp(static_cast<size_t>(result), 1024u);
        utfBuffer.reset(new wchar_t[utfBufferSize]);

        // Retry conversion
        result = MultiByteToWideChar(CP_UTF8, 0, text, -1, utfBuffer.get(), static_cast<int>(utfBufferSize));
    }

    if (!result)
    {
        DebugTrace("ERROR: MultiByteToWideChar failed with error %u.\n", GetLastError());
        throw std::system_error(std::error_code(static_cast<int>(GetLastError()), std::system_category()), "MultiByteToWideChar");
    }

    return utfBuffer.get();
}


// Construct from a binary file created by the MakeSpriteFont utility.
_Use_decl_annotations_
SpriteFont::SpriteFont(ID3D11Device* device, wchar_t const* fileName, bool forceSRGB)
{
    BinaryReader reader(fileName);

    pImpl = std::make_unique<Impl>(device, &reader, forceSRGB);
}


// Construct from a binary blob created by the MakeSpriteFont utility and already loaded into memory.
_Use_decl_annotations_
SpriteFont::SpriteFont(ID3D11Device* device, uint8_t const* dataBlob, size_t dataSize, bool forceSRGB)
{
    BinaryReader reader(dataBlob, dataSize);

    pImpl = std::make_unique<Impl>(device, &reader, forceSRGB);
}


// Construct from arbitrary user specified glyph data (for those not using the MakeSpriteFont utility).
_Use_decl_annotations_
SpriteFont::SpriteFont(ID3D11ShaderResourceView* texture, Glyph const* glyphs, size_t glyphCount, float lineSpacing)
    : pImpl(std::make_unique<Impl>(texture, glyphs, glyphCount, lineSpacing))
{
}


SpriteFont::SpriteFont(SpriteFont&&) noexcept = default;
SpriteFont& SpriteFont::operator= (SpriteFont&&) noexcept = default;
SpriteFont::~SpriteFont() = default;


// Wide-character / UTF-16LE
void XM_CALLCONV SpriteFont::DrawString(_In_ SpriteBatch* spriteBatch, _In_z_ wchar_t const* text, XMFLOAT2 const& position, FXMVECTOR color, float rotation, XMFLOAT2 const& origin, float scale, SpriteEffects effects, float layerDepth) const
{
    DrawString(spriteBatch, text, XMLoadFloat2(&position), color, rotation, XMLoadFloat2(&origin), XMVectorReplicate(scale), effects, layerDepth);
}


void XM_CALLCONV SpriteFont::DrawString(_In_ SpriteBatch* spriteBatch, _In_z_ wchar_t const* text, XMFLOAT2 const& position, FXMVECTOR color, float rotation, XMFLOAT2 const& origin, XMFLOAT2 const& scale, SpriteEffects effects, float layerDepth) const
{
    DrawString(spriteBatch, text, XMLoadFloat2(&position), color, rotation, XMLoadFloat2(&origin), XMLoadFloat2(&scale), effects, layerDepth);
}


void XM_CALLCONV SpriteFont::DrawString(_In_ SpriteBatch* spriteBatch, _In_z_ wchar_t const* text, FXMVECTOR position, FXMVECTOR color, float rotation, FXMVECTOR origin, float scale, SpriteEffects effects, float layerDepth) const
{
    DrawString(spriteBatch, text, position, color, rotation, origin, XMVectorReplicate(scale), effects, layerDepth);
}


void XM_CALLCONV SpriteFont::DrawString(_In_ SpriteBatch* spriteBatch, _In_z_ wchar_t const* text, FXMVECTOR position, FXMVECTOR color, float rotation, FXMVECTOR origin, GXMVECTOR scale, SpriteEffects effects, float layerDepth) const
{
    static_assert(SpriteEffects_FlipHorizontally == 1 &&
        SpriteEffects_FlipVertically == 2, "If you change these enum values, the following tables must be updated to match");

// Lookup table indicates which way to move along each axis per SpriteEffects enum value.
    static XMVECTORF32 axisDirectionTable[4] =
    {
        { { { -1, -1, 0, 0 } } },
        { { {  1, -1, 0, 0 } } },
        { { { -1,  1, 0, 0 } } },
        { { {  1,  1, 0, 0 } } },
    };

    // Lookup table indicates which axes are mirrored for each SpriteEffects enum value.
    static XMVECTORF32 axisIsMirroredTable[4] =
    {
        { { { 0, 0, 0, 0 } } },
        { { { 1, 0, 0, 0 } } },
        { { { 0, 1, 0, 0 } } },
        { { { 1, 1, 0, 0 } } },
    };

    XMVECTOR baseOffset = origin;

    // If the text is mirrored, offset the start position accordingly.
    if (effects)
    {
        baseOffset = XMVectorNegativeMultiplySubtract(
            MeasureString(text),
            axisIsMirroredTable[effects & 3],
            baseOffset);
    }

    // Draw each character in turn.
    pImpl->ForEachGlyph(text, [&](Glyph const* glyph, float x, float y, float advance)
        {
            UNREFERENCED_PARAMETER(advance);

            XMVECTOR offset = XMVectorMultiplyAdd(XMVectorSet(x, y + glyph->YOffset, 0, 0), axisDirectionTable[effects & 3], baseOffset);

            if (effects)
            {
                // For mirrored characters, specify bottom and/or right instead of top left.
                XMVECTOR glyphRect = XMConvertVectorIntToFloat(XMLoadInt4(reinterpret_cast<uint32_t const*>(&glyph->Subrect)), 0);

                // xy = glyph width/height.
                glyphRect = XMVectorSubtract(XMVectorSwizzle<2, 3, 0, 1>(glyphRect), glyphRect);

                offset = XMVectorMultiplyAdd(glyphRect, axisIsMirroredTable[effects & 3], offset);
            }

            spriteBatch->Draw(pImpl->texture.Get(), position, &glyph->Subrect, color, rotation, offset, scale, effects, layerDepth);
        }, true);
}


XMVECTOR XM_CALLCONV SpriteFont::MeasureString(_In_z_ wchar_t const* text, bool ignoreWhitespace) const
{
    XMVECTOR result = XMVectorZero();

    pImpl->ForEachGlyph(text, [&](Glyph const* glyph, float x, float y, float advance)
        {
            UNREFERENCED_PARAMETER(advance);

            auto const w = static_cast<float>(glyph->Subrect.right - glyph->Subrect.left);
            auto h = static_cast<float>(glyph->Subrect.bottom - glyph->Subrect.top) + glyph->YOffset;

            h = iswspace(wchar_t(glyph->Character)) ?
                pImpl->lineSpacing :
                std::max(h, pImpl->lineSpacing);

            result = XMVectorMax(result, XMVectorSet(x + w, y + h, 0, 0));
        }, ignoreWhitespace);

    return result;
}


RECT SpriteFont::MeasureDrawBounds(_In_z_ wchar_t const* text, XMFLOAT2 const& position, bool ignoreWhitespace) const
{
    RECT result = { LONG_MAX, LONG_MAX, 0, 0 };

    pImpl->ForEachGlyph(text, [&](Glyph const* glyph, float x, float y, float advance) noexcept
        {
            auto const isWhitespace = iswspace(wchar_t(glyph->Character));
            auto const w = static_cast<float>(glyph->Subrect.right - glyph->Subrect.left);
            auto const h = isWhitespace ?
                pImpl->lineSpacing :
                static_cast<float>(glyph->Subrect.bottom - glyph->Subrect.top);

            const float minX = position.x + x;
            const float minY = position.y + y + (isWhitespace ? 0.0f : glyph->YOffset);

            const float maxX = std::max(minX + advance, minX + w);
            const float maxY = minY + h;

            if (minX < float(result.left))
                result.left = long(minX);

            if (minY < float(result.top))
                result.top = long(minY);

            if (float(result.right) < maxX)
                result.right = long(maxX);

            if (float(result.bottom) < maxY)
                result.bottom = long(maxY);
        }, ignoreWhitespace);

    if (result.left == LONG_MAX)
    {
        result.left = 0;
        result.top = 0;
    }

    return result;
}


RECT XM_CALLCONV SpriteFont::MeasureDrawBounds(_In_z_ wchar_t const* text, FXMVECTOR position, bool ignoreWhitespace) const
{
    XMFLOAT2 pos;
    XMStoreFloat2(&pos, position);

    return MeasureDrawBounds(text, pos, ignoreWhitespace);
}


// UTF-8
void XM_CALLCONV SpriteFont::DrawString(_In_ SpriteBatch* spriteBatch, _In_z_ char const* text, XMFLOAT2 const& position, FXMVECTOR color, float rotation, XMFLOAT2 const& origin, float scale, SpriteEffects effects, float layerDepth) const
{
    DrawString(spriteBatch, pImpl->ConvertUTF8(text), XMLoadFloat2(&position), color, rotation, XMLoadFloat2(&origin), XMVectorReplicate(scale), effects, layerDepth);
}


void XM_CALLCONV SpriteFont::DrawString(_In_ SpriteBatch* spriteBatch, _In_z_ char const* text, XMFLOAT2 const& position, FXMVECTOR color, float rotation, XMFLOAT2 const& origin, XMFLOAT2 const& scale, SpriteEffects effects, float layerDepth) const
{
    DrawString(spriteBatch, pImpl->ConvertUTF8(text), XMLoadFloat2(&position), color, rotation, XMLoadFloat2(&origin), XMLoadFloat2(&scale), effects, layerDepth);
}


void XM_CALLCONV SpriteFont::DrawString(_In_ SpriteBatch* spriteBatch, _In_z_ char const* text, FXMVECTOR position, FXMVECTOR color, float rotation, FXMVECTOR origin, float scale, SpriteEffects effects, float layerDepth) const
{
    DrawString(spriteBatch, pImpl->ConvertUTF8(text), position, color, rotation, origin, XMVectorReplicate(scale), effects, layerDepth);
}


void XM_CALLCONV SpriteFont::DrawString(_In_ SpriteBatch* spriteBatch, _In_z_ char const* text, FXMVECTOR position, FXMVECTOR color, float rotation, FXMVECTOR origin, GXMVECTOR scale, SpriteEffects effects, float layerDepth) const
{
    DrawString(spriteBatch, pImpl->ConvertUTF8(text), position, color, rotation, origin, scale, effects, layerDepth);
}


XMVECTOR XM_CALLCONV SpriteFont::MeasureString(_In_z_ char const* text, bool ignoreWhitespace) const
{
    return MeasureString(pImpl->ConvertUTF8(text), ignoreWhitespace);
}


RECT SpriteFont::MeasureDrawBounds(_In_z_ char const* text, XMFLOAT2 const& position, bool ignoreWhitespace) const
{
    return MeasureDrawBounds(pImpl->ConvertUTF8(text), position, ignoreWhitespace);
}


RECT XM_CALLCONV SpriteFont::MeasureDrawBounds(_In_z_ char const* text, FXMVECTOR position, bool ignoreWhitespace) const
{
    XMFLOAT2 pos;
    XMStoreFloat2(&pos, position);

    return MeasureDrawBounds(pImpl->ConvertUTF8(text), pos, ignoreWhitespace);
}


// Spacing properties
float SpriteFont::GetLineSpacing() const noexcept
{
    return pImpl->lineSpacing;
}


void SpriteFont::SetLineSpacing(float spacing)
{
    pImpl->lineSpacing = spacing;
}


// Font properties
wchar_t SpriteFont::GetDefaultCharacter() const noexcept
{
    return static_cast<wchar_t>(pImpl->defaultGlyph ? pImpl->defaultGlyph->Character : 0);
}


void SpriteFont::SetDefaultCharacter(wchar_t character)
{
    pImpl->SetDefaultCharacter(character);
}


bool SpriteFont::ContainsCharacter(wchar_t character) const
{
    return std::binary_search(pImpl->glyphs.begin(), pImpl->glyphs.end(), character);
}


// Custom layout/rendering
SpriteFont::Glyph const* SpriteFont::FindGlyph(wchar_t character) const
{
    return pImpl->FindGlyph(character);
}


void SpriteFont::GetSpriteSheet(ID3D11ShaderResourceView** texture) const
{
    if (!texture)
        return;

    ThrowIfFailed(pImpl->texture.CopyTo(texture));
}
