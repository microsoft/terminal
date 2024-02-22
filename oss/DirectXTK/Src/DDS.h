//--------------------------------------------------------------------------------------
// DDS.h
//
// This header defines constants and structures that are useful when parsing
// DDS files.  DDS files were originally designed to use several structures
// and constants that are native to DirectDraw and are defined in ddraw.h,
// such as DDSURFACEDESC2 and DDSCAPS2.  This file defines similar
// (compatible) constants and structures so that one can use DDS files
// without needing to include ddraw.h.
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
// http://go.microsoft.com/fwlink/?LinkId=248929
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#pragma once

#include <cstdint>

namespace DirectX
{

#pragma pack(push,1)

    constexpr uint32_t DDS_MAGIC = 0x20534444; // "DDS "

    struct DDS_PIXELFORMAT
    {
        uint32_t    size;
        uint32_t    flags;
        uint32_t    fourCC;
        uint32_t    RGBBitCount;
        uint32_t    RBitMask;
        uint32_t    GBitMask;
        uint32_t    BBitMask;
        uint32_t    ABitMask;
    };

#define DDS_FOURCC      0x00000004  // DDPF_FOURCC
#define DDS_RGB         0x00000040  // DDPF_RGB
#define DDS_RGBA        0x00000041  // DDPF_RGB | DDPF_ALPHAPIXELS
#define DDS_LUMINANCE   0x00020000  // DDPF_LUMINANCE
#define DDS_LUMINANCEA  0x00020001  // DDPF_LUMINANCE | DDPF_ALPHAPIXELS
#define DDS_ALPHAPIXELS 0x00000001  // DDPF_ALPHAPIXELS
#define DDS_ALPHA       0x00000002  // DDPF_ALPHA
#define DDS_PAL8        0x00000020  // DDPF_PALETTEINDEXED8
#define DDS_PAL8A       0x00000021  // DDPF_PALETTEINDEXED8 | DDPF_ALPHAPIXELS
#define DDS_BUMPDUDV    0x00080000  // DDPF_BUMPDUDV
// DDS_BUMPLUMINANCE 0x00040000

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3) \
                (static_cast<uint32_t>(static_cast<uint8_t>(ch0)) \
                | (static_cast<uint32_t>(static_cast<uint8_t>(ch1)) << 8) \
                | (static_cast<uint32_t>(static_cast<uint8_t>(ch2)) << 16) \
                | (static_cast<uint32_t>(static_cast<uint8_t>(ch3)) << 24))
#endif /* MAKEFOURCC */

#ifndef DDSGLOBALCONST
#if defined(__GNUC__) && !defined(__MINGW32__)
#define DDSGLOBALCONST extern const __attribute__((weak))
#else
#define DDSGLOBALCONST extern const __declspec(selectany)
#endif
#endif /* DDSGLOBALCONST */

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_DXT1 =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D','X','T','1'), 0, 0, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_DXT2 =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D','X','T','2'), 0, 0, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_DXT3 =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D','X','T','3'), 0, 0, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_DXT4 =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D','X','T','4'), 0, 0, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_DXT5 =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D','X','T','5'), 0, 0, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_BC4_UNORM =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('B','C','4','U'), 0, 0, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_BC4_SNORM =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('B','C','4','S'), 0, 0, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_BC5_UNORM =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('B','C','5','U'), 0, 0, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_BC5_SNORM =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('B','C','5','S'), 0, 0, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_R8G8_B8G8 =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('R','G','B','G'), 0, 0, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_G8R8_G8B8 =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('G','R','G','B'), 0, 0, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_YUY2 =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('Y','U','Y','2'), 0, 0, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_UYVY =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('U','Y','V','Y'), 0, 0, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_A8R8G8B8 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_X8R8G8B8 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGB,  0, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_A8B8G8R8 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_X8B8G8R8 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGB,  0, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_G16R16 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGB,  0, 32, 0x0000ffff, 0xffff0000, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_R5G6B5 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 16, 0xf800, 0x07e0, 0x001f, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_A1R5G5B5 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 16, 0x7c00, 0x03e0, 0x001f, 0x8000 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_X1R5G5B5 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 16, 0x7c00, 0x03e0, 0x001f, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_A4R4G4B4 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 16, 0x0f00, 0x00f0, 0x000f, 0xf000 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_X4R4G4B4 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 16, 0x0f00, 0x00f0, 0x000f, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_R8G8B8 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 24, 0xff0000, 0x00ff00, 0x0000ff, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_A8R3G3B2 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 16, 0x00e0, 0x001c, 0x0003, 0xff00 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_R3G3B2 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 8, 0xe0, 0x1c, 0x03, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_A4L4 =
    { sizeof(DDS_PIXELFORMAT), DDS_LUMINANCEA, 0, 8, 0x0f, 0, 0, 0xf0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_L8 =
    { sizeof(DDS_PIXELFORMAT), DDS_LUMINANCE, 0, 8, 0xff, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_L16 =
    { sizeof(DDS_PIXELFORMAT), DDS_LUMINANCE, 0, 16, 0xffff, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_A8L8 =
    { sizeof(DDS_PIXELFORMAT), DDS_LUMINANCEA, 0, 16, 0x00ff, 0, 0, 0xff00 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_A8L8_ALT =
    { sizeof(DDS_PIXELFORMAT), DDS_LUMINANCEA, 0, 8, 0x00ff, 0, 0, 0xff00 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_L8_NVTT1 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 8, 0xff, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_L16_NVTT1 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 16, 0xffff, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_A8L8_NVTT1 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 16, 0x00ff, 0, 0, 0xff00 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_A8 =
    { sizeof(DDS_PIXELFORMAT), DDS_ALPHA, 0, 8, 0, 0, 0, 0xff };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_V8U8 =
    { sizeof(DDS_PIXELFORMAT), DDS_BUMPDUDV, 0, 16, 0x00ff, 0xff00, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_Q8W8V8U8 =
    { sizeof(DDS_PIXELFORMAT), DDS_BUMPDUDV, 0, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_V16U16 =
    { sizeof(DDS_PIXELFORMAT), DDS_BUMPDUDV, 0, 32, 0x0000ffff, 0xffff0000, 0, 0 };

// D3DFMT_A2R10G10B10/D3DFMT_A2B10G10R10 should be written using DX10 extension to avoid D3DX 10:10:10:2 reversal issue
    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_A2R10G10B10 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 32, 0x000003ff, 0x000ffc00, 0x3ff00000, 0xc0000000 };
    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_A2B10G10R10 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 32, 0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000 };

// We do not support the following legacy Direct3D 9 formats:
// DDSPF_A2W10V10U10 = { sizeof(DDS_PIXELFORMAT), DDS_BUMPDUDV, 0, 32, 0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000 };
// DDSPF_L6V5U5 = { sizeof(DDS_PIXELFORMAT), DDS_BUMPLUMINANCE, 0, 16, 0x001f, 0x03e0, 0xfc00, 0 };
// DDSPF_X8L8V8U8 = { sizeof(DDS_PIXELFORMAT), DDS_BUMPLUMINANCE, 0, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0 };

// This indicates the DDS_HEADER_DXT10 extension is present (the format is in dxgiFormat)
    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_DX10 =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D','X','1','0'), 0, 0, 0, 0, 0 };

#define DDS_HEADER_FLAGS_TEXTURE        0x00001007  // DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT
#define DDS_HEADER_FLAGS_MIPMAP         0x00020000  // DDSD_MIPMAPCOUNT
#define DDS_HEADER_FLAGS_VOLUME         0x00800000  // DDSD_DEPTH
#define DDS_HEADER_FLAGS_PITCH          0x00000008  // DDSD_PITCH
#define DDS_HEADER_FLAGS_LINEARSIZE     0x00080000  // DDSD_LINEARSIZE

#define DDS_HEIGHT 0x00000002 // DDSD_HEIGHT
#define DDS_WIDTH  0x00000004 // DDSD_WIDTH

#define DDS_SURFACE_FLAGS_TEXTURE 0x00001000 // DDSCAPS_TEXTURE
#define DDS_SURFACE_FLAGS_MIPMAP  0x00400008 // DDSCAPS_COMPLEX | DDSCAPS_MIPMAP
#define DDS_SURFACE_FLAGS_CUBEMAP 0x00000008 // DDSCAPS_COMPLEX

#define DDS_CUBEMAP_POSITIVEX 0x00000600 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEX
#define DDS_CUBEMAP_NEGATIVEX 0x00000a00 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEX
#define DDS_CUBEMAP_POSITIVEY 0x00001200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEY
#define DDS_CUBEMAP_NEGATIVEY 0x00002200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEY
#define DDS_CUBEMAP_POSITIVEZ 0x00004200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEZ
#define DDS_CUBEMAP_NEGATIVEZ 0x00008200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEZ

#define DDS_CUBEMAP_ALLFACES ( DDS_CUBEMAP_POSITIVEX | DDS_CUBEMAP_NEGATIVEX |\
                               DDS_CUBEMAP_POSITIVEY | DDS_CUBEMAP_NEGATIVEY |\
                               DDS_CUBEMAP_POSITIVEZ | DDS_CUBEMAP_NEGATIVEZ )

#define DDS_CUBEMAP 0x00000200 // DDSCAPS2_CUBEMAP

#define DDS_FLAGS_VOLUME 0x00200000 // DDSCAPS2_VOLUME

// Subset here matches D3D10_RESOURCE_DIMENSION and D3D11_RESOURCE_DIMENSION
    enum DDS_RESOURCE_DIMENSION : uint32_t
    {
        DDS_DIMENSION_TEXTURE1D = 2,
        DDS_DIMENSION_TEXTURE2D = 3,
        DDS_DIMENSION_TEXTURE3D = 4,
    };

    // Subset here matches D3D10_RESOURCE_MISC_FLAG and D3D11_RESOURCE_MISC_FLAG
    enum DDS_RESOURCE_MISC_FLAG : uint32_t
    {
        DDS_RESOURCE_MISC_TEXTURECUBE = 0x4L,
    };

    enum DDS_MISC_FLAGS2 : uint32_t
    {
        DDS_MISC_FLAGS2_ALPHA_MODE_MASK = 0x7L,
    };

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

    struct DDS_HEADER
    {
        uint32_t        size;
        uint32_t        flags;
        uint32_t        height;
        uint32_t        width;
        uint32_t        pitchOrLinearSize;
        uint32_t        depth; // only if DDS_HEADER_FLAGS_VOLUME is set in flags
        uint32_t        mipMapCount;
        uint32_t        reserved1[11];
        DDS_PIXELFORMAT ddspf;
        uint32_t        caps;
        uint32_t        caps2;
        uint32_t        caps3;
        uint32_t        caps4;
        uint32_t        reserved2;
    };

    struct DDS_HEADER_DXT10
    {
        DXGI_FORMAT     dxgiFormat;
        uint32_t        resourceDimension;
        uint32_t        miscFlag; // see D3D11_RESOURCE_MISC_FLAG
        uint32_t        arraySize;
        uint32_t        miscFlags2; // see DDS_MISC_FLAGS2
    };

#pragma pack(pop)

    static_assert(sizeof(DDS_HEADER) == 124, "DDS Header size mismatch");
    static_assert(sizeof(DDS_HEADER_DXT10) == 20, "DDS DX10 Extended Header size mismatch");

} // namespace
