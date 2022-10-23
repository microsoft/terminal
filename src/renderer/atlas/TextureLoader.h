// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace Microsoft::Console::Render
{
    struct ShaderTexture
    {
        wil::com_ptr<ID3D11Resource> Texture;
        wil::com_ptr<ID3D11ShaderResourceView> TextureView;
    };

    Microsoft::Console::Render::ShaderTexture LoadShaderTextureFromFile(
        ID3D11Device* d3dDevice,
        const std::wstring& fileName);
}
