#include "pch.h"

#include "TextureLoader.h"

#include "WICTextureLoader.h"

namespace Microsoft::Console::Render
{
    Microsoft::Console::Render::ShaderTexture LoadShaderTextureFromFile(
        ID3D11Device* d3dDevice,
        const std::wstring& fileName)
    {
        Microsoft::Console::Render::ShaderTexture result;

        const auto hr = DirectX::CreateWICTextureFromFileEx(
            d3dDevice,
            fileName.c_str(),
            0,
            D3D11_USAGE_DEFAULT,
            D3D11_BIND_SHADER_RESOURCE,
            0,
            0,
            DirectX::WIC_LOADER_DEFAULT
                // TODO: Should we ignore SRGB conversion?
                //  If we use the default settings the texture is converted
                //  from SRGB into linear RGB which do make a lot of sense but
                //  can also be a somewhat surprising to devs not used to it.
                | DirectX::WIC_LOADER_IGNORE_SRGB,
            result.Texture.addressof(),
            result.TextureView.addressof());

        if (FAILED(hr))
        {
            LOG_HR(hr);
            return Microsoft::Console::Render::ShaderTexture();
        }

        return result;
    }
}
