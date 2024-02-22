//--------------------------------------------------------------------------------------
// File: PBREffectFactory.cpp
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

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace
{
    template<typename T>
    void SetPBRProperties(
        _In_ T* effect,
        const EffectFactory::EffectInfo& info,
        _In_ IEffectFactory* factory,
        _In_opt_ ID3D11DeviceContext* deviceContext)
    {
        // We don't use EnableDefaultLighting generally for PBR as it uses Image-Based Lighting instead.

        effect->SetAlpha(info.alpha);

        if (info.diffuseTexture && *info.diffuseTexture)
        {
            // Textured PBR material
            ComPtr<ID3D11ShaderResourceView> albedoSrv;
            factory->CreateTexture(info.diffuseTexture, deviceContext, albedoSrv.GetAddressOf());

            ComPtr<ID3D11ShaderResourceView> normalSrv;
            if (info.normalTexture && *info.normalTexture)
            {
                factory->CreateTexture(info.normalTexture, deviceContext, normalSrv.GetAddressOf());
            }

            ComPtr<ID3D11ShaderResourceView> rmaSrv;
            if (info.specularTexture && *info.specularTexture)
            {
                // We use the specular texture for the roughness/metalness/ambient-occlusion texture
                factory->CreateTexture(info.specularTexture, deviceContext, rmaSrv.GetAddressOf());
            }

            effect->SetSurfaceTextures(albedoSrv.Get(), normalSrv.Get(), rmaSrv.Get());

            if (info.emissiveTexture && *info.emissiveTexture)
            {
                ComPtr<ID3D11ShaderResourceView> srv;
                factory->CreateTexture(info.emissiveTexture, deviceContext, srv.GetAddressOf());

                effect->SetEmissiveTexture(srv.Get());
            }
        }
        else
        {
            // Untextured material (for PBR this still requires texture coordinates)
            const XMVECTOR color = XMLoadFloat3(&info.diffuseColor);
            effect->SetConstantAlbedo(color);

            if (info.specularColor.x != 0 || info.specularColor.y != 0 || info.specularColor.z != 0)
            {
                // Derived from specularPower = 2 / roughness ^ 4 - 2
                // http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html

                const float roughness = powf(2.f / (info.specularPower + 2.f), 1.f / 4.f);
                effect->SetConstantRoughness(roughness);
            }

            // info.ambientColor, info.specularColor, and info.emissiveColor are unused by PBR.
        }

        if (info.biasedVertexNormals)
        {
            effect->SetBiasedVertexNormals(true);
        }
    }
}

// Internal PBREffectFactory implementation class. Only one of these helpers is allocated
// per D3D device, even if there are multiple public facing PBREffectFactory instances.
class PBREffectFactory::Impl
{
public:
    explicit Impl(_In_ ID3D11Device* device)
        : mPath{},
        mDevice(device),
        mSharing(true),
        mForceSRGB(false)
    {}

    std::shared_ptr<IEffect> CreateEffect(
        _In_ IEffectFactory* factory,
        _In_ const IEffectFactory::EffectInfo& info,
        _In_opt_ ID3D11DeviceContext* deviceContext);

    void CreateTexture(_In_z_ const wchar_t* texture,
        _In_opt_ ID3D11DeviceContext* deviceContext,
        _Outptr_ ID3D11ShaderResourceView** textureView);

    void ReleaseCache();
    void SetSharing(bool enabled) noexcept { mSharing = enabled; }
    void EnableForceSRGB(bool forceSRGB) noexcept { mForceSRGB = forceSRGB; }

    static SharedResourcePool<ID3D11Device*, Impl> instancePool;

    wchar_t mPath[MAX_PATH];

    ComPtr<ID3D11Device> mDevice;

private:
    using EffectCache = std::map< std::wstring, std::shared_ptr<IEffect> >;
    using TextureCache = std::map< std::wstring, ComPtr<ID3D11ShaderResourceView> >;

    EffectCache  mEffectCache;
    EffectCache  mEffectCacheSkinning;
    TextureCache mTextureCache;

    bool mSharing;
    bool mForceSRGB;

    std::mutex mutex;
};


// Global instance pool.
SharedResourcePool<ID3D11Device*, PBREffectFactory::Impl> PBREffectFactory::Impl::instancePool;


_Use_decl_annotations_
std::shared_ptr<IEffect> PBREffectFactory::Impl::CreateEffect(
    IEffectFactory* factory,
    const IEffectFactory::EffectInfo& info,
    ID3D11DeviceContext* deviceContext)
{
    // info.perVertexColor and info.enableDualTexture are ignored by PBREffectFactory

    if (info.enableSkinning)
    {
        // SkinnedPBREffect
        if (mSharing && info.name && *info.name)
        {
            auto it = mEffectCacheSkinning.find(info.name);
            if (mSharing && it != mEffectCacheSkinning.end())
            {
                return it->second;
            }
        }

        auto effect = std::make_shared<SkinnedPBREffect>(mDevice.Get());

        SetPBRProperties(effect.get(), info, factory, deviceContext);

        if (mSharing && info.name && *info.name)
        {
            std::lock_guard<std::mutex> lock(mutex);
            EffectCache::value_type v(info.name, effect);
            mEffectCacheSkinning.insert(v);
        }

        return std::move(effect);
    }
    else
    {
        // PBREffect
        if (mSharing && info.name && *info.name)
        {
            auto it = mEffectCache.find(info.name);
            if (mSharing && it != mEffectCache.end())
            {
                return it->second;
            }
        }

        auto effect = std::make_shared<PBREffect>(mDevice.Get());

        SetPBRProperties(effect.get(), info, factory, deviceContext);

        if (mSharing && info.name && *info.name)
        {
            std::lock_guard<std::mutex> lock(mutex);
            EffectCache::value_type v(info.name, effect);
            mEffectCache.insert(v);
        }

        return std::move(effect);
    }
}

_Use_decl_annotations_
void PBREffectFactory::Impl::CreateTexture(
    const wchar_t* name,
    ID3D11DeviceContext* deviceContext,
    ID3D11ShaderResourceView** textureView)
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
                DebugTrace("ERROR: PBREffectFactory could not find texture file '%ls'\n", name);
                throw std::system_error(std::error_code(static_cast<int>(GetLastError()), std::system_category()), "PBREffectFactory::CreateTexture");
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
                throw std::runtime_error("PBREffectFactory::CreateDDSTextureFromFile");
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
                throw std::runtime_error("PBREffectFactory::CreateWICTextureFromFile");
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
                throw std::runtime_error("PBREffectFactory::CreateWICTextureFromFile");
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

void PBREffectFactory::Impl::ReleaseCache()
{
    std::lock_guard<std::mutex> lock(mutex);
    mEffectCache.clear();
    mEffectCacheSkinning.clear();
    mTextureCache.clear();
}



//--------------------------------------------------------------------------------------
// PBREffectFactory
//--------------------------------------------------------------------------------------

PBREffectFactory::PBREffectFactory(_In_ ID3D11Device* device)
    : pImpl(Impl::instancePool.DemandCreate(device))
{
}

PBREffectFactory::PBREffectFactory(PBREffectFactory&&) noexcept = default;
PBREffectFactory& PBREffectFactory::operator= (PBREffectFactory&&) noexcept = default;
PBREffectFactory::~PBREffectFactory() = default;


_Use_decl_annotations_
std::shared_ptr<IEffect> PBREffectFactory::CreateEffect(const EffectInfo& info, ID3D11DeviceContext* deviceContext)
{
    return pImpl->CreateEffect(this, info, deviceContext);
}

_Use_decl_annotations_
void PBREffectFactory::CreateTexture(const wchar_t* name, ID3D11DeviceContext* deviceContext, ID3D11ShaderResourceView** textureView)
{
    return pImpl->CreateTexture(name, deviceContext, textureView);
}

void PBREffectFactory::ReleaseCache()
{
    pImpl->ReleaseCache();
}

void PBREffectFactory::SetSharing(bool enabled) noexcept
{
    pImpl->SetSharing(enabled);
}

void PBREffectFactory::EnableForceSRGB(bool forceSRGB) noexcept
{
    pImpl->EnableForceSRGB(forceSRGB);
}

void PBREffectFactory::SetDirectory(_In_opt_z_ const wchar_t* path) noexcept
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

ID3D11Device* PBREffectFactory::GetDevice() const noexcept
{
    return pImpl->mDevice.Get();
}
