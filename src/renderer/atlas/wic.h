// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace Microsoft::Console::Render::Atlas::WIC
{
    void SaveTextureToPNG(ID3D11DeviceContext* deviceContext, ID3D11Resource* source, double dpi, const wchar_t* fileName);
    void LoadTextureFromFile(ID3D11Device* device, const wchar_t* fileName, ID3D11Texture2D** out_texture, ID3D11ShaderResourceView** out_textureView);
}
