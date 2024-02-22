//--------------------------------------------------------------------------------------
// File: WICTextureLoader.cpp
//
// Function for loading a WIC image and creating a Direct3D runtime texture for it
// (auto-generating mipmaps if possible)
//
// Note: Assumes application has already called CoInitializeEx
//
// Warning: CreateWICTexture* functions are not thread-safe if given a d3dContext instance for
//          auto-gen mipmap support.
//
// Note these functions are useful for images created as simple 2D textures. For
// more complex resources, DDSTextureLoader is an excellent light-weight runtime loader.
// For a full-featured DDS file reader, writer, and texture processing pipeline see
// the 'Texconv' sample and the 'DirectXTex' library.
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

// We could load multi-frame images (TIFF/GIF) into a texture array.
// For now, we just load the first frame (note: DirectXTex supports multi-frame images)

#include "pch.h"

#include "WICTextureLoader.h"

#include "DirectXHelpers.h"
#include "PlatformHelpers.h"
#include "LoaderHelpers.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace
{
    //-------------------------------------------------------------------------------------
    // WIC Pixel Format Translation Data
    //-------------------------------------------------------------------------------------
    struct WICTranslate
    {
        const GUID&         wic;
        DXGI_FORMAT         format;
    };

    constexpr WICTranslate g_WICFormats[] =
    {
        { GUID_WICPixelFormat128bppRGBAFloat,       DXGI_FORMAT_R32G32B32A32_FLOAT },

        { GUID_WICPixelFormat64bppRGBAHalf,         DXGI_FORMAT_R16G16B16A16_FLOAT },
        { GUID_WICPixelFormat64bppRGBA,             DXGI_FORMAT_R16G16B16A16_UNORM },

        { GUID_WICPixelFormat32bppRGBA,             DXGI_FORMAT_R8G8B8A8_UNORM },
        { GUID_WICPixelFormat32bppBGRA,             DXGI_FORMAT_B8G8R8A8_UNORM }, // DXGI 1.1
        { GUID_WICPixelFormat32bppBGR,              DXGI_FORMAT_B8G8R8X8_UNORM }, // DXGI 1.1

        { GUID_WICPixelFormat32bppRGBA1010102XR,    DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM }, // DXGI 1.1
        { GUID_WICPixelFormat32bppRGBA1010102,      DXGI_FORMAT_R10G10B10A2_UNORM },

        { GUID_WICPixelFormat16bppBGRA5551,         DXGI_FORMAT_B5G5R5A1_UNORM },
        { GUID_WICPixelFormat16bppBGR565,           DXGI_FORMAT_B5G6R5_UNORM },

        { GUID_WICPixelFormat32bppGrayFloat,        DXGI_FORMAT_R32_FLOAT },
        { GUID_WICPixelFormat16bppGrayHalf,         DXGI_FORMAT_R16_FLOAT },
        { GUID_WICPixelFormat16bppGray,             DXGI_FORMAT_R16_UNORM },
        { GUID_WICPixelFormat8bppGray,              DXGI_FORMAT_R8_UNORM },

        { GUID_WICPixelFormat8bppAlpha,             DXGI_FORMAT_A8_UNORM },
    };

    //-------------------------------------------------------------------------------------
    // WIC Pixel Format nearest conversion table
    //-------------------------------------------------------------------------------------
    struct WICConvert
    {
        const GUID& source;
        const GUID& target;
    };

    constexpr WICConvert g_WICConvert[] =
    {
        // Note target GUID in this conversion table must be one of those directly supported formats (above).

        { GUID_WICPixelFormatBlackWhite,            GUID_WICPixelFormat8bppGray }, // DXGI_FORMAT_R8_UNORM

        { GUID_WICPixelFormat1bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat2bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat4bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat8bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM

        { GUID_WICPixelFormat2bppGray,              GUID_WICPixelFormat8bppGray }, // DXGI_FORMAT_R8_UNORM
        { GUID_WICPixelFormat4bppGray,              GUID_WICPixelFormat8bppGray }, // DXGI_FORMAT_R8_UNORM

        { GUID_WICPixelFormat16bppGrayFixedPoint,   GUID_WICPixelFormat16bppGrayHalf }, // DXGI_FORMAT_R16_FLOAT
        { GUID_WICPixelFormat32bppGrayFixedPoint,   GUID_WICPixelFormat32bppGrayFloat }, // DXGI_FORMAT_R32_FLOAT

        { GUID_WICPixelFormat16bppBGR555,           GUID_WICPixelFormat16bppBGRA5551 }, // DXGI_FORMAT_B5G5R5A1_UNORM

        { GUID_WICPixelFormat32bppBGR101010,        GUID_WICPixelFormat32bppRGBA1010102 }, // DXGI_FORMAT_R10G10B10A2_UNORM

        { GUID_WICPixelFormat24bppBGR,              GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat24bppRGB,              GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat32bppPBGRA,            GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat32bppPRGBA,            GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM

        { GUID_WICPixelFormat48bppRGB,              GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
        { GUID_WICPixelFormat48bppBGR,              GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
        { GUID_WICPixelFormat64bppBGRA,             GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
        { GUID_WICPixelFormat64bppPRGBA,            GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
        { GUID_WICPixelFormat64bppPBGRA,            GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM

        { GUID_WICPixelFormat48bppRGBFixedPoint,    GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
        { GUID_WICPixelFormat48bppBGRFixedPoint,    GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
        { GUID_WICPixelFormat64bppRGBAFixedPoint,   GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
        { GUID_WICPixelFormat64bppBGRAFixedPoint,   GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
        { GUID_WICPixelFormat64bppRGBFixedPoint,    GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
        { GUID_WICPixelFormat64bppRGBHalf,          GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
        { GUID_WICPixelFormat48bppRGBHalf,          GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT

        { GUID_WICPixelFormat128bppPRGBAFloat,      GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT
        { GUID_WICPixelFormat128bppRGBFloat,        GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT
        { GUID_WICPixelFormat128bppRGBAFixedPoint,  GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT
        { GUID_WICPixelFormat128bppRGBFixedPoint,   GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT
        { GUID_WICPixelFormat32bppRGBE,             GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT

        { GUID_WICPixelFormat32bppCMYK,             GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat64bppCMYK,             GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
        { GUID_WICPixelFormat40bppCMYKAlpha,        GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat80bppCMYKAlpha,        GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM

    #if (_WIN32_WINNT >= _WIN32_WINNT_WIN8) || defined(_WIN7_PLATFORM_UPDATE)
        { GUID_WICPixelFormat32bppRGB,              GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat64bppRGB,              GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
        { GUID_WICPixelFormat64bppPRGBAHalf,        GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
    #endif

        // We don't support n-channel formats
    };

    bool g_WIC2 = false;

    BOOL WINAPI InitializeWICFactory(PINIT_ONCE, PVOID, PVOID *ifactory) noexcept
    {
    #if (_WIN32_WINNT >= _WIN32_WINNT_WIN8) || defined(_WIN7_PLATFORM_UPDATE)
        HRESULT hr = CoCreateInstance(
            CLSID_WICImagingFactory2,
            nullptr,
            CLSCTX_INPROC_SERVER,
            __uuidof(IWICImagingFactory2),
            ifactory
        );

        if (SUCCEEDED(hr))
        {
            // WIC2 is available on Windows 10, Windows 8.x, and Windows 7 SP1 with KB 2670838 installed
            g_WIC2 = true;
            return TRUE;
        }
        else
        {
            hr = CoCreateInstance(
                CLSID_WICImagingFactory1,
                nullptr,
                CLSCTX_INPROC_SERVER,
                __uuidof(IWICImagingFactory),
                ifactory
            );
            return SUCCEEDED(hr) ? TRUE : FALSE;
        }
    #else
        return SUCCEEDED(CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            __uuidof(IWICImagingFactory),
            ifactory)) ? TRUE : FALSE;
    #endif
    }
}

//--------------------------------------------------------------------------------------
namespace DirectX
{
    inline namespace DX11
    {
        namespace Internal
        {
            bool IsWIC2() noexcept;
            IWICImagingFactory* GetWIC() noexcept;
            // Also used by ScreenGrab
        }
    }
}

bool DirectX::DX11::Internal::IsWIC2() noexcept
{
    return g_WIC2;
}

IWICImagingFactory* DirectX::DX11::Internal::GetWIC() noexcept
{
    static INIT_ONCE s_initOnce = INIT_ONCE_STATIC_INIT;

    IWICImagingFactory* factory = nullptr;
    if (!InitOnceExecuteOnce(
        &s_initOnce,
        InitializeWICFactory,
        nullptr,
        reinterpret_cast<LPVOID*>(&factory)))
    {
        return nullptr;
    }

    return factory;
}

using namespace DirectX::DX11::Internal;

namespace
{
    //---------------------------------------------------------------------------------
    DXGI_FORMAT WICToDXGI(const GUID& guid) noexcept
    {
        for (size_t i = 0; i < std::size(g_WICFormats); ++i)
        {
            if (memcmp(&g_WICFormats[i].wic, &guid, sizeof(GUID)) == 0)
                return g_WICFormats[i].format;
        }

    #if (_WIN32_WINNT >= _WIN32_WINNT_WIN8) || defined(_WIN7_PLATFORM_UPDATE)
        if (g_WIC2)
        {
            if (memcmp(&GUID_WICPixelFormat96bppRGBFloat, &guid, sizeof(GUID)) == 0)
                return DXGI_FORMAT_R32G32B32_FLOAT;
        }
    #endif

        return DXGI_FORMAT_UNKNOWN;
    }

    //---------------------------------------------------------------------------------
    size_t WICBitsPerPixel(REFGUID targetGuid) noexcept
    {
        auto pWIC = GetWIC();
        if (!pWIC)
            return 0;

        ComPtr<IWICComponentInfo> cinfo;
        if (FAILED(pWIC->CreateComponentInfo(targetGuid, cinfo.GetAddressOf())))
            return 0;

        WICComponentType type;
        if (FAILED(cinfo->GetComponentType(&type)))
            return 0;

        if (type != WICPixelFormat)
            return 0;

        ComPtr<IWICPixelFormatInfo> pfinfo;
        if (FAILED(cinfo.As(&pfinfo)))
            return 0;

        UINT bpp;
        if (FAILED(pfinfo->GetBitsPerPixel(&bpp)))
            return 0;

        return bpp;
    }

    //---------------------------------------------------------------------------------
    HRESULT CreateTextureFromWIC(
        _In_ ID3D11Device* d3dDevice,
        _In_opt_ ID3D11DeviceContext* d3dContext,
    #if defined(_XBOX_ONE) && defined(_TITLE)
        _In_opt_ ID3D11DeviceX* d3dDeviceX,
        _In_opt_ ID3D11DeviceContextX* d3dContextX,
    #endif
        _In_ IWICBitmapFrameDecode *frame,
        _In_ size_t maxsize,
        _In_ D3D11_USAGE usage,
        _In_ unsigned int bindFlags,
        _In_ unsigned int cpuAccessFlags,
        _In_ unsigned int miscFlags,
        _In_ WIC_LOADER_FLAGS loadFlags,
        _Outptr_opt_ ID3D11Resource** texture,
        _Outptr_opt_ ID3D11ShaderResourceView** textureView) noexcept
    {
        UINT width, height;
        HRESULT hr = frame->GetSize(&width, &height);
        if (FAILED(hr))
            return hr;

        if (maxsize > UINT32_MAX)
            return E_INVALIDARG;

        assert(width > 0 && height > 0);

        if (!maxsize)
        {
            // This is a bit conservative because the hardware could support larger textures than
            // the Feature Level defined minimums, but doing it this way is much easier and more
            // performant for WIC than the 'fail and retry' model used by DDSTextureLoader

            switch (d3dDevice->GetFeatureLevel())
            {
            case D3D_FEATURE_LEVEL_9_1:
            case D3D_FEATURE_LEVEL_9_2:
                maxsize = 2048u /*D3D_FL9_1_REQ_TEXTURE2D_U_OR_V_DIMENSION*/;
                break;

            case D3D_FEATURE_LEVEL_9_3:
                maxsize = 4096u /*D3D_FL9_3_REQ_TEXTURE2D_U_OR_V_DIMENSION*/;
                break;

            case D3D_FEATURE_LEVEL_10_0:
            case D3D_FEATURE_LEVEL_10_1:
                maxsize = 8192u /*D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION*/;
                break;

            default:
                maxsize = size_t(D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION);
                break;
            }
        }

        assert(maxsize > 0);

        UINT twidth = width;
        UINT theight = height;
        if (loadFlags & WIC_LOADER_FIT_POW2)
        {
            LoaderHelpers::FitPowerOf2(width, height, twidth, theight, maxsize);
        }
        else if (width > maxsize || height > maxsize)
        {
            const float ar = static_cast<float>(height) / static_cast<float>(width);
            if (width > height)
            {
                twidth = static_cast<UINT>(maxsize);
                theight = std::max<UINT>(1, static_cast<UINT>(static_cast<float>(maxsize) * ar));
            }
            else
            {
                theight = static_cast<UINT>(maxsize);
                twidth = std::max<UINT>(1, static_cast<UINT>(static_cast<float>(maxsize) / ar));
            }
            assert(twidth <= maxsize && theight <= maxsize);
        }

        if (loadFlags & WIC_LOADER_MAKE_SQUARE)
        {
            twidth = std::max<UINT>(twidth, theight);
            theight = twidth;
        }

        // Determine format
        WICPixelFormatGUID pixelFormat;
        hr = frame->GetPixelFormat(&pixelFormat);
        if (FAILED(hr))
            return hr;

        WICPixelFormatGUID convertGUID;
        memcpy_s(&convertGUID, sizeof(WICPixelFormatGUID), &pixelFormat, sizeof(GUID));

        size_t bpp = 0;

        DXGI_FORMAT format = WICToDXGI(pixelFormat);
        if (format == DXGI_FORMAT_UNKNOWN)
        {
            if (memcmp(&GUID_WICPixelFormat96bppRGBFixedPoint, &pixelFormat, sizeof(WICPixelFormatGUID)) == 0)
            {
            #if (_WIN32_WINNT >= _WIN32_WINNT_WIN8) || defined(_WIN7_PLATFORM_UPDATE)
                if (g_WIC2)
                {
                    memcpy_s(&convertGUID, sizeof(WICPixelFormatGUID), &GUID_WICPixelFormat96bppRGBFloat, sizeof(GUID));
                    format = DXGI_FORMAT_R32G32B32_FLOAT;
                    bpp = 96;
                }
                else
                #endif
                {
                    memcpy_s(&convertGUID, sizeof(WICPixelFormatGUID), &GUID_WICPixelFormat128bppRGBAFloat, sizeof(GUID));
                    format = DXGI_FORMAT_R32G32B32A32_FLOAT;
                    bpp = 128;
                }
            }
            else
            {
                for (size_t i = 0; i < std::size(g_WICConvert); ++i)
                {
                    if (memcmp(&g_WICConvert[i].source, &pixelFormat, sizeof(WICPixelFormatGUID)) == 0)
                    {
                        memcpy_s(&convertGUID, sizeof(WICPixelFormatGUID), &g_WICConvert[i].target, sizeof(GUID));

                        format = WICToDXGI(g_WICConvert[i].target);
                        assert(format != DXGI_FORMAT_UNKNOWN);
                        bpp = WICBitsPerPixel(convertGUID);
                        break;
                    }
                }
            }

            if (format == DXGI_FORMAT_UNKNOWN)
            {
                DebugTrace("ERROR: WICTextureLoader does not support all DXGI formats (WIC GUID {%8.8lX-%4.4X-%4.4X-%2.2X%2.2X-%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X}). Consider using DirectXTex.\n",
                    pixelFormat.Data1, pixelFormat.Data2, pixelFormat.Data3,
                    pixelFormat.Data4[0], pixelFormat.Data4[1], pixelFormat.Data4[2], pixelFormat.Data4[3],
                    pixelFormat.Data4[4], pixelFormat.Data4[5], pixelFormat.Data4[6], pixelFormat.Data4[7]);
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            }
        }
        else
        {
            bpp = WICBitsPerPixel(pixelFormat);
        }

    #if (_WIN32_WINNT >= _WIN32_WINNT_WIN8) || defined(_WIN7_PLATFORM_UPDATE)
        if ((format == DXGI_FORMAT_R32G32B32_FLOAT) && d3dContext && textureView)
        {
            // Special case test for optional device support for autogen mipchains for R32G32B32_FLOAT
            UINT fmtSupport = 0;
            hr = d3dDevice->CheckFormatSupport(DXGI_FORMAT_R32G32B32_FLOAT, &fmtSupport);
            if (FAILED(hr) || !(fmtSupport & D3D11_FORMAT_SUPPORT_MIP_AUTOGEN))
            {
                // Use R32G32B32A32_FLOAT instead which is required for Feature Level 10.0 and up
                memcpy_s(&convertGUID, sizeof(WICPixelFormatGUID), &GUID_WICPixelFormat128bppRGBAFloat, sizeof(GUID));
                format = DXGI_FORMAT_R32G32B32A32_FLOAT;
                bpp = 128;
            }
        }
    #endif

        if (loadFlags & WIC_LOADER_FORCE_RGBA32)
        {
            memcpy_s(&convertGUID, sizeof(WICPixelFormatGUID), &GUID_WICPixelFormat32bppRGBA, sizeof(GUID));
            format = DXGI_FORMAT_R8G8B8A8_UNORM;
            bpp = 32;
        }

        if (!bpp)
            return E_FAIL;

        // Handle sRGB formats
        if (loadFlags & WIC_LOADER_FORCE_SRGB)
        {
            format = LoaderHelpers::MakeSRGB(format);
        }
        else if (!(loadFlags & WIC_LOADER_IGNORE_SRGB))
        {
            ComPtr<IWICMetadataQueryReader> metareader;
            if (SUCCEEDED(frame->GetMetadataQueryReader(metareader.GetAddressOf())))
            {
                GUID containerFormat;
                if (SUCCEEDED(metareader->GetContainerFormat(&containerFormat)))
                {
                    bool sRGB = false;

                    PROPVARIANT value;
                    PropVariantInit(&value);

                    // Check for colorspace chunks
                    if (memcmp(&containerFormat, &GUID_ContainerFormatPng, sizeof(GUID)) == 0)
                    {
                        // Check for sRGB chunk
                        if (SUCCEEDED(metareader->GetMetadataByName(L"/sRGB/RenderingIntent", &value)) && value.vt == VT_UI1)
                        {
                            sRGB = true;
                        }
                        else if (SUCCEEDED(metareader->GetMetadataByName(L"/gAMA/ImageGamma", &value)) && value.vt == VT_UI4)
                        {
                            sRGB = (value.uintVal == 45455);
                        }
                        else
                        {
                            sRGB = (loadFlags & WIC_LOADER_SRGB_DEFAULT) != 0;
                        }
                    }
                #if defined(_XBOX_ONE) && defined(_TITLE)
                    else if (memcmp(&containerFormat, &GUID_ContainerFormatJpeg, sizeof(GUID)) == 0)
                    {
                        if (SUCCEEDED(metareader->GetMetadataByName(L"/app1/ifd/exif/{ushort=40961}", &value)) && value.vt == VT_UI2)
                        {
                            sRGB = (value.uiVal == 1);
                        }
                        else
                        {
                            sRGB = (loadFlags & WIC_LOADER_SRGB_DEFAULT) != 0;
                        }
                    }
                    else if (memcmp(&containerFormat, &GUID_ContainerFormatTiff, sizeof(GUID)) == 0)
                    {
                        if (SUCCEEDED(metareader->GetMetadataByName(L"/ifd/exif/{ushort=40961}", &value)) && value.vt == VT_UI2)
                        {
                            sRGB = (value.uiVal == 1);
                        }
                        else
                        {
                            sRGB = (loadFlags & WIC_LOADER_SRGB_DEFAULT) != 0;
                        }
                    }
                #else
                    else if (SUCCEEDED(metareader->GetMetadataByName(L"System.Image.ColorSpace", &value)) && value.vt == VT_UI2)
                    {
                        sRGB = (value.uiVal == 1);
                    }
                    else
                    {
                        sRGB = (loadFlags & WIC_LOADER_SRGB_DEFAULT) != 0;
                    }
                #endif

                    std::ignore = PropVariantClear(&value);

                    if (sRGB)
                        format = LoaderHelpers::MakeSRGB(format);
                }
            }
        }

        // Verify our target format is supported by the current device
        // (handles WDDM 1.0 or WDDM 1.1 device driver cases as well as DirectX 11.0 Runtime without 16bpp format support)
        UINT support = 0;
        hr = d3dDevice->CheckFormatSupport(format, &support);
        if (FAILED(hr) || !(support & D3D11_FORMAT_SUPPORT_TEXTURE2D))
        {
            // Fallback to RGBA 32-bit format which is supported by all devices
            memcpy_s(&convertGUID, sizeof(WICPixelFormatGUID), &GUID_WICPixelFormat32bppRGBA, sizeof(GUID));
            format = DXGI_FORMAT_R8G8B8A8_UNORM;
            bpp = 32;
        }

        // Allocate temporary memory for image
        const uint64_t rowBytes = (uint64_t(twidth) * uint64_t(bpp) + 7u) / 8u;
        const uint64_t numBytes = rowBytes * uint64_t(theight);

        if (rowBytes > UINT32_MAX || numBytes > UINT32_MAX)
            return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);

        auto const rowPitch = static_cast<size_t>(rowBytes);
        auto const imageSize = static_cast<size_t>(numBytes);

        std::unique_ptr<uint8_t[]> temp(new (std::nothrow) uint8_t[imageSize]);
        if (!temp)
            return E_OUTOFMEMORY;

        // Load image data
        if (memcmp(&convertGUID, &pixelFormat, sizeof(GUID)) == 0
            && twidth == width
            && theight == height)
        {
            // No format conversion or resize needed
            hr = frame->CopyPixels(nullptr, static_cast<UINT>(rowPitch), static_cast<UINT>(imageSize), temp.get());
            if (FAILED(hr))
                return hr;
        }
        else if (twidth != width || theight != height)
        {
            // Resize
            auto pWIC = GetWIC();
            if (!pWIC)
                return E_NOINTERFACE;

            ComPtr<IWICBitmapScaler> scaler;
            hr = pWIC->CreateBitmapScaler(scaler.GetAddressOf());
            if (FAILED(hr))
                return hr;

            hr = scaler->Initialize(frame, twidth, theight, WICBitmapInterpolationModeFant);
            if (FAILED(hr))
                return hr;

            WICPixelFormatGUID pfScaler;
            hr = scaler->GetPixelFormat(&pfScaler);
            if (FAILED(hr))
                return hr;

            if (memcmp(&convertGUID, &pfScaler, sizeof(GUID)) == 0)
            {
                // No format conversion needed
                hr = scaler->CopyPixels(nullptr, static_cast<UINT>(rowPitch), static_cast<UINT>(imageSize), temp.get());
                if (FAILED(hr))
                    return hr;
            }
            else
            {
                ComPtr<IWICFormatConverter> FC;
                hr = pWIC->CreateFormatConverter(FC.GetAddressOf());
                if (FAILED(hr))
                    return hr;

                BOOL canConvert = FALSE;
                hr = FC->CanConvert(pfScaler, convertGUID, &canConvert);
                if (FAILED(hr) || !canConvert)
                {
                    return E_UNEXPECTED;
                }

                hr = FC->Initialize(scaler.Get(), convertGUID, WICBitmapDitherTypeErrorDiffusion, nullptr, 0, WICBitmapPaletteTypeMedianCut);
                if (FAILED(hr))
                    return hr;

                hr = FC->CopyPixels(nullptr, static_cast<UINT>(rowPitch), static_cast<UINT>(imageSize), temp.get());
                if (FAILED(hr))
                    return hr;
            }
        }
        else
        {
            // Format conversion but no resize
            auto pWIC = GetWIC();
            if (!pWIC)
                return E_NOINTERFACE;

            ComPtr<IWICFormatConverter> FC;
            hr = pWIC->CreateFormatConverter(FC.GetAddressOf());
            if (FAILED(hr))
                return hr;

            BOOL canConvert = FALSE;
            hr = FC->CanConvert(pixelFormat, convertGUID, &canConvert);
            if (FAILED(hr) || !canConvert)
            {
                return E_UNEXPECTED;
            }

            hr = FC->Initialize(frame, convertGUID, WICBitmapDitherTypeErrorDiffusion, nullptr, 0, WICBitmapPaletteTypeMedianCut);
            if (FAILED(hr))
                return hr;

            hr = FC->CopyPixels(nullptr, static_cast<UINT>(rowPitch), static_cast<UINT>(imageSize), temp.get());
            if (FAILED(hr))
                return hr;
        }

        // See if format is supported for auto-gen mipmaps (varies by feature level)
        bool autogen = false;
        if (d3dContext && textureView) // Must have context and shader-view to auto generate mipmaps
        {
            UINT fmtSupport = 0;
            hr = d3dDevice->CheckFormatSupport(format, &fmtSupport);
            if (SUCCEEDED(hr) && (fmtSupport & D3D11_FORMAT_SUPPORT_MIP_AUTOGEN))
            {
                autogen = true;
            #if defined(_XBOX_ONE) && defined(_TITLE)
                if (!d3dDeviceX || !d3dContextX)
                    return E_INVALIDARG;
            #endif
            }
        }

        // Create texture
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = twidth;
        desc.Height = theight;
        desc.MipLevels = (autogen) ? 0u : 1u;
        desc.ArraySize = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = usage;
        desc.CPUAccessFlags = cpuAccessFlags;

        if (autogen)
        {
            desc.BindFlags = bindFlags | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
            desc.MiscFlags = miscFlags | D3D11_RESOURCE_MISC_GENERATE_MIPS;
        }
        else
        {
            desc.BindFlags = bindFlags;
            desc.MiscFlags = miscFlags;
        }

        D3D11_SUBRESOURCE_DATA initData = { temp.get(), static_cast<UINT>(rowPitch), static_cast<UINT>(imageSize) };

        ID3D11Texture2D* tex = nullptr;
        hr = d3dDevice->CreateTexture2D(&desc, (autogen) ? nullptr : &initData, &tex);
        if (SUCCEEDED(hr) && tex)
        {
            if (textureView)
            {
                D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
                SRVDesc.Format = desc.Format;

                SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                SRVDesc.Texture2D.MipLevels = (autogen) ? unsigned(-1) : 1u;

                hr = d3dDevice->CreateShaderResourceView(tex, &SRVDesc, textureView);
                if (FAILED(hr))
                {
                    tex->Release();
                    return hr;
                }

                if (autogen)
                {
                    assert(d3dContext != nullptr);

                #if defined(_XBOX_ONE) && defined(_TITLE)
                    ID3D11Texture2D *pStaging = nullptr;
                    CD3D11_TEXTURE2D_DESC stagingDesc(format, twidth, theight, 1, 1, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ, 1, 0, 0);
                    initData.pSysMem = temp.get();
                    initData.SysMemPitch = static_cast<UINT>(rowPitch);
                    initData.SysMemSlicePitch = static_cast<UINT>(imageSize);

                    hr = d3dDevice->CreateTexture2D(&stagingDesc, &initData, &pStaging);
                    if (SUCCEEDED(hr))
                    {
                        d3dContext->CopySubresourceRegion(tex, 0, 0, 0, 0, pStaging, 0, nullptr);

                        UINT64 copyFence = d3dContextX->InsertFence(0);
                        while (d3dDeviceX->IsFencePending(copyFence)) { SwitchToThread(); }
                        pStaging->Release();
                    }
                #else
                    d3dContext->UpdateSubresource(tex, 0, nullptr, temp.get(), static_cast<UINT>(rowPitch), static_cast<UINT>(imageSize));
                #endif
                    d3dContext->GenerateMips(*textureView);
                }
            }

            if (texture)
            {
                *texture = tex;
            }
            else
            {
                SetDebugObjectName(tex, "WICTextureLoader");
                tex->Release();
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
HRESULT DirectX::CreateWICTextureFromMemory(
    ID3D11Device* d3dDevice,
    const uint8_t* wicData,
    size_t wicDataSize,
    ID3D11Resource** texture,
    ID3D11ShaderResourceView** textureView,
    size_t maxsize) noexcept
{
    return CreateWICTextureFromMemoryEx(d3dDevice,
        wicData, wicDataSize,
        maxsize,
        D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0,
        WIC_LOADER_DEFAULT,
        texture, textureView);
}

_Use_decl_annotations_
#if defined(_XBOX_ONE) && defined(_TITLE)
HRESULT DirectX::CreateWICTextureFromMemory(
    ID3D11DeviceX* d3dDevice,
    ID3D11DeviceContextX* d3dContext,
#else
    HRESULT DirectX::CreateWICTextureFromMemory(
        ID3D11Device* d3dDevice,
        ID3D11DeviceContext* d3dContext,
    #endif
        const uint8_t* wicData,
        size_t wicDataSize,
        ID3D11Resource** texture,
        ID3D11ShaderResourceView** textureView,
        size_t maxsize) noexcept
{
    return CreateWICTextureFromMemoryEx(d3dDevice, d3dContext,
        wicData, wicDataSize,
        maxsize,
        D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0,
        WIC_LOADER_DEFAULT,
        texture, textureView);
}

_Use_decl_annotations_
HRESULT DirectX::CreateWICTextureFromMemoryEx(
    ID3D11Device* d3dDevice,
    const uint8_t* wicData,
    size_t wicDataSize,
    size_t maxsize,
    D3D11_USAGE usage,
    unsigned int bindFlags,
    unsigned int cpuAccessFlags,
    unsigned int miscFlags,
    WIC_LOADER_FLAGS loadFlags,
    ID3D11Resource** texture,
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

    if (!d3dDevice || !wicData || (!texture && !textureView))
    {
        return E_INVALIDARG;
    }

    if (textureView && !(bindFlags & D3D11_BIND_SHADER_RESOURCE))
    {
        return E_INVALIDARG;
    }

    if (!wicDataSize)
        return E_FAIL;

    if (wicDataSize > UINT32_MAX)
        return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);

    auto pWIC = GetWIC();
    if (!pWIC)
        return E_NOINTERFACE;

    // Create input stream for memory
    ComPtr<IWICStream> stream;
    HRESULT hr = pWIC->CreateStream(stream.GetAddressOf());
    if (FAILED(hr))
        return hr;

    hr = stream->InitializeFromMemory(const_cast<uint8_t*>(wicData), static_cast<DWORD>(wicDataSize));
    if (FAILED(hr))
        return hr;

    // Initialize WIC
    ComPtr<IWICBitmapDecoder> decoder;
    hr = pWIC->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, decoder.GetAddressOf());
    if (FAILED(hr))
        return hr;

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(hr))
        return hr;

    hr = CreateTextureFromWIC(d3dDevice, nullptr,
    #if defined(_XBOX_ONE) && defined(_TITLE)
        nullptr, nullptr,
    #endif
        frame.Get(), maxsize,
        usage, bindFlags, cpuAccessFlags, miscFlags,
        loadFlags,
        texture, textureView);
    if (FAILED(hr))
        return hr;

    if (texture && *texture)
    {
        SetDebugObjectName(*texture, "WICTextureLoader");
    }

    if (textureView && *textureView)
    {
        SetDebugObjectName(*textureView, "WICTextureLoader");
    }

    return hr;
}

_Use_decl_annotations_
#if defined(_XBOX_ONE) && defined(_TITLE)
HRESULT DirectX::CreateWICTextureFromMemoryEx(
    ID3D11DeviceX* d3dDevice,
    ID3D11DeviceContextX* d3dContext,
#else
    HRESULT DirectX::CreateWICTextureFromMemoryEx(
        ID3D11Device* d3dDevice,
        ID3D11DeviceContext* d3dContext,
    #endif
        const uint8_t* wicData,
        size_t wicDataSize,
        size_t maxsize,
        D3D11_USAGE usage,
        unsigned int bindFlags,
        unsigned int cpuAccessFlags,
        unsigned int miscFlags,
        WIC_LOADER_FLAGS loadFlags,
        ID3D11Resource** texture,
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

    if (!d3dDevice || !wicData || (!texture && !textureView))
    {
        return E_INVALIDARG;
    }

    if (textureView && !(bindFlags & D3D11_BIND_SHADER_RESOURCE))
    {
        return E_INVALIDARG;
    }

    if (!wicDataSize)
        return E_FAIL;

    if (wicDataSize > UINT32_MAX)
        return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);

    auto pWIC = GetWIC();
    if (!pWIC)
        return E_NOINTERFACE;

    // Create input stream for memory
    ComPtr<IWICStream> stream;
    HRESULT hr = pWIC->CreateStream(stream.GetAddressOf());
    if (FAILED(hr))
        return hr;

    hr = stream->InitializeFromMemory(const_cast<uint8_t*>(wicData), static_cast<DWORD>(wicDataSize));
    if (FAILED(hr))
        return hr;

    // Initialize WIC
    ComPtr<IWICBitmapDecoder> decoder;
    hr = pWIC->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, decoder.GetAddressOf());
    if (FAILED(hr))
        return hr;

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(hr))
        return hr;

    hr = CreateTextureFromWIC(d3dDevice, d3dContext,
    #if defined(_XBOX_ONE) && defined(_TITLE)
        d3dDevice, d3dContext,
    #endif
        frame.Get(),
        maxsize,
        usage, bindFlags, cpuAccessFlags, miscFlags,
        loadFlags,
        texture, textureView);
    if (FAILED(hr))
        return hr;

    if (texture && *texture)
    {
        SetDebugObjectName(*texture, "WICTextureLoader");
    }

    if (textureView && *textureView)
    {
        SetDebugObjectName(*textureView, "WICTextureLoader");
    }

    return hr;
}

//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::CreateWICTextureFromFile(
    ID3D11Device* d3dDevice,
    const wchar_t* fileName,
    ID3D11Resource** texture,
    ID3D11ShaderResourceView** textureView,
    size_t maxsize) noexcept
{
    return CreateWICTextureFromFileEx(d3dDevice,
        fileName,
        maxsize,
        D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0,
        WIC_LOADER_DEFAULT,
        texture, textureView);
}

_Use_decl_annotations_
#if defined(_XBOX_ONE) && defined(_TITLE)
HRESULT DirectX::CreateWICTextureFromFile(
    ID3D11DeviceX* d3dDevice,
    ID3D11DeviceContextX* d3dContext,
#else
    HRESULT DirectX::CreateWICTextureFromFile(
        ID3D11Device* d3dDevice,
        ID3D11DeviceContext* d3dContext,
    #endif
        const wchar_t* fileName,
        ID3D11Resource** texture,
        ID3D11ShaderResourceView** textureView,
        size_t maxsize) noexcept
{
    return CreateWICTextureFromFileEx(d3dDevice, d3dContext,
        fileName,
        maxsize,
        D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0,
        WIC_LOADER_DEFAULT,
        texture, textureView);
}

_Use_decl_annotations_
HRESULT DirectX::CreateWICTextureFromFileEx(
    ID3D11Device* d3dDevice,
    const wchar_t* fileName,
    size_t maxsize,
    D3D11_USAGE usage,
    unsigned int bindFlags,
    unsigned int cpuAccessFlags,
    unsigned int miscFlags,
    WIC_LOADER_FLAGS loadFlags,
    ID3D11Resource** texture,
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

    if (!d3dDevice || !fileName || (!texture && !textureView))
    {
        return E_INVALIDARG;
    }

    if (textureView && !(bindFlags & D3D11_BIND_SHADER_RESOURCE))
    {
        return E_INVALIDARG;
    }

    auto pWIC = GetWIC();
    if (!pWIC)
        return E_NOINTERFACE;

    // Initialize WIC
    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = pWIC->CreateDecoderFromFilename(fileName,
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        decoder.GetAddressOf());
    if (FAILED(hr))
        return hr;

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(hr))
        return hr;

    hr = CreateTextureFromWIC(d3dDevice, nullptr,
    #if defined(_XBOX_ONE) && defined(_TITLE)
        nullptr, nullptr,
    #endif
        frame.Get(),
        maxsize,
        usage, bindFlags, cpuAccessFlags, miscFlags,
        loadFlags,
        texture, textureView);

    if (SUCCEEDED(hr))
    {
        SetDebugTextureInfo(fileName, texture, textureView);
    }

    return hr;
}

_Use_decl_annotations_
#if defined(_XBOX_ONE) && defined(_TITLE)
HRESULT DirectX::CreateWICTextureFromFileEx(
    ID3D11DeviceX* d3dDevice,
    ID3D11DeviceContextX* d3dContext,
#else
    HRESULT DirectX::CreateWICTextureFromFileEx(
        ID3D11Device* d3dDevice,
        ID3D11DeviceContext* d3dContext,
    #endif
        const wchar_t* fileName,
        size_t maxsize,
        D3D11_USAGE usage,
        unsigned int bindFlags,
        unsigned int cpuAccessFlags,
        unsigned int miscFlags,
        WIC_LOADER_FLAGS loadFlags,
        ID3D11Resource** texture,
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

    if (!d3dDevice || !fileName || (!texture && !textureView))
    {
        return E_INVALIDARG;
    }

    if (textureView && !(bindFlags & D3D11_BIND_SHADER_RESOURCE))
    {
        return E_INVALIDARG;
    }

    auto pWIC = GetWIC();
    if (!pWIC)
        return E_NOINTERFACE;

    // Initialize WIC
    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = pWIC->CreateDecoderFromFilename(fileName,
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        decoder.GetAddressOf());
    if (FAILED(hr))
        return hr;

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(hr))
        return hr;

    hr = CreateTextureFromWIC(d3dDevice, d3dContext,
    #if defined(_XBOX_ONE) && defined(_TITLE)
        d3dDevice, d3dContext,
    #endif
        frame.Get(),
        maxsize,
        usage, bindFlags, cpuAccessFlags, miscFlags,
        loadFlags,
        texture, textureView);

    if (SUCCEEDED(hr))
    {
        SetDebugTextureInfo(fileName, texture, textureView);
    }

    return hr;
}
