//--------------------------------------------------------------------------------------
// File: BufferHelpers.h
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#pragma once

#include <cassert>
#include <cstddef>

#if defined(_XBOX_ONE) && defined(_TITLE)
#include <d3d11_x.h>
#include "GraphicsMemory.h"
#else
#include <d3d11_1.h>
#endif

#include <wrl\client.h>


namespace DirectX
{
    // Helpers for creating initialized Direct3D buffer resources.
    HRESULT __cdecl CreateStaticBuffer(_In_ ID3D11Device* device,
        _In_reads_bytes_(count* stride) const void* ptr,
        size_t count,
        size_t stride,
        unsigned int bindFlags,
        _COM_Outptr_ ID3D11Buffer** pBuffer) noexcept;

    template<typename T>
    HRESULT CreateStaticBuffer(_In_ ID3D11Device* device,
        _In_reads_(count) T const* data,
        size_t count,
        unsigned int bindFlags,
        _COM_Outptr_ ID3D11Buffer** pBuffer) noexcept
    {
        return CreateStaticBuffer(device, data, count, sizeof(T), bindFlags, pBuffer);
    }

    template<typename T>
    HRESULT CreateStaticBuffer(_In_ ID3D11Device* device,
        T const& data,
        unsigned int bindFlags,
        _COM_Outptr_ ID3D11Buffer** pBuffer) noexcept
    {
        return CreateStaticBuffer(device, data.data(), data.size(), sizeof(typename T::value_type), bindFlags, pBuffer);
    }

    // Helpers for creating texture from memory arrays.
    HRESULT __cdecl CreateTextureFromMemory(_In_ ID3D11Device* device,
        size_t width,
        DXGI_FORMAT format,
        const D3D11_SUBRESOURCE_DATA& initData,
        _COM_Outptr_opt_ ID3D11Texture1D** texture,
        _COM_Outptr_opt_ ID3D11ShaderResourceView** textureView,
        unsigned int bindFlags = D3D11_BIND_SHADER_RESOURCE) noexcept;

    HRESULT __cdecl CreateTextureFromMemory(_In_ ID3D11Device* device,
        size_t width, size_t height,
        DXGI_FORMAT format,
        const D3D11_SUBRESOURCE_DATA& initData,
        _COM_Outptr_opt_ ID3D11Texture2D** texture,
        _COM_Outptr_opt_ ID3D11ShaderResourceView** textureView,
        unsigned int bindFlags = D3D11_BIND_SHADER_RESOURCE) noexcept;

    HRESULT __cdecl CreateTextureFromMemory(
    #if defined(_XBOX_ONE) && defined(_TITLE)
        _In_ ID3D11DeviceX* d3dDeviceX,
        _In_ ID3D11DeviceContextX* d3dContextX,
    #else
        _In_ ID3D11Device* device,
        _In_ ID3D11DeviceContext* d3dContext,
    #endif
        size_t width, size_t height,
        DXGI_FORMAT format,
        const D3D11_SUBRESOURCE_DATA& initData,
        _COM_Outptr_opt_ ID3D11Texture2D** texture,
        _COM_Outptr_opt_ ID3D11ShaderResourceView** textureView) noexcept;

    HRESULT __cdecl CreateTextureFromMemory(_In_ ID3D11Device* device,
        size_t width, size_t height, size_t depth,
        DXGI_FORMAT format,
        const D3D11_SUBRESOURCE_DATA& initData,
        _COM_Outptr_opt_ ID3D11Texture3D** texture,
        _COM_Outptr_opt_ ID3D11ShaderResourceView** textureView,
        unsigned int bindFlags = D3D11_BIND_SHADER_RESOURCE) noexcept;

    // Strongly typed wrapper around a Direct3D constant buffer.
    inline namespace DX11
    {
        namespace Private
        {
            // Base class, not to be used directly: clients should access this via the derived PrimitiveBatch<T>.
            class ConstantBufferBase
            {
            protected:
                void __cdecl CreateBuffer(_In_ ID3D11Device* device, size_t bytes, _Outptr_ ID3D11Buffer** pBuffer);
            };
        }
    }

    template<typename T>
    class ConstantBuffer : public DX11::Private::ConstantBufferBase
    {
    public:
        // Constructor.
        ConstantBuffer() = default;
        explicit ConstantBuffer(_In_ ID3D11Device* device) noexcept(false)
        {
            CreateBuffer(device, sizeof(T), mConstantBuffer.GetAddressOf());
        }

        ConstantBuffer(ConstantBuffer&&) = default;
        ConstantBuffer& operator= (ConstantBuffer&&) = default;

        ConstantBuffer(ConstantBuffer const&) = delete;
        ConstantBuffer& operator= (ConstantBuffer const&) = delete;

        void Create(_In_ ID3D11Device* device)
        {
            CreateBuffer(device, sizeof(T), mConstantBuffer.ReleaseAndGetAddressOf());
        }

        // Writes new data into the constant buffer.
    #if defined(_XBOX_ONE) && defined(_TITLE)
        void __cdecl SetData(_In_ ID3D11DeviceContext* deviceContext, T const& value, void** grfxMemory)
        {
            assert(grfxMemory != nullptr);

            void* ptr = GraphicsMemory::Get().Allocate(deviceContext, sizeof(T), 64);
            assert(ptr != nullptr);

            *(T*)ptr = value;

            *grfxMemory = ptr;
        }
    #else

        void __cdecl SetData(_In_ ID3D11DeviceContext* deviceContext, T const& value) noexcept
        {
            assert(mConstantBuffer);

            D3D11_MAPPED_SUBRESOURCE mappedResource;
            if (SUCCEEDED(deviceContext->Map(mConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
            {
                *static_cast<T*>(mappedResource.pData) = value;

                deviceContext->Unmap(mConstantBuffer.Get(), 0);
            }
        }
    #endif // _XBOX_ONE && _TITLE

        // Looks up the underlying D3D constant buffer.
        ID3D11Buffer* GetBuffer() const noexcept { return mConstantBuffer.Get(); }

    private:
        Microsoft::WRL::ComPtr<ID3D11Buffer> mConstantBuffer;
    };
}
