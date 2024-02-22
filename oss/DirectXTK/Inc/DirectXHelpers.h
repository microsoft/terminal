//--------------------------------------------------------------------------------------
// File: DirectXHelpers.h
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

#if !defined(NO_D3D11_DEBUG_NAME) && ( defined(_DEBUG) || defined(PROFILE) )
#if !defined(_XBOX_ONE) || !defined(_TITLE)
#pragma comment(lib,"dxguid.lib")
#endif
#endif

#ifndef IID_GRAPHICS_PPV_ARGS
#define IID_GRAPHICS_PPV_ARGS(x) IID_PPV_ARGS(x)
#endif

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>

//
// The core Direct3D headers provide the following helper C++ classes
//  CD3D11_RECT
//  CD3D11_BOX
//  CD3D11_DEPTH_STENCIL_DESC
//  CD3D11_BLEND_DESC, CD3D11_BLEND_DESC1
//  CD3D11_RASTERIZER_DESC, CD3D11_RASTERIZER_DESC1
//  CD3D11_BUFFER_DESC
//  CD3D11_TEXTURE1D_DESC
//  CD3D11_TEXTURE2D_DESC
//  CD3D11_TEXTURE3D_DESC
//  CD3D11_SHADER_RESOURCE_VIEW_DESC
//  CD3D11_RENDER_TARGET_VIEW_DESC
//  CD3D11_VIEWPORT
//  CD3D11_DEPTH_STENCIL_VIEW_DESC
//  CD3D11_UNORDERED_ACCESS_VIEW_DESC
//  CD3D11_SAMPLER_DESC
//  CD3D11_QUERY_DESC
//  CD3D11_COUNTER_DESC
//


namespace DirectX
{
    inline namespace DX11
    {
        class IEffect;
    }

    // simliar to std::lock_guard for exception-safe Direct3D resource locking
    class MapGuard : public D3D11_MAPPED_SUBRESOURCE
    {
    public:
        MapGuard(_In_ ID3D11DeviceContext* context,
            _In_ ID3D11Resource *resource,
            _In_ unsigned int subresource,
            _In_ D3D11_MAP mapType,
            _In_ unsigned int mapFlags) noexcept(false)
            : mContext(context), mResource(resource), mSubresource(subresource)
        {
            HRESULT hr = mContext->Map(resource, subresource, mapType, mapFlags, this);
            if (FAILED(hr))
            {
                throw std::exception();
            }
        }

        MapGuard(MapGuard&&) = default;
        MapGuard& operator= (MapGuard&&) = default;

        MapGuard(MapGuard const&) = delete;
        MapGuard& operator= (MapGuard const&) = delete;

        ~MapGuard()
        {
            mContext->Unmap(mResource, mSubresource);
        }

        uint8_t* get() const noexcept
        {
            return static_cast<uint8_t*>(pData);
        }
        uint8_t* get(size_t slice) const noexcept
        {
            return static_cast<uint8_t*>(pData) + (slice * DepthPitch);
        }

        uint8_t* scanline(size_t row) const noexcept
        {
            return static_cast<uint8_t*>(pData) + (row * RowPitch);
        }
        uint8_t* scanline(size_t slice, size_t row) const noexcept
        {
            return static_cast<uint8_t*>(pData) + (slice * DepthPitch) + (row * RowPitch);
        }

        template<typename T>
        void copy(_In_reads_(count) T const* data, size_t count) noexcept
        {
            memcpy(pData, data, count * sizeof(T));
        }

        template<typename T>
        void copy(T const& data) noexcept
        {
            memcpy(pData, data.data(), data.size() * sizeof(typename T::value_type));
        }

    private:
        ID3D11DeviceContext*    mContext;
        ID3D11Resource*         mResource;
        unsigned int            mSubresource;
    };


    // Helper sets a D3D resource name string (used by PIX and debug layer leak reporting).
    #if !defined(NO_D3D11_DEBUG_NAME) && ( defined(_DEBUG) || defined(PROFILE) )
    template<UINT TNameLength>
    inline void SetDebugObjectName(_In_ ID3D11DeviceChild* resource, _In_z_ const char(&name)[TNameLength]) noexcept
    {
    #if defined(_XBOX_ONE) && defined(_TITLE)
        wchar_t wname[MAX_PATH];
        int result = MultiByteToWideChar(CP_UTF8, 0, name, TNameLength, wname, MAX_PATH);
        if (result > 0)
        {
            resource->SetName(wname);
        }
    #else
        resource->SetPrivateData(WKPDID_D3DDebugObjectName, TNameLength - 1, name);
    #endif
    }
    #else
    template<UINT TNameLength>
    inline void SetDebugObjectName(_In_ ID3D11DeviceChild*, _In_z_ const char(&)[TNameLength]) noexcept
    {
    }
    #endif

    template<UINT TNameLength>
    inline void SetDebugObjectName(_In_ ID3D11DeviceChild* resource, _In_z_ const wchar_t(&name)[TNameLength])
    {
    #if !defined(NO_D3D11_DEBUG_NAME) && ( defined(_DEBUG) || defined(PROFILE) )
    #if defined(_XBOX_ONE) && defined(_TITLE)
        resource->SetName(name);
    #else
        char aname[MAX_PATH];
        int result = WideCharToMultiByte(CP_UTF8, 0, name, TNameLength, aname, MAX_PATH, nullptr, nullptr);
        if (result > 0)
        {
            resource->SetPrivateData(WKPDID_D3DDebugObjectName, TNameLength - 1, aname);
        }
    #endif
    #else
        UNREFERENCED_PARAMETER(resource);
        UNREFERENCED_PARAMETER(name);
    #endif
    }

    inline namespace DX11
    {
        // Helper to check for power-of-2
        template<typename T>
        constexpr bool IsPowerOf2(T x) noexcept { return ((x != 0) && !(x & (x - 1))); }

        // Helpers for aligning values by a power of 2
        template<typename T>
        inline T AlignDown(T size, size_t alignment) noexcept
        {
            if (alignment > 0)
            {
                assert(((alignment - 1) & alignment) == 0);
                auto mask = static_cast<T>(alignment - 1);
                return size & ~mask;
            }
            return size;
        }

        template<typename T>
        inline T AlignUp(T size, size_t alignment) noexcept
        {
            if (alignment > 0)
            {
                assert(((alignment - 1) & alignment) == 0);
                auto mask = static_cast<T>(alignment - 1);
                return (size + mask) & ~mask;
            }
            return size;
        }
    }

    // Helper for creating a Direct3D input layout to match a shader from an IEffect
    HRESULT __cdecl CreateInputLayoutFromEffect(_In_ ID3D11Device* device,
        _In_ IEffect* effect,
        _In_reads_(count) const D3D11_INPUT_ELEMENT_DESC* desc,
        size_t count,
        _COM_Outptr_ ID3D11InputLayout** pInputLayout) noexcept;

    template<typename T>
    HRESULT CreateInputLayoutFromEffect(_In_ ID3D11Device* device,
        _In_ IEffect* effect,
        _COM_Outptr_ ID3D11InputLayout** pInputLayout) noexcept
    {
        return CreateInputLayoutFromEffect(device, effect, T::InputElements, T::InputElementCount, pInputLayout);
    }
}
