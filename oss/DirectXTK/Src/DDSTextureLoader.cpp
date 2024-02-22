//--------------------------------------------------------------------------------------
// File: DDSTextureLoader.cpp
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

#include "pch.h"

#include "DDSTextureLoader.h"

#include "PlatformHelpers.h"
#include "DDS.h"
#include "DirectXHelpers.h"
#include "LoaderHelpers.h"

using namespace DirectX;
using namespace DirectX::LoaderHelpers;

static_assert(static_cast<int>(DDS_DIMENSION_TEXTURE1D) == static_cast<int>(D3D11_RESOURCE_DIMENSION_TEXTURE1D), "dds mismatch");
static_assert(static_cast<int>(DDS_DIMENSION_TEXTURE2D) == static_cast<int>(D3D11_RESOURCE_DIMENSION_TEXTURE2D), "dds mismatch");
static_assert(static_cast<int>(DDS_DIMENSION_TEXTURE3D) == static_cast<int>(D3D11_RESOURCE_DIMENSION_TEXTURE3D), "dds mismatch");
static_assert(static_cast<int>(DDS_RESOURCE_MISC_TEXTURECUBE) == static_cast<int>(D3D11_RESOURCE_MISC_TEXTURECUBE), "dds mismatch");

namespace
{
    //--------------------------------------------------------------------------------------
    HRESULT FillInitData(
        _In_ size_t width,
        _In_ size_t height,
        _In_ size_t depth,
        _In_ size_t mipCount,
        _In_ size_t arraySize,
        _In_ DXGI_FORMAT format,
        _In_ size_t maxsize,
        _In_ size_t bitSize,
        _In_reads_bytes_(bitSize) const uint8_t* bitData,
        _Out_ size_t& twidth,
        _Out_ size_t& theight,
        _Out_ size_t& tdepth,
        _Out_ size_t& skipMip,
        _Out_writes_(mipCount*arraySize) D3D11_SUBRESOURCE_DATA* initData) noexcept
    {
        if (!bitData || !initData)
        {
            return E_POINTER;
        }

        skipMip = 0;
        twidth = 0;
        theight = 0;
        tdepth = 0;

        size_t NumBytes = 0;
        size_t RowBytes = 0;
        const uint8_t* pSrcBits = bitData;
        const uint8_t* pEndBits = bitData + bitSize;

        size_t index = 0;
        for (size_t j = 0; j < arraySize; j++)
        {
            size_t w = width;
            size_t h = height;
            size_t d = depth;
            for (size_t i = 0; i < mipCount; i++)
            {
                HRESULT hr = GetSurfaceInfo(w, h, format, &NumBytes, &RowBytes, nullptr);
                if (FAILED(hr))
                    return hr;

                if (NumBytes > UINT32_MAX || RowBytes > UINT32_MAX)
                    return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);

                if ((mipCount <= 1) || !maxsize || (w <= maxsize && h <= maxsize && d <= maxsize))
                {
                    if (!twidth)
                    {
                        twidth = w;
                        theight = h;
                        tdepth = d;
                    }

                    assert(index < mipCount * arraySize);
                    _Analysis_assume_(index < mipCount * arraySize);
                    initData[index].pSysMem = pSrcBits;
                    initData[index].SysMemPitch = static_cast<UINT>(RowBytes);
                    initData[index].SysMemSlicePitch = static_cast<UINT>(NumBytes);
                    ++index;
                }
                else if (!j)
                {
                    // Count number of skipped mipmaps (first item only)
                    ++skipMip;
                }

                if (pSrcBits + (NumBytes*d) > pEndBits)
                {
                    return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
                }

                pSrcBits += NumBytes * d;

                w = w >> 1;
                h = h >> 1;
                d = d >> 1;
                if (w == 0)
                {
                    w = 1;
                }
                if (h == 0)
                {
                    h = 1;
                }
                if (d == 0)
                {
                    d = 1;
                }
            }
        }

        return (index > 0) ? S_OK : E_FAIL;
    }

    //--------------------------------------------------------------------------------------
    HRESULT CreateD3DResources(
        _In_ ID3D11Device* d3dDevice,
        _In_ uint32_t resDim,
        _In_ size_t width,
        _In_ size_t height,
        _In_ size_t depth,
        _In_ size_t mipCount,
        _In_ size_t arraySize,
        _In_ DXGI_FORMAT format,
        _In_ D3D11_USAGE usage,
        _In_ unsigned int bindFlags,
        _In_ unsigned int cpuAccessFlags,
        _In_ unsigned int miscFlags,
        _In_ DDS_LOADER_FLAGS loadFlags,
        _In_ bool isCubeMap,
        _In_reads_opt_(mipCount*arraySize) const D3D11_SUBRESOURCE_DATA* initData,
        _Outptr_opt_ ID3D11Resource** texture,
        _Outptr_opt_ ID3D11ShaderResourceView** textureView) noexcept
    {
        if (!d3dDevice)
            return E_POINTER;

        HRESULT hr = E_FAIL;

        if (loadFlags & DDS_LOADER_FORCE_SRGB)
        {
            format = MakeSRGB(format);
        }
        else if (loadFlags & DDS_LOADER_IGNORE_SRGB)
        {
            format = MakeLinear(format);
        }

        switch (resDim)
        {
        case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
            {
                D3D11_TEXTURE1D_DESC desc = {};
                desc.Width = static_cast<UINT>(width);
                desc.MipLevels = static_cast<UINT>(mipCount);
                desc.ArraySize = static_cast<UINT>(arraySize);
                desc.Format = format;
                desc.Usage = usage;
                desc.BindFlags = bindFlags;
                desc.CPUAccessFlags = cpuAccessFlags;
                desc.MiscFlags = miscFlags & ~static_cast<unsigned int>(D3D11_RESOURCE_MISC_TEXTURECUBE);

                ID3D11Texture1D* tex = nullptr;
                hr = d3dDevice->CreateTexture1D(&desc,
                    initData,
                    &tex
                );
                if (SUCCEEDED(hr) && tex)
                {
                    if (textureView)
                    {
                        D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
                        SRVDesc.Format = format;

                        if (arraySize > 1)
                        {
                            SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1DARRAY;
                            SRVDesc.Texture1DArray.MipLevels = (!mipCount) ? UINT(-1) : desc.MipLevels;
                            SRVDesc.Texture1DArray.ArraySize = static_cast<UINT>(arraySize);
                        }
                        else
                        {
                            SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
                            SRVDesc.Texture1D.MipLevels = (!mipCount) ? UINT(-1) : desc.MipLevels;
                        }

                        hr = d3dDevice->CreateShaderResourceView(tex,
                            &SRVDesc,
                            textureView
                        );
                        if (FAILED(hr))
                        {
                            tex->Release();
                            return hr;
                        }
                    }

                    if (texture)
                    {
                        *texture = tex;
                    }
                    else
                    {
                        SetDebugObjectName(tex, "DDSTextureLoader");
                        tex->Release();
                    }
                }
            }
            break;

        case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
            {
                D3D11_TEXTURE2D_DESC desc = {};
                desc.Width = static_cast<UINT>(width);
                desc.Height = static_cast<UINT>(height);
                desc.MipLevels = static_cast<UINT>(mipCount);
                desc.ArraySize = static_cast<UINT>(arraySize);
                desc.Format = format;
                desc.SampleDesc.Count = 1;
                desc.SampleDesc.Quality = 0;
                desc.Usage = usage;
                desc.BindFlags = bindFlags;
                desc.CPUAccessFlags = cpuAccessFlags;
                if (isCubeMap)
                {
                    desc.MiscFlags = miscFlags | D3D11_RESOURCE_MISC_TEXTURECUBE;
                }
                else
                {
                    desc.MiscFlags = miscFlags & ~static_cast<unsigned int>(D3D11_RESOURCE_MISC_TEXTURECUBE);
                }

                ID3D11Texture2D* tex = nullptr;
                hr = d3dDevice->CreateTexture2D(&desc,
                    initData,
                    &tex
                );
                if (SUCCEEDED(hr) && tex)
                {
                    if (textureView)
                    {
                        D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
                        SRVDesc.Format = format;

                        if (isCubeMap)
                        {
                            if (arraySize > 6)
                            {
                                SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
                                SRVDesc.TextureCubeArray.MipLevels = (!mipCount) ? UINT(-1) : desc.MipLevels;

                                // Earlier we set arraySize to (NumCubes * 6)
                                SRVDesc.TextureCubeArray.NumCubes = static_cast<UINT>(arraySize / 6);
                            }
                            else
                            {
                                SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
                                SRVDesc.TextureCube.MipLevels = (!mipCount) ? UINT(-1) : desc.MipLevels;
                            }
                        }
                        else if (arraySize > 1)
                        {
                            SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
                            SRVDesc.Texture2DArray.MipLevels = (!mipCount) ? UINT(-1) : desc.MipLevels;
                            SRVDesc.Texture2DArray.ArraySize = static_cast<UINT>(arraySize);
                        }
                        else
                        {
                            SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                            SRVDesc.Texture2D.MipLevels = (!mipCount) ? UINT(-1) : desc.MipLevels;
                        }

                        hr = d3dDevice->CreateShaderResourceView(tex,
                            &SRVDesc,
                            textureView
                        );
                        if (FAILED(hr))
                        {
                            tex->Release();
                            return hr;
                        }
                    }

                    if (texture)
                    {
                        *texture = tex;
                    }
                    else
                    {
                        SetDebugObjectName(tex, "DDSTextureLoader");
                        tex->Release();
                    }
                }
            }
            break;

        case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
            {
                D3D11_TEXTURE3D_DESC desc = {};
                desc.Width = static_cast<UINT>(width);
                desc.Height = static_cast<UINT>(height);
                desc.Depth = static_cast<UINT>(depth);
                desc.MipLevels = static_cast<UINT>(mipCount);
                desc.Format = format;
                desc.Usage = usage;
                desc.BindFlags = bindFlags;
                desc.CPUAccessFlags = cpuAccessFlags;
                desc.MiscFlags = miscFlags & ~UINT(D3D11_RESOURCE_MISC_TEXTURECUBE);

                ID3D11Texture3D* tex = nullptr;
                hr = d3dDevice->CreateTexture3D(&desc,
                    initData,
                    &tex
                );
                if (SUCCEEDED(hr) && tex)
                {
                    if (textureView)
                    {
                        D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
                        SRVDesc.Format = format;

                        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
                        SRVDesc.Texture3D.MipLevels = (!mipCount) ? UINT(-1) : desc.MipLevels;

                        hr = d3dDevice->CreateShaderResourceView(tex,
                            &SRVDesc,
                            textureView
                        );
                        if (FAILED(hr))
                        {
                            tex->Release();
                            return hr;
                        }
                    }

                    if (texture)
                    {
                        *texture = tex;
                    }
                    else
                    {
                        SetDebugObjectName(tex, "DDSTextureLoader");
                        tex->Release();
                    }
                }
            }
            break;
        }

        return hr;
    }

    //--------------------------------------------------------------------------------------
    HRESULT CreateTextureFromDDS(
        _In_ ID3D11Device* d3dDevice,
        _In_opt_ ID3D11DeviceContext* d3dContext,
    #if defined(_XBOX_ONE) && defined(_TITLE)
        _In_opt_ ID3D11DeviceX* d3dDeviceX,
        _In_opt_ ID3D11DeviceContextX* d3dContextX,
    #endif
        _In_ const DDS_HEADER* header,
        _In_reads_bytes_(bitSize) const uint8_t* bitData,
        _In_ size_t bitSize,
        _In_ size_t maxsize,
        _In_ D3D11_USAGE usage,
        _In_ unsigned int bindFlags,
        _In_ unsigned int cpuAccessFlags,
        _In_ unsigned int miscFlags,
        _In_ DDS_LOADER_FLAGS loadFlags,
        _Outptr_opt_ ID3D11Resource** texture,
        _Outptr_opt_ ID3D11ShaderResourceView** textureView) noexcept
    {
        HRESULT hr = S_OK;

        const UINT width = header->width;
        UINT height = header->height;
        UINT depth = header->depth;

        uint32_t resDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
        UINT arraySize = 1;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        bool isCubeMap = false;

        size_t mipCount = header->mipMapCount;
        if (0 == mipCount)
        {
            mipCount = 1;
        }

        if ((header->ddspf.flags & DDS_FOURCC) &&
            (MAKEFOURCC('D', 'X', '1', '0') == header->ddspf.fourCC))
        {
            auto d3d10ext = reinterpret_cast<const DDS_HEADER_DXT10*>(reinterpret_cast<const char*>(header) + sizeof(DDS_HEADER));

            arraySize = d3d10ext->arraySize;
            if (arraySize == 0)
            {
                return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
            }

            switch (d3d10ext->dxgiFormat)
            {
            case DXGI_FORMAT_AI44:
            case DXGI_FORMAT_IA44:
            case DXGI_FORMAT_P8:
            case DXGI_FORMAT_A8P8:
                DebugTrace("ERROR: DDSTextureLoader does not support video textures. Consider using DirectXTex instead.\n");
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

            default:
                if (BitsPerPixel(d3d10ext->dxgiFormat) == 0)
                {
                    DebugTrace("ERROR: Unknown DXGI format (%u)\n", static_cast<uint32_t>(d3d10ext->dxgiFormat));
                    return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
                }
            }

            format = d3d10ext->dxgiFormat;

            switch (d3d10ext->resourceDimension)
            {
            case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
                // D3DX writes 1D textures with a fixed Height of 1
                if ((header->flags & DDS_HEIGHT) && height != 1)
                {
                    return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
                }
                height = depth = 1;
                break;

            case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
                if (d3d10ext->miscFlag & D3D11_RESOURCE_MISC_TEXTURECUBE)
                {
                    arraySize *= 6;
                    isCubeMap = true;
                }
                depth = 1;
                break;

            case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
                if (!(header->flags & DDS_HEADER_FLAGS_VOLUME))
                {
                    return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
                }

                if (arraySize > 1)
                {
                    DebugTrace("ERROR: Volume textures are not texture arrays\n");
                    return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
                }
                break;

            case D3D11_RESOURCE_DIMENSION_BUFFER:
                DebugTrace("ERROR: Resource dimension buffer type not supported for textures\n");
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

            case D3D11_RESOURCE_DIMENSION_UNKNOWN:
            default:
                DebugTrace("ERROR: Unknown resource dimension (%u)\n", static_cast<uint32_t>(d3d10ext->resourceDimension));
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            }

            resDim = d3d10ext->resourceDimension;
        }
        else
        {
            format = GetDXGIFormat(header->ddspf);

            if (format == DXGI_FORMAT_UNKNOWN)
            {
                DebugTrace("ERROR: DDSTextureLoader does not support all legacy DDS formats. Consider using DirectXTex.\n");
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            }

            if (header->flags & DDS_HEADER_FLAGS_VOLUME)
            {
                resDim = D3D11_RESOURCE_DIMENSION_TEXTURE3D;
            }
            else
            {
                if (header->caps2 & DDS_CUBEMAP)
                {
                    // We require all six faces to be defined
                    if ((header->caps2 & DDS_CUBEMAP_ALLFACES) != DDS_CUBEMAP_ALLFACES)
                    {
                        DebugTrace("ERROR: DirectX 11 does not support partial cubemaps\n");
                        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
                    }

                    arraySize = 6;
                    isCubeMap = true;
                }

                depth = 1;
                resDim = D3D11_RESOURCE_DIMENSION_TEXTURE2D;

                // Note there's no way for a legacy Direct3D 9 DDS to express a '1D' texture
            }

            assert(BitsPerPixel(format) != 0);
        }

        if ((miscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE)
            && (resDim == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
            && ((arraySize % 6) == 0))
        {
            isCubeMap = true;
        }

        // Bound sizes (for security purposes we don't trust DDS file metadata larger than the Direct3D hardware requirements)
        if (mipCount > D3D11_REQ_MIP_LEVELS)
        {
            DebugTrace("ERROR: Too many mipmap levels defined for DirectX 11 (%zu).\n", mipCount);
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }

        switch (resDim)
        {
        case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
            if ((arraySize > D3D11_REQ_TEXTURE1D_ARRAY_AXIS_DIMENSION) ||
                (width > D3D11_REQ_TEXTURE1D_U_DIMENSION))
            {
                DebugTrace("ERROR: Resource dimensions too large for DirectX 11 (1D: array %u, size %u)\n", arraySize, width);
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            }
            break;

        case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
            if (isCubeMap)
            {
                // This is the right bound because we set arraySize to (NumCubes*6) above
                if ((arraySize > D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION) ||
                    (width > D3D11_REQ_TEXTURECUBE_DIMENSION) ||
                    (height > D3D11_REQ_TEXTURECUBE_DIMENSION))
                {
                    DebugTrace("ERROR: Resource dimensions too large for DirectX 11 (2D cubemap: array %u, size %u by %u)\n", arraySize, width, height);
                    return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
                }
            }
            else if ((arraySize > D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION) ||
                (width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION) ||
                (height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION))
            {
                DebugTrace("ERROR: Resource dimensions too large for DirectX 11 (2D: array %u, size %u by %u)\n", arraySize, width, height);
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            }
            break;

        case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
            if ((arraySize > 1) ||
                (width > D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION) ||
                (height > D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION) ||
                (depth > D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION))
            {
                DebugTrace("ERROR: Resource dimensions too large for DirectX 11 (3D: array %u, size %u by %u by %u)\n", arraySize, width, height, depth);
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            }
            break;

        case D3D11_RESOURCE_DIMENSION_BUFFER:
            DebugTrace("ERROR: Resource dimension buffer type not supported for textures\n");
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

        case D3D11_RESOURCE_DIMENSION_UNKNOWN:
        default:
            DebugTrace("ERROR: Unknown resource dimension (%u)\n", static_cast<uint32_t>(resDim));
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }

        bool autogen = false;
        if (mipCount == 1 && d3dContext && textureView) // Must have context and shader-view to auto generate mipmaps
        {
            // See if format is supported for auto-gen mipmaps (varies by feature level)
            UINT fmtSupport = 0;
            hr = d3dDevice->CheckFormatSupport(format, &fmtSupport);
            if (SUCCEEDED(hr) && (fmtSupport & D3D11_FORMAT_SUPPORT_MIP_AUTOGEN))
            {
                // 10level9 feature levels do not support auto-gen mipgen for volume textures
                if ((resDim != D3D11_RESOURCE_DIMENSION_TEXTURE3D)
                    || (d3dDevice->GetFeatureLevel() >= D3D_FEATURE_LEVEL_10_0))
                {
                    autogen = true;
                #if defined(_XBOX_ONE) && defined(_TITLE)
                    if (!d3dDeviceX || !d3dContextX)
                        return E_INVALIDARG;
                #endif
                }
            }
        }

        if (autogen)
        {
            // Create texture with auto-generated mipmaps
            ID3D11Resource* tex = nullptr;
            hr = CreateD3DResources(d3dDevice,
                resDim, width, height, depth, 0, arraySize,
                format,
                usage,
                bindFlags | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
                cpuAccessFlags,
                miscFlags | D3D11_RESOURCE_MISC_GENERATE_MIPS, loadFlags,
                isCubeMap,
                nullptr,
                &tex, textureView);
            if (SUCCEEDED(hr))
            {
                size_t numBytes = 0;
                size_t rowBytes = 0;
                hr = GetSurfaceInfo(width, height, format, &numBytes, &rowBytes, nullptr);
                if (FAILED(hr))
                    return hr;

                if (numBytes > bitSize)
                {
                    (*textureView)->Release();
                    *textureView = nullptr;
                    tex->Release();
                    return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
                }

                if (numBytes > UINT32_MAX || rowBytes > UINT32_MAX)
                    return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);

                D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
                (*textureView)->GetDesc(&desc);

                UINT mipLevels = 1;

                switch (desc.ViewDimension)
                {
                case D3D_SRV_DIMENSION_TEXTURE1D:       mipLevels = desc.Texture1D.MipLevels; break;
                case D3D_SRV_DIMENSION_TEXTURE1DARRAY:  mipLevels = desc.Texture1DArray.MipLevels; break;
                case D3D_SRV_DIMENSION_TEXTURE2D:       mipLevels = desc.Texture2D.MipLevels; break;
                case D3D_SRV_DIMENSION_TEXTURE2DARRAY:  mipLevels = desc.Texture2DArray.MipLevels; break;
                case D3D_SRV_DIMENSION_TEXTURECUBE:     mipLevels = desc.TextureCube.MipLevels; break;
                case D3D_SRV_DIMENSION_TEXTURECUBEARRAY:mipLevels = desc.TextureCubeArray.MipLevels; break;
                case D3D_SRV_DIMENSION_TEXTURE3D:       mipLevels = desc.Texture3D.MipLevels; break;
                default:
                    (*textureView)->Release();
                    *textureView = nullptr;
                    tex->Release();
                    return E_UNEXPECTED;
                }

            #if defined(_XBOX_ONE) && defined(_TITLE)

                std::unique_ptr<D3D11_SUBRESOURCE_DATA[]> initData(new (std::nothrow) D3D11_SUBRESOURCE_DATA[arraySize]);
                if (!initData)
                {
                    return E_OUTOFMEMORY;
                }

                const uint8_t* pSrcBits = bitData;
                const uint8_t* pEndBits = bitData + bitSize;
                for (UINT item = 0; item < arraySize; ++item)
                {
                    if ((pSrcBits + numBytes) > pEndBits)
                    {
                        (*textureView)->Release();
                        *textureView = nullptr;
                        tex->Release();
                        return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
                    }

                    initData[item].pSysMem = pSrcBits;
                    initData[item].SysMemPitch = static_cast<UINT>(rowBytes);
                    initData[item].SysMemSlicePitch = static_cast<UINT>(numBytes);
                    pSrcBits += numBytes;
                }

                ID3D11Resource* pStaging = nullptr;
                switch (resDim)
                {
                case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
                    {
                        ID3D11Texture1D *temp = nullptr;
                        CD3D11_TEXTURE1D_DESC stagingDesc(format, width, arraySize, 1, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ);
                        hr = d3dDevice->CreateTexture1D(&stagingDesc, initData.get(), &temp);
                        if (SUCCEEDED(hr))
                            pStaging = temp;
                    }
                    break;

                case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
                    {
                        ID3D11Texture2D *temp = nullptr;
                        CD3D11_TEXTURE2D_DESC stagingDesc(format, width, height, arraySize, 1, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ, 1, 0, isCubeMap ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0);
                        hr = d3dDevice->CreateTexture2D(&stagingDesc, initData.get(), &temp);
                        if (SUCCEEDED(hr))
                            pStaging = temp;
                    }
                    break;

                case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
                    {
                        ID3D11Texture3D *temp = nullptr;
                        CD3D11_TEXTURE3D_DESC stagingDesc(format, width, height, depth, 1, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ);
                        hr = d3dDevice->CreateTexture3D(&stagingDesc, initData.get(), &temp);
                        if (SUCCEEDED(hr))
                            pStaging = temp;
                    }
                    break;
                };

                if (SUCCEEDED(hr))
                {
                    for (UINT item = 0; item < arraySize; ++item)
                    {
                        UINT res = D3D11CalcSubresource(0, item, mipLevels);
                        d3dContext->CopySubresourceRegion(tex, res, 0, 0, 0, pStaging, item, nullptr);
                    }

                    UINT64 copyFence = d3dContextX->InsertFence(0);
                    while (d3dDeviceX->IsFencePending(copyFence)) { SwitchToThread(); }
                    pStaging->Release();
                }
            #else
                if (arraySize > 1)
                {
                    const uint8_t* pSrcBits = bitData;
                    const uint8_t* pEndBits = bitData + bitSize;
                    for (UINT item = 0; item < arraySize; ++item)
                    {
                        if ((pSrcBits + numBytes) > pEndBits)
                        {
                            (*textureView)->Release();
                            *textureView = nullptr;
                            tex->Release();
                            return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
                        }

                        const UINT res = D3D11CalcSubresource(0, item, mipLevels);
                        d3dContext->UpdateSubresource(tex, res, nullptr, pSrcBits, static_cast<UINT>(rowBytes), static_cast<UINT>(numBytes));
                        pSrcBits += numBytes;
                    }
                }
                else
                {
                    d3dContext->UpdateSubresource(tex, 0, nullptr, bitData, static_cast<UINT>(rowBytes), static_cast<UINT>(numBytes));
                }
            #endif

                d3dContext->GenerateMips(*textureView);

                if (texture)
                {
                    *texture = tex;
                }
                else
                {
                    tex->Release();
                }
            }
        }
        else
        {
            // Create the texture
            std::unique_ptr<D3D11_SUBRESOURCE_DATA[]> initData(new (std::nothrow) D3D11_SUBRESOURCE_DATA[mipCount * arraySize]);
            if (!initData)
            {
                return E_OUTOFMEMORY;
            }

            size_t skipMip = 0;
            size_t twidth = 0;
            size_t theight = 0;
            size_t tdepth = 0;
            hr = FillInitData(width, height, depth, mipCount, arraySize, format,
                maxsize, bitSize, bitData,
                twidth, theight, tdepth, skipMip, initData.get());

            if (SUCCEEDED(hr))
            {
                hr = CreateD3DResources(d3dDevice,
                    resDim, twidth, theight, tdepth, mipCount - skipMip, arraySize,
                    format,
                    usage, bindFlags, cpuAccessFlags, miscFlags,
                    loadFlags,
                    isCubeMap,
                    initData.get(),
                    texture, textureView);

                if (FAILED(hr) && !maxsize && (mipCount > 1))
                {
                    // Retry with a maxsize determined by feature level
                    switch (d3dDevice->GetFeatureLevel())
                    {
                    case D3D_FEATURE_LEVEL_9_1:
                    case D3D_FEATURE_LEVEL_9_2:
                        if (isCubeMap)
                        {
                            maxsize = 512u /*D3D_FL9_1_REQ_TEXTURECUBE_DIMENSION*/;
                        }
                        else
                        {
                            maxsize = (resDim == D3D11_RESOURCE_DIMENSION_TEXTURE3D)
                                ? 256u /*D3D_FL9_1_REQ_TEXTURE3D_U_V_OR_W_DIMENSION*/
                                : 2048u /*D3D_FL9_1_REQ_TEXTURE2D_U_OR_V_DIMENSION*/;
                        }
                        break;

                    case D3D_FEATURE_LEVEL_9_3:
                        maxsize = (resDim == D3D11_RESOURCE_DIMENSION_TEXTURE3D)
                            ? 256u /*D3D_FL9_1_REQ_TEXTURE3D_U_V_OR_W_DIMENSION*/
                            : 4096u /*D3D_FL9_3_REQ_TEXTURE2D_U_OR_V_DIMENSION*/;
                        break;

                    default: // D3D_FEATURE_LEVEL_10_0 & D3D_FEATURE_LEVEL_10_1
                        maxsize = (resDim == D3D11_RESOURCE_DIMENSION_TEXTURE3D)
                            ? 2048u /*D3D10_REQ_TEXTURE3D_U_V_OR_W_DIMENSION*/
                            : 8192u /*D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION*/;
                        break;
                    }

                    hr = FillInitData(width, height, depth, mipCount, arraySize, format,
                        maxsize, bitSize, bitData,
                        twidth, theight, tdepth, skipMip, initData.get());
                    if (SUCCEEDED(hr))
                    {
                        hr = CreateD3DResources(d3dDevice,
                            resDim, twidth, theight, tdepth, mipCount - skipMip, arraySize,
                            format,
                            usage, bindFlags, cpuAccessFlags, miscFlags,
                            loadFlags,
                            isCubeMap,
                            initData.get(),
                            texture, textureView);
                    }
                }
            }
        }

        return hr;
    }

    //--------------------------------------------------------------------------------------
    void SetDebugTextureInfo(
        _In_z_ const wchar_t* fileName,
        _In_opt_ ID3D11Resource** texture,
        _In_opt_ ID3D11ShaderResourceView** textureView) noexcept
    {
    #if !defined(NO_D3D11_DEBUG_NAME) && ( defined(_DEBUG) || defined(PROFILE) )
        if (texture || textureView)
        {
        #if defined(_XBOX_ONE) && defined(_TITLE)
            const wchar_t* pstrName = wcsrchr(fileName, '\\');
            if (!pstrName)
            {
                pstrName = fileName;
            }
            else
            {
                pstrName++;
            }
            if (texture && *texture)
            {
                (*texture)->SetName(pstrName);
            }
            if (textureView && *textureView)
            {
                (*textureView)->SetName(pstrName);
            }
        #else
            CHAR strFileA[MAX_PATH];
            const int result = WideCharToMultiByte(CP_UTF8,
                WC_NO_BEST_FIT_CHARS,
                fileName,
                -1,
                strFileA,
                MAX_PATH,
                nullptr,
                nullptr
            );
            if (result > 0)
            {
                const char* pstrName = strrchr(strFileA, '\\');
                if (!pstrName)
                {
                    pstrName = strFileA;
                }
                else
                {
                    pstrName++;
                }

                if (texture && *texture)
                {
                    (*texture)->SetPrivateData(WKPDID_D3DDebugObjectName,
                        static_cast<UINT>(strnlen_s(pstrName, MAX_PATH)),
                        pstrName
                    );
                }

                if (textureView && *textureView)
                {
                    (*textureView)->SetPrivateData(WKPDID_D3DDebugObjectName,
                        static_cast<UINT>(strnlen_s(pstrName, MAX_PATH)),
                        pstrName
                    );
                }
            }
        #endif
        }
    #else
        UNREFERENCED_PARAMETER(fileName);
        UNREFERENCED_PARAMETER(texture);
        UNREFERENCED_PARAMETER(textureView);
    #endif
    }
} // anonymous namespace


//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::CreateDDSTextureFromMemory(
    ID3D11Device* d3dDevice,
    const uint8_t* ddsData,
    size_t ddsDataSize,
    ID3D11Resource** texture,
    ID3D11ShaderResourceView** textureView,
    size_t maxsize,
    DDS_ALPHA_MODE* alphaMode) noexcept
{
    return CreateDDSTextureFromMemoryEx(d3dDevice,
        ddsData, ddsDataSize,
        maxsize,
        D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0,
        DDS_LOADER_DEFAULT,
        texture, textureView, alphaMode);
}

_Use_decl_annotations_
#if defined(_XBOX_ONE) && defined(_TITLE)
HRESULT DirectX::CreateDDSTextureFromMemory(
    ID3D11DeviceX* d3dDevice,
    ID3D11DeviceContextX* d3dContext,
#else
    HRESULT DirectX::CreateDDSTextureFromMemory(
        ID3D11Device* d3dDevice,
        ID3D11DeviceContext* d3dContext,
    #endif
        const uint8_t* ddsData,
        size_t ddsDataSize,
        ID3D11Resource** texture,
        ID3D11ShaderResourceView** textureView,
        size_t maxsize,
        DDS_ALPHA_MODE* alphaMode) noexcept
{
    return CreateDDSTextureFromMemoryEx(d3dDevice, d3dContext,
        ddsData, ddsDataSize,
        maxsize,
        D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0,
        DDS_LOADER_DEFAULT,
        texture, textureView, alphaMode);
}

_Use_decl_annotations_
HRESULT DirectX::CreateDDSTextureFromMemoryEx(
    ID3D11Device* d3dDevice,
    const uint8_t* ddsData,
    size_t ddsDataSize,
    size_t maxsize,
    D3D11_USAGE usage,
    unsigned int bindFlags,
    unsigned int cpuAccessFlags,
    unsigned int miscFlags,
    DDS_LOADER_FLAGS loadFlags,
    ID3D11Resource** texture,
    ID3D11ShaderResourceView** textureView,
    DDS_ALPHA_MODE* alphaMode) noexcept
{
    if (texture)
    {
        *texture = nullptr;
    }
    if (textureView)
    {
        *textureView = nullptr;
    }
    if (alphaMode)
    {
        *alphaMode = DDS_ALPHA_MODE_UNKNOWN;
    }

    if (!d3dDevice || !ddsData || (!texture && !textureView))
    {
        return E_INVALIDARG;
    }

    if (textureView && !(bindFlags & D3D11_BIND_SHADER_RESOURCE))
    {
        return E_INVALIDARG;
    }

    // Validate DDS file in memory
    const DDS_HEADER* header = nullptr;
    const uint8_t* bitData = nullptr;
    size_t bitSize = 0;

    HRESULT hr = LoadTextureDataFromMemory(ddsData, ddsDataSize,
        &header,
        &bitData,
        &bitSize
    );
    if (FAILED(hr))
    {
        return hr;
    }

    hr = CreateTextureFromDDS(d3dDevice, nullptr,
    #if defined(_XBOX_ONE) && defined(_TITLE)
        nullptr, nullptr,
    #endif
        header, bitData, bitSize,
        maxsize,
        usage, bindFlags, cpuAccessFlags, miscFlags,
        loadFlags,
        texture, textureView);
    if (SUCCEEDED(hr))
    {
        if (texture && *texture)
        {
            SetDebugObjectName(*texture, "DDSTextureLoader");
        }

        if (textureView && *textureView)
        {
            SetDebugObjectName(*textureView, "DDSTextureLoader");
        }

        if (alphaMode)
            *alphaMode = GetAlphaMode(header);
    }

    return hr;
}

_Use_decl_annotations_
#if defined(_XBOX_ONE) && defined(_TITLE)
HRESULT DirectX::CreateDDSTextureFromMemoryEx(
    ID3D11DeviceX* d3dDevice,
    ID3D11DeviceContextX* d3dContext,
#else
    HRESULT DirectX::CreateDDSTextureFromMemoryEx(
        ID3D11Device* d3dDevice,
        ID3D11DeviceContext* d3dContext,
    #endif
        const uint8_t* ddsData,
        size_t ddsDataSize,
        size_t maxsize,
        D3D11_USAGE usage,
        unsigned int bindFlags,
        unsigned int cpuAccessFlags,
        unsigned int miscFlags,
        DDS_LOADER_FLAGS loadFlags,
        ID3D11Resource** texture,
        ID3D11ShaderResourceView** textureView,
        DDS_ALPHA_MODE* alphaMode) noexcept
{
    if (texture)
    {
        *texture = nullptr;
    }
    if (textureView)
    {
        *textureView = nullptr;
    }
    if (alphaMode)
    {
        *alphaMode = DDS_ALPHA_MODE_UNKNOWN;
    }

    if (!d3dDevice || !ddsData || (!texture && !textureView))
    {
        return E_INVALIDARG;
    }

    if (textureView && !(bindFlags & D3D11_BIND_SHADER_RESOURCE))
    {
        return E_INVALIDARG;
    }

    // Validate DDS file in memory
    const DDS_HEADER* header = nullptr;
    const uint8_t* bitData = nullptr;
    size_t bitSize = 0;

    HRESULT hr = LoadTextureDataFromMemory(ddsData, ddsDataSize,
        &header,
        &bitData,
        &bitSize
    );
    if (FAILED(hr))
    {
        return hr;
    }

    hr = CreateTextureFromDDS(d3dDevice, d3dContext,
    #if defined(_XBOX_ONE) && defined(_TITLE)
        d3dDevice, d3dContext,
    #endif
        header, bitData, bitSize,
        maxsize,
        usage, bindFlags, cpuAccessFlags, miscFlags,
        loadFlags,
        texture, textureView);
    if (SUCCEEDED(hr))
    {
        if (texture && *texture)
        {
            SetDebugObjectName(*texture, "DDSTextureLoader");
        }

        if (textureView && *textureView)
        {
            SetDebugObjectName(*textureView, "DDSTextureLoader");
        }

        if (alphaMode)
            *alphaMode = GetAlphaMode(header);
    }

    return hr;
}

//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::CreateDDSTextureFromFile(
    ID3D11Device* d3dDevice,
    const wchar_t* fileName,
    ID3D11Resource** texture,
    ID3D11ShaderResourceView** textureView,
    size_t maxsize,
    DDS_ALPHA_MODE* alphaMode) noexcept
{
    return CreateDDSTextureFromFileEx(d3dDevice,
        fileName,
        maxsize,
        D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0,
        DDS_LOADER_DEFAULT,
        texture, textureView, alphaMode);
}

_Use_decl_annotations_
#if defined(_XBOX_ONE) && defined(_TITLE)
HRESULT DirectX::CreateDDSTextureFromFile(
    ID3D11DeviceX* d3dDevice,
    ID3D11DeviceContextX* d3dContext,
#else
    HRESULT DirectX::CreateDDSTextureFromFile(
        ID3D11Device* d3dDevice,
        ID3D11DeviceContext* d3dContext,
    #endif
        const wchar_t* fileName,
        ID3D11Resource** texture,
        ID3D11ShaderResourceView** textureView,
        size_t maxsize,
        DDS_ALPHA_MODE* alphaMode) noexcept
{
    return CreateDDSTextureFromFileEx(d3dDevice, d3dContext,
        fileName,
        maxsize,
        D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0,
        DDS_LOADER_DEFAULT,
        texture, textureView, alphaMode);
}

_Use_decl_annotations_
HRESULT DirectX::CreateDDSTextureFromFileEx(
    ID3D11Device* d3dDevice,
    const wchar_t* fileName,
    size_t maxsize,
    D3D11_USAGE usage,
    unsigned int bindFlags,
    unsigned int cpuAccessFlags,
    unsigned int miscFlags,
    DDS_LOADER_FLAGS loadFlags,
    ID3D11Resource** texture,
    ID3D11ShaderResourceView** textureView,
    DDS_ALPHA_MODE* alphaMode) noexcept
{
    if (texture)
    {
        *texture = nullptr;
    }
    if (textureView)
    {
        *textureView = nullptr;
    }
    if (alphaMode)
    {
        *alphaMode = DDS_ALPHA_MODE_UNKNOWN;
    }

    if (!d3dDevice || !fileName || (!texture && !textureView))
    {
        return E_INVALIDARG;
    }

    if (textureView && !(bindFlags & D3D11_BIND_SHADER_RESOURCE))
    {
        return E_INVALIDARG;
    }

    const DDS_HEADER* header = nullptr;
    const uint8_t* bitData = nullptr;
    size_t bitSize = 0;

    std::unique_ptr<uint8_t[]> ddsData;
    HRESULT hr = LoadTextureDataFromFile(fileName,
        ddsData,
        &header,
        &bitData,
        &bitSize
    );
    if (FAILED(hr))
    {
        return hr;
    }

    hr = CreateTextureFromDDS(d3dDevice, nullptr,
    #if defined(_XBOX_ONE) && defined(_TITLE)
        nullptr, nullptr,
    #endif
        header, bitData, bitSize,
        maxsize,
        usage, bindFlags, cpuAccessFlags, miscFlags,
        loadFlags,
        texture, textureView);

    if (SUCCEEDED(hr))
    {
        SetDebugTextureInfo(fileName, texture, textureView);

        if (alphaMode)
            *alphaMode = GetAlphaMode(header);
    }

    return hr;
}

_Use_decl_annotations_
#if defined(_XBOX_ONE) && defined(_TITLE)
HRESULT DirectX::CreateDDSTextureFromFileEx(
    ID3D11DeviceX* d3dDevice,
    ID3D11DeviceContextX* d3dContext,
#else
    HRESULT DirectX::CreateDDSTextureFromFileEx(
        ID3D11Device* d3dDevice,
        ID3D11DeviceContext* d3dContext,
    #endif
        const wchar_t* fileName,
        size_t maxsize,
        D3D11_USAGE usage,
        unsigned int bindFlags,
        unsigned int cpuAccessFlags,
        unsigned int miscFlags,
        DDS_LOADER_FLAGS loadFlags,
        ID3D11Resource** texture,
        ID3D11ShaderResourceView** textureView,
        DDS_ALPHA_MODE* alphaMode) noexcept
{
    if (texture)
    {
        *texture = nullptr;
    }
    if (textureView)
    {
        *textureView = nullptr;
    }
    if (alphaMode)
    {
        *alphaMode = DDS_ALPHA_MODE_UNKNOWN;
    }

    if (!d3dDevice || !fileName || (!texture && !textureView))
    {
        return E_INVALIDARG;
    }

    if (textureView && !(bindFlags & D3D11_BIND_SHADER_RESOURCE))
    {
        return E_INVALIDARG;
    }

    const DDS_HEADER* header = nullptr;
    const uint8_t* bitData = nullptr;
    size_t bitSize = 0;

    std::unique_ptr<uint8_t[]> ddsData;
    HRESULT hr = LoadTextureDataFromFile(fileName,
        ddsData,
        &header,
        &bitData,
        &bitSize
    );
    if (FAILED(hr))
    {
        return hr;
    }

    hr = CreateTextureFromDDS(d3dDevice, d3dContext,
    #if defined(_XBOX_ONE) && defined(_TITLE)
        d3dDevice, d3dContext,
    #endif
        header, bitData, bitSize,
        maxsize,
        usage, bindFlags, cpuAccessFlags, miscFlags,
        loadFlags,
        texture, textureView);

    if (SUCCEEDED(hr))
    {
        SetDebugTextureInfo(fileName, texture, textureView);

        if (alphaMode)
            *alphaMode = GetAlphaMode(header);
    }

    return hr;
}
