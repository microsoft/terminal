//--------------------------------------------------------------------------------------
// File: GeometricPrimitive.h
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#pragma once

#include "VertexTypes.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include <DirectXColors.h>


namespace DirectX
{
    inline namespace DX11
    {
        class IEffect;

        class GeometricPrimitive
        {
        public:
            GeometricPrimitive(GeometricPrimitive&&) = default;
            GeometricPrimitive& operator= (GeometricPrimitive&&) = default;

            GeometricPrimitive(GeometricPrimitive const&) = delete;
            GeometricPrimitive& operator= (GeometricPrimitive const&) = delete;

            using VertexType = VertexPositionNormalTexture;
            using VertexCollection = std::vector<VertexType>;
            using IndexCollection = std::vector<uint16_t>;

            virtual ~GeometricPrimitive();

            // Factory methods.
            static std::unique_ptr<GeometricPrimitive> __cdecl CreateCube(_In_ ID3D11DeviceContext* deviceContext, float size = 1, bool rhcoords = true);
            static std::unique_ptr<GeometricPrimitive> __cdecl CreateBox(_In_ ID3D11DeviceContext* deviceContext, const XMFLOAT3& size, bool rhcoords = true, bool invertn = false);
            static std::unique_ptr<GeometricPrimitive> __cdecl CreateSphere(_In_ ID3D11DeviceContext* deviceContext, float diameter = 1, size_t tessellation = 16, bool rhcoords = true, bool invertn = false);
            static std::unique_ptr<GeometricPrimitive> __cdecl CreateGeoSphere(_In_ ID3D11DeviceContext* deviceContext, float diameter = 1, size_t tessellation = 3, bool rhcoords = true);
            static std::unique_ptr<GeometricPrimitive> __cdecl CreateCylinder(_In_ ID3D11DeviceContext* deviceContext, float height = 1, float diameter = 1, size_t tessellation = 32, bool rhcoords = true);
            static std::unique_ptr<GeometricPrimitive> __cdecl CreateCone(_In_ ID3D11DeviceContext* deviceContext, float diameter = 1, float height = 1, size_t tessellation = 32, bool rhcoords = true);
            static std::unique_ptr<GeometricPrimitive> __cdecl CreateTorus(_In_ ID3D11DeviceContext* deviceContext, float diameter = 1, float thickness = 0.333f, size_t tessellation = 32, bool rhcoords = true);
            static std::unique_ptr<GeometricPrimitive> __cdecl CreateTetrahedron(_In_ ID3D11DeviceContext* deviceContext, float size = 1, bool rhcoords = true);
            static std::unique_ptr<GeometricPrimitive> __cdecl CreateOctahedron(_In_ ID3D11DeviceContext* deviceContext, float size = 1, bool rhcoords = true);
            static std::unique_ptr<GeometricPrimitive> __cdecl CreateDodecahedron(_In_ ID3D11DeviceContext* deviceContext, float size = 1, bool rhcoords = true);
            static std::unique_ptr<GeometricPrimitive> __cdecl CreateIcosahedron(_In_ ID3D11DeviceContext* deviceContext, float size = 1, bool rhcoords = true);
            static std::unique_ptr<GeometricPrimitive> __cdecl CreateTeapot(_In_ ID3D11DeviceContext* deviceContext, float size = 1, size_t tessellation = 8, bool rhcoords = true);
            static std::unique_ptr<GeometricPrimitive> __cdecl CreateCustom(_In_ ID3D11DeviceContext* deviceContext, const VertexCollection& vertices, const IndexCollection& indices);

            static void __cdecl CreateCube(VertexCollection& vertices, IndexCollection& indices, float size = 1, bool rhcoords = true);
            static void __cdecl CreateBox(VertexCollection& vertices, IndexCollection& indices, const XMFLOAT3& size, bool rhcoords = true, bool invertn = false);
            static void __cdecl CreateSphere(VertexCollection& vertices, IndexCollection& indices, float diameter = 1, size_t tessellation = 16, bool rhcoords = true, bool invertn = false);
            static void __cdecl CreateGeoSphere(VertexCollection& vertices, IndexCollection& indices, float diameter = 1, size_t tessellation = 3, bool rhcoords = true);
            static void __cdecl CreateCylinder(VertexCollection& vertices, IndexCollection& indices, float height = 1, float diameter = 1, size_t tessellation = 32, bool rhcoords = true);
            static void __cdecl CreateCone(VertexCollection& vertices, IndexCollection& indices, float diameter = 1, float height = 1, size_t tessellation = 32, bool rhcoords = true);
            static void __cdecl CreateTorus(VertexCollection& vertices, IndexCollection& indices, float diameter = 1, float thickness = 0.333f, size_t tessellation = 32, bool rhcoords = true);
            static void __cdecl CreateTetrahedron(VertexCollection& vertices, IndexCollection& indices, float size = 1, bool rhcoords = true);
            static void __cdecl CreateOctahedron(VertexCollection& vertices, IndexCollection& indices, float size = 1, bool rhcoords = true);
            static void __cdecl CreateDodecahedron(VertexCollection& vertices, IndexCollection& indices, float size = 1, bool rhcoords = true);
            static void __cdecl CreateIcosahedron(VertexCollection& vertices, IndexCollection& indices, float size = 1, bool rhcoords = true);
            static void __cdecl CreateTeapot(VertexCollection& vertices, IndexCollection& indices, float size = 1, size_t tessellation = 8, bool rhcoords = true);

            // Draw the primitive.
            void XM_CALLCONV Draw(FXMMATRIX world, CXMMATRIX view, CXMMATRIX projection,
                FXMVECTOR color = Colors::White,
                _In_opt_ ID3D11ShaderResourceView* texture = nullptr,
                bool wireframe = false,
                _In_opt_ std::function<void __cdecl()> setCustomState = nullptr) const;

            // Draw the primitive using a custom effect.
            void __cdecl Draw(_In_ IEffect* effect,
                _In_ ID3D11InputLayout* inputLayout,
                bool alpha = false, bool wireframe = false,
                _In_opt_ std::function<void __cdecl()> setCustomState = nullptr) const;

            void __cdecl DrawInstanced(_In_ IEffect* effect,
                _In_ ID3D11InputLayout* inputLayout,
                uint32_t instanceCount,
                bool alpha = false, bool wireframe = false,
                uint32_t startInstanceLocation = 0,
                _In_opt_ std::function<void __cdecl()> setCustomState = nullptr) const;

           // Create input layout for drawing with a custom effect.
            void __cdecl CreateInputLayout(_In_ IEffect* effect, _Outptr_ ID3D11InputLayout** inputLayout) const;

            static void SetDepthBufferMode(bool reverseZ)
            {
                s_reversez = reverseZ;
            }

        private:
            static bool s_reversez;

            GeometricPrimitive() noexcept(false);

            // Private implementation.
            class Impl;

            std::unique_ptr<Impl> pImpl;
        };
    }
}
