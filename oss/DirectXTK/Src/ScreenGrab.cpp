//--------------------------------------------------------------------------------------
// File: ScreenGrab.cpp
//
// Function for capturing a 2D texture and saving it to a file (aka a 'screenshot'
// when used on a Direct3D Render Target).
//
// Note these functions are useful as a light-weight runtime screen grabber. For
// full-featured texture capture, DDS writer, and texture processing pipeline,
// see the 'Texconv' sample and the 'DirectXTex' library.
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

// Does not capture 1D textures or 3D textures (volume maps)

// Does not capture mipmap chains, only the top-most texture level is saved

// For 2D array textures and cubemaps, it captures only the first image in the array

#include "pch.h"

#include "ScreenGrab.h"
#include "DirectXHelpers.h"

#include "PlatformHelpers.h"
#include "DDS.h"
#include "LoaderHelpers.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::LoaderHelpers;

namespace
{
    //--------------------------------------------------------------------------------------
    HRESULT CaptureTexture(
        _In_ ID3D11DeviceContext* pContext,
        _In_ ID3D11Resource* pSource,
        D3D11_TEXTURE2D_DESC& desc,
        ComPtr<ID3D11Texture2D>& pStaging) noexcept
    {
        if (!pContext || !pSource)
            return E_INVALIDARG;

        D3D11_RESOURCE_DIMENSION resType = D3D11_RESOURCE_DIMENSION_UNKNOWN;
        pSource->GetType(&resType);

        if (resType != D3D11_RESOURCE_DIMENSION_TEXTURE2D)
        {
            DebugTrace("ERROR: ScreenGrab does not support 1D or volume textures. Consider using DirectXTex instead.\n");
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }

        ComPtr<ID3D11Texture2D> pTexture;
        HRESULT hr = pSource->QueryInterface(IID_GRAPHICS_PPV_ARGS(pTexture.GetAddressOf()));
        if (FAILED(hr))
            return hr;

        assert(pTexture);

        pTexture->GetDesc(&desc);

        if (desc.ArraySize > 1 || desc.MipLevels > 1)
        {
            DebugTrace("WARNING: ScreenGrab does not support 2D arrays, cubemaps, or mipmaps; only the first surface is written. Consider using DirectXTex instead.\n");
        }

        ComPtr<ID3D11Device> d3dDevice;
        pContext->GetDevice(d3dDevice.GetAddressOf());

        if (desc.SampleDesc.Count > 1)
        {
            // MSAA content must be resolved before being copied to a staging texture
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;

            ComPtr<ID3D11Texture2D> pTemp;
            hr = d3dDevice->CreateTexture2D(&desc, nullptr, pTemp.GetAddressOf());
            if (FAILED(hr))
                return hr;

            assert(pTemp);

            const DXGI_FORMAT fmt = EnsureNotTypeless(desc.Format);

            UINT support = 0;
            hr = d3dDevice->CheckFormatSupport(fmt, &support);
            if (FAILED(hr))
                return hr;

            if (!(support & D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE))
                return E_FAIL;

            for (UINT item = 0; item < desc.ArraySize; ++item)
            {
                for (UINT level = 0; level < desc.MipLevels; ++level)
                {
                    const UINT index = D3D11CalcSubresource(level, item, desc.MipLevels);
                    pContext->ResolveSubresource(pTemp.Get(), index, pSource, index, fmt);
                }
            }

            desc.BindFlags = 0;
            desc.MiscFlags &= D3D11_RESOURCE_MISC_TEXTURECUBE;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            desc.Usage = D3D11_USAGE_STAGING;

            hr = d3dDevice->CreateTexture2D(&desc, nullptr, pStaging.ReleaseAndGetAddressOf());
            if (FAILED(hr))
                return hr;

            assert(pStaging);

            pContext->CopyResource(pStaging.Get(), pTemp.Get());
        }
        else if ((desc.Usage == D3D11_USAGE_STAGING) && (desc.CPUAccessFlags & D3D11_CPU_ACCESS_READ))
        {
            // Handle case where the source is already a staging texture we can use directly
            pStaging = pTexture;
        }
        else
        {
            // Otherwise, create a staging texture from the non-MSAA source
            desc.BindFlags = 0;
            desc.MiscFlags &= D3D11_RESOURCE_MISC_TEXTURECUBE;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            desc.Usage = D3D11_USAGE_STAGING;

            hr = d3dDevice->CreateTexture2D(&desc, nullptr, pStaging.ReleaseAndGetAddressOf());
            if (FAILED(hr))
                return hr;

            assert(pStaging);

            pContext->CopyResource(pStaging.Get(), pSource);
        }

    #if defined(_XBOX_ONE) && defined(_TITLE)

        if (d3dDevice->GetCreationFlags() & D3D11_CREATE_DEVICE_IMMEDIATE_CONTEXT_FAST_SEMANTICS)
        {
            ComPtr<ID3D11DeviceX> d3dDeviceX;
            hr = d3dDevice.As(&d3dDeviceX);
            if (FAILED(hr))
                return hr;

            ComPtr<ID3D11DeviceContextX> d3dContextX;
            hr = pContext->QueryInterface(IID_GRAPHICS_PPV_ARGS(d3dContextX.GetAddressOf()));
            if (FAILED(hr))
                return hr;

            UINT64 copyFence = d3dContextX->InsertFence(0);

            while (d3dDeviceX->IsFencePending(copyFence))
            {
                SwitchToThread();
            }
        }

    #endif

        return S_OK;
    }
} // anonymous namespace


//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::SaveDDSTextureToFile(
    ID3D11DeviceContext* pContext,
    ID3D11Resource* pSource,
    const wchar_t* fileName) noexcept
{
    if (!fileName)
        return E_INVALIDARG;

    D3D11_TEXTURE2D_DESC desc = {};
    ComPtr<ID3D11Texture2D> pStaging;
    HRESULT hr = CaptureTexture(pContext, pSource, desc, pStaging);
    if (FAILED(hr))
        return hr;

    // Create file
#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
    ScopedHandle hFile(safe_handle(CreateFile2(
        fileName,
        GENERIC_WRITE | DELETE, 0, CREATE_ALWAYS,
        nullptr)));
#else
    ScopedHandle hFile(safe_handle(CreateFileW(
        fileName,
        GENERIC_WRITE | DELETE, 0,
        nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
        nullptr)));
#endif
    if (!hFile)
        return HRESULT_FROM_WIN32(GetLastError());

    auto_delete_file delonfail(hFile.get());

    // Setup header
    constexpr size_t MAX_HEADER_SIZE = sizeof(uint32_t) + sizeof(DDS_HEADER) + sizeof(DDS_HEADER_DXT10);
    uint8_t fileHeader[MAX_HEADER_SIZE] = {};

    *reinterpret_cast<uint32_t*>(&fileHeader[0]) = DDS_MAGIC;

    auto header = reinterpret_cast<DDS_HEADER*>(&fileHeader[0] + sizeof(uint32_t));
    size_t headerSize = sizeof(uint32_t) + sizeof(DDS_HEADER);
    header->size = sizeof(DDS_HEADER);
    header->flags = DDS_HEADER_FLAGS_TEXTURE | DDS_HEADER_FLAGS_MIPMAP;
    header->height = desc.Height;
    header->width = desc.Width;
    header->mipMapCount = 1;
    header->caps = DDS_SURFACE_FLAGS_TEXTURE;

    // Try to use a legacy .DDS pixel format for better tools support, otherwise fallback to 'DX10' header extension
    DDS_HEADER_DXT10* extHeader = nullptr;
    switch (desc.Format)
    {
    case DXGI_FORMAT_R8G8B8A8_UNORM:        memcpy(&header->ddspf, &DDSPF_A8B8G8R8, sizeof(DDS_PIXELFORMAT));    break;
    case DXGI_FORMAT_R16G16_UNORM:          memcpy(&header->ddspf, &DDSPF_G16R16, sizeof(DDS_PIXELFORMAT));      break;
    case DXGI_FORMAT_R8G8_UNORM:            memcpy(&header->ddspf, &DDSPF_A8L8, sizeof(DDS_PIXELFORMAT));        break;
    case DXGI_FORMAT_R16_UNORM:             memcpy(&header->ddspf, &DDSPF_L16, sizeof(DDS_PIXELFORMAT));         break;
    case DXGI_FORMAT_R8_UNORM:              memcpy(&header->ddspf, &DDSPF_L8, sizeof(DDS_PIXELFORMAT));          break;
    case DXGI_FORMAT_A8_UNORM:              memcpy(&header->ddspf, &DDSPF_A8, sizeof(DDS_PIXELFORMAT));          break;
    case DXGI_FORMAT_R8G8_B8G8_UNORM:       memcpy(&header->ddspf, &DDSPF_R8G8_B8G8, sizeof(DDS_PIXELFORMAT));   break;
    case DXGI_FORMAT_G8R8_G8B8_UNORM:       memcpy(&header->ddspf, &DDSPF_G8R8_G8B8, sizeof(DDS_PIXELFORMAT));   break;
    case DXGI_FORMAT_BC1_UNORM:             memcpy(&header->ddspf, &DDSPF_DXT1, sizeof(DDS_PIXELFORMAT));        break;
    case DXGI_FORMAT_BC2_UNORM:             memcpy(&header->ddspf, &DDSPF_DXT3, sizeof(DDS_PIXELFORMAT));        break;
    case DXGI_FORMAT_BC3_UNORM:             memcpy(&header->ddspf, &DDSPF_DXT5, sizeof(DDS_PIXELFORMAT));        break;
    case DXGI_FORMAT_BC4_UNORM:             memcpy(&header->ddspf, &DDSPF_BC4_UNORM, sizeof(DDS_PIXELFORMAT));   break;
    case DXGI_FORMAT_BC4_SNORM:             memcpy(&header->ddspf, &DDSPF_BC4_SNORM, sizeof(DDS_PIXELFORMAT));   break;
    case DXGI_FORMAT_BC5_UNORM:             memcpy(&header->ddspf, &DDSPF_BC5_UNORM, sizeof(DDS_PIXELFORMAT));   break;
    case DXGI_FORMAT_BC5_SNORM:             memcpy(&header->ddspf, &DDSPF_BC5_SNORM, sizeof(DDS_PIXELFORMAT));   break;
    case DXGI_FORMAT_B5G6R5_UNORM:          memcpy(&header->ddspf, &DDSPF_R5G6B5, sizeof(DDS_PIXELFORMAT));      break;
    case DXGI_FORMAT_B5G5R5A1_UNORM:        memcpy(&header->ddspf, &DDSPF_A1R5G5B5, sizeof(DDS_PIXELFORMAT));    break;
    case DXGI_FORMAT_R8G8_SNORM:            memcpy(&header->ddspf, &DDSPF_V8U8, sizeof(DDS_PIXELFORMAT));        break;
    case DXGI_FORMAT_R8G8B8A8_SNORM:        memcpy(&header->ddspf, &DDSPF_Q8W8V8U8, sizeof(DDS_PIXELFORMAT));    break;
    case DXGI_FORMAT_R16G16_SNORM:          memcpy(&header->ddspf, &DDSPF_V16U16, sizeof(DDS_PIXELFORMAT));      break;
    case DXGI_FORMAT_B8G8R8A8_UNORM:        memcpy(&header->ddspf, &DDSPF_A8R8G8B8, sizeof(DDS_PIXELFORMAT));    break; // DXGI 1.1
    case DXGI_FORMAT_B8G8R8X8_UNORM:        memcpy(&header->ddspf, &DDSPF_X8R8G8B8, sizeof(DDS_PIXELFORMAT));    break; // DXGI 1.1
    case DXGI_FORMAT_YUY2:                  memcpy(&header->ddspf, &DDSPF_YUY2, sizeof(DDS_PIXELFORMAT));        break; // DXGI 1.2
    case DXGI_FORMAT_B4G4R4A4_UNORM:        memcpy(&header->ddspf, &DDSPF_A4R4G4B4, sizeof(DDS_PIXELFORMAT));    break; // DXGI 1.2

    // Legacy D3DX formats using D3DFMT enum value as FourCC
    case DXGI_FORMAT_R32G32B32A32_FLOAT:    header->ddspf.size = sizeof(DDS_PIXELFORMAT); header->ddspf.flags = DDS_FOURCC; header->ddspf.fourCC = 116; break; // D3DFMT_A32B32G32R32F
    case DXGI_FORMAT_R16G16B16A16_FLOAT:    header->ddspf.size = sizeof(DDS_PIXELFORMAT); header->ddspf.flags = DDS_FOURCC; header->ddspf.fourCC = 113; break; // D3DFMT_A16B16G16R16F
    case DXGI_FORMAT_R16G16B16A16_UNORM:    header->ddspf.size = sizeof(DDS_PIXELFORMAT); header->ddspf.flags = DDS_FOURCC; header->ddspf.fourCC = 36;  break; // D3DFMT_A16B16G16R16
    case DXGI_FORMAT_R16G16B16A16_SNORM:    header->ddspf.size = sizeof(DDS_PIXELFORMAT); header->ddspf.flags = DDS_FOURCC; header->ddspf.fourCC = 110; break; // D3DFMT_Q16W16V16U16
    case DXGI_FORMAT_R32G32_FLOAT:          header->ddspf.size = sizeof(DDS_PIXELFORMAT); header->ddspf.flags = DDS_FOURCC; header->ddspf.fourCC = 115; break; // D3DFMT_G32R32F
    case DXGI_FORMAT_R16G16_FLOAT:          header->ddspf.size = sizeof(DDS_PIXELFORMAT); header->ddspf.flags = DDS_FOURCC; header->ddspf.fourCC = 112; break; // D3DFMT_G16R16F
    case DXGI_FORMAT_R32_FLOAT:             header->ddspf.size = sizeof(DDS_PIXELFORMAT); header->ddspf.flags = DDS_FOURCC; header->ddspf.fourCC = 114; break; // D3DFMT_R32F
    case DXGI_FORMAT_R16_FLOAT:             header->ddspf.size = sizeof(DDS_PIXELFORMAT); header->ddspf.flags = DDS_FOURCC; header->ddspf.fourCC = 111; break; // D3DFMT_R16F

    case DXGI_FORMAT_AI44:
    case DXGI_FORMAT_IA44:
    case DXGI_FORMAT_P8:
    case DXGI_FORMAT_A8P8:
        DebugTrace("ERROR: ScreenGrab does not support video textures. Consider using DirectXTex.\n");
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

    default:
        memcpy(&header->ddspf, &DDSPF_DX10, sizeof(DDS_PIXELFORMAT));

        headerSize += sizeof(DDS_HEADER_DXT10);
        extHeader = reinterpret_cast<DDS_HEADER_DXT10*>(fileHeader + sizeof(uint32_t) + sizeof(DDS_HEADER));
        extHeader->dxgiFormat = desc.Format;
        extHeader->resourceDimension = D3D11_RESOURCE_DIMENSION_TEXTURE2D;
        extHeader->arraySize = 1;
        break;
    }

    size_t rowPitch, slicePitch, rowCount;
    hr = GetSurfaceInfo(desc.Width, desc.Height, desc.Format, &slicePitch, &rowPitch, &rowCount);
    if (FAILED(hr))
        return hr;

    if (rowPitch > UINT32_MAX || slicePitch > UINT32_MAX)
        return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);

    if (IsCompressed(desc.Format))
    {
        header->flags |= DDS_HEADER_FLAGS_LINEARSIZE;
        header->pitchOrLinearSize = static_cast<uint32_t>(slicePitch);
    }
    else
    {
        header->flags |= DDS_HEADER_FLAGS_PITCH;
        header->pitchOrLinearSize = static_cast<uint32_t>(rowPitch);
    }

    // Setup pixels
    std::unique_ptr<uint8_t[]> pixels(new (std::nothrow) uint8_t[slicePitch]);
    if (!pixels)
        return E_OUTOFMEMORY;

    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = pContext->Map(pStaging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr))
        return hr;

    auto sptr = static_cast<const uint8_t*>(mapped.pData);
    if (!sptr)
    {
        pContext->Unmap(pStaging.Get(), 0);
        return E_POINTER;
    }

    uint8_t* dptr = pixels.get();

    const size_t msize = std::min<size_t>(rowPitch, mapped.RowPitch);
    for (size_t h = 0; h < rowCount; ++h)
    {
        memcpy(dptr, sptr, msize);
        sptr += mapped.RowPitch;
        dptr += rowPitch;
    }

    pContext->Unmap(pStaging.Get(), 0);

    // Write header & pixels
    DWORD bytesWritten;
    if (!WriteFile(hFile.get(), fileHeader, static_cast<DWORD>(headerSize), &bytesWritten, nullptr))
        return HRESULT_FROM_WIN32(GetLastError());

    if (bytesWritten != headerSize)
        return E_FAIL;

    if (!WriteFile(hFile.get(), pixels.get(), static_cast<DWORD>(slicePitch), &bytesWritten, nullptr))
        return HRESULT_FROM_WIN32(GetLastError());

    if (bytesWritten != slicePitch)
        return E_FAIL;

    delonfail.clear();

    return S_OK;
}

//--------------------------------------------------------------------------------------
namespace DirectX
{
    inline namespace DX11
    {
        namespace Internal
        {
            extern bool IsWIC2() noexcept;
            extern IWICImagingFactory* GetWIC() noexcept;
        }
    }
}

_Use_decl_annotations_
HRESULT DirectX::SaveWICTextureToFile(
    ID3D11DeviceContext* pContext,
    ID3D11Resource* pSource,
    REFGUID guidContainerFormat,
    const wchar_t* fileName,
    const GUID* targetFormat,
    std::function<void(IPropertyBag2*)> setCustomProps,
    bool forceSRGB)
{
    using namespace DX11::Internal;

    if (!fileName)
        return E_INVALIDARG;

    D3D11_TEXTURE2D_DESC desc = {};
    ComPtr<ID3D11Texture2D> pStaging;
    HRESULT hr = CaptureTexture(pContext, pSource, desc, pStaging);
    if (FAILED(hr))
        return hr;

    // Determine source format's WIC equivalent
    WICPixelFormatGUID pfGuid = {};
    bool sRGB = forceSRGB;
    switch (desc.Format)
    {
    case DXGI_FORMAT_R32G32B32A32_FLOAT:            pfGuid = GUID_WICPixelFormat128bppRGBAFloat; break;
    case DXGI_FORMAT_R16G16B16A16_FLOAT:            pfGuid = GUID_WICPixelFormat64bppRGBAHalf; break;
    case DXGI_FORMAT_R16G16B16A16_UNORM:            pfGuid = GUID_WICPixelFormat64bppRGBA; break;
    case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:    pfGuid = GUID_WICPixelFormat32bppRGBA1010102XR; break; // DXGI 1.1
    case DXGI_FORMAT_R10G10B10A2_UNORM:             pfGuid = GUID_WICPixelFormat32bppRGBA1010102; break;
    case DXGI_FORMAT_B5G5R5A1_UNORM:                pfGuid = GUID_WICPixelFormat16bppBGRA5551; break;
    case DXGI_FORMAT_B5G6R5_UNORM:                  pfGuid = GUID_WICPixelFormat16bppBGR565; break;
    case DXGI_FORMAT_R32_FLOAT:                     pfGuid = GUID_WICPixelFormat32bppGrayFloat; break;
    case DXGI_FORMAT_R16_FLOAT:                     pfGuid = GUID_WICPixelFormat16bppGrayHalf; break;
    case DXGI_FORMAT_R16_UNORM:                     pfGuid = GUID_WICPixelFormat16bppGray; break;
    case DXGI_FORMAT_R8_UNORM:                      pfGuid = GUID_WICPixelFormat8bppGray; break;
    case DXGI_FORMAT_A8_UNORM:                      pfGuid = GUID_WICPixelFormat8bppAlpha; break;

    case DXGI_FORMAT_R8G8B8A8_UNORM:
        pfGuid = GUID_WICPixelFormat32bppRGBA;
        break;

    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        pfGuid = GUID_WICPixelFormat32bppRGBA;
        sRGB = true;
        break;

    case DXGI_FORMAT_B8G8R8A8_UNORM: // DXGI 1.1
        pfGuid = GUID_WICPixelFormat32bppBGRA;
        break;

    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: // DXGI 1.1
        pfGuid = GUID_WICPixelFormat32bppBGRA;
        sRGB = true;
        break;

    case DXGI_FORMAT_B8G8R8X8_UNORM: // DXGI 1.1
        pfGuid = GUID_WICPixelFormat32bppBGR;
        break;

    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB: // DXGI 1.1
        pfGuid = GUID_WICPixelFormat32bppBGR;
        sRGB = true;
        break;

    default:
        DebugTrace("ERROR: ScreenGrab does not support all DXGI formats (%u). Consider using DirectXTex.\n", static_cast<uint32_t>(desc.Format));
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }

    auto pWIC = GetWIC();
    if (!pWIC)
        return E_NOINTERFACE;

    ComPtr<IWICStream> stream;
    hr = pWIC->CreateStream(stream.GetAddressOf());
    if (FAILED(hr))
        return hr;

    hr = stream->InitializeFromFilename(fileName, GENERIC_WRITE);
    if (FAILED(hr))
        return hr;

    auto_delete_file_wic delonfail(stream, fileName);

    ComPtr<IWICBitmapEncoder> encoder;
    hr = pWIC->CreateEncoder(guidContainerFormat, nullptr, encoder.GetAddressOf());
    if (FAILED(hr))
        return hr;

    hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    if (FAILED(hr))
        return hr;

    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> props;
    hr = encoder->CreateNewFrame(frame.GetAddressOf(), props.GetAddressOf());
    if (FAILED(hr))
        return hr;

    if (targetFormat && memcmp(&guidContainerFormat, &GUID_ContainerFormatBmp, sizeof(WICPixelFormatGUID)) == 0 && IsWIC2())
    {
        // Opt-in to the WIC2 support for writing 32-bit Windows BMP files with an alpha channel
        PROPBAG2 option = {};
        option.pstrName = const_cast<wchar_t*>(L"EnableV5Header32bppBGRA");

        VARIANT varValue;
        varValue.vt = VT_BOOL;
        varValue.boolVal = VARIANT_TRUE;
        std::ignore = props->Write(1, &option, &varValue);
    }

    if (setCustomProps)
    {
        setCustomProps(props.Get());
    }

    hr = frame->Initialize(props.Get());
    if (FAILED(hr))
        return hr;

    hr = frame->SetSize(desc.Width, desc.Height);
    if (FAILED(hr))
        return hr;

    hr = frame->SetResolution(72, 72);
    if (FAILED(hr))
        return hr;

    // Pick a target format
    WICPixelFormatGUID targetGuid = {};
    if (targetFormat)
    {
        targetGuid = *targetFormat;
    }
    else
    {
        // Screenshots don't typically include the alpha channel of the render target
        switch (desc.Format)
        {
        #if (_WIN32_WINNT >= _WIN32_WINNT_WIN8) || defined(_WIN7_PLATFORM_UPDATE)
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
            if (IsWIC2())
            {
                targetGuid = GUID_WICPixelFormat96bppRGBFloat;
            }
            else
            {
                targetGuid = GUID_WICPixelFormat24bppBGR;
            }
            break;
        #endif

        case DXGI_FORMAT_R16G16B16A16_UNORM: targetGuid = GUID_WICPixelFormat48bppBGR; break;
        case DXGI_FORMAT_B5G5R5A1_UNORM:     targetGuid = GUID_WICPixelFormat16bppBGR555; break;
        case DXGI_FORMAT_B5G6R5_UNORM:       targetGuid = GUID_WICPixelFormat16bppBGR565; break;

        case DXGI_FORMAT_R32_FLOAT:
        case DXGI_FORMAT_R16_FLOAT:
        case DXGI_FORMAT_R16_UNORM:
        case DXGI_FORMAT_R8_UNORM:
        case DXGI_FORMAT_A8_UNORM:
            targetGuid = GUID_WICPixelFormat8bppGray;
            break;

        default:
            targetGuid = GUID_WICPixelFormat24bppBGR;
            break;
        }
    }

    hr = frame->SetPixelFormat(&targetGuid);
    if (FAILED(hr))
        return hr;

    if (targetFormat && memcmp(targetFormat, &targetGuid, sizeof(WICPixelFormatGUID)) != 0)
    {
        // Requested output pixel format is not supported by the WIC codec
        return E_FAIL;
    }

    // Encode WIC metadata
    ComPtr<IWICMetadataQueryWriter> metawriter;
    if (SUCCEEDED(frame->GetMetadataQueryWriter(metawriter.GetAddressOf())))
    {
        PROPVARIANT value;
        PropVariantInit(&value);

        value.vt = VT_LPSTR;
        value.pszVal = const_cast<char*>("DirectXTK");

        if (memcmp(&guidContainerFormat, &GUID_ContainerFormatPng, sizeof(GUID)) == 0)
        {
            // Set Software name
            std::ignore = metawriter->SetMetadataByName(L"/tEXt/{str=Software}", &value);

            // Set sRGB chunk
            if (sRGB)
            {
                value.vt = VT_UI1;
                value.bVal = 0;
                std::ignore = metawriter->SetMetadataByName(L"/sRGB/RenderingIntent", &value);
            }
            else
            {
                // add gAMA chunk with gamma 1.0
                value.vt = VT_UI4;
                value.uintVal = 100000; // gama value * 100,000 -- i.e. gamma 1.0
                std::ignore = metawriter->SetMetadataByName(L"/gAMA/ImageGamma", &value);

                // remove sRGB chunk which is added by default.
                std::ignore = metawriter->RemoveMetadataByName(L"/sRGB/RenderingIntent");
            }
        }
    #if defined(_XBOX_ONE) && defined(_TITLE)
        else if (memcmp(&guidContainerFormat, &GUID_ContainerFormatJpeg, sizeof(GUID)) == 0)
        {
            // Set Software name
            std::ignore = metawriter->SetMetadataByName(L"/app1/ifd/{ushort=305}", &value);

            if (sRGB)
            {
                // Set EXIF Colorspace of sRGB
                value.vt = VT_UI2;
                value.uiVal = 1;
                std::ignore = metawriter->SetMetadataByName(L"/app1/ifd/exif/{ushort=40961}", &value);
            }
        }
        else if (memcmp(&guidContainerFormat, &GUID_ContainerFormatTiff, sizeof(GUID)) == 0)
        {
            // Set Software name
            std::ignore = metawriter->SetMetadataByName(L"/ifd/{ushort=305}", &value);

            if (sRGB)
            {
                // Set EXIF Colorspace of sRGB
                value.vt = VT_UI2;
                value.uiVal = 1;
                std::ignore = metawriter->SetMetadataByName(L"/ifd/exif/{ushort=40961}", &value);
            }
        }
    #else
        else
        {
            // Set Software name
            std::ignore = metawriter->SetMetadataByName(L"System.ApplicationName", &value);

            if (sRGB)
            {
                // Set EXIF Colorspace of sRGB
                value.vt = VT_UI2;
                value.uiVal = 1;
                std::ignore = metawriter->SetMetadataByName(L"System.Image.ColorSpace", &value);
            }
        }
    #endif
    }

    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = pContext->Map(pStaging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr))
        return hr;

    const uint64_t imageSize = uint64_t(mapped.RowPitch) * uint64_t(desc.Height);
    if (imageSize > UINT32_MAX)
    {
        pContext->Unmap(pStaging.Get(), 0);
        return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
    }

    if (memcmp(&targetGuid, &pfGuid, sizeof(WICPixelFormatGUID)) != 0)
    {
        // Conversion required to write
        ComPtr<IWICBitmap> source;
        hr = pWIC->CreateBitmapFromMemory(desc.Width, desc.Height,
            pfGuid,
            mapped.RowPitch, static_cast<UINT>(imageSize),
            static_cast<BYTE*>(mapped.pData), source.GetAddressOf());
        if (FAILED(hr))
        {
            pContext->Unmap(pStaging.Get(), 0);
            return hr;
        }

        ComPtr<IWICFormatConverter> FC;
        hr = pWIC->CreateFormatConverter(FC.GetAddressOf());
        if (FAILED(hr))
        {
            pContext->Unmap(pStaging.Get(), 0);
            return hr;
        }

        BOOL canConvert = FALSE;
        hr = FC->CanConvert(pfGuid, targetGuid, &canConvert);
        if (FAILED(hr) || !canConvert)
        {
            pContext->Unmap(pStaging.Get(), 0);
            return E_UNEXPECTED;
        }

        hr = FC->Initialize(source.Get(), targetGuid, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeMedianCut);
        if (FAILED(hr))
        {
            pContext->Unmap(pStaging.Get(), 0);
            return hr;
        }

        WICRect rect = { 0, 0, static_cast<INT>(desc.Width), static_cast<INT>(desc.Height) };
        hr = frame->WriteSource(FC.Get(), &rect);
    }
    else
    {
        // No conversion required
        hr = frame->WritePixels(desc.Height,
            mapped.RowPitch, static_cast<UINT>(imageSize),
            static_cast<BYTE*>(mapped.pData));
    }

    pContext->Unmap(pStaging.Get(), 0);

    if (FAILED(hr))
        return hr;

    hr = frame->Commit();
    if (FAILED(hr))
        return hr;

    hr = encoder->Commit();
    if (FAILED(hr))
        return hr;

    delonfail.clear();

    return S_OK;
}
