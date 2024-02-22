//--------------------------------------------------------------------------------------
// File: XboxDDSTextureLoader.cpp
//
// Functions for loading a DDS texture using the XBOX extended header and creating a
// Direct3D11.X runtime resource for it via the CreatePlacement APIs
//
// Note these functions will not load standard DDS files. Use the DDSTextureLoader
// module in the DirectXTex package or as part of the DirectXTK library to load
// these files which use standard Direct3D resource creation APIs.
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#include "pch.h"

#include "XboxDDSTextureLoader.h"

#include "PlatformHelpers.h"
#include "DDS.h"
#include "DirectXHelpers.h"

#include <xdk.h>

using namespace DirectX;
using namespace Xbox;

namespace
{
    //--------------------------------------------------------------------------------------
    // Default XMemAlloc attributes for texture loading
    //--------------------------------------------------------------------------------------
    const uint64_t c_XMemAllocAttributes = MAKE_XALLOC_ATTRIBUTES(
        eXALLOCAllocatorId_MiddlewareReservedMin,
        0,
        XALLOC_MEMTYPE_GRAPHICS_WRITECOMBINE_GPU_READONLY,
        XALLOC_PAGESIZE_64KB,
        XALLOC_ALIGNMENT_64K);

    //--------------------------------------------------------------------------------------
    // DDS file structure definitions
    //
    // See DDS.h in the 'Texconv' sample and the 'DirectXTex' library
    //--------------------------------------------------------------------------------------
    #pragma pack(push,1)

    struct DDS_HEADER_XBOX
        // Must match structure defined in xtexconv tool
    {
        DXGI_FORMAT dxgiFormat;
        uint32_t    resourceDimension;
        uint32_t    miscFlag; // see DDS_RESOURCE_MISC_FLAG
        uint32_t    arraySize;
        uint32_t    miscFlags2; // see DDS_MISC_FLAGS2
        uint32_t    tileMode; // see XG_TILE_MODE
        uint32_t    baseAlignment;
        uint32_t    dataSize;
        uint32_t    xdkVer; // matching _XDK_VER
    };

    static_assert(sizeof(DDS_HEADER_XBOX) == 36, "DDS XBOX Header size mismatch");

    #pragma pack(pop)

    //--------------------------------------------------------------------------------------
    HRESULT LoadTextureDataFromFile(_In_z_ const wchar_t* fileName,
        std::unique_ptr<uint8_t[]>& ddsData,
        DDS_HEADER** header,
        uint8_t** bitData,
        size_t* bitSize) noexcept
    {
        if (!header || !bitData || !bitSize)
        {
            return E_POINTER;
        }

        // open the file
        ScopedHandle hFile(safe_handle(CreateFile2(
            fileName,
            GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING,
            nullptr)));

        if (!hFile)
        {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        // Get the file size
        LARGE_INTEGER FileSize = {};

        FILE_STANDARD_INFO fileInfo;
        if (!GetFileInformationByHandleEx(hFile.get(), FileStandardInfo, &fileInfo, sizeof(fileInfo)))
        {
            return HRESULT_FROM_WIN32(GetLastError());
        }
        FileSize = fileInfo.EndOfFile;

        // File is too big for 32-bit allocation, so reject read
        if (FileSize.HighPart > 0)
        {
            return E_FAIL;
        }

        // Need at least enough data to fill the header and magic number to be a valid DDS
        if (FileSize.LowPart < (sizeof(DDS_HEADER) + sizeof(uint32_t)))
        {
            return E_FAIL;
        }

        // create enough space for the file data
        ddsData.reset(new (std::nothrow) uint8_t[FileSize.LowPart]);
        if (!ddsData)
        {
            return E_OUTOFMEMORY;
        }

        // read the data in
        DWORD BytesRead = 0;
        if (!ReadFile(hFile.get(),
            ddsData.get(),
            FileSize.LowPart,
            &BytesRead,
            nullptr
        ))
        {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        if (BytesRead < FileSize.LowPart)
        {
            return E_FAIL;
        }

        // DDS files always start with the same magic number ("DDS ")
        auto dwMagicNumber = *reinterpret_cast<uint32_t*>(ddsData.get());
        if (dwMagicNumber != DDS_MAGIC)
        {
            return E_FAIL;
        }

        auto hdr = reinterpret_cast<DDS_HEADER*>(ddsData.get() + sizeof(uint32_t));

        // Verify header to validate DDS file
        if (hdr->size != sizeof(DDS_HEADER) ||
            hdr->ddspf.size != sizeof(DDS_PIXELFORMAT))
        {
            return E_FAIL;
        }

        // Check for XBOX extension
        if (!(hdr->ddspf.flags & DDS_FOURCC)
            || (MAKEFOURCC('X', 'B', 'O', 'X') != hdr->ddspf.fourCC))
        {
            // Use standard DDSTextureLoader instead
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }

        // Must be long enough for both headers and magic value
        if (FileSize.LowPart < (sizeof(DDS_HEADER) + sizeof(uint32_t) + sizeof(DDS_HEADER_XBOX)))
        {
            return E_FAIL;
        }

        // setup the pointers in the process request
        *header = hdr;
        auto offset = sizeof(uint32_t) + sizeof(DDS_HEADER) + sizeof(DDS_HEADER_XBOX);
        *bitData = ddsData.get() + offset;
        *bitSize = FileSize.LowPart - offset;

        return S_OK;
    }

    //--------------------------------------------------------------------------------------
    DXGI_FORMAT MakeSRGB(_In_ DXGI_FORMAT format) noexcept
    {
        switch (format)
        {
        case DXGI_FORMAT_R8G8B8A8_UNORM:
            return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

        case DXGI_FORMAT_BC1_UNORM:
            return DXGI_FORMAT_BC1_UNORM_SRGB;

        case DXGI_FORMAT_BC2_UNORM:
            return DXGI_FORMAT_BC2_UNORM_SRGB;

        case DXGI_FORMAT_BC3_UNORM:
            return DXGI_FORMAT_BC3_UNORM_SRGB;

        case DXGI_FORMAT_B8G8R8A8_UNORM:
            return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

        case DXGI_FORMAT_B8G8R8X8_UNORM:
            return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;

        case DXGI_FORMAT_BC7_UNORM:
            return DXGI_FORMAT_BC7_UNORM_SRGB;

        default:
            return format;
        }
    }

    //--------------------------------------------------------------------------------------
    HRESULT CreateD3DResources(_In_ ID3D11DeviceX* d3dDevice,
        _In_ const DDS_HEADER_XBOX* xboxext,
        _In_ uint32_t width,
        _In_ uint32_t height,
        _In_ uint32_t depth,
        _In_ uint32_t mipCount,
        _In_ uint32_t arraySize,
        _In_ bool forceSRGB,
        _In_ bool isCubeMap,
        _In_ void* grfxMemory,
        _Outptr_opt_ ID3D11Resource** texture,
        _Outptr_opt_ ID3D11ShaderResourceView** textureView) noexcept
    {
        if (!d3dDevice || !grfxMemory)
            return E_POINTER;

        HRESULT hr = E_FAIL;

        DXGI_FORMAT format = xboxext->dxgiFormat;
        if (forceSRGB)
        {
            format = MakeSRGB(format);
        }

        switch (xboxext->resourceDimension)
        {
        case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
        {
            D3D11_TEXTURE1D_DESC desc = {};
            desc.Width = static_cast<UINT>(width);
            desc.MipLevels = static_cast<UINT>(mipCount);
            desc.ArraySize = static_cast<UINT>(arraySize);
            desc.Format = format;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

            ID3D11Texture1D* tex = nullptr;
            hr = d3dDevice->CreatePlacementTexture1D(&desc, xboxext->tileMode, 0, grfxMemory, &tex);
            if (SUCCEEDED(hr) && tex != 0)
            {
                if (textureView != 0)
                {
                    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
                    SRVDesc.Format = format;

                    if (arraySize > 1)
                    {
                        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1DARRAY;
                        SRVDesc.Texture1DArray.MipLevels = desc.MipLevels;
                        SRVDesc.Texture1DArray.ArraySize = static_cast<UINT>(arraySize);
                    }
                    else
                    {
                        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
                        SRVDesc.Texture1D.MipLevels = desc.MipLevels;
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

                if (texture != 0)
                {
                    *texture = tex;
                }
                else
                {
                    SetDebugObjectName(tex, "XboxDDSTextureLoader");
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
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            desc.MiscFlags = (isCubeMap) ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0;

            ID3D11Texture2D* tex = nullptr;
            hr = d3dDevice->CreatePlacementTexture2D(&desc, xboxext->tileMode, 0, grfxMemory, &tex);
            if (SUCCEEDED(hr) && tex != 0)
            {
                if (textureView != 0)
                {
                    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
                    SRVDesc.Format = format;

                    if (isCubeMap)
                    {
                        if (arraySize > 6)
                        {
                            SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
                            SRVDesc.TextureCubeArray.MipLevels = desc.MipLevels;

                            // Earlier we set arraySize to (NumCubes * 6)
                            SRVDesc.TextureCubeArray.NumCubes = static_cast<UINT>(arraySize / 6);
                        }
                        else
                        {
                            SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
                            SRVDesc.TextureCube.MipLevels = desc.MipLevels;
                        }
                    }
                    else if (arraySize > 1)
                    {
                        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
                        SRVDesc.Texture2DArray.MipLevels = desc.MipLevels;
                        SRVDesc.Texture2DArray.ArraySize = static_cast<UINT>(arraySize);
                    }
                    else
                    {
                        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                        SRVDesc.Texture2D.MipLevels = desc.MipLevels;
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

                if (texture != 0)
                {
                    *texture = tex;
                }
                else
                {
                    SetDebugObjectName(tex, "XboxDDSTextureLoader");
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
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

            ID3D11Texture3D* tex = nullptr;
            hr = d3dDevice->CreatePlacementTexture3D(&desc, xboxext->tileMode, 0, grfxMemory, &tex);
            if (SUCCEEDED(hr) && tex != 0)
            {
                if (textureView != 0)
                {
                    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
                    SRVDesc.Format = format;

                    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
                    SRVDesc.Texture3D.MipLevels = desc.MipLevels;

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

                if (texture != 0)
                {
                    *texture = tex;
                }
                else
                {
                    SetDebugObjectName(tex, "XboxDDSTextureLoader");
                    tex->Release();
                }
            }
        }
        break;
        }

        return hr;
    }

    //--------------------------------------------------------------------------------------
    HRESULT CreateTextureFromDDS(_In_ ID3D11DeviceX* d3dDevice,
        _In_ const DDS_HEADER* header,
        _In_reads_bytes_(bitSize) const uint8_t* bitData,
        _In_ size_t bitSize,
        _In_ bool forceSRGB,
        _Outptr_opt_ ID3D11Resource** texture,
        _Outptr_opt_ ID3D11ShaderResourceView** textureView,
        _Outptr_ void** grfxMemory) noexcept
    {
        HRESULT hr = S_OK;

        uint32_t width = header->width;
        uint32_t height = header->height;
        uint32_t depth = header->depth;

        uint32_t mipCount = header->mipMapCount;
        if (0 == mipCount)
        {
            mipCount = 1;
        }

        if (!(header->ddspf.flags & DDS_FOURCC)
            || (MAKEFOURCC('X', 'B', 'O', 'X') != header->ddspf.fourCC))
        {
            // Use standard DDSTextureLoader instead
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }

        auto xboxext = reinterpret_cast<const DDS_HEADER_XBOX*>(reinterpret_cast<const uint8_t*>(header) + sizeof(DDS_HEADER));

#ifndef NDEBUG
        if (xboxext->xdkVer < _XDK_VER)
        {
            OutputDebugStringA("WARNING: DDS XBOX file may be outdated and need regeneration\n");
        }
#endif

        uint32_t arraySize = xboxext->arraySize;
        if (arraySize == 0)
        {
            return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        }

        bool isCubeMap = false;

        switch (xboxext->resourceDimension)
        {
        case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
            if ((header->flags & DDS_HEIGHT) && height != 1)
            {
                return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
            }
            height = depth = 1;
            break;

        case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
            if (xboxext->miscFlag & D3D11_RESOURCE_MISC_TEXTURECUBE)
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
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            }
            break;

        default:
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }

        if (xboxext->tileMode == uint32_t(-1))
        {
            return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        }

        // Bound sizes
        if (mipCount > D3D11_REQ_MIP_LEVELS)
        {
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }

        switch (xboxext->resourceDimension)
        {
        case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
            if ((arraySize > D3D11_REQ_TEXTURE1D_ARRAY_AXIS_DIMENSION) ||
                (width > D3D11_REQ_TEXTURE1D_U_DIMENSION))
            {
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
                    return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
                }
            }
            else if ((arraySize > D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION) ||
                (width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION) ||
                (height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION))
            {
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            }
            break;

        case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
            if ((arraySize > 1) ||
                (width > D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION) ||
                (height > D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION) ||
                (depth > D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION))
            {
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            }
            break;
        }

        if (xboxext->dxgiFormat == DXGI_FORMAT_UNKNOWN)
        {
            return E_FAIL;
        }

        if (!xboxext->dataSize || !xboxext->baseAlignment)
        {
            return E_FAIL;
        }

        if (xboxext->dataSize > bitSize)
        {
            return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
        }

        // Allocate graphics memory. Depending on the data size it uses 4MB or 64K pages.
        *grfxMemory = XMemAlloc(xboxext->dataSize, c_XMemAllocAttributes);
        if (!*grfxMemory)
            return E_OUTOFMEMORY;

        // Copy tiled data into graphics memory
        memcpy(*grfxMemory, bitData, xboxext->dataSize);

        // Create the texture
        hr = CreateD3DResources(d3dDevice, xboxext,
            width, height, depth, mipCount, arraySize,
            forceSRGB, isCubeMap, *grfxMemory,
            texture, textureView);
        if (FAILED(hr))
        {
            XMemFree(grfxMemory, c_XMemAllocAttributes);
            *grfxMemory = nullptr;
        }

        return hr;
    }

    //--------------------------------------------------------------------------------------
    DDS_ALPHA_MODE GetAlphaMode(_In_ const DDS_HEADER* header) noexcept
    {
        if (header->ddspf.flags & DDS_FOURCC)
        {
            if (MAKEFOURCC('X', 'B', 'O', 'X') == header->ddspf.fourCC)
            {
                auto xboxext = reinterpret_cast<const DDS_HEADER_XBOX*>(reinterpret_cast<const uint8_t*>(header) + sizeof(DDS_HEADER));
                auto mode = static_cast<DDS_ALPHA_MODE>(xboxext->miscFlags2 & DDS_MISC_FLAGS2_ALPHA_MODE_MASK);
                switch (mode)
                {
                case DDS_ALPHA_MODE_STRAIGHT:
                case DDS_ALPHA_MODE_PREMULTIPLIED:
                case DDS_ALPHA_MODE_OPAQUE:
                case DDS_ALPHA_MODE_CUSTOM:
                    return mode;

                default:
                    break;
                }
            }
        }

        return DDS_ALPHA_MODE_UNKNOWN;
    }
} // anonymous namespace


//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT Xbox::CreateDDSTextureFromMemory(
    ID3D11DeviceX* d3dDevice,
    const uint8_t* ddsData,
    size_t ddsDataSize,
    ID3D11Resource** texture,
    ID3D11ShaderResourceView** textureView,
    void** grfxMemory,
    DDS_ALPHA_MODE* alphaMode,
    bool forceSRGB ) noexcept
{
    if ( texture )
    {
        *texture = nullptr;
    }
    if ( textureView )
    {
        *textureView = nullptr;
    }
    if ( grfxMemory )
    {
        *grfxMemory = nullptr;
    }
    if ( alphaMode )
    {
        *alphaMode = DDS_ALPHA_MODE_UNKNOWN;
    }

    if ( !d3dDevice || !ddsData || (!texture && !textureView) || !grfxMemory )
    {
        return E_INVALIDARG;
    }

    // Validate DDS file in memory
    if (ddsDataSize < (sizeof(uint32_t) + sizeof(DDS_HEADER)))
    {
        return E_FAIL;
    }

    auto dwMagicNumber = *reinterpret_cast<const uint32_t*>(ddsData);
    if (dwMagicNumber != DDS_MAGIC)
    {
        return E_FAIL;
    }

    auto header = reinterpret_cast<const DDS_HEADER*>( ddsData + sizeof( uint32_t ) );

    // Verify header to validate DDS file
    if (header->size != sizeof(DDS_HEADER) ||
        header->ddspf.size != sizeof(DDS_PIXELFORMAT))
    {
        return E_FAIL;
    }

    // Check for XBOX extension
    if ( !( header->ddspf.flags & DDS_FOURCC )
         || ( MAKEFOURCC( 'X', 'B', 'O', 'X' ) != header->ddspf.fourCC ) )
    {
        // Use standard DDSTextureLoader instead
        return HRESULT_FROM_WIN32( ERROR_NOT_SUPPORTED );
    }

    // Must be long enough for both headers and magic value
    if (ddsDataSize < (sizeof(DDS_HEADER) + sizeof(uint32_t) + sizeof(DDS_HEADER_XBOX)))
    {
        return E_FAIL;
    }

    auto offset = sizeof( uint32_t ) + sizeof( DDS_HEADER ) + sizeof( DDS_HEADER_XBOX );

    HRESULT hr = CreateTextureFromDDS( d3dDevice, header,
                                       ddsData + offset, ddsDataSize - offset, forceSRGB,
                                       texture, textureView,
                                       grfxMemory );
    if ( SUCCEEDED(hr) )
    {
        if (texture != 0 && *texture != 0)
        {
            SetDebugObjectName(*texture, "XboxDDSTextureLoader");
        }

        if (textureView != 0 && *textureView != 0)
        {
            SetDebugObjectName(*textureView, "XboxDDSTextureLoader");
        }

        if ( alphaMode )
            *alphaMode = GetAlphaMode( header );
    }

    return hr;
}

//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT Xbox::CreateDDSTextureFromFile(
    ID3D11DeviceX* d3dDevice,
    const wchar_t* fileName,
    ID3D11Resource** texture,
    ID3D11ShaderResourceView** textureView,
    void** grfxMemory,
    DDS_ALPHA_MODE* alphaMode,
    bool forceSRGB ) noexcept
{
    if ( texture )
    {
        *texture = nullptr;
    }
    if ( textureView )
    {
        *textureView = nullptr;
    }
    if ( grfxMemory )
    {
        *grfxMemory = nullptr;
    }
    if ( alphaMode )
    {
        *alphaMode = DDS_ALPHA_MODE_UNKNOWN;
    }

    if ( !d3dDevice || !fileName || (!texture && !textureView) || !grfxMemory )
    {
        return E_INVALIDARG;
    }

    DDS_HEADER* header = nullptr;
    uint8_t* bitData = nullptr;
    size_t bitSize = 0;

    std::unique_ptr<uint8_t[]> ddsData;
    HRESULT hr = LoadTextureDataFromFile( fileName,
                                          ddsData,
                                          &header,
                                          &bitData,
                                          &bitSize
                                        );
    if (FAILED(hr))
    {
        return hr;
    }

    hr = CreateTextureFromDDS( d3dDevice, header,
                               bitData, bitSize, forceSRGB,
                               texture, textureView,
                               grfxMemory );

    if ( SUCCEEDED(hr) )
    {
#if !defined(NO_D3D11_DEBUG_NAME) && ( defined(_DEBUG) || defined(PROFILE) )
        if (texture != 0 && *texture != 0)
        {
            (*texture)->SetName( fileName );
        }
        if (textureView != 0 && *textureView != 0 )
        {
            (*textureView)->SetName( fileName );
        }
#endif

        if ( alphaMode )
            *alphaMode = GetAlphaMode( header );
    }

    return hr;
}

//--------------------------------------------------------------------------------------
_Use_decl_annotations_
void Xbox::FreeDDSTextureMemory(void* grfxMemory) noexcept
{
    if (grfxMemory)
    {
        XMemFree(grfxMemory, c_XMemAllocAttributes);
    }
}
