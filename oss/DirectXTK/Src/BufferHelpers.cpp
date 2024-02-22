//--------------------------------------------------------------------------------------
// File: BufferHelpers.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "BufferHelpers.h"
#include "PlatformHelpers.h"


using namespace DirectX;
using Microsoft::WRL::ComPtr;

//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::CreateStaticBuffer(
    ID3D11Device* device,
    const void* ptr,
    size_t count,
    size_t stride,
    unsigned int bindFlags,
    ID3D11Buffer** pBuffer) noexcept
{
    if (!pBuffer)
        return E_INVALIDARG;

    *pBuffer = nullptr;

    if (!device || !ptr || !count || !stride)
        return E_INVALIDARG;

    const uint64_t sizeInbytes = uint64_t(count) * uint64_t(stride);

    static constexpr uint64_t c_maxBytes = D3D11_REQ_RESOURCE_SIZE_IN_MEGABYTES_EXPRESSION_A_TERM * 1024u * 1024u;
    static_assert(c_maxBytes <= UINT32_MAX, "Exceeded integer limits");

    if (sizeInbytes > c_maxBytes)
    {
        DebugTrace("ERROR: Resource size too large for DirectX 11 (size %llu)\n", sizeInbytes);
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.ByteWidth = static_cast<UINT>(sizeInbytes);
    bufferDesc.BindFlags = bindFlags;
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;

    D3D11_SUBRESOURCE_DATA initData = { ptr, 0, 0 };

    return device->CreateBuffer(&bufferDesc, &initData, pBuffer);
}


//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::CreateTextureFromMemory(
    ID3D11Device* device,
    size_t width,
    DXGI_FORMAT format,
    const D3D11_SUBRESOURCE_DATA& initData,
    ID3D11Texture1D** texture,
    ID3D11ShaderResourceView** textureView,
    unsigned int bindFlags) noexcept
{
    if (texture)
    {
        *texture = nullptr;
    }
    if (textureView)
    {
        *textureView = nullptr;
    }

    if (!device || !width || !initData.pSysMem)
        return E_INVALIDARG;

    if (!texture && !textureView)
        return E_INVALIDARG;

    static_assert(D3D11_REQ_TEXTURE1D_U_DIMENSION <= UINT32_MAX, "Exceeded integer limits");

    if (width > D3D11_REQ_TEXTURE1D_U_DIMENSION)
    {
        DebugTrace("ERROR: Resource dimensions too large for DirectX 11 (1D: size %zu)\n", width);
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }

    D3D11_TEXTURE1D_DESC desc = {};
    desc.Width = static_cast<UINT>(width);
    desc.MipLevels = desc.ArraySize = 1;
    desc.Format = format;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = bindFlags;

    ComPtr<ID3D11Texture1D> tex;
    HRESULT hr = device->CreateTexture1D(&desc, &initData, tex.GetAddressOf());
    if (SUCCEEDED(hr))
    {
        if (textureView)
        {
            hr = device->CreateShaderResourceView(tex.Get(), nullptr, textureView);
            if (FAILED(hr))
                return hr;
        }

        if (texture)
        {
            *texture = tex.Detach();
        }
    }

    return hr;
}

_Use_decl_annotations_
HRESULT DirectX::CreateTextureFromMemory(
    ID3D11Device* device,
    size_t width,
    size_t height,
    DXGI_FORMAT format,
    const D3D11_SUBRESOURCE_DATA& initData,
    ID3D11Texture2D** texture,
    ID3D11ShaderResourceView** textureView,
    unsigned int bindFlags) noexcept
{
    if (texture)
    {
        *texture = nullptr;
    }
    if (textureView)
    {
        *textureView = nullptr;
    }

    if (!device || !width || !height
        || !initData.pSysMem || !initData.SysMemPitch)
        return E_INVALIDARG;

    if (!texture && !textureView)
        return E_INVALIDARG;

    static_assert(D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION <= UINT32_MAX, "Exceeded integer limits");

    if ((width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION)
        || (height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION))
    {
        DebugTrace("ERROR: Resource dimensions too large for DirectX 11 (2D: size %zu by %zu)\n", width, height);
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = static_cast<UINT>(width);
    desc.Height = static_cast<UINT>(height);
    desc.MipLevels = desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = bindFlags;

    ComPtr<ID3D11Texture2D> tex;
    HRESULT hr = device->CreateTexture2D(&desc, &initData, tex.GetAddressOf());
    if (SUCCEEDED(hr))
    {
        if (textureView)
        {
            hr = device->CreateShaderResourceView(tex.Get(), nullptr, textureView);
            if (FAILED(hr))
                return hr;
        }

        if (texture)
        {
            *texture = tex.Detach();
        }
    }

    return hr;
}


_Use_decl_annotations_
HRESULT DirectX::CreateTextureFromMemory(
#if defined(_XBOX_ONE) && defined(_TITLE)
    _In_ ID3D11DeviceX* device,
    _In_ ID3D11DeviceContextX* d3dContext,
#else
    _In_ ID3D11Device* device,
    _In_ ID3D11DeviceContext* d3dContext,
#endif
    size_t width,
    size_t height,
    DXGI_FORMAT format,
    const D3D11_SUBRESOURCE_DATA& initData,
    ID3D11Texture2D** texture,
    ID3D11ShaderResourceView** textureView) noexcept
{
    if (texture)
    {
        *texture = nullptr;
    }
    if (textureView)
    {
        *textureView = nullptr;
    }

    if (!device || !d3dContext || !width || !height
        || !initData.pSysMem || !initData.SysMemPitch)
        return E_INVALIDARG;

    if (!texture && !textureView)
        return E_INVALIDARG;

    static_assert(D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION <= UINT32_MAX, "Exceeded integer limits");

    if ((width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION)
        || (height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION))
    {
        DebugTrace("ERROR: Resource dimensions too large for DirectX 11 (2D: size %zu by %zu)\n", width, height);
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = static_cast<UINT>(width);
    desc.Height = static_cast<UINT>(height);
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    UINT fmtSupport = 0;
    if (SUCCEEDED(device->CheckFormatSupport(format, &fmtSupport)) && (fmtSupport & D3D11_FORMAT_SUPPORT_MIP_AUTOGEN))
    {
        desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
        desc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
    }
    else
    {
        // Autogen not supported.
        desc.MipLevels = 1;
    }

    ComPtr<ID3D11Texture2D> tex;
    HRESULT hr = device->CreateTexture2D(&desc, nullptr, tex.GetAddressOf());
    if (SUCCEEDED(hr))
    {
        ComPtr<ID3D11ShaderResourceView> srv;
        hr = device->CreateShaderResourceView(tex.Get(), nullptr, srv.GetAddressOf());
        if (FAILED(hr))
            return hr;

        if (desc.MipLevels != 1)
        {
        #if defined(_XBOX_ONE) && defined(_TITLE)
            ComPtr<ID3D11Texture2D> staging;
            desc.MipLevels = 1;
            desc.Usage = D3D11_USAGE_STAGING;
            desc.BindFlags = desc.MiscFlags = 0;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            hr = device->CreateTexture2D(&desc, &initData, staging.GetAddressOf());
            if (FAILED(hr))
                return hr;

            d3dContext->CopySubresourceRegion(tex.Get(), 0, 0, 0, 0, staging.Get(), 0, nullptr);
            UINT64 copyFence = d3dContext->InsertFence(0);
            while (device->IsFencePending(copyFence)) { SwitchToThread(); }
        #else
            d3dContext->UpdateSubresource(tex.Get(), 0, nullptr, initData.pSysMem, initData.SysMemPitch, 0);
        #endif
            d3dContext->GenerateMips(srv.Get());
        }

        if (texture)
        {
            *texture = tex.Detach();
        }
        if (textureView)
        {
            *textureView = srv.Detach();
        }
    }

    return hr;
}

_Use_decl_annotations_
HRESULT DirectX::CreateTextureFromMemory(
    ID3D11Device* device,
    size_t width, size_t height, size_t depth,
    DXGI_FORMAT format,
    const D3D11_SUBRESOURCE_DATA& initData,
    ID3D11Texture3D** texture,
    ID3D11ShaderResourceView** textureView,
    unsigned int bindFlags) noexcept
{
    if (texture)
    {
        *texture = nullptr;
    }
    if (textureView)
    {
        *textureView = nullptr;
    }

    if (!device || !width || !height || !depth
        || !initData.pSysMem || !initData.SysMemPitch || !initData.SysMemSlicePitch)
        return E_INVALIDARG;

    if (!texture && !textureView)
        return E_INVALIDARG;

    static_assert(D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION <= UINT32_MAX, "Exceeded integer limits");

    if ((width > D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION)
        || (height > D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION)
        || (depth > D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION))
    {
        DebugTrace("ERROR: Resource dimensions too large for DirectX 11 (3D: size %zu by %zu by %zu)\n", width, height, depth);
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }

    D3D11_TEXTURE3D_DESC desc = {};
    desc.Width = static_cast<UINT>(width);
    desc.Height = static_cast<UINT>(height);
    desc.Depth = static_cast<UINT>(depth);
    desc.MipLevels = 1;
    desc.Format = format;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = bindFlags;

    ComPtr<ID3D11Texture3D> tex;
    HRESULT hr = device->CreateTexture3D(&desc, &initData, tex.GetAddressOf());
    if (SUCCEEDED(hr))
    {
        if (textureView)
        {
            hr = device->CreateShaderResourceView(tex.Get(), nullptr, textureView);
            if (FAILED(hr))
                return hr;
        }

        if (texture)
        {
            *texture = tex.Detach();
        }
    }

    return hr;
}


//--------------------------------------------------------------------------------------
_Use_decl_annotations_
void Private::ConstantBufferBase::CreateBuffer(
    ID3D11Device* device,
    size_t bytes,
    ID3D11Buffer** pBuffer)
{
    if (!pBuffer)
        throw std::invalid_argument("ConstantBuffer needs valid buffer parameter");

    *pBuffer = nullptr;

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = static_cast<UINT>(bytes);
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

#if defined(_XBOX_ONE) && defined(_TITLE)

    ComPtr<ID3D11DeviceX> deviceX;
    ThrowIfFailed(device->QueryInterface(IID_GRAPHICS_PPV_ARGS(deviceX.GetAddressOf())));

    ThrowIfFailed(deviceX->CreatePlacementBuffer(&desc, nullptr, pBuffer));

#else

    desc.Usage = D3D11_USAGE_DYNAMIC;

    ThrowIfFailed(
        device->CreateBuffer(&desc, nullptr, pBuffer)
    );

#endif

    assert(pBuffer != nullptr && *pBuffer != nullptr);
    _Analysis_assume_(pBuffer != nullptr && *pBuffer != nullptr);
}
