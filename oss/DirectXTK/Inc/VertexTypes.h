//--------------------------------------------------------------------------------------
// File: VertexTypes.h
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#pragma once

#if defined(_XBOX_ONE) && defined(_TITLE)
#include <d3d11_x.h>
#else
#include <d3d11_1.h>
#endif

#include <cstdint>

#include <DirectXMath.h>


namespace DirectX
{
    inline namespace DX11
    {
    // Vertex struct holding position information.
        struct VertexPosition
        {
            VertexPosition() = default;

            VertexPosition(const VertexPosition&) = default;
            VertexPosition& operator=(const VertexPosition&) = default;

            VertexPosition(VertexPosition&&) = default;
            VertexPosition& operator=(VertexPosition&&) = default;

            VertexPosition(XMFLOAT3 const& iposition) noexcept
                : position(iposition)
            {
            }

            VertexPosition(FXMVECTOR iposition) noexcept
            {
                XMStoreFloat3(&this->position, iposition);
            }

            XMFLOAT3 position;

            static constexpr unsigned int InputElementCount = 1;
            static const D3D11_INPUT_ELEMENT_DESC InputElements[InputElementCount];
        };


        // Vertex struct holding position and color information.
        struct VertexPositionColor
        {
            VertexPositionColor() = default;

            VertexPositionColor(const VertexPositionColor&) = default;
            VertexPositionColor& operator=(const VertexPositionColor&) = default;

            VertexPositionColor(VertexPositionColor&&) = default;
            VertexPositionColor& operator=(VertexPositionColor&&) = default;

            VertexPositionColor(XMFLOAT3 const& iposition, XMFLOAT4 const& icolor) noexcept
                : position(iposition),
                color(icolor)
            {
            }

            VertexPositionColor(FXMVECTOR iposition, FXMVECTOR icolor) noexcept
            {
                XMStoreFloat3(&this->position, iposition);
                XMStoreFloat4(&this->color, icolor);
            }

            XMFLOAT3 position;
            XMFLOAT4 color;

            static constexpr unsigned int InputElementCount = 2;
            static const D3D11_INPUT_ELEMENT_DESC InputElements[InputElementCount];
        };


        // Vertex struct holding position and texture mapping information.
        struct VertexPositionTexture
        {
            VertexPositionTexture() = default;

            VertexPositionTexture(const VertexPositionTexture&) = default;
            VertexPositionTexture& operator=(const VertexPositionTexture&) = default;

            VertexPositionTexture(VertexPositionTexture&&) = default;
            VertexPositionTexture& operator=(VertexPositionTexture&&) = default;

            VertexPositionTexture(XMFLOAT3 const& iposition, XMFLOAT2 const& itextureCoordinate) noexcept
                : position(iposition),
                textureCoordinate(itextureCoordinate)
            {
            }

            VertexPositionTexture(FXMVECTOR iposition, FXMVECTOR itextureCoordinate) noexcept
            {
                XMStoreFloat3(&this->position, iposition);
                XMStoreFloat2(&this->textureCoordinate, itextureCoordinate);
            }

            XMFLOAT3 position;
            XMFLOAT2 textureCoordinate;

            static constexpr unsigned int InputElementCount = 2;
            static const D3D11_INPUT_ELEMENT_DESC InputElements[InputElementCount];
        };


        // Vertex struct holding position and dual texture mapping information.
        struct VertexPositionDualTexture
        {
            VertexPositionDualTexture() = default;

            VertexPositionDualTexture(const VertexPositionDualTexture&) = default;
            VertexPositionDualTexture& operator=(const VertexPositionDualTexture&) = default;

            VertexPositionDualTexture(VertexPositionDualTexture&&) = default;
            VertexPositionDualTexture& operator=(VertexPositionDualTexture&&) = default;

            VertexPositionDualTexture(
                XMFLOAT3 const& iposition,
                XMFLOAT2 const& itextureCoordinate0,
                XMFLOAT2 const& itextureCoordinate1) noexcept
                : position(iposition),
                textureCoordinate0(itextureCoordinate0),
                textureCoordinate1(itextureCoordinate1)
            {
            }

            VertexPositionDualTexture(
                FXMVECTOR iposition,
                FXMVECTOR itextureCoordinate0,
                FXMVECTOR itextureCoordinate1) noexcept
            {
                XMStoreFloat3(&this->position, iposition);
                XMStoreFloat2(&this->textureCoordinate0, itextureCoordinate0);
                XMStoreFloat2(&this->textureCoordinate1, itextureCoordinate1);
            }

            XMFLOAT3 position;
            XMFLOAT2 textureCoordinate0;
            XMFLOAT2 textureCoordinate1;

            static constexpr unsigned int InputElementCount = 3;
            static const D3D11_INPUT_ELEMENT_DESC InputElements[InputElementCount];
        };


        // Vertex struct holding position and normal vector.
        struct VertexPositionNormal
        {
            VertexPositionNormal() = default;

            VertexPositionNormal(const VertexPositionNormal&) = default;
            VertexPositionNormal& operator=(const VertexPositionNormal&) = default;

            VertexPositionNormal(VertexPositionNormal&&) = default;
            VertexPositionNormal& operator=(VertexPositionNormal&&) = default;

            VertexPositionNormal(XMFLOAT3 const& iposition, XMFLOAT3 const& inormal) noexcept
                : position(iposition),
                normal(inormal)
            {
            }

            VertexPositionNormal(FXMVECTOR iposition, FXMVECTOR inormal) noexcept
            {
                XMStoreFloat3(&this->position, iposition);
                XMStoreFloat3(&this->normal, inormal);
            }

            XMFLOAT3 position;
            XMFLOAT3 normal;

            static constexpr unsigned int InputElementCount = 2;
            static const D3D11_INPUT_ELEMENT_DESC InputElements[InputElementCount];
        };


        // Vertex struct holding position, color, and texture mapping information.
        struct VertexPositionColorTexture
        {
            VertexPositionColorTexture() = default;

            VertexPositionColorTexture(const VertexPositionColorTexture&) = default;
            VertexPositionColorTexture& operator=(const VertexPositionColorTexture&) = default;

            VertexPositionColorTexture(VertexPositionColorTexture&&) = default;
            VertexPositionColorTexture& operator=(VertexPositionColorTexture&&) = default;

            VertexPositionColorTexture(XMFLOAT3 const& iposition, XMFLOAT4 const& icolor, XMFLOAT2 const& itextureCoordinate) noexcept
                : position(iposition),
                color(icolor),
                textureCoordinate(itextureCoordinate)
            {
            }

            VertexPositionColorTexture(FXMVECTOR iposition, FXMVECTOR icolor, FXMVECTOR itextureCoordinate) noexcept
            {
                XMStoreFloat3(&this->position, iposition);
                XMStoreFloat4(&this->color, icolor);
                XMStoreFloat2(&this->textureCoordinate, itextureCoordinate);
            }

            XMFLOAT3 position;
            XMFLOAT4 color;
            XMFLOAT2 textureCoordinate;

            static constexpr unsigned int InputElementCount = 3;
            static const D3D11_INPUT_ELEMENT_DESC InputElements[InputElementCount];
        };


        // Vertex struct holding position, normal vector, and color information.
        struct VertexPositionNormalColor
        {
            VertexPositionNormalColor() = default;

            VertexPositionNormalColor(const VertexPositionNormalColor&) = default;
            VertexPositionNormalColor& operator=(const VertexPositionNormalColor&) = default;

            VertexPositionNormalColor(VertexPositionNormalColor&&) = default;
            VertexPositionNormalColor& operator=(VertexPositionNormalColor&&) = default;

            VertexPositionNormalColor(XMFLOAT3 const& iposition, XMFLOAT3 const& inormal, XMFLOAT4 const& icolor) noexcept
                : position(iposition),
                normal(inormal),
                color(icolor)
            {
            }

            VertexPositionNormalColor(FXMVECTOR iposition, FXMVECTOR inormal, FXMVECTOR icolor) noexcept
            {
                XMStoreFloat3(&this->position, iposition);
                XMStoreFloat3(&this->normal, inormal);
                XMStoreFloat4(&this->color, icolor);
            }

            XMFLOAT3 position;
            XMFLOAT3 normal;
            XMFLOAT4 color;

            static constexpr unsigned int InputElementCount = 3;
            static const D3D11_INPUT_ELEMENT_DESC InputElements[InputElementCount];
        };


        // Vertex struct holding position, normal vector, and texture mapping information.
        struct VertexPositionNormalTexture
        {
            VertexPositionNormalTexture() = default;

            VertexPositionNormalTexture(const VertexPositionNormalTexture&) = default;
            VertexPositionNormalTexture& operator=(const VertexPositionNormalTexture&) = default;

            VertexPositionNormalTexture(VertexPositionNormalTexture&&) = default;
            VertexPositionNormalTexture& operator=(VertexPositionNormalTexture&&) = default;

            VertexPositionNormalTexture(XMFLOAT3 const& iposition, XMFLOAT3 const& inormal, XMFLOAT2 const& itextureCoordinate) noexcept
                : position(iposition),
                normal(inormal),
                textureCoordinate(itextureCoordinate)
            {
            }

            VertexPositionNormalTexture(FXMVECTOR iposition, FXMVECTOR inormal, FXMVECTOR itextureCoordinate) noexcept
            {
                XMStoreFloat3(&this->position, iposition);
                XMStoreFloat3(&this->normal, inormal);
                XMStoreFloat2(&this->textureCoordinate, itextureCoordinate);
            }

            XMFLOAT3 position;
            XMFLOAT3 normal;
            XMFLOAT2 textureCoordinate;

            static constexpr unsigned int InputElementCount = 3;
            static const D3D11_INPUT_ELEMENT_DESC InputElements[InputElementCount];
        };


        // Vertex struct holding position, normal vector, color, and texture mapping information.
        struct VertexPositionNormalColorTexture
        {
            VertexPositionNormalColorTexture() = default;

            VertexPositionNormalColorTexture(const VertexPositionNormalColorTexture&) = default;
            VertexPositionNormalColorTexture& operator=(const VertexPositionNormalColorTexture&) = default;

            VertexPositionNormalColorTexture(VertexPositionNormalColorTexture&&) = default;
            VertexPositionNormalColorTexture& operator=(VertexPositionNormalColorTexture&&) = default;

            VertexPositionNormalColorTexture(
                XMFLOAT3 const& iposition,
                XMFLOAT3 const& inormal,
                XMFLOAT4 const& icolor,
                XMFLOAT2 const& itextureCoordinate) noexcept
                : position(iposition),
                normal(inormal),
                color(icolor),
                textureCoordinate(itextureCoordinate)
            {
            }

            VertexPositionNormalColorTexture(FXMVECTOR iposition, FXMVECTOR inormal, FXMVECTOR icolor, CXMVECTOR itextureCoordinate) noexcept
            {
                XMStoreFloat3(&this->position, iposition);
                XMStoreFloat3(&this->normal, inormal);
                XMStoreFloat4(&this->color, icolor);
                XMStoreFloat2(&this->textureCoordinate, itextureCoordinate);
            }

            XMFLOAT3 position;
            XMFLOAT3 normal;
            XMFLOAT4 color;
            XMFLOAT2 textureCoordinate;

            static constexpr unsigned int InputElementCount = 4;
            static const D3D11_INPUT_ELEMENT_DESC InputElements[InputElementCount];
        };


        // Vertex struct for Visual Studio Shader Designer (DGSL) holding position, normal,
        // tangent, color (RGBA), and texture mapping information
        struct VertexPositionNormalTangentColorTexture
        {
            VertexPositionNormalTangentColorTexture() = default;

            VertexPositionNormalTangentColorTexture(const VertexPositionNormalTangentColorTexture&) = default;
            VertexPositionNormalTangentColorTexture& operator=(const VertexPositionNormalTangentColorTexture&) = default;

            VertexPositionNormalTangentColorTexture(VertexPositionNormalTangentColorTexture&&) = default;
            VertexPositionNormalTangentColorTexture& operator=(VertexPositionNormalTangentColorTexture&&) = default;

            XMFLOAT3 position;
            XMFLOAT3 normal;
            XMFLOAT4 tangent;
            uint32_t color;
            XMFLOAT2 textureCoordinate;

            VertexPositionNormalTangentColorTexture(
                XMFLOAT3 const& iposition,
                XMFLOAT3 const& inormal,
                XMFLOAT4 const& itangent,
                uint32_t irgba,
                XMFLOAT2 const& itextureCoordinate) noexcept
                : position(iposition),
                normal(inormal),
                tangent(itangent),
                color(irgba),
                textureCoordinate(itextureCoordinate)
            {
            }

            VertexPositionNormalTangentColorTexture(
                FXMVECTOR iposition,
                FXMVECTOR inormal,
                FXMVECTOR itangent,
                uint32_t irgba,
                CXMVECTOR itextureCoordinate) noexcept
                : color(irgba)
            {
                XMStoreFloat3(&this->position, iposition);
                XMStoreFloat3(&this->normal, inormal);
                XMStoreFloat4(&this->tangent, itangent);
                XMStoreFloat2(&this->textureCoordinate, itextureCoordinate);
            }

            VertexPositionNormalTangentColorTexture(
                XMFLOAT3 const& iposition,
                XMFLOAT3 const& inormal,
                XMFLOAT4 const& itangent,
                XMFLOAT4 const& icolor,
                XMFLOAT2 const& itextureCoordinate) noexcept
                : position(iposition),
                normal(inormal),
                tangent(itangent),
                color{},
                textureCoordinate(itextureCoordinate)
            {
                SetColor(icolor);
            }

            VertexPositionNormalTangentColorTexture(
                FXMVECTOR iposition,
                FXMVECTOR inormal,
                FXMVECTOR itangent,
                CXMVECTOR icolor,
                CXMVECTOR itextureCoordinate) noexcept
                : color{}
            {
                XMStoreFloat3(&this->position, iposition);
                XMStoreFloat3(&this->normal, inormal);
                XMStoreFloat4(&this->tangent, itangent);
                XMStoreFloat2(&this->textureCoordinate, itextureCoordinate);

                SetColor(icolor);
            }

            void __cdecl SetColor(XMFLOAT4 const& icolor) noexcept { SetColor(XMLoadFloat4(&icolor)); }
            void XM_CALLCONV SetColor(FXMVECTOR icolor) noexcept;

            static constexpr unsigned int InputElementCount = 5;
            static const D3D11_INPUT_ELEMENT_DESC InputElements[InputElementCount];
        };


        // Vertex struct for Visual Studio Shader Designer (DGSL) holding position, normal,
        // tangent, color (RGBA), texture mapping information, and skinning weights
        struct VertexPositionNormalTangentColorTextureSkinning : public VertexPositionNormalTangentColorTexture
        {
            VertexPositionNormalTangentColorTextureSkinning() = default;

            VertexPositionNormalTangentColorTextureSkinning(const VertexPositionNormalTangentColorTextureSkinning&) = default;
            VertexPositionNormalTangentColorTextureSkinning& operator=(const VertexPositionNormalTangentColorTextureSkinning&) = default;

            VertexPositionNormalTangentColorTextureSkinning(VertexPositionNormalTangentColorTextureSkinning&&) = default;
            VertexPositionNormalTangentColorTextureSkinning& operator=(VertexPositionNormalTangentColorTextureSkinning&&) = default;

            uint32_t indices;
            uint32_t weights;

            VertexPositionNormalTangentColorTextureSkinning(
                XMFLOAT3 const& iposition,
                XMFLOAT3 const& inormal,
                XMFLOAT4 const& itangent,
                uint32_t irgba,
                XMFLOAT2 const& itextureCoordinate,
                XMUINT4 const& iindices,
                XMFLOAT4 const& iweights) noexcept
                : VertexPositionNormalTangentColorTexture(iposition, inormal, itangent, irgba, itextureCoordinate),
                indices{},
                weights{}
            {
                SetBlendIndices(iindices);
                SetBlendWeights(iweights);
            }

            VertexPositionNormalTangentColorTextureSkinning(
                FXMVECTOR iposition,
                FXMVECTOR inormal,
                FXMVECTOR itangent,
                uint32_t irgba,
                CXMVECTOR itextureCoordinate,
                XMUINT4 const& iindices,
                CXMVECTOR iweights) noexcept
                : VertexPositionNormalTangentColorTexture(iposition, inormal, itangent, irgba, itextureCoordinate),
                indices{},
                weights{}
            {
                SetBlendIndices(iindices);
                SetBlendWeights(iweights);
            }

            VertexPositionNormalTangentColorTextureSkinning(
                XMFLOAT3 const& iposition,
                XMFLOAT3 const& inormal,
                XMFLOAT4 const& itangent,
                XMFLOAT4 const& icolor,
                XMFLOAT2 const& itextureCoordinate,
                XMUINT4 const& iindices,
                XMFLOAT4 const& iweights) noexcept
                : VertexPositionNormalTangentColorTexture(iposition, inormal, itangent, icolor, itextureCoordinate),
                indices{},
                weights{}
            {
                SetBlendIndices(iindices);
                SetBlendWeights(iweights);
            }

            VertexPositionNormalTangentColorTextureSkinning(
                FXMVECTOR iposition,
                FXMVECTOR inormal,
                FXMVECTOR itangent,
                CXMVECTOR icolor,
                CXMVECTOR itextureCoordinate,
                XMUINT4 const& iindices,
                CXMVECTOR iweights) noexcept
                : VertexPositionNormalTangentColorTexture(iposition, inormal, itangent, icolor, itextureCoordinate),
                indices{},
                weights{}
            {
                SetBlendIndices(iindices);
                SetBlendWeights(iweights);
            }

            void __cdecl SetBlendIndices(XMUINT4 const& iindices) noexcept;

            void __cdecl SetBlendWeights(XMFLOAT4 const& iweights) noexcept { SetBlendWeights(XMLoadFloat4(&iweights)); }
            void XM_CALLCONV SetBlendWeights(FXMVECTOR iweights) noexcept;

            static constexpr unsigned int InputElementCount = 7;
            static const D3D11_INPUT_ELEMENT_DESC InputElements[InputElementCount];
        };
    }
}
