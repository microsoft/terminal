//--------------------------------------------------------------------------------------
// File: CommonStates.h
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

#include <memory>


namespace DirectX
{
    inline namespace DX11
    {
        class CommonStates
        {
        public:
            explicit CommonStates(_In_ ID3D11Device* device);

            CommonStates(CommonStates&&) noexcept;
            CommonStates& operator= (CommonStates&&) noexcept;

            CommonStates(CommonStates const&) = delete;
            CommonStates& operator= (CommonStates const&) = delete;

            virtual ~CommonStates();

            // Blend states.
            ID3D11BlendState* __cdecl Opaque() const;
            ID3D11BlendState* __cdecl AlphaBlend() const;
            ID3D11BlendState* __cdecl Additive() const;
            ID3D11BlendState* __cdecl NonPremultiplied() const;

            // Depth stencil states.
            ID3D11DepthStencilState* __cdecl DepthNone() const;
            ID3D11DepthStencilState* __cdecl DepthDefault() const;
            ID3D11DepthStencilState* __cdecl DepthRead() const;
            ID3D11DepthStencilState* __cdecl DepthReverseZ() const;
            ID3D11DepthStencilState* __cdecl DepthReadReverseZ() const;

            // Rasterizer states.
            ID3D11RasterizerState* __cdecl CullNone() const;
            ID3D11RasterizerState* __cdecl CullClockwise() const;
            ID3D11RasterizerState* __cdecl CullCounterClockwise() const;
            ID3D11RasterizerState* __cdecl Wireframe() const;

            // Sampler states.
            ID3D11SamplerState* __cdecl PointWrap() const;
            ID3D11SamplerState* __cdecl PointClamp() const;
            ID3D11SamplerState* __cdecl LinearWrap() const;
            ID3D11SamplerState* __cdecl LinearClamp() const;
            ID3D11SamplerState* __cdecl AnisotropicWrap() const;
            ID3D11SamplerState* __cdecl AnisotropicClamp() const;

        private:
            // Private implementation.
            class Impl;

            std::shared_ptr<Impl> pImpl;
        };
    }
}
