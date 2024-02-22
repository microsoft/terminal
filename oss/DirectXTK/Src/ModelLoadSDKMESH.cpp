//--------------------------------------------------------------------------------------
// File: ModelLoadSDKMESH.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "Model.h"
#include "DirectXHelpers.h"
#include "Effects.h"
#include "VertexTypes.h"
#include "BinaryReader.h"
#include "PlatformHelpers.h"
#include "SDKMesh.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace
{
    enum : unsigned int
    {
        PER_VERTEX_COLOR = 0x1,
        SKINNING = 0x2,
        DUAL_TEXTURE = 0x4,
        NORMAL_MAPS = 0x8,
        BIASED_VERTEX_NORMALS = 0x10,
        USES_OBSOLETE_DEC3N = 0x20,
    };

    struct MaterialRecordSDKMESH
    {
        std::shared_ptr<IEffect> effect;
        bool alpha;

        MaterialRecordSDKMESH() noexcept : alpha(false) {}
    };

    inline XMFLOAT3 GetMaterialColor(float r, float g, float b, bool srgb)
    {
        if (srgb)
        {
            XMVECTOR v = XMVectorSet(r, g, b, 1.f);
            v = XMColorSRGBToRGB(v);

            XMFLOAT3 result;
            XMStoreFloat3(&result, v);
            return result;
        }
        else
        {
            return XMFLOAT3(r, g, b);
        }
    }

    template<size_t sizeOfBuffer>
    inline void ASCIIToWChar(wchar_t(&buffer)[sizeOfBuffer], const char *ascii)
    {
    #ifdef _WIN32
        MultiByteToWideChar(CP_UTF8, 0, ascii, -1, buffer, sizeOfBuffer);
    #else
        mbtowc(nullptr, nullptr, 0);
        mbtowc(buffer, ascii, sizeOfBuffer);
    #endif
    }

    void LoadMaterial(const DXUT::SDKMESH_MATERIAL& mh,
        unsigned int flags,
        IEffectFactory& fxFactory,
        MaterialRecordSDKMESH& m,
        bool srgb)
    {
        wchar_t matName[DXUT::MAX_MATERIAL_NAME] = {};
        ASCIIToWChar(matName, mh.Name);

        wchar_t diffuseName[DXUT::MAX_TEXTURE_NAME] = {};
        ASCIIToWChar(diffuseName, mh.DiffuseTexture);

        wchar_t specularName[DXUT::MAX_TEXTURE_NAME] = {};
        ASCIIToWChar(specularName, mh.SpecularTexture);

        wchar_t normalName[DXUT::MAX_TEXTURE_NAME] = {};
        ASCIIToWChar(normalName, mh.NormalTexture);

        if (flags & DUAL_TEXTURE && !mh.SpecularTexture[0])
        {
            DebugTrace("WARNING: Material '%s' has multiple texture coords but not multiple textures\n", mh.Name);
            flags &= ~static_cast<unsigned int>(DUAL_TEXTURE);
        }

        if (mh.NormalTexture[0])
        {
            flags |= NORMAL_MAPS;
        }

        EffectFactory::EffectInfo info;
        info.name = matName;
        info.perVertexColor = (flags & PER_VERTEX_COLOR) != 0;
        info.enableSkinning = (flags & SKINNING) != 0;
        info.enableDualTexture = (flags & DUAL_TEXTURE) != 0;
        info.enableNormalMaps = (flags & NORMAL_MAPS) != 0;
        info.biasedVertexNormals = (flags & BIASED_VERTEX_NORMALS) != 0;

        if (mh.Ambient.x == 0 && mh.Ambient.y == 0 && mh.Ambient.z == 0 && mh.Ambient.w == 0
            && mh.Diffuse.x == 0 && mh.Diffuse.y == 0 && mh.Diffuse.z == 0 && mh.Diffuse.w == 0)
        {
            // SDKMESH material color block is uninitalized; assume defaults
            info.diffuseColor = XMFLOAT3(1.f, 1.f, 1.f);
            info.alpha = 1.f;
        }
        else
        {
            info.ambientColor = GetMaterialColor(mh.Ambient.x, mh.Ambient.y, mh.Ambient.z, srgb);
            info.diffuseColor = GetMaterialColor(mh.Diffuse.x, mh.Diffuse.y, mh.Diffuse.z, srgb);
            info.emissiveColor = GetMaterialColor(mh.Emissive.x, mh.Emissive.y, mh.Emissive.z, srgb);

            if (mh.Diffuse.w != 1.f && mh.Diffuse.w != 0.f)
            {
                info.alpha = mh.Diffuse.w;
            }
            else
                info.alpha = 1.f;

            if (mh.Power > 0)
            {
                info.specularPower = mh.Power;
                info.specularColor = XMFLOAT3(mh.Specular.x, mh.Specular.y, mh.Specular.z);
            }
        }

        info.diffuseTexture = diffuseName;
        info.specularTexture = specularName;
        info.normalTexture = normalName;

        m.effect = fxFactory.CreateEffect(info, nullptr);
        m.alpha = (info.alpha < 1.f);
    }

    void LoadMaterial(const DXUT::SDKMESH_MATERIAL_V2& mh,
        unsigned int flags,
        IEffectFactory& fxFactory,
        MaterialRecordSDKMESH& m)
    {
        wchar_t matName[DXUT::MAX_MATERIAL_NAME] = {};
        ASCIIToWChar(matName, mh.Name);

        wchar_t albedoTexture[DXUT::MAX_TEXTURE_NAME] = {};
        ASCIIToWChar(albedoTexture, mh.AlbedoTexture);

        wchar_t normalName[DXUT::MAX_TEXTURE_NAME] = {};
        ASCIIToWChar(normalName, mh.NormalTexture);

        wchar_t rmaName[DXUT::MAX_TEXTURE_NAME] = {};
        ASCIIToWChar(rmaName, mh.RMATexture);

        wchar_t emissiveName[DXUT::MAX_TEXTURE_NAME] = {};
        ASCIIToWChar(emissiveName, mh.EmissiveTexture);

        EffectFactory::EffectInfo info;
        info.name = matName;
        info.perVertexColor = false;
        info.enableSkinning = (flags & SKINNING) != 0;
        info.enableDualTexture = false;
        info.enableNormalMaps = true;
        info.biasedVertexNormals = (flags & BIASED_VERTEX_NORMALS) != 0;
        info.alpha = (mh.Alpha == 0.f) ? 1.f : mh.Alpha;

        info.diffuseTexture = albedoTexture;
        info.specularTexture = rmaName;
        info.normalTexture = normalName;
        info.emissiveTexture = emissiveName;

        m.effect = fxFactory.CreateEffect(info, nullptr);
        m.alpha = (info.alpha < 1.f);
    }


    //--------------------------------------------------------------------------------------
    // Direct3D 9 Vertex Declaration to Direct3D 11 Input Layout mapping


#ifndef __MINGW32__
    static_assert(D3D11_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT >= 32, "SDKMESH supports decls up to 32 entries");
#endif

    unsigned int GetInputLayoutDesc(
        _In_reads_(32) const DXUT::D3DVERTEXELEMENT9 decl[],
        ModelMeshPart::InputLayoutCollection& inputDesc)
    {
        static const D3D11_INPUT_ELEMENT_DESC s_elements[] =
        {
            { "SV_Position",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",        0, DXGI_FORMAT_B8G8R8A8_UNORM,  0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TANGENT",      0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "BINORMAL",     0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT,   0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "BLENDWEIGHT",  0, DXGI_FORMAT_R8G8B8A8_UNORM,  0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        using namespace DXUT;

        uint32_t offset = 0;
        uint32_t texcoords = 0;
        unsigned int flags = 0;

        bool posfound = false;

        for (uint32_t index = 0; index < DXUT::MAX_VERTEX_ELEMENTS; ++index)
        {
            if (decl[index].Usage == 0xFF)
                break;

            if (decl[index].Type == D3DDECLTYPE_UNUSED)
                break;

            if (decl[index].Offset != offset)
                break;

            if (decl[index].Usage == D3DDECLUSAGE_POSITION)
            {
                if (decl[index].Type == D3DDECLTYPE_FLOAT3)
                {
                    inputDesc.push_back(s_elements[0]);
                    offset += 12;
                    posfound = true;
                }
                else
                    break;
            }
            else if (decl[index].Usage == D3DDECLUSAGE_NORMAL
                || decl[index].Usage == D3DDECLUSAGE_TANGENT
                || decl[index].Usage == D3DDECLUSAGE_BINORMAL)
            {
                size_t base = 1;
                if (decl[index].Usage == D3DDECLUSAGE_TANGENT)
                    base = 3;
                else if (decl[index].Usage == D3DDECLUSAGE_BINORMAL)
                    base = 4;

                D3D11_INPUT_ELEMENT_DESC desc = s_elements[base];

                bool unk = false;
                switch (decl[index].Type)
                {
                case D3DDECLTYPE_FLOAT3:                 assert(desc.Format == DXGI_FORMAT_R32G32B32_FLOAT); offset += 12; break;
                case D3DDECLTYPE_UBYTE4N:                desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; flags |= BIASED_VERTEX_NORMALS; offset += 4; break;
                case D3DDECLTYPE_SHORT4N:                desc.Format = DXGI_FORMAT_R16G16B16A16_SNORM; offset += 8; break;
                case D3DDECLTYPE_FLOAT16_4:              desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; offset += 8; break;
                case D3DDECLTYPE_DXGI_R10G10B10A2_UNORM: desc.Format = DXGI_FORMAT_R10G10B10A2_UNORM; flags |= BIASED_VERTEX_NORMALS; offset += 4; break;
                case D3DDECLTYPE_DXGI_R11G11B10_FLOAT:   desc.Format = DXGI_FORMAT_R11G11B10_FLOAT; flags |= BIASED_VERTEX_NORMALS; offset += 4; break;
                case D3DDECLTYPE_DXGI_R8G8B8A8_SNORM:    desc.Format = DXGI_FORMAT_R8G8B8A8_SNORM; offset += 4; break;

                #if defined(_XBOX_ONE) && defined(_TITLE)
                case D3DDECLTYPE_DEC3N:                  desc.Format = DXGI_FORMAT_R10G10B10_SNORM_A2_UNORM; offset += 4; break;
                case (32 + DXGI_FORMAT_R10G10B10_SNORM_A2_UNORM): desc.Format = DXGI_FORMAT_R10G10B10_SNORM_A2_UNORM; offset += 4; break;
                #else
                case D3DDECLTYPE_DEC3N:                  desc.Format = DXGI_FORMAT_R10G10B10A2_UNORM; flags |= USES_OBSOLETE_DEC3N; offset += 4; break;
                #endif

                default:
                    unk = true;
                    break;
                }

                if (unk)
                    break;

                inputDesc.push_back(desc);
            }
            else if (decl[index].Usage == D3DDECLUSAGE_COLOR)
            {
                D3D11_INPUT_ELEMENT_DESC desc = s_elements[2];

                bool unk = false;
                switch (decl[index].Type)
                {
                case D3DDECLTYPE_FLOAT4:                 desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; offset += 16; break;
                case D3DDECLTYPE_D3DCOLOR:               assert(desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM); offset += 4; break;
                case D3DDECLTYPE_UBYTE4N:                desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; offset += 4; break;
                case D3DDECLTYPE_FLOAT16_4:              desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; offset += 8; break;
                case D3DDECLTYPE_DXGI_R10G10B10A2_UNORM: desc.Format = DXGI_FORMAT_R10G10B10A2_UNORM; offset += 4; break;
                case D3DDECLTYPE_DXGI_R11G11B10_FLOAT:   desc.Format = DXGI_FORMAT_R11G11B10_FLOAT; offset += 4; break;

                default:
                    unk = true;
                    break;
                }

                if (unk)
                    break;

                flags |= PER_VERTEX_COLOR;

                inputDesc.push_back(desc);
            }
            else if (decl[index].Usage == D3DDECLUSAGE_TEXCOORD)
            {
                D3D11_INPUT_ELEMENT_DESC desc = s_elements[5];
                desc.SemanticIndex = decl[index].UsageIndex;

                bool unk = false;
                switch (decl[index].Type)
                {
                case D3DDECLTYPE_FLOAT1:    desc.Format = DXGI_FORMAT_R32_FLOAT; offset += 4; break;
                case D3DDECLTYPE_FLOAT2:    assert(desc.Format == DXGI_FORMAT_R32G32_FLOAT); offset += 8; break;
                case D3DDECLTYPE_FLOAT3:    desc.Format = DXGI_FORMAT_R32G32B32_FLOAT; offset += 12; break;
                case D3DDECLTYPE_FLOAT4:    desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; offset += 16; break;
                case D3DDECLTYPE_FLOAT16_2: desc.Format = DXGI_FORMAT_R16G16_FLOAT; offset += 4; break;
                case D3DDECLTYPE_FLOAT16_4: desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; offset += 8; break;

                default:
                    unk = true;
                    break;
                }

                if (unk)
                    break;

                ++texcoords;

                inputDesc.push_back(desc);
            }
            else if (decl[index].Usage == D3DDECLUSAGE_BLENDINDICES)
            {
                if (decl[index].Type == D3DDECLTYPE_UBYTE4)
                {
                    flags |= SKINNING;
                    inputDesc.push_back(s_elements[6]);
                    offset += 4;
                }
                else
                    break;
            }
            else if (decl[index].Usage == D3DDECLUSAGE_BLENDWEIGHT)
            {
                if (decl[index].Type == D3DDECLTYPE_UBYTE4N)
                {
                    flags |= SKINNING;
                    inputDesc.push_back(s_elements[7]);
                    offset += 4;
                }
                else
                    break;
            }
            else
                break;
        }

        if (!posfound)
            throw std::runtime_error("SV_Position is required");

        if (texcoords == 2)
        {
            flags |= DUAL_TEXTURE;
        }

        return flags;
    }
}


//======================================================================================
// Model Loader
//======================================================================================

_Use_decl_annotations_
std::unique_ptr<Model> DirectX::Model::CreateFromSDKMESH(
    ID3D11Device* d3dDevice,
    const uint8_t* meshData,
    size_t idataSize,
    IEffectFactory& fxFactory,
    ModelLoaderFlags flags)
{
    if (!d3dDevice || !meshData)
        throw std::invalid_argument("Device and meshData cannot be null");

    const uint64_t dataSize = idataSize;

    // File Headers
    if (dataSize < sizeof(DXUT::SDKMESH_HEADER))
        throw std::runtime_error("End of file");
    auto header = reinterpret_cast<const DXUT::SDKMESH_HEADER*>(meshData);

    const size_t headerSize = sizeof(DXUT::SDKMESH_HEADER)
        + header->NumVertexBuffers * sizeof(DXUT::SDKMESH_VERTEX_BUFFER_HEADER)
        + header->NumIndexBuffers * sizeof(DXUT::SDKMESH_INDEX_BUFFER_HEADER);
    if (header->HeaderSize != headerSize)
        throw std::runtime_error("Not a valid SDKMESH file");

    if (dataSize < header->HeaderSize)
        throw std::runtime_error("End of file");

    if (header->Version != DXUT::SDKMESH_FILE_VERSION && header->Version != DXUT::SDKMESH_FILE_VERSION_V2)
        throw std::runtime_error("Not a supported SDKMESH version");

    if (header->IsBigEndian)
        throw std::runtime_error("Loading BigEndian SDKMESH files not supported");

    if (!header->NumMeshes)
        throw std::runtime_error("No meshes found");

    if (!header->NumVertexBuffers)
        throw std::runtime_error("No vertex buffers found");

    if (!header->NumIndexBuffers)
        throw std::runtime_error("No index buffers found");

    if (!header->NumTotalSubsets)
        throw std::runtime_error("No subsets found");

    if (!header->NumMaterials)
        throw std::runtime_error("No materials found");

    // Sub-headers
    if (dataSize < header->VertexStreamHeadersOffset
        || (dataSize < (header->VertexStreamHeadersOffset + uint64_t(header->NumVertexBuffers) * sizeof(DXUT::SDKMESH_VERTEX_BUFFER_HEADER))))
        throw std::runtime_error("End of file");
    auto vbArray = reinterpret_cast<const DXUT::SDKMESH_VERTEX_BUFFER_HEADER*>(meshData + header->VertexStreamHeadersOffset);

    if (dataSize < header->IndexStreamHeadersOffset
        || (dataSize < (header->IndexStreamHeadersOffset + uint64_t(header->NumIndexBuffers) * sizeof(DXUT::SDKMESH_INDEX_BUFFER_HEADER))))
        throw std::runtime_error("End of file");
    auto ibArray = reinterpret_cast<const DXUT::SDKMESH_INDEX_BUFFER_HEADER*>(meshData + header->IndexStreamHeadersOffset);

    if (dataSize < header->MeshDataOffset
        || (dataSize < (header->MeshDataOffset + uint64_t(header->NumMeshes) * sizeof(DXUT::SDKMESH_MESH))))
        throw std::runtime_error("End of file");
    auto meshArray = reinterpret_cast<const DXUT::SDKMESH_MESH*>(meshData + header->MeshDataOffset);

    if (dataSize < header->SubsetDataOffset
        || (dataSize < (header->SubsetDataOffset + uint64_t(header->NumTotalSubsets) * sizeof(DXUT::SDKMESH_SUBSET))))
        throw std::runtime_error("End of file");
    auto subsetArray = reinterpret_cast<const DXUT::SDKMESH_SUBSET*>(meshData + header->SubsetDataOffset);

    const DXUT::SDKMESH_FRAME* frameArray = nullptr;
    if (header->NumFrames > 0)
    {
        if (dataSize < header->FrameDataOffset
            || (dataSize < (header->FrameDataOffset + uint64_t(header->NumFrames) * sizeof(DXUT::SDKMESH_FRAME))))
            throw std::runtime_error("End of file");

        if (flags & ModelLoader_IncludeBones)
        {
            frameArray = reinterpret_cast<const DXUT::SDKMESH_FRAME*>(meshData + header->FrameDataOffset);
        }
    }

    if (dataSize < header->MaterialDataOffset
        || (dataSize < (header->MaterialDataOffset + uint64_t(header->NumMaterials) * sizeof(DXUT::SDKMESH_MATERIAL))))
        throw std::runtime_error("End of file");

    const DXUT::SDKMESH_MATERIAL* materialArray = nullptr;
    const DXUT::SDKMESH_MATERIAL_V2* materialArray_v2 = nullptr;
    if (header->Version == DXUT::SDKMESH_FILE_VERSION_V2)
    {
        materialArray_v2 = reinterpret_cast<const DXUT::SDKMESH_MATERIAL_V2*>(meshData + header->MaterialDataOffset);
    }
    else
    {
        materialArray = reinterpret_cast<const DXUT::SDKMESH_MATERIAL*>(meshData + header->MaterialDataOffset);
    }

    // Buffer data
    const uint64_t bufferDataOffset = header->HeaderSize + header->NonBufferDataSize;
    if ((dataSize < bufferDataOffset)
        || (dataSize < bufferDataOffset + header->BufferDataSize))
        throw std::runtime_error("End of file");
    const uint8_t* bufferData = meshData + bufferDataOffset;

    // Create vertex buffers
    std::vector<ComPtr<ID3D11Buffer>> vbs;
    vbs.resize(header->NumVertexBuffers);

    std::vector<std::shared_ptr<ModelMeshPart::InputLayoutCollection>> vbDecls;
    vbDecls.resize(header->NumVertexBuffers);

    std::vector<unsigned int> materialFlags;
    materialFlags.resize(header->NumVertexBuffers);

    bool dec3nwarning = false;
    for (size_t j = 0; j < header->NumVertexBuffers; ++j)
    {
        auto& vh = vbArray[j];

        if (vh.SizeBytes > UINT32_MAX)
            throw std::runtime_error("VB too large");

        if (!(flags & ModelLoader_AllowLargeModels))
        {
            if (vh.SizeBytes > (D3D11_REQ_RESOURCE_SIZE_IN_MEGABYTES_EXPRESSION_A_TERM * 1024u * 1024u))
                throw std::runtime_error("VB too large for DirectX 11");
        }

        if (dataSize < vh.DataOffset
            || (dataSize < vh.DataOffset + vh.SizeBytes))
            throw std::runtime_error("End of file");

        vbDecls[j] = std::make_shared<ModelMeshPart::InputLayoutCollection>();
        unsigned int ilflags = GetInputLayoutDesc(vh.Decl, *vbDecls[j].get());

        if (flags & ModelLoader_DisableSkinning)
        {
            ilflags &= ~static_cast<unsigned int>(SKINNING);
        }

        if (ilflags & SKINNING)
        {
            ilflags &= ~static_cast<unsigned int>(DUAL_TEXTURE);
        }
        if (ilflags & USES_OBSOLETE_DEC3N)
        {
            dec3nwarning = true;
        }

        materialFlags[j] = ilflags;

        auto verts = bufferData + (vh.DataOffset - bufferDataOffset);

        D3D11_BUFFER_DESC desc = {};
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.ByteWidth = static_cast<UINT>(vh.SizeBytes);
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA initData = { verts, 0, 0 };

        ThrowIfFailed(
            d3dDevice->CreateBuffer(&desc, &initData, &vbs[j])
        );

        SetDebugObjectName(vbs[j].Get(), "ModelSDKMESH");
    }

    if (dec3nwarning)
    {
        DebugTrace("WARNING: Vertex declaration uses legacy Direct3D 9 D3DDECLTYPE_DEC3N which has no DXGI equivalent\n"
            "         (treating as DXGI_FORMAT_R10G10B10A2_UNORM which is not a signed format)\n");
    }

    // Create index buffers
    std::vector<ComPtr<ID3D11Buffer>> ibs;
    ibs.resize(header->NumIndexBuffers);

    for (size_t j = 0; j < header->NumIndexBuffers; ++j)
    {
        auto& ih = ibArray[j];

        if (ih.SizeBytes > UINT32_MAX)
            throw std::runtime_error("IB too large");

        if (!(flags & ModelLoader_AllowLargeModels))
        {
            if (ih.SizeBytes > (D3D11_REQ_RESOURCE_SIZE_IN_MEGABYTES_EXPRESSION_A_TERM * 1024u * 1024u))
                throw std::runtime_error("IB too large for DirectX 11");
        }

        if (dataSize < ih.DataOffset
            || (dataSize < ih.DataOffset + ih.SizeBytes))
            throw std::runtime_error("End of file");

        if (ih.IndexType != DXUT::IT_16BIT && ih.IndexType != DXUT::IT_32BIT)
            throw std::runtime_error("Invalid index buffer type found");

        auto indices = bufferData + (ih.DataOffset - bufferDataOffset);

        D3D11_BUFFER_DESC desc = {};
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.ByteWidth = static_cast<UINT>(ih.SizeBytes);
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA initData = { indices, 0, 0 };

        ThrowIfFailed(
            d3dDevice->CreateBuffer(&desc, &initData, &ibs[j])
        );

        SetDebugObjectName(ibs[j].Get(), "ModelSDKMESH");
    }

    // Create meshes
    std::vector<MaterialRecordSDKMESH> materials;
    materials.resize(header->NumMaterials);

    auto model = std::make_unique<Model>();
    model->meshes.reserve(header->NumMeshes);

    for (size_t meshIndex = 0; meshIndex < header->NumMeshes; ++meshIndex)
    {
        auto& mh = meshArray[meshIndex];

        if (!mh.NumSubsets
            || !mh.NumVertexBuffers
            || mh.IndexBuffer >= header->NumIndexBuffers
            || mh.VertexBuffers[0] >= header->NumVertexBuffers)
            throw std::out_of_range("Invalid mesh found");

        // mh.NumVertexBuffers is sometimes not what you'd expect, so we skip validating it

        if (dataSize < mh.SubsetOffset
            || (dataSize < mh.SubsetOffset + uint64_t(mh.NumSubsets) * sizeof(uint32_t)))
            throw std::runtime_error("End of file");

        auto subsets = reinterpret_cast<const uint32_t*>(meshData + mh.SubsetOffset);

        const uint32_t* influences = nullptr;
        if (mh.NumFrameInfluences > 0)
        {
            if (dataSize < mh.FrameInfluenceOffset
                || (dataSize < mh.FrameInfluenceOffset + uint64_t(mh.NumFrameInfluences) * sizeof(uint32_t)))
                throw std::runtime_error("End of file");

            if (flags & ModelLoader_IncludeBones)
            {
                influences = reinterpret_cast<const uint32_t*>(meshData + mh.FrameInfluenceOffset);
            }
        }

        auto mesh = std::make_shared<ModelMesh>();
        wchar_t meshName[DXUT::MAX_MESH_NAME] = {};
        ASCIIToWChar(meshName, mh.Name);
        mesh->name = meshName;
        mesh->ccw = (flags & ModelLoader_CounterClockwise) != 0;
        mesh->pmalpha = (flags & ModelLoader_PremultipledAlpha) != 0;

        // Extents
        mesh->boundingBox.Center = mh.BoundingBoxCenter;
        mesh->boundingBox.Extents = mh.BoundingBoxExtents;
        BoundingSphere::CreateFromBoundingBox(mesh->boundingSphere, mesh->boundingBox);

        if (influences)
        {
            mesh->boneInfluences.resize(mh.NumFrameInfluences);
            memcpy(mesh->boneInfluences.data(), influences, sizeof(uint32_t) * mh.NumFrameInfluences);
        }

        // Create subsets
        mesh->meshParts.reserve(mh.NumSubsets);
        for (size_t j = 0; j < mh.NumSubsets; ++j)
        {
            auto const sIndex = subsets[j];
            if (sIndex >= header->NumTotalSubsets)
                throw std::out_of_range("Invalid mesh found");

            auto& subset = subsetArray[sIndex];

            D3D11_PRIMITIVE_TOPOLOGY primType;
            switch (subset.PrimitiveType)
            {
            case DXUT::PT_TRIANGLE_LIST:        primType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;       break;
            case DXUT::PT_TRIANGLE_STRIP:       primType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;      break;
            case DXUT::PT_LINE_LIST:            primType = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;           break;
            case DXUT::PT_LINE_STRIP:           primType = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;          break;
            case DXUT::PT_POINT_LIST:           primType = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;          break;
            case DXUT::PT_TRIANGLE_LIST_ADJ:    primType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;   break;
            case DXUT::PT_TRIANGLE_STRIP_ADJ:   primType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;  break;
            case DXUT::PT_LINE_LIST_ADJ:        primType = D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;       break;
            case DXUT::PT_LINE_STRIP_ADJ:       primType = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ;      break;

            case DXUT::PT_QUAD_PATCH_LIST:
            case DXUT::PT_TRIANGLE_PATCH_LIST:
                throw std::runtime_error("Direct3D9 era tessellation not supported");

            default:
                throw std::runtime_error("Unknown primitive type");
            }

            if (subset.MaterialID >= header->NumMaterials)
                throw std::out_of_range("Invalid mesh found");

            auto& mat = materials[subset.MaterialID];

            if (!mat.effect)
            {
                const size_t vi = mh.VertexBuffers[0];

                if (materialArray_v2)
                {
                    LoadMaterial(
                        materialArray_v2[subset.MaterialID],
                        materialFlags[vi],
                        fxFactory,
                        mat);
                }
                else
                {
                    LoadMaterial(
                        materialArray[subset.MaterialID],
                        materialFlags[vi],
                        fxFactory,
                        mat,
                        (flags & ModelLoader_MaterialColorsSRGB) != 0);
                }
            }

            ComPtr<ID3D11InputLayout> il;
            ThrowIfFailed(
                CreateInputLayoutFromEffect(d3dDevice, mat.effect.get(),
                    vbDecls[mh.VertexBuffers[0]]->data(), vbDecls[mh.VertexBuffers[0]]->size(), il.GetAddressOf())
            );

            SetDebugObjectName(il.Get(), "ModelSDKMESH");

            auto part = new ModelMeshPart();
            part->isAlpha = mat.alpha;

            part->indexCount = static_cast<uint32_t>(subset.IndexCount);
            part->startIndex = static_cast<uint32_t>(subset.IndexStart);
            part->vertexOffset = static_cast<int32_t>(subset.VertexStart);
            part->vertexStride = static_cast<uint32_t>(vbArray[mh.VertexBuffers[0]].StrideBytes);
            part->indexFormat = (ibArray[mh.IndexBuffer].IndexType == DXUT::IT_32BIT) ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
            part->primitiveType = primType;
            part->inputLayout = il;
            part->indexBuffer = ibs[mh.IndexBuffer];
            part->vertexBuffer = vbs[mh.VertexBuffers[0]];
            part->effect = mat.effect;
            part->vbDecl = vbDecls[mh.VertexBuffers[0]];

            mesh->meshParts.emplace_back(part);
        }

        model->meshes.emplace_back(mesh);
    }

    // Load model bones (if present and requested)
    if (frameArray)
    {
        static_assert(DXUT::INVALID_FRAME == ModelBone::c_Invalid, "ModelBone invalid type mismatch");

        ModelBone::Collection bones;
        bones.reserve(header->NumFrames);
        auto transforms = ModelBone::MakeArray(header->NumFrames);

        for (uint32_t j = 0; j < header->NumFrames; ++j)
        {
            ModelBone bone(
                frameArray[j].ParentFrame,
                frameArray[j].ChildFrame,
                frameArray[j].SiblingFrame);

            wchar_t boneName[DXUT::MAX_FRAME_NAME] = {};
            ASCIIToWChar(boneName, frameArray[j].Name);
            bone.name = boneName;
            bones.emplace_back(bone);

            transforms[j] = XMLoadFloat4x4(&frameArray[j].Matrix);

            const uint32_t index = frameArray[j].Mesh;
            if (index != DXUT::INVALID_MESH)
            {
                if (index >= model->meshes.size())
                {
                    throw std::out_of_range("Invalid mesh index found in frame data");
                }

                if (model->meshes[index]->boneIndex == ModelBone::c_Invalid)
                {
                    // Bind the first bone that links to a given mesh
                    model->meshes[index]->boneIndex = j;
                }
            }
        }

        std::swap(model->bones, bones);

        // Compute inverse bind pose matrices for the model
        auto bindPose = ModelBone::MakeArray(header->NumFrames);
        model->CopyAbsoluteBoneTransforms(header->NumFrames, transforms.get(), bindPose.get());

        auto invBoneTransforms = ModelBone::MakeArray(header->NumFrames);
        for (size_t j = 0; j < header->NumFrames; ++j)
        {
            invBoneTransforms[j] = XMMatrixInverse(nullptr, bindPose[j]);
        }

        std::swap(model->boneMatrices, transforms);
        std::swap(model->invBindPoseMatrices, invBoneTransforms);
    }

    return model;
}


//--------------------------------------------------------------------------------------
_Use_decl_annotations_
std::unique_ptr<Model> DirectX::Model::CreateFromSDKMESH(
    ID3D11Device* device,
    const wchar_t* szFileName,
    IEffectFactory& fxFactory,
    ModelLoaderFlags flags)
{
    size_t dataSize = 0;
    std::unique_ptr<uint8_t[]> data;
    HRESULT hr = BinaryReader::ReadEntireFile(szFileName, data, &dataSize);
    if (FAILED(hr))
    {
        DebugTrace("ERROR: CreateFromSDKMESH failed (%08X) loading '%ls'\n",
            static_cast<unsigned int>(hr), szFileName);
        throw std::runtime_error("CreateFromSDKMESH");
    }

    auto model = CreateFromSDKMESH(device, data.get(), dataSize, fxFactory, flags);

    model->name = szFileName;

    return model;
}
