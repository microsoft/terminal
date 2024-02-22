//--------------------------------------------------------------------------------------
// File: DGSLEffectFactory.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "Effects.h"
#include "DemandCreate.h"
#include "SharedResourcePool.h"

#include "DDSTextureLoader.h"
#include "WICTextureLoader.h"

#include <string.h>

#include "BinaryReader.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

static_assert(DGSLEffect::MaxTextures == DGSLEffectFactory::DGSLEffectInfo::BaseTextureOffset + _countof(DGSLEffectFactory::DGSLEffectInfo::textures), "DGSL supports 8 textures");

// Internal DGSLEffectFactory implementation class. Only one of these helpers is allocated
// per D3D device, even if there are multiple public facing DGSLEffectFactory instances.
class DGSLEffectFactory::Impl
{
public:
    explicit Impl(_In_ ID3D11Device* device)
        : mPath{},
        mDevice(device),
        mSharing(true),
        mForceSRGB(false)
    {}

    std::shared_ptr<IEffect> CreateEffect(_In_ DGSLEffectFactory* factory, _In_ const IEffectFactory::EffectInfo& info, _In_opt_ ID3D11DeviceContext* deviceContext);
    std::shared_ptr<IEffect> CreateDGSLEffect(_In_ DGSLEffectFactory* factory, _In_ const DGSLEffectInfo& info, _In_opt_ ID3D11DeviceContext* deviceContext);
    void CreateTexture(_In_z_ const wchar_t* texture, _In_opt_ ID3D11DeviceContext* deviceContext, _Outptr_ ID3D11ShaderResourceView** textureView);
    void CreatePixelShader(_In_z_ const wchar_t* shader, _Outptr_ ID3D11PixelShader** pixelShader);

    void ReleaseCache();
    void SetSharing(bool enabled) noexcept { mSharing = enabled; }
    void EnableForceSRGB(bool forceSRGB) noexcept { mForceSRGB = forceSRGB; }

    static SharedResourcePool<ID3D11Device*, Impl> instancePool;

    wchar_t mPath[MAX_PATH];

    ComPtr<ID3D11Device> mDevice;

private:
    using EffectCache = std::map< std::wstring, std::shared_ptr<IEffect> >;
    using TextureCache = std::map< std::wstring, ComPtr<ID3D11ShaderResourceView> >;
    using ShaderCache = std::map< std::wstring, ComPtr<ID3D11PixelShader> >;

    EffectCache  mEffectCache;
    EffectCache  mEffectCacheSkinning;
    TextureCache mTextureCache;
    ShaderCache  mShaderCache;

    bool mSharing;
    bool mForceSRGB;

    std::mutex mutex;
};


// Global instance pool.
SharedResourcePool<ID3D11Device*, DGSLEffectFactory::Impl> DGSLEffectFactory::Impl::instancePool;


_Use_decl_annotations_
std::shared_ptr<IEffect> DGSLEffectFactory::Impl::CreateEffect(DGSLEffectFactory* factory, const DGSLEffectFactory::EffectInfo& info, ID3D11DeviceContext* deviceContext)
{
    if (info.enableDualTexture)
    {
        throw std::runtime_error("DGSLEffect does not support multiple texcoords");
    }

    if (mSharing && info.name && *info.name)
    {
        if (info.enableSkinning)
        {
            auto it = mEffectCacheSkinning.find(info.name);
            if (it != mEffectCacheSkinning.end())
            {
                return it->second;
            }
        }
        else
        {
            auto it = mEffectCache.find(info.name);
            if (it != mEffectCache.end())
            {
                return it->second;
            }
        }
    }

    std::shared_ptr<DGSLEffect> effect;
    if (info.enableSkinning)
    {
        effect = std::make_shared<SkinnedDGSLEffect>(mDevice.Get(), nullptr);
    }
    else
    {
        effect = std::make_shared<DGSLEffect>(mDevice.Get(), nullptr);
    }

    effect->EnableDefaultLighting();
    effect->SetLightingEnabled(true);

    XMVECTOR color = XMLoadFloat3(&info.ambientColor);
    effect->SetAmbientColor(color);

    color = XMLoadFloat3(&info.diffuseColor);
    effect->SetDiffuseColor(color);

    effect->SetAlpha(info.alpha);

    if (info.perVertexColor)
    {
        effect->SetVertexColorEnabled(true);
    }

    if (info.specularColor.x != 0 || info.specularColor.y != 0 || info.specularColor.z != 0)
    {
        color = XMLoadFloat3(&info.specularColor);
        effect->SetSpecularColor(color);
        effect->SetSpecularPower(info.specularPower);
    }

    if (info.emissiveColor.x != 0 || info.emissiveColor.y != 0 || info.emissiveColor.z != 0)
    {
        color = XMLoadFloat3(&info.emissiveColor);
        effect->SetEmissiveColor(color);
    }

    if (info.diffuseTexture && *info.diffuseTexture)
    {
        ComPtr<ID3D11ShaderResourceView> srv;

        factory->CreateTexture(info.diffuseTexture, deviceContext, srv.GetAddressOf());

        effect->SetTexture(srv.Get());
        effect->SetTextureEnabled(true);
    }

    if (mSharing && info.name && *info.name)
    {
        std::lock_guard<std::mutex> lock(mutex);
        EffectCache::value_type v(info.name, effect);
        if (info.enableSkinning)
        {
            mEffectCacheSkinning.insert(v);
        }
        else
        {
            mEffectCache.insert(v);
        }
    }

    return std::move(effect);
}


_Use_decl_annotations_
std::shared_ptr<IEffect> DGSLEffectFactory::Impl::CreateDGSLEffect(DGSLEffectFactory* factory, const DGSLEffectFactory::DGSLEffectInfo& info, ID3D11DeviceContext* deviceContext)
{
    if (mSharing && info.name && *info.name)
    {
        if (info.enableSkinning)
        {
            auto it = mEffectCacheSkinning.find(info.name);
            if (it != mEffectCacheSkinning.end())
            {
                return it->second;
            }
        }
        else
        {
            auto it = mEffectCache.find(info.name);
            if (it != mEffectCache.end())
            {
                return it->second;
            }
        }
    }

    std::shared_ptr<DGSLEffect> effect;

    bool lighting = true;
    bool allowSpecular = true;

    if (!info.pixelShader || !*info.pixelShader)
    {
        if (info.enableSkinning)
        {
            effect = std::make_shared<SkinnedDGSLEffect>(mDevice.Get(), nullptr);
        }
        else
        {
            effect = std::make_shared<DGSLEffect>(mDevice.Get(), nullptr);
        }
    }
    else
    {
        wchar_t root[MAX_PATH] = {};
        auto last = wcsrchr(info.pixelShader, '_');
        if (last)
        {
            wcscpy_s(root, last + 1);
        }
        else
        {
            wcscpy_s(root, info.pixelShader);
        }

        auto first = wcschr(root, '.');
        if (first)
            *first = 0;

        ComPtr<ID3D11PixelShader> ps;
        if (!_wcsicmp(root, L"lambert"))
        {
            allowSpecular = false;
        }
        else if (!_wcsicmp(root, L"phong"))
        {
            // lighting, allowSpecular = true
        }
        else if (!_wcsicmp(root, L"unlit"))
        {
            lighting = false;
        }
        else if (mDevice->GetFeatureLevel() < D3D_FEATURE_LEVEL_10_0)
        {
            // DGSL shaders are not compatible with Feature Level 9.x, use fallback shader
            wcscat_s(root, L".cso");

            factory->CreatePixelShader(root, ps.GetAddressOf());
        }
        else
        {
            // Create DGSL shader and use it for the effect
            factory->CreatePixelShader(info.pixelShader, ps.GetAddressOf());
        }

        if (info.enableSkinning)
        {
            effect = std::make_shared<SkinnedDGSLEffect>(mDevice.Get(), ps.Get());
        }
        else
        {
            effect = std::make_shared<DGSLEffect>(mDevice.Get(), ps.Get());
        }
    }

    if (lighting)
    {
        effect->EnableDefaultLighting();
        effect->SetLightingEnabled(true);
    }

    XMVECTOR color = XMLoadFloat3(&info.ambientColor);
    effect->SetAmbientColor(color);

    color = XMLoadFloat3(&info.diffuseColor);
    effect->SetDiffuseColor(color);
    effect->SetAlpha(info.alpha);

    if (info.perVertexColor)
    {
        effect->SetVertexColorEnabled(true);
    }

    effect->SetAlphaDiscardEnable(true);

    if (allowSpecular
        && (info.specularColor.x != 0 || info.specularColor.y != 0 || info.specularColor.z != 0))
    {
        color = XMLoadFloat3(&info.specularColor);
        effect->SetSpecularColor(color);
        effect->SetSpecularPower(info.specularPower);
    }
    else
    {
        effect->DisableSpecular();
    }

    if (info.emissiveColor.x != 0 || info.emissiveColor.y != 0 || info.emissiveColor.z != 0)
    {
        color = XMLoadFloat3(&info.emissiveColor);
        effect->SetEmissiveColor(color);
    }

    if (info.diffuseTexture && *info.diffuseTexture)
    {
        ComPtr<ID3D11ShaderResourceView> srv;

        factory->CreateTexture(info.diffuseTexture, deviceContext, srv.GetAddressOf());

        effect->SetTexture(srv.Get());
        effect->SetTextureEnabled(true);
    }

    if (info.specularTexture && *info.specularTexture)
    {
        ComPtr<ID3D11ShaderResourceView> srv;

        factory->CreateTexture(info.specularTexture, deviceContext, srv.GetAddressOf());

        effect->SetTexture(1, srv.Get());
        effect->SetTextureEnabled(true);
    }

    if (info.normalTexture && *info.normalTexture)
    {
        ComPtr<ID3D11ShaderResourceView> srv;

        factory->CreateTexture(info.normalTexture, deviceContext, srv.GetAddressOf());

        effect->SetTexture(2, srv.Get());
        effect->SetTextureEnabled(true);
    }

    if (info.emissiveTexture && *info.emissiveTexture)
    {
        ComPtr<ID3D11ShaderResourceView> srv;

        factory->CreateTexture(info.emissiveTexture, deviceContext, srv.GetAddressOf());

        effect->SetTexture(3, srv.Get());
        effect->SetTextureEnabled(true);
    }

    for (size_t j = 0; j < std::size(info.textures); ++j)
    {
        if (info.textures[j] && *info.textures[j])
        {
            ComPtr<ID3D11ShaderResourceView> srv;

            factory->CreateTexture(info.textures[j], deviceContext, srv.GetAddressOf());

            effect->SetTexture(static_cast<int>(j) + DGSLEffectInfo::BaseTextureOffset, srv.Get());
            effect->SetTextureEnabled(true);
        }
    }

    if (mSharing && info.name && *info.name)
    {
        std::lock_guard<std::mutex> lock(mutex);
        EffectCache::value_type v(info.name, effect);
        if (info.enableSkinning)
        {
            mEffectCacheSkinning.insert(v);
        }
        else
        {
            mEffectCache.insert(v);
        }
    }

    return std::move(effect);
}


_Use_decl_annotations_
void DGSLEffectFactory::Impl::CreateTexture(const wchar_t* name, ID3D11DeviceContext* deviceContext, ID3D11ShaderResourceView** textureView)
{
    if (!name || !textureView)
        throw std::invalid_argument("name and textureView parameters can't be null");

#if defined(_XBOX_ONE) && defined(_TITLE)
    UNREFERENCED_PARAMETER(deviceContext);
#endif

    auto it = mTextureCache.find(name);

    if (mSharing && it != mTextureCache.end())
    {
        ID3D11ShaderResourceView* srv = it->second.Get();
        srv->AddRef();
        *textureView = srv;
    }
    else
    {
        wchar_t fullName[MAX_PATH] = {};
        wcscpy_s(fullName, mPath);
        wcscat_s(fullName, name);

        WIN32_FILE_ATTRIBUTE_DATA fileAttr = {};
        if (!GetFileAttributesExW(fullName, GetFileExInfoStandard, &fileAttr))
        {
            // Try Current Working Directory (CWD)
            wcscpy_s(fullName, name);
            if (!GetFileAttributesExW(fullName, GetFileExInfoStandard, &fileAttr))
            {
                DebugTrace("ERROR: DGSLEffectFactory could not find texture file '%ls'\n", name);
                throw std::system_error(std::error_code(static_cast<int>(GetLastError()), std::system_category()), "DGSLEffectFactory::CreateTexture");
            }
        }

        wchar_t ext[_MAX_EXT] = {};
        _wsplitpath_s(name, nullptr, 0, nullptr, 0, nullptr, 0, ext, _MAX_EXT);
        const bool isdds = _wcsicmp(ext, L".dds") == 0;

        if (isdds)
        {
            HRESULT hr = CreateDDSTextureFromFileEx(
                mDevice.Get(), fullName, 0,
                D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0,
                mForceSRGB ? DDS_LOADER_FORCE_SRGB : DDS_LOADER_DEFAULT, nullptr, textureView);
            if (FAILED(hr))
            {
                DebugTrace("ERROR: CreateDDSTextureFromFile failed (%08X) for '%ls'\n",
                    static_cast<unsigned int>(hr), fullName);
                throw std::runtime_error("DGSLEffectFactory::CreateDDSTextureFromFile");
            }
        }
    #if !defined(_XBOX_ONE) || !defined(_TITLE)
        else if (deviceContext)
        {
            std::lock_guard<std::mutex> lock(mutex);
            HRESULT hr = CreateWICTextureFromFileEx(
                mDevice.Get(), deviceContext, fullName, 0,
                D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0,
                mForceSRGB ? WIC_LOADER_FORCE_SRGB : WIC_LOADER_DEFAULT, nullptr, textureView);
            if (FAILED(hr))
            {
                DebugTrace("ERROR: CreateWICTextureFromFile failed (%08X) for '%ls'\n",
                    static_cast<unsigned int>(hr), fullName);
                throw std::runtime_error("DGSLEffectFactory::CreateWICTextureFromFile");
            }
        }
    #endif
        else
        {
            HRESULT hr = CreateWICTextureFromFileEx(
                mDevice.Get(), fullName, 0,
                D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0,
                mForceSRGB ? WIC_LOADER_FORCE_SRGB : WIC_LOADER_DEFAULT, nullptr, textureView);
            if (FAILED(hr))
            {
                DebugTrace("ERROR: CreateWICTextureFromFile failed (%08X) for '%ls'\n",
                    static_cast<unsigned int>(hr), fullName);
                throw std::runtime_error("DGSLEffectFactory::CreateWICTextureFromFile");
            }
        }

        if (mSharing && *name && it == mTextureCache.end())
        {
            std::lock_guard<std::mutex> lock(mutex);
            TextureCache::value_type v(name, *textureView);
            mTextureCache.insert(v);
        }
    }
}


_Use_decl_annotations_
void DGSLEffectFactory::Impl::CreatePixelShader(const wchar_t* name, ID3D11PixelShader** pixelShader)
{
    if (!name || !pixelShader)
        throw std::invalid_argument("name and pixelShader parameters can't be null");

    auto it = mShaderCache.find(name);

    if (mSharing && it != mShaderCache.end())
    {
        ID3D11PixelShader* ps = it->second.Get();
        ps->AddRef();
        *pixelShader = ps;
    }
    else
    {
        wchar_t fullName[MAX_PATH] = {};
        wcscpy_s(fullName, mPath);
        wcscat_s(fullName, name);

        WIN32_FILE_ATTRIBUTE_DATA fileAttr = {};
        if (!GetFileAttributesExW(fullName, GetFileExInfoStandard, &fileAttr))
        {
            // Try Current Working Directory (CWD)
            wcscpy_s(fullName, name);
            if (!GetFileAttributesExW(fullName, GetFileExInfoStandard, &fileAttr))
            {
                DebugTrace("ERROR: DGSLEffectFactory could not find shader file '%ls'\n", name);
                throw std::system_error(std::error_code(static_cast<int>(GetLastError()), std::system_category()), "DGSLEffectFactory::CreatePixelShader");
            }
        }

        size_t dataSize = 0;
        std::unique_ptr<uint8_t[]> data;
        HRESULT hr = BinaryReader::ReadEntireFile(fullName, data, &dataSize);
        if (FAILED(hr))
        {
            DebugTrace("ERROR: CreatePixelShader failed (%08X) to load shader file '%ls'\n",
                static_cast<unsigned int>(hr), fullName);
            throw std::runtime_error("DGSLEffectFactory::CreatePixelShader");
        }

        ThrowIfFailed(
            mDevice->CreatePixelShader(data.get(), dataSize, nullptr, pixelShader));

        assert(pixelShader != nullptr && *pixelShader != nullptr);
        _Analysis_assume_(pixelShader != nullptr && *pixelShader != nullptr);

        if (mSharing && *name && it == mShaderCache.end())
        {
            std::lock_guard<std::mutex> lock(mutex);
            ShaderCache::value_type v(name, *pixelShader);
            mShaderCache.insert(v);
        }
    }
}


void DGSLEffectFactory::Impl::ReleaseCache()
{
    std::lock_guard<std::mutex> lock(mutex);
    mEffectCache.clear();
    mEffectCacheSkinning.clear();
    mTextureCache.clear();
    mShaderCache.clear();
}



//--------------------------------------------------------------------------------------
// DGSLEffectFactory
//--------------------------------------------------------------------------------------

DGSLEffectFactory::DGSLEffectFactory(_In_ ID3D11Device* device)
    : pImpl(Impl::instancePool.DemandCreate(device))
{
}


DGSLEffectFactory::DGSLEffectFactory(DGSLEffectFactory&&) noexcept = default;
DGSLEffectFactory& DGSLEffectFactory::operator= (DGSLEffectFactory&&) noexcept = default;
DGSLEffectFactory::~DGSLEffectFactory() = default;


// IEffectFactory methods
_Use_decl_annotations_
std::shared_ptr<IEffect> DGSLEffectFactory::CreateEffect(const EffectInfo& info, ID3D11DeviceContext* deviceContext)
{
    return pImpl->CreateEffect(this, info, deviceContext);
}

_Use_decl_annotations_
void DGSLEffectFactory::CreateTexture(const wchar_t* name, ID3D11DeviceContext* deviceContext, ID3D11ShaderResourceView** textureView)
{
    return pImpl->CreateTexture(name, deviceContext, textureView);
}


// DGSL methods.
_Use_decl_annotations_
std::shared_ptr<IEffect> DGSLEffectFactory::CreateDGSLEffect(const DGSLEffectInfo& info, ID3D11DeviceContext* deviceContext)
{
    return pImpl->CreateDGSLEffect(this, info, deviceContext);
}


_Use_decl_annotations_
void DGSLEffectFactory::CreatePixelShader(const wchar_t* shader, ID3D11PixelShader** pixelShader)
{
    pImpl->CreatePixelShader(shader, pixelShader);
}


// Settings
void DGSLEffectFactory::ReleaseCache()
{
    pImpl->ReleaseCache();
}

void DGSLEffectFactory::SetSharing(bool enabled) noexcept
{
    pImpl->SetSharing(enabled);
}

void DGSLEffectFactory::EnableForceSRGB(bool forceSRGB) noexcept
{
    pImpl->EnableForceSRGB(forceSRGB);
}

void DGSLEffectFactory::SetDirectory(_In_opt_z_ const wchar_t* path) noexcept
{
    if (path && *path != 0)
    {
        wcscpy_s(pImpl->mPath, path);
        size_t len = wcsnlen(pImpl->mPath, MAX_PATH);
        if (len > 0 && len < (MAX_PATH - 1))
        {
            // Ensure it has a trailing slash
            if (pImpl->mPath[len - 1] != L'\\')
            {
                pImpl->mPath[len] = L'\\';
                pImpl->mPath[len + 1] = 0;
            }
        }
    }
    else
        *pImpl->mPath = 0;
}

ID3D11Device* DGSLEffectFactory::GetDevice() const noexcept
{
    return pImpl->mDevice.Get();
}
