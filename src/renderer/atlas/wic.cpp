// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "wic.h"

#include "Backend.h"

using namespace Microsoft::Console::Render::Atlas;
using namespace Microsoft::Console::Render::Atlas::WIC;

static wil::com_ptr<IWICImagingFactory2> getWicFactory()
{
    static const auto coUninitialize = wil::CoInitializeEx();
    static const auto wicFactory = wil::CoCreateInstance<IWICImagingFactory2>(CLSID_WICImagingFactory2);
    return wicFactory;
}

void WIC::SaveTextureToPNG(ID3D11DeviceContext* deviceContext, ID3D11Resource* source, double dpi, const wchar_t* fileName)
{
    __assume(deviceContext != nullptr);
    __assume(source != nullptr);

    wil::com_ptr<ID3D11Texture2D> texture;
    THROW_IF_FAILED(source->QueryInterface(IID_PPV_ARGS(texture.addressof())));

    wil::com_ptr<ID3D11Device> d3dDevice;
    deviceContext->GetDevice(d3dDevice.addressof());

    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;

    wil::com_ptr<ID3D11Texture2D> staging;
    THROW_IF_FAILED(d3dDevice->CreateTexture2D(&desc, nullptr, staging.put()));

    deviceContext->CopyResource(staging.get(), source);

    const auto wicFactory = getWicFactory();

    wil::com_ptr<IWICStream> stream;
    THROW_IF_FAILED(wicFactory->CreateStream(stream.addressof()));
    THROW_IF_FAILED(stream->InitializeFromFilename(fileName, GENERIC_WRITE));

    wil::com_ptr<IWICBitmapEncoder> encoder;
    THROW_IF_FAILED(wicFactory->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.addressof()));
    THROW_IF_FAILED(encoder->Initialize(stream.get(), WICBitmapEncoderNoCache));

    wil::com_ptr<IWICBitmapFrameEncode> frame;
    wil::com_ptr<IPropertyBag2> props;
    THROW_IF_FAILED(encoder->CreateNewFrame(frame.addressof(), props.addressof()));
    THROW_IF_FAILED(frame->Initialize(props.get()));
    THROW_IF_FAILED(frame->SetSize(desc.Width, desc.Height));
    THROW_IF_FAILED(frame->SetResolution(dpi, dpi));
    auto pixelFormat = GUID_WICPixelFormat32bppBGRA;
    THROW_IF_FAILED(frame->SetPixelFormat(&pixelFormat));

    D3D11_MAPPED_SUBRESOURCE mapped;
    THROW_IF_FAILED(deviceContext->Map(staging.get(), 0, D3D11_MAP_READ, 0, &mapped));
    THROW_IF_FAILED(frame->WritePixels(desc.Height, mapped.RowPitch, mapped.RowPitch * desc.Height, static_cast<BYTE*>(mapped.pData)));
    deviceContext->Unmap(staging.get(), 0);

    THROW_IF_FAILED(frame->Commit());
    THROW_IF_FAILED(encoder->Commit());
}

void WIC::LoadTextureFromFile(ID3D11Device* device, const wchar_t* fileName, ID3D11Texture2D** out_texture, ID3D11ShaderResourceView** out_textureView)
{
    __assume(device != nullptr);
    __assume(fileName != nullptr);
    __assume(out_texture != nullptr);
    __assume(out_textureView != nullptr);

    const auto wicFactory = getWicFactory();

    wil::com_ptr<IWICBitmapDecoder> decoder;
    THROW_IF_FAILED(wicFactory->CreateDecoderFromFilename(fileName, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, decoder.addressof()));

    wil::com_ptr<IWICBitmapFrameDecode> frame;
    THROW_IF_FAILED(decoder->GetFrame(0, frame.addressof()));

    WICPixelFormatGUID srcFormat;
    THROW_IF_FAILED(frame->GetPixelFormat(&srcFormat));

    UINT srcWidth, srcHeight;
    THROW_IF_FAILED(frame->GetSize(&srcWidth, &srcHeight));

    static constexpr u32 maxSizeU32 = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    static constexpr f32 maxSizeF32 = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    UINT tgtWidth = srcWidth;
    UINT tgtHeight = srcHeight;
    wil::com_ptr<IWICBitmapScaler> scaler;
    IWICBitmapSource* currentSource = frame.get();
    if (srcWidth > maxSizeU32 || srcHeight > maxSizeU32)
    {
        const auto ar = static_cast<float>(srcHeight) / static_cast<float>(srcWidth);
        if (srcWidth > srcHeight)
        {
            tgtWidth = maxSizeU32;
            tgtHeight = std::max<UINT>(1, lroundf(maxSizeF32 * ar));
        }
        else
        {
            tgtHeight = maxSizeU32;
            tgtWidth = std::max<UINT>(1, lroundf(maxSizeF32 / ar));
        }

        THROW_IF_FAILED(wicFactory->CreateBitmapScaler(scaler.addressof()));
        THROW_IF_FAILED(scaler->Initialize(currentSource, tgtWidth, tgtHeight, WICBitmapInterpolationModeFant));
        currentSource = scaler.get();
    }

    wil::com_ptr<IWICFormatConverter> converter;
    THROW_IF_FAILED(wicFactory->CreateFormatConverter(converter.addressof()));
    BOOL canConvert = FALSE;
    THROW_IF_FAILED(converter->CanConvert(srcFormat, GUID_WICPixelFormat32bppPBGRA, &canConvert));
    THROW_HR_IF(E_UNEXPECTED, !canConvert);
    THROW_IF_FAILED(converter->Initialize(currentSource, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeErrorDiffusion, nullptr, 0, WICBitmapPaletteTypeMedianCut));

    // Aligning the width by 8 pixels, results in a 32 byte stride, which is better for memcpy on contemporary hardware.
    const uint64_t stride = alignForward<uint64_t>(tgtWidth, 8) * sizeof(u32);
    const uint64_t bytes = stride * static_cast<uint64_t>(tgtHeight);
    THROW_HR_IF(ERROR_ARITHMETIC_OVERFLOW, bytes > UINT32_MAX);

    Buffer<BYTE, 32> buffer{ gsl::narrow_cast<size_t>(bytes) };
    THROW_IF_FAILED(converter->CopyPixels(nullptr, gsl::narrow_cast<UINT>(stride), gsl::narrow_cast<UINT>(bytes), buffer.data()));

    const D3D11_TEXTURE2D_DESC desc = {
        .Width = tgtWidth,
        .Height = tgtHeight,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
        .SampleDesc = { 1, 0 },
        .Usage = D3D11_USAGE_IMMUTABLE,
        .BindFlags = D3D11_BIND_SHADER_RESOURCE,
    };
    const D3D11_SUBRESOURCE_DATA initData = {
        .pSysMem = buffer.data(),
        .SysMemPitch = gsl::narrow_cast<UINT>(stride),
        .SysMemSlicePitch = gsl::narrow_cast<UINT>(bytes),
    };
    wil::com_ptr<ID3D11Texture2D> texture;
    THROW_IF_FAILED(device->CreateTexture2D(&desc, &initData, texture.addressof()));

    wil::com_ptr<ID3D11ShaderResourceView> textureView;
    THROW_IF_FAILED(device->CreateShaderResourceView(texture.get(), nullptr, textureView.addressof()));

    *out_texture = texture.detach();
    *out_textureView = textureView.detach();
}
