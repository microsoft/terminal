//--------------------------------------------------------------------------------------
// File: ModelLoadCMO.cpp
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

using namespace DirectX;
using Microsoft::WRL::ComPtr;


//--------------------------------------------------------------------------------------
// .CMO files are built by Visual Studio's MeshContentTask and an example renderer was
// provided in the VS Direct3D Starter Kit
// https://devblogs.microsoft.com/cppblog/developing-an-app-with-the-visual-studio-3d-starter-kit-part-1-of-3/
// https://devblogs.microsoft.com/cppblog/developing-an-app-with-the-visual-studio-3d-starter-kit-part-2-of-3/
// https://devblogs.microsoft.com/cppblog/developing-an-app-with-the-visual-studio-3d-starter-kit-part-3-of-3/
//--------------------------------------------------------------------------------------

namespace VSD3DStarter
{
    // .CMO files

    // UINT - Mesh count
    // { [Mesh count]
    //      UINT - Length of name
    //      wchar_t[] - Name of mesh (if length > 0)
    //      UINT - Material count
    //      { [Material count]
    //          UINT - Length of material name
    //          wchar_t[] - Name of material (if length > 0)
    //          Material structure
    //          UINT - Length of pixel shader name
    //          wchar_t[] - Name of pixel shader (if length > 0)
    //          { [8]
    //              UINT - Length of texture name
    //              wchar_t[] - Name of texture (if length > 0)
    //          }
    //      }
    //      BYTE - 1 if there is skeletal animation data present
    //      UINT - SubMesh count
    //      { [SubMesh count]
    //          SubMesh structure
    //      }
    //      UINT - IB Count
    //      { [IB Count]
    //          UINT - Number of USHORTs in IB
    //          USHORT[] - Array of indices
    //      }
    //      UINT - VB Count
    //      { [VB Count]
    //          UINT - Number of verts in VB
    //          Vertex[] - Array of vertices
    //      }
    //      UINT - Skinning VB Count
    //      { [Skinning VB Count]
    //          UINT - Number of verts in Skinning VB
    //          SkinningVertex[] - Array of skinning verts
    //      }
    //      MeshExtents structure
    //      [If skeleton animation data is not present, file ends here]
    //      UINT - Bone count
    //      { [Bone count]
    //          UINT - Length of bone name
    //          wchar_t[] - Bone name (if length > 0)
    //          Bone structure
    //      }
    //      UINT - Animation clip count
    //      { [Animation clip count]
    //          UINT - Length of clip name
    //          wchar_t[] - Clip name (if length > 0)
    //          float - Start time
    //          float - End time
    //          UINT - Keyframe count
    //          { [Keyframe count]
    //              Keyframe structure
    //          }
    //      }
    // }

#pragma pack(push,1)

    struct Material
    {
        DirectX::XMFLOAT4   Ambient;
        DirectX::XMFLOAT4   Diffuse;
        DirectX::XMFLOAT4   Specular;
        float               SpecularPower;
        DirectX::XMFLOAT4   Emissive;
        DirectX::XMFLOAT4X4 UVTransform;
    };

    constexpr uint32_t MAX_TEXTURE = 8;

    struct SubMesh
    {
        uint32_t MaterialIndex;
        uint32_t IndexBufferIndex;
        uint32_t VertexBufferIndex;
        uint32_t StartIndex;
        uint32_t PrimCount;
    };

    constexpr uint32_t NUM_BONE_INFLUENCES = 4;

    static_assert(sizeof(VertexPositionNormalTangentColorTexture) == 52, "mismatch with CMO vertex type");

    struct SkinningVertex
    {
        uint32_t boneIndex[NUM_BONE_INFLUENCES];
        float boneWeight[NUM_BONE_INFLUENCES];
    };

    struct MeshExtents
    {
        float CenterX, CenterY, CenterZ;
        float Radius;

        float MinX, MinY, MinZ;
        float MaxX, MaxY, MaxZ;
    };

    struct Bone
    {
        int32_t ParentIndex;
        DirectX::XMFLOAT4X4 InvBindPos;
        DirectX::XMFLOAT4X4 BindPos;
        DirectX::XMFLOAT4X4 LocalTransform;
    };

    struct Clip
    {
        float StartTime;
        float EndTime;
        uint32_t keys;
    };

    struct Keyframe
    {
        uint32_t BoneIndex;
        float Time;
        DirectX::XMFLOAT4X4 Transform;
    };

#pragma pack(pop)

    const Material s_defMaterial =
    {
        { 0.2f, 0.2f, 0.2f, 1.f },
        { 0.8f, 0.8f, 0.8f, 1.f },
        { 0.0f, 0.0f, 0.0f, 1.f },
        1.f,
        { 0.0f, 0.0f, 0.0f, 1.0f },
        { 1.f, 0.f, 0.f, 0.f,
          0.f, 1.f, 0.f, 0.f,
          0.f, 0.f, 1.f, 0.f,
          0.f, 0.f, 0.f, 1.f },
    };
} // namespace

static_assert(sizeof(VSD3DStarter::Material) == 132, "CMO Mesh structure size incorrect");
static_assert(sizeof(VSD3DStarter::SubMesh) == 20, "CMO Mesh structure size incorrect");
static_assert(sizeof(VSD3DStarter::SkinningVertex) == 32, "CMO Mesh structure size incorrect");
static_assert(sizeof(VSD3DStarter::MeshExtents) == 40, "CMO Mesh structure size incorrect");
static_assert(sizeof(VSD3DStarter::Bone) == 196, "CMO Mesh structure size incorrect");
static_assert(sizeof(VSD3DStarter::Clip) == 12, "CMO Mesh structure size incorrect");
static_assert(sizeof(VSD3DStarter::Keyframe) == 72, "CMO Mesh structure size incorrect");

namespace
{
    //----------------------------------------------------------------------------------
    struct MaterialRecordCMO
    {
        const VSD3DStarter::Material*   pMaterial;
        std::wstring                    name;
        std::wstring                    pixelShader;
        std::wstring                    texture[VSD3DStarter::MAX_TEXTURE];
        std::shared_ptr<IEffect>        effect;
        ComPtr<ID3D11InputLayout>       il;

        MaterialRecordCMO() noexcept :
            pMaterial(nullptr),
            texture{} {}
    };

    // Helper for creating a D3D input layout.
    void CreateCMOInputLayout(_In_ ID3D11Device* device, _In_ IEffect* effect, _Outptr_ ID3D11InputLayout** pInputLayout, bool skinning)
    {
        if (skinning)
        {
            ThrowIfFailed(
                CreateInputLayoutFromEffect<VertexPositionNormalTangentColorTextureSkinning>(device, effect, pInputLayout)
            );
        }
        else
        {
            ThrowIfFailed(
                CreateInputLayoutFromEffect<VertexPositionNormalTangentColorTexture>(device, effect, pInputLayout)
            );
        }

        assert(pInputLayout != nullptr && *pInputLayout != nullptr);
        _Analysis_assume_(pInputLayout != nullptr && *pInputLayout != nullptr);

        SetDebugObjectName(*pInputLayout, "ModelCMO");
    }

    // Shared VB input element description
    INIT_ONCE g_InitOnce = INIT_ONCE_STATIC_INIT;
    std::shared_ptr<ModelMeshPart::InputLayoutCollection> g_vbdecl;
    std::shared_ptr<ModelMeshPart::InputLayoutCollection> g_vbdeclSkinning;

    BOOL CALLBACK InitializeDecl(PINIT_ONCE initOnce, PVOID Parameter, PVOID *lpContext)
    {
        UNREFERENCED_PARAMETER(initOnce);
        UNREFERENCED_PARAMETER(Parameter);
        UNREFERENCED_PARAMETER(lpContext);

        g_vbdecl = std::make_shared<ModelMeshPart::InputLayoutCollection>(
            VertexPositionNormalTangentColorTexture::InputElements,
            VertexPositionNormalTangentColorTexture::InputElements + VertexPositionNormalTangentColorTexture::InputElementCount);

        g_vbdeclSkinning = std::make_shared<ModelMeshPart::InputLayoutCollection>(
            VertexPositionNormalTangentColorTextureSkinning::InputElements,
            VertexPositionNormalTangentColorTextureSkinning::InputElements + VertexPositionNormalTangentColorTextureSkinning::InputElementCount);
        return TRUE;
    }

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
}


//======================================================================================
// Model Loader
//======================================================================================

_Use_decl_annotations_
std::unique_ptr<Model> DirectX::Model::CreateFromCMO(
    ID3D11Device* device,
    const uint8_t* meshData, size_t dataSize,
    IEffectFactory& fxFactory,
    ModelLoaderFlags flags,
    size_t* animsOffset)
{
    if (animsOffset)
    {
        *animsOffset = 0;
    }

    if (!InitOnceExecuteOnce(&g_InitOnce, InitializeDecl, nullptr, nullptr))
        throw std::system_error(std::error_code(static_cast<int>(GetLastError()), std::system_category()), "InitOnceExecuteOnce");

    if (!device || !meshData)
        throw std::invalid_argument("Device and meshData cannot be null");

    auto fxFactoryDGSL = dynamic_cast<DGSLEffectFactory*>(&fxFactory);

    // Meshes
    auto nMesh = reinterpret_cast<const uint32_t*>(meshData);
    size_t usedSize = sizeof(uint32_t);
    if (dataSize < usedSize)
        throw std::runtime_error("End of file");

    if (!*nMesh)
        throw std::runtime_error("No meshes found");

    auto model = std::make_unique<Model>();

    for (size_t meshIndex = 0; meshIndex < *nMesh; ++meshIndex)
    {
        // Mesh name
        auto nName = reinterpret_cast<const uint32_t*>(meshData + usedSize);
        usedSize += sizeof(uint32_t);
        if (dataSize < usedSize)
            throw std::runtime_error("End of file");

        auto meshName = reinterpret_cast<const wchar_t*>(meshData + usedSize);

        usedSize += sizeof(wchar_t)*(*nName);
        if (dataSize < usedSize)
            throw std::runtime_error("End of file");

        auto mesh = std::make_shared<ModelMesh>();
        mesh->name.assign(meshName, *nName);
        mesh->ccw = (flags & ModelLoader_CounterClockwise) != 0;
        mesh->pmalpha = (flags & ModelLoader_PremultipledAlpha) != 0;

        // Materials
        auto nMats = reinterpret_cast<const uint32_t*>(meshData + usedSize);
        usedSize += sizeof(uint32_t);
        if (dataSize < usedSize)
            throw std::runtime_error("End of file");

        std::vector<MaterialRecordCMO> materials;
        materials.reserve(*nMats);
        for (size_t j = 0; j < *nMats; ++j)
        {
            MaterialRecordCMO m;

            // Material name
            nName = reinterpret_cast<const uint32_t*>(meshData + usedSize);
            usedSize += sizeof(uint32_t);
            if (dataSize < usedSize)
                throw std::runtime_error("End of file");

            auto matName = reinterpret_cast<const wchar_t*>(meshData + usedSize);

            usedSize += sizeof(wchar_t)*(*nName);
            if (dataSize < usedSize)
                throw std::runtime_error("End of file");

            m.name.assign(matName, *nName);

            // Material settings
            auto matSetting = reinterpret_cast<const VSD3DStarter::Material*>(meshData + usedSize);
            usedSize += sizeof(VSD3DStarter::Material);
            if (dataSize < usedSize)
                throw std::runtime_error("End of file");

            m.pMaterial = matSetting;

            // Pixel shader name
            nName = reinterpret_cast<const uint32_t*>(meshData + usedSize);
            usedSize += sizeof(uint32_t);
            if (dataSize < usedSize)
                throw std::runtime_error("End of file");

            auto psName = reinterpret_cast<const wchar_t*>(meshData + usedSize);

            usedSize += sizeof(wchar_t)*(*nName);
            if (dataSize < usedSize)
                throw std::runtime_error("End of file");

            m.pixelShader.assign(psName, *nName);

            for (size_t t = 0; t < VSD3DStarter::MAX_TEXTURE; ++t)
            {
                nName = reinterpret_cast<const uint32_t*>(meshData + usedSize);
                usedSize += sizeof(uint32_t);
                if (dataSize < usedSize)
                    throw std::runtime_error("End of file");

                auto txtName = reinterpret_cast<const wchar_t*>(meshData + usedSize);

                usedSize += sizeof(wchar_t)*(*nName);
                if (dataSize < usedSize)
                    throw std::runtime_error("End of file");

                m.texture[t].assign(txtName, *nName);
            }

            materials.emplace_back(m);
        }

        assert(materials.size() == *nMats);

        if (materials.empty())
        {
            // Add default material if none defined
            MaterialRecordCMO m;
            m.pMaterial = &VSD3DStarter::s_defMaterial;
            m.name = L"Default";
            materials.emplace_back(m);
        }

        // Skeletal data?
        const uint8_t* bSkeleton = meshData + usedSize;
        usedSize += sizeof(uint8_t);
        if (dataSize < usedSize)
            throw std::runtime_error("End of file");

        // Submeshes
        auto nSubmesh = reinterpret_cast<const uint32_t*>(meshData + usedSize);
        usedSize += sizeof(uint32_t);
        if (dataSize < usedSize)
            throw std::runtime_error("End of file");

        if (!*nSubmesh)
            throw std::runtime_error("No submeshes found\n");

        auto subMesh = reinterpret_cast<const VSD3DStarter::SubMesh*>(meshData + usedSize);
        usedSize += sizeof(VSD3DStarter::SubMesh) * (*nSubmesh);
        if (dataSize < usedSize)
            throw std::runtime_error("End of file");

        // Index buffers
        auto nIBs = reinterpret_cast<const uint32_t*>(meshData + usedSize);
        usedSize += sizeof(uint32_t);
        if (dataSize < usedSize)
            throw std::runtime_error("End of file");

        if (!*nIBs)
            throw std::runtime_error("No index buffers found\n");

        struct IBData
        {
            size_t          nIndices;
            const uint16_t* ptr;
        };

        std::vector<IBData> ibData;
        ibData.reserve(*nIBs);

        std::vector<ComPtr<ID3D11Buffer>> ibs;
        ibs.resize(*nIBs);

        for (size_t j = 0; j < *nIBs; ++j)
        {
            auto nIndexes = reinterpret_cast<const uint32_t*>(meshData + usedSize);
            usedSize += sizeof(uint32_t);
            if (dataSize < usedSize)
                throw std::runtime_error("End of file");

            if (!*nIndexes)
                throw std::runtime_error("Empty index buffer found\n");

            const uint64_t sizeInBytes = uint64_t(*(nIndexes)) * sizeof(uint16_t);

            if (sizeInBytes > UINT32_MAX)
                throw std::runtime_error("IB too large");

            if (!(flags & ModelLoader_AllowLargeModels))
            {
                if (sizeInBytes > (D3D11_REQ_RESOURCE_SIZE_IN_MEGABYTES_EXPRESSION_A_TERM * 1024u * 1024u))
                    throw std::runtime_error("IB too large for DirectX 11");
            }

            auto const ibBytes = static_cast<size_t>(sizeInBytes);

            auto indexes = reinterpret_cast<const uint16_t*>(meshData + usedSize);
            usedSize += ibBytes;
            if (dataSize < usedSize)
                throw std::runtime_error("End of file");

            IBData ib;
            ib.nIndices = *nIndexes;
            ib.ptr = indexes;
            ibData.emplace_back(ib);

            D3D11_BUFFER_DESC desc = {};
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.ByteWidth = static_cast<UINT>(ibBytes);
            desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

            D3D11_SUBRESOURCE_DATA initData = { indexes, 0, 0 };

            ThrowIfFailed(
                device->CreateBuffer(&desc, &initData, &ibs[j])
            );

            SetDebugObjectName(ibs[j].Get(), "ModelCMO");
        }

        assert(ibData.size() == *nIBs);
        assert(ibs.size() == *nIBs);

        // Vertex buffers
        auto nVBs = reinterpret_cast<const uint32_t*>(meshData + usedSize);
        usedSize += sizeof(uint32_t);
        if (dataSize < usedSize)
            throw std::runtime_error("End of file");

        if (!*nVBs)
            throw std::runtime_error("No vertex buffers found\n");

        struct VBData
        {
            size_t                                          nVerts;
            const VertexPositionNormalTangentColorTexture*  ptr;
            const VSD3DStarter::SkinningVertex*             skinPtr;
        };

        std::vector<VBData> vbData;
        vbData.reserve(*nVBs);
        for (size_t j = 0; j < *nVBs; ++j)
        {
            auto nVerts = reinterpret_cast<const uint32_t*>(meshData + usedSize);
            usedSize += sizeof(uint32_t);
            if (dataSize < usedSize)
                throw std::runtime_error("End of file");

            if (!*nVerts)
                throw std::runtime_error("Empty vertex buffer found\n");

            const size_t vbBytes = sizeof(VertexPositionNormalTangentColorTexture) * (*(nVerts));

            auto verts = reinterpret_cast<const VertexPositionNormalTangentColorTexture*>(meshData + usedSize);
            usedSize += vbBytes;
            if (dataSize < usedSize)
                throw std::runtime_error("End of file");

            VBData vb;
            vb.nVerts = *nVerts;
            vb.ptr = verts;
            vb.skinPtr = nullptr;
            vbData.emplace_back(vb);
        }

        assert(vbData.size() == *nVBs);

        // Skinning vertex buffers
        auto nSkinVBs = reinterpret_cast<const uint32_t*>(meshData + usedSize);
        usedSize += sizeof(uint32_t);
        if (dataSize < usedSize)
            throw std::runtime_error("End of file");

        if (*nSkinVBs)
        {
            if (*nSkinVBs != *nVBs)
                throw std::runtime_error("Number of VBs not equal to number of skin VBs");

            for (size_t j = 0; j < *nSkinVBs; ++j)
            {
                auto nVerts = reinterpret_cast<const uint32_t*>(meshData + usedSize);
                usedSize += sizeof(uint32_t);
                if (dataSize < usedSize)
                    throw std::runtime_error("End of file");

                if (!*nVerts)
                    throw std::runtime_error("Empty skinning vertex buffer found\n");

                if (vbData[j].nVerts != *nVerts)
                    throw std::runtime_error("Mismatched number of verts for skin VBs");

                const size_t vbBytes = sizeof(VSD3DStarter::SkinningVertex) * (*(nVerts));

                auto verts = reinterpret_cast<const VSD3DStarter::SkinningVertex*>(meshData + usedSize);
                usedSize += vbBytes;
                if (dataSize < usedSize)
                    throw std::runtime_error("End of file");

                vbData[j].skinPtr = verts;
            }
        }

        // Extents
        auto extents = reinterpret_cast<const VSD3DStarter::MeshExtents*>(meshData + usedSize);
        usedSize += sizeof(VSD3DStarter::MeshExtents);
        if (dataSize < usedSize)
            throw std::runtime_error("End of file");

        mesh->boundingSphere.Center.x = extents->CenterX;
        mesh->boundingSphere.Center.y = extents->CenterY;
        mesh->boundingSphere.Center.z = extents->CenterZ;
        mesh->boundingSphere.Radius = extents->Radius;

        const XMVECTOR min = XMVectorSet(extents->MinX, extents->MinY, extents->MinZ, 0.f);
        const XMVECTOR max = XMVectorSet(extents->MaxX, extents->MaxY, extents->MaxZ, 0.f);
        BoundingBox::CreateFromPoints(mesh->boundingBox, min, max);

        // Load model bones (if present and requested)
        if (*bSkeleton && (flags & ModelLoader_IncludeBones))
        {
            // Bones
            auto nBones = reinterpret_cast<const uint32_t*>(meshData + usedSize);
            usedSize += sizeof(uint32_t);
            if (dataSize < usedSize)
                throw std::runtime_error("End of file");

            if (!*nBones)
                throw std::runtime_error("Animation bone data is missing\n");

            ModelBone::Collection bones;
            bones.resize(*nBones);
            auto transforms = ModelBone::MakeArray(*nBones);
            auto invTransforms = ModelBone::MakeArray(*nBones);

            for (uint32_t j = 0; j < *nBones; ++j)
            {
                // Bone name
                nName = reinterpret_cast<const uint32_t*>(meshData + usedSize);
                usedSize += sizeof(uint32_t);
                if (dataSize < usedSize)
                    throw std::runtime_error("End of file");

                auto boneName = reinterpret_cast<const wchar_t*>(meshData + usedSize);

                usedSize += sizeof(wchar_t) * (*nName);
                if (dataSize < usedSize)
                    throw std::runtime_error("End of file");

                bones[j].name = boneName;

                // Bone settings
                auto cmobones = reinterpret_cast<const VSD3DStarter::Bone*>(meshData + usedSize);
                usedSize += sizeof(VSD3DStarter::Bone);
                if (dataSize < usedSize)
                    throw std::runtime_error("End of file");

                transforms[j] = XMLoadFloat4x4(&cmobones->LocalTransform);
                invTransforms[j] = XMLoadFloat4x4(&cmobones->InvBindPos);

                if (cmobones->ParentIndex < 0)
                {
                    if (!j)
                        continue;

                    // Add as a sibling of the root bone
                    uint32_t index = 0;
                    for (size_t visited = 0;; ++visited)
                    {
                        if (visited >= *nBones)
                            throw std::runtime_error("Skeleton bones form an invalid graph");

                        const uint32_t sibling = bones[index].siblingIndex;
                        if (sibling == ModelBone::c_Invalid)
                        {
                            bones[index].siblingIndex = j;
                            break;
                        }

                        if (sibling >= *nBones)
                            throw std::runtime_error("Skeleton bones corrupt");

                        index = sibling;
                    }
                }
                else if (static_cast<uint32_t>(cmobones->ParentIndex) >= *nBones)
                {
                    throw std::runtime_error("Skeleton bones corrupt");
                }
                else
                {
                    if (!j)
                        throw std::runtime_error("First bone must be root!");

                    auto index = static_cast<uint32_t>(cmobones->ParentIndex);

                    bones[j].parentIndex = index;

                    // Add as the only child of the parent
                    if (bones[index].childIndex == ModelBone::c_Invalid)
                    {
                        bones[index].childIndex = j;
                    }
                    else
                    {
                        // Otherwise add as a sibling of the parent's other children
                        index = bones[index].childIndex;
                        for (size_t visited = 0;; ++visited)
                        {
                            if (visited >= *nBones)
                                throw std::runtime_error("Skeleton bones form an invalid graph");

                            const uint32_t sibling = bones[index].siblingIndex;
                            if (sibling == ModelBone::c_Invalid)
                            {
                                bones[index].siblingIndex = j;
                                break;
                            }

                            if (sibling >= *nBones)
                                throw std::runtime_error("Skeleton bones corrupt");

                            index = sibling;
                        }
                    }
                }
            }

            std::swap(model->bones, bones);
            std::swap(model->boneMatrices, transforms);
            std::swap(model->invBindPoseMatrices, invTransforms);

            // Animation Clips
            if (animsOffset)
            {
                // Optional return for offset to start of animation clips in the CMO.

                size_t offset = usedSize;

                auto nClips = reinterpret_cast<const uint32_t*>(meshData + usedSize);
                usedSize += sizeof(uint32_t);
                if (dataSize < usedSize)
                    throw std::runtime_error("End of file");

                if (*nClips > 0)
                {
                    *animsOffset = offset;
                }
            }
        }

        const bool enableSkinning = (*nSkinVBs) != 0 && !(flags & ModelLoader_DisableSkinning);

        // Build vertex buffers
        std::vector<ComPtr<ID3D11Buffer>> vbs;
        vbs.resize(*nVBs);

        const size_t stride = enableSkinning ? sizeof(VertexPositionNormalTangentColorTextureSkinning)
            : sizeof(VertexPositionNormalTangentColorTexture);

        for (size_t j = 0; j < *nVBs; ++j)
        {
            const size_t nVerts = vbData[j].nVerts;

            const uint64_t sizeInBytes = uint64_t(stride) * uint64_t(nVerts);

            if (sizeInBytes > UINT32_MAX)
                throw std::runtime_error("VB too large");

            if (!(flags & ModelLoader_AllowLargeModels))
            {
                if (sizeInBytes > uint64_t(D3D11_REQ_RESOURCE_SIZE_IN_MEGABYTES_EXPRESSION_A_TERM * 1024u * 1024u))
                    throw std::runtime_error("VB too large for DirectX 11");
            }

            const size_t bytes = static_cast<size_t>(sizeInBytes);

            D3D11_BUFFER_DESC desc = {};
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.ByteWidth = static_cast<UINT>(bytes);
            desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

            if (fxFactoryDGSL && !enableSkinning)
            {
                // Can use CMO vertex data directly
                D3D11_SUBRESOURCE_DATA initData = { vbData[j].ptr, 0, 0 };

                ThrowIfFailed(
                    device->CreateBuffer(&desc, &initData, &vbs[j])
                );
            }
            else
            {
                auto temp = std::make_unique<uint8_t[]>(bytes + (sizeof(uint32_t) * nVerts));

                auto visited = reinterpret_cast<uint32_t*>(temp.get() + bytes);
                memset(visited, 0xff, sizeof(uint32_t) * nVerts);

                assert(vbData[j].ptr != nullptr);

                if (enableSkinning)
                {
                    // Combine CMO multi-stream data into a single stream
                    auto skinptr = vbData[j].skinPtr;
                    assert(skinptr != nullptr);

                    uint8_t* ptr = temp.get();

                    auto sptr = vbData[j].ptr;

                    for (size_t v = 0; v < nVerts; ++v)
                    {
                        *reinterpret_cast<VertexPositionNormalTangentColorTexture*>(ptr) = sptr[v];

                        auto skinv = reinterpret_cast<VertexPositionNormalTangentColorTextureSkinning*>(ptr);
                        skinv->SetBlendIndices(*reinterpret_cast<const XMUINT4*>(skinptr[v].boneIndex));
                        skinv->SetBlendWeights(*reinterpret_cast<const XMFLOAT4*>(skinptr[v].boneWeight));

                        ptr += stride;
                    }
                }
                else
                {
                    memcpy(temp.get(), vbData[j].ptr, bytes);
                }

                if (!fxFactoryDGSL)
                {
                    // Need to fix up VB tex coords for UV transform which is not supported by basic effects
                    for (size_t k = 0; k < *nSubmesh; ++k)
                    {
                        auto& sm = subMesh[k];

                        if (sm.VertexBufferIndex != j)
                            continue;

                        if ((sm.IndexBufferIndex >= *nIBs)
                            || (sm.MaterialIndex >= materials.size()))
                            throw std::out_of_range("Invalid submesh found\n");

                        const XMMATRIX uvTransform = XMLoadFloat4x4(&materials[sm.MaterialIndex].pMaterial->UVTransform);

                        auto ib = ibData[sm.IndexBufferIndex].ptr;

                        const size_t count = ibData[sm.IndexBufferIndex].nIndices;

                        for (size_t q = 0; q < count; ++q)
                        {
                            size_t v = ib[q];

                            if (v >= nVerts)
                                throw std::out_of_range("Invalid index found\n");

                            auto verts = reinterpret_cast<VertexPositionNormalTangentColorTexture*>(temp.get() + (v * stride));
                            if (visited[v] == uint32_t(-1))
                            {
                                visited[v] = sm.MaterialIndex;

                                XMVECTOR t = XMLoadFloat2(&verts->textureCoordinate);

                                t = XMVectorSelect(g_XMIdentityR3, t, g_XMSelect1110);

                                t = XMVector4Transform(t, uvTransform);

                                XMStoreFloat2(&verts->textureCoordinate, t);
                            }
                            else if (visited[v] != sm.MaterialIndex)
                            {
                            #ifdef _DEBUG
                                const XMMATRIX uv2 = XMLoadFloat4x4(&materials[visited[v]].pMaterial->UVTransform);

                                if (XMVector4NotEqual(uvTransform.r[0], uv2.r[0])
                                    || XMVector4NotEqual(uvTransform.r[1], uv2.r[1])
                                    || XMVector4NotEqual(uvTransform.r[2], uv2.r[2])
                                    || XMVector4NotEqual(uvTransform.r[3], uv2.r[3]))
                                {
                                    DebugTrace("WARNING: %ls - mismatched UV transforms for the same vertex; texture coordinates may not be correct\n", mesh->name.c_str());
                                }
                            #endif
                            }
                        }
                    }
                }

                // Create vertex buffer from temporary buffer
                D3D11_SUBRESOURCE_DATA initData = { temp.get(), 0, 0 };

                ThrowIfFailed(
                    device->CreateBuffer(&desc, &initData, &vbs[j])
                );
            }

            SetDebugObjectName(vbs[j].Get(), "ModelCMO");
        }

        assert(vbs.size() == *nVBs);

        // Create Effects
        const bool srgb = (flags & ModelLoader_MaterialColorsSRGB) != 0;

        for (size_t j = 0; j < materials.size(); ++j)
        {
            auto& m = materials[j];

            if (fxFactoryDGSL)
            {
                DGSLEffectFactory::DGSLEffectInfo info;
                info.name = m.name.c_str();
                info.specularPower = m.pMaterial->SpecularPower;
                info.perVertexColor = true;
                info.enableSkinning = enableSkinning;
                info.alpha = m.pMaterial->Diffuse.w;
                info.ambientColor = GetMaterialColor(m.pMaterial->Ambient.x, m.pMaterial->Ambient.y, m.pMaterial->Ambient.z, srgb);
                info.diffuseColor = GetMaterialColor(m.pMaterial->Diffuse.x, m.pMaterial->Diffuse.y, m.pMaterial->Diffuse.z, srgb);
                info.specularColor = GetMaterialColor(m.pMaterial->Specular.x, m.pMaterial->Specular.y, m.pMaterial->Specular.z, srgb);
                info.emissiveColor = GetMaterialColor(m.pMaterial->Emissive.x, m.pMaterial->Emissive.y, m.pMaterial->Emissive.z, srgb);
                info.diffuseTexture = m.texture[0].empty() ? nullptr : m.texture[0].c_str();
                info.specularTexture = m.texture[1].empty() ? nullptr : m.texture[1].c_str();
                info.normalTexture = m.texture[2].empty() ? nullptr : m.texture[2].c_str();
                info.emissiveTexture = m.texture[3].empty() ? nullptr : m.texture[3].c_str();
                info.pixelShader = m.pixelShader.c_str();

                constexpr int offset = DGSLEffectFactory::DGSLEffectInfo::BaseTextureOffset;
                for (int i = 0; i < (DGSLEffect::MaxTextures - offset); ++i)
                {
                    info.textures[i] = m.texture[i + offset].empty() ? nullptr : m.texture[i + offset].c_str();
                }

                m.effect = fxFactoryDGSL->CreateDGSLEffect(info, nullptr);

                auto dgslEffect = static_cast<DGSLEffect*>(m.effect.get());
                dgslEffect->SetUVTransform(XMLoadFloat4x4(&m.pMaterial->UVTransform));
            }
            else
            {
                EffectFactory::EffectInfo info;
                info.name = m.name.c_str();
                info.specularPower = m.pMaterial->SpecularPower;
                info.perVertexColor = true;
                info.enableSkinning = enableSkinning;
                info.alpha = m.pMaterial->Diffuse.w;
                info.ambientColor = GetMaterialColor(m.pMaterial->Ambient.x, m.pMaterial->Ambient.y, m.pMaterial->Ambient.z, srgb);
                info.diffuseColor = GetMaterialColor(m.pMaterial->Diffuse.x, m.pMaterial->Diffuse.y, m.pMaterial->Diffuse.z, srgb);
                info.specularColor = GetMaterialColor(m.pMaterial->Specular.x, m.pMaterial->Specular.y, m.pMaterial->Specular.z, srgb);
                info.emissiveColor = GetMaterialColor(m.pMaterial->Emissive.x, m.pMaterial->Emissive.y, m.pMaterial->Emissive.z, srgb);
                info.diffuseTexture = m.texture[0].c_str();

                m.effect = fxFactory.CreateEffect(info, nullptr);
            }

            CreateCMOInputLayout(device, m.effect.get(), &m.il, enableSkinning);
        }

        // Build mesh parts
        for (size_t j = 0; j < *nSubmesh; ++j)
        {
            auto& sm = subMesh[j];

            if ((sm.IndexBufferIndex >= *nIBs)
                || (sm.VertexBufferIndex >= *nVBs)
                || (sm.MaterialIndex >= materials.size()))
                throw std::out_of_range("Invalid submesh found\n");

            auto& mat = materials[sm.MaterialIndex];

            auto part = new ModelMeshPart();

            if (mat.pMaterial->Diffuse.w < 1)
                part->isAlpha = true;

            part->indexCount = sm.PrimCount * 3;
            part->startIndex = sm.StartIndex;
            part->vertexStride = static_cast<UINT>(stride);
            part->inputLayout = mat.il;
            part->indexBuffer = ibs[sm.IndexBufferIndex];
            part->vertexBuffer = vbs[sm.VertexBufferIndex];
            part->effect = mat.effect;
            part->vbDecl = enableSkinning ? g_vbdeclSkinning : g_vbdecl;

            mesh->meshParts.emplace_back(part);
        }

        model->meshes.emplace_back(mesh);
    }

    return model;
}


//--------------------------------------------------------------------------------------
_Use_decl_annotations_
std::unique_ptr<Model> DirectX::Model::CreateFromCMO(
    ID3D11Device* device,
    const wchar_t* szFileName,
    IEffectFactory& fxFactory,
    ModelLoaderFlags flags,
    size_t* animsOffset)
{
    if (animsOffset)
    {
        *animsOffset = 0;
    }

    size_t dataSize = 0;
    std::unique_ptr<uint8_t[]> data;
    HRESULT hr = BinaryReader::ReadEntireFile(szFileName, data, &dataSize);
    if (FAILED(hr))
    {
        DebugTrace("ERROR: CreateFromCMO failed (%08X) loading '%ls'\n",
            static_cast<unsigned int>(hr), szFileName);
        throw std::runtime_error("CreateFromCMO");
    }

    auto model = CreateFromCMO(device, data.get(), dataSize, fxFactory, flags, animsOffset);

    model->name = szFileName;

    return model;
}
