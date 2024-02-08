// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <wincodec.h>

inline void SaveTextureToPNG(ID3D11DeviceContext* deviceContext, ID3D11Resource* source, double dpi, const wchar_t* fileName)
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

    static const auto coUninitialize = wil::CoInitializeEx();
    static const auto wicFactory = wil::CoCreateInstance<IWICImagingFactory2>(CLSID_WICImagingFactory2);

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
