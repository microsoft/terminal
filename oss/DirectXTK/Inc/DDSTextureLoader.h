//--------------------------------------------------------------------------------------
// File: DDSTextureLoader.h
//
// Functions for loading a DDS texture and creating a Direct3D runtime resource for it
//
// Note these functions are useful as a light-weight runtime loader for DDS files. For
// a full-featured DDS file reader, writer, and texture processing pipeline see
// the 'Texconv' sample and the 'DirectXTex' library.
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#pragma once

#if defined(_XBOX_ONE) && defined(_TITLE)
#include <d3d11_x.h>
#else
#include <d3d11_1.h>
#endif

#include <cstddef>
#include <cstdint>


namespace DirectX
{
#ifndef DDS_ALPHA_MODE_DEFINED
#define DDS_ALPHA_MODE_DEFINED
    enum DDS_ALPHA_MODE : uint32_t
    {
        DDS_ALPHA_MODE_UNKNOWN = 0,
        DDS_ALPHA_MODE_STRAIGHT = 1,
        DDS_ALPHA_MODE_PREMULTIPLIED = 2,
        DDS_ALPHA_MODE_OPAQUE = 3,
        DDS_ALPHA_MODE_CUSTOM = 4,
    };
#endif

    inline namespace DX11
    {
        enum DDS_LOADER_FLAGS : uint32_t
        {
            DDS_LOADER_DEFAULT = 0,
            DDS_LOADER_FORCE_SRGB = 0x1,
            DDS_LOADER_IGNORE_SRGB = 0x2,
        };
    }

    // Standard version
    HRESULT __cdecl CreateDDSTextureFromMemory(
        _In_ ID3D11Device* d3dDevice,
        _In_reads_bytes_(ddsDataSize) const uint8_t* ddsData,
        _In_ size_t ddsDataSize,
        _Outptr_opt_ ID3D11Resource** texture,
        _Outptr_opt_ ID3D11ShaderResourceView** textureView,
        _In_ size_t maxsize = 0,
        _Out_opt_ DDS_ALPHA_MODE* alphaMode = nullptr) noexcept;

    HRESULT __cdecl CreateDDSTextureFromFile(
        _In_ ID3D11Device* d3dDevice,
        _In_z_ const wchar_t* szFileName,
        _Outptr_opt_ ID3D11Resource** texture,
        _Outptr_opt_ ID3D11ShaderResourceView** textureView,
        _In_ size_t maxsize = 0,
        _Out_opt_ DDS_ALPHA_MODE* alphaMode = nullptr) noexcept;

    // Standard version with optional auto-gen mipmap support
    HRESULT __cdecl CreateDDSTextureFromMemory(
    #if defined(_XBOX_ONE) && defined(_TITLE)
        _In_ ID3D11DeviceX* d3dDevice,
        _In_opt_ ID3D11DeviceContextX* d3dContext,
    #else
        _In_ ID3D11Device* d3dDevice,
        _In_opt_ ID3D11DeviceContext* d3dContext,
    #endif
        _In_reads_bytes_(ddsDataSize) const uint8_t* ddsData,
        _In_ size_t ddsDataSize,
        _Outptr_opt_ ID3D11Resource** texture,
        _Outptr_opt_ ID3D11ShaderResourceView** textureView,
        _In_ size_t maxsize = 0,
        _Out_opt_ DDS_ALPHA_MODE* alphaMode = nullptr) noexcept;

    HRESULT __cdecl CreateDDSTextureFromFile(
    #if defined(_XBOX_ONE) && defined(_TITLE)
        _In_ ID3D11DeviceX* d3dDevice,
        _In_opt_ ID3D11DeviceContextX* d3dContext,
    #else
        _In_ ID3D11Device* d3dDevice,
        _In_opt_ ID3D11DeviceContext* d3dContext,
    #endif
        _In_z_ const wchar_t* szFileName,
        _Outptr_opt_ ID3D11Resource** texture,
        _Outptr_opt_ ID3D11ShaderResourceView** textureView,
        _In_ size_t maxsize = 0,
        _Out_opt_ DDS_ALPHA_MODE* alphaMode = nullptr) noexcept;

    // Extended version
    HRESULT __cdecl CreateDDSTextureFromMemoryEx(
        _In_ ID3D11Device* d3dDevice,
        _In_reads_bytes_(ddsDataSize) const uint8_t* ddsData,
        _In_ size_t ddsDataSize,
        _In_ size_t maxsize,
        _In_ D3D11_USAGE usage,
        _In_ unsigned int bindFlags,
        _In_ unsigned int cpuAccessFlags,
        _In_ unsigned int miscFlags,
        _In_ DDS_LOADER_FLAGS loadFlags,
        _Outptr_opt_ ID3D11Resource** texture,
        _Outptr_opt_ ID3D11ShaderResourceView** textureView,
        _Out_opt_ DDS_ALPHA_MODE* alphaMode = nullptr) noexcept;

    HRESULT __cdecl CreateDDSTextureFromFileEx(
        _In_ ID3D11Device* d3dDevice,
        _In_z_ const wchar_t* szFileName,
        _In_ size_t maxsize,
        _In_ D3D11_USAGE usage,
        _In_ unsigned int bindFlags,
        _In_ unsigned int cpuAccessFlags,
        _In_ unsigned int miscFlags,
        _In_ DDS_LOADER_FLAGS loadFlags,
        _Outptr_opt_ ID3D11Resource** texture,
        _Outptr_opt_ ID3D11ShaderResourceView** textureView,
        _Out_opt_ DDS_ALPHA_MODE* alphaMode = nullptr) noexcept;

    // Extended version with optional auto-gen mipmap support
    HRESULT __cdecl CreateDDSTextureFromMemoryEx(
    #if defined(_XBOX_ONE) && defined(_TITLE)
        _In_ ID3D11DeviceX* d3dDevice,
        _In_opt_ ID3D11DeviceContextX* d3dContext,
    #else
        _In_ ID3D11Device* d3dDevice,
        _In_opt_ ID3D11DeviceContext* d3dContext,
    #endif
        _In_reads_bytes_(ddsDataSize) const uint8_t* ddsData,
        _In_ size_t ddsDataSize,
        _In_ size_t maxsize,
        _In_ D3D11_USAGE usage,
        _In_ unsigned int bindFlags,
        _In_ unsigned int cpuAccessFlags,
        _In_ unsigned int miscFlags,
        _In_ DDS_LOADER_FLAGS loadFlags,
        _Outptr_opt_ ID3D11Resource** texture,
        _Outptr_opt_ ID3D11ShaderResourceView** textureView,
        _Out_opt_ DDS_ALPHA_MODE* alphaMode = nullptr) noexcept;

    HRESULT __cdecl CreateDDSTextureFromFileEx(
    #if defined(_XBOX_ONE) && defined(_TITLE)
        _In_ ID3D11DeviceX* d3dDevice,
        _In_opt_ ID3D11DeviceContextX* d3dContext,
    #else
        _In_ ID3D11Device* d3dDevice,
        _In_opt_ ID3D11DeviceContext* d3dContext,
    #endif
        _In_z_ const wchar_t* szFileName,
        _In_ size_t maxsize,
        _In_ D3D11_USAGE usage,
        _In_ unsigned int bindFlags,
        _In_ unsigned int cpuAccessFlags,
        _In_ unsigned int miscFlags,
        _In_ DDS_LOADER_FLAGS loadFlags,
        _Outptr_opt_ ID3D11Resource** texture,
        _Outptr_opt_ ID3D11ShaderResourceView** textureView,
        _Out_opt_ DDS_ALPHA_MODE* alphaMode = nullptr) noexcept;

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-dynamic-exception-spec"
#endif

    inline namespace DX11
    {
        DEFINE_ENUM_FLAG_OPERATORS(DDS_LOADER_FLAGS);
    }

#ifdef __clang__
#pragma clang diagnostic pop
#endif
}
