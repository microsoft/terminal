//--------------------------------------------------------------------------------------
// File: Model.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "Model.h"
#include "CommonStates.h"
#include "DirectXHelpers.h"
#include "Effects.h"
#include "PlatformHelpers.h"

using namespace DirectX;

#if !defined(_CPPRTTI) && !defined(__GXX_RTTI)
#error Model requires RTTI
#endif

//--------------------------------------------------------------------------------------
// ModelMeshPart
//--------------------------------------------------------------------------------------

ModelMeshPart::ModelMeshPart() noexcept :
    indexCount(0),
    startIndex(0),
    vertexOffset(0),
    vertexStride(0),
    primitiveType(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST),
    indexFormat(DXGI_FORMAT_R16_UINT),
    isAlpha(false)
{
}


ModelMeshPart::~ModelMeshPart()
{
}


// Draws using a custom override effect.
_Use_decl_annotations_
void ModelMeshPart::Draw(
    ID3D11DeviceContext* deviceContext,
    IEffect* ieffect,
    ID3D11InputLayout* iinputLayout,
    std::function<void()> setCustomState) const
{
    deviceContext->IASetInputLayout(iinputLayout);

    auto vb = vertexBuffer.Get();
    const UINT vbStride = vertexStride;
    constexpr UINT vbOffset = 0;
    deviceContext->IASetVertexBuffers(0, 1, &vb, &vbStride, &vbOffset);

    // Note that if indexFormat is DXGI_FORMAT_R32_UINT, this model mesh part requires a Feature Level 9.2 or greater device
    deviceContext->IASetIndexBuffer(indexBuffer.Get(), indexFormat, 0);

    assert(ieffect != nullptr);
    ieffect->Apply(deviceContext);

    // Hook lets the caller replace our shaders or state settings with whatever else they see fit.
    if (setCustomState)
    {
        setCustomState();
    }

    // Draw the primitive.
    deviceContext->IASetPrimitiveTopology(primitiveType);

    deviceContext->DrawIndexed(indexCount, startIndex, vertexOffset);
}


// Draws using a custom override effect w/ instancing.
_Use_decl_annotations_
void ModelMeshPart::DrawInstanced(
    ID3D11DeviceContext* deviceContext,
    IEffect* ieffect,
    ID3D11InputLayout* iinputLayout,
    uint32_t instanceCount, uint32_t startInstanceLocation,
    std::function<void()> setCustomState) const
{
    deviceContext->IASetInputLayout(iinputLayout);

    auto vb = vertexBuffer.Get();
    const UINT vbStride = vertexStride;
    constexpr UINT vbOffset = 0;
    deviceContext->IASetVertexBuffers(0, 1, &vb, &vbStride, &vbOffset);

    // Note that if indexFormat is DXGI_FORMAT_R32_UINT, this model mesh part requires a Feature Level 9.2 or greater device
    deviceContext->IASetIndexBuffer(indexBuffer.Get(), indexFormat, 0);

    assert(ieffect != nullptr);
    ieffect->Apply(deviceContext);

    // Hook lets the caller replace our shaders or state settings with whatever else they see fit.
    if (setCustomState)
    {
        setCustomState();
    }

    // Draw the primitive.
    deviceContext->IASetPrimitiveTopology(primitiveType);

    deviceContext->DrawIndexedInstanced(
        indexCount, instanceCount, startIndex,
        vertexOffset,
        startInstanceLocation);
}


// Creates input layout for use with custom override effects.
_Use_decl_annotations_
void ModelMeshPart::CreateInputLayout(ID3D11Device* d3dDevice, IEffect* ieffect, ID3D11InputLayout** iinputLayout) const
{
    if (iinputLayout)
    {
        *iinputLayout = nullptr;
    }

    if (!vbDecl || vbDecl->empty())
        throw std::runtime_error("Model mesh part missing vertex buffer input elements data");

    if (vbDecl->size() > 32 /* D3D11_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT */)
        throw std::runtime_error("Model mesh part input layout size is too large for DirectX 11");

    ThrowIfFailed(
        CreateInputLayoutFromEffect(d3dDevice, ieffect, vbDecl->data(), vbDecl->size(), iinputLayout)
    );

    assert(iinputLayout != nullptr && *iinputLayout != nullptr);
    _Analysis_assume_(iinputLayout != nullptr && *iinputLayout != nullptr);
}


// Assigns a new effect and re-generates input layout.
_Use_decl_annotations_
void ModelMeshPart::ModifyEffect(ID3D11Device* d3dDevice, const std::shared_ptr<IEffect>& ieffect, bool isalpha)
{
    if (!vbDecl || vbDecl->empty())
        throw std::runtime_error("Model mesh part missing vertex buffer input elements data");

    if (vbDecl->size() > 32 /* D3D11_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT */)
        throw std::runtime_error("Model mesh part input layout size is too large for DirectX 11");

    assert(ieffect != nullptr);
    this->effect = ieffect;
    this->isAlpha = isalpha;

    assert(d3dDevice != nullptr);

    ThrowIfFailed(
        CreateInputLayoutFromEffect(d3dDevice, effect.get(), vbDecl->data(), vbDecl->size(), inputLayout.ReleaseAndGetAddressOf())
    );
}


//--------------------------------------------------------------------------------------
// ModelMesh
//--------------------------------------------------------------------------------------

bool ModelMesh::s_reversez = false;

ModelMesh::ModelMesh() noexcept :
    boneIndex(ModelBone::c_Invalid),
    ccw(true),
    pmalpha(true)
{
}


ModelMesh::~ModelMesh()
{
}


// Set render state for mesh part rendering.
_Use_decl_annotations_
void ModelMesh::PrepareForRendering(
    ID3D11DeviceContext* deviceContext,
    const CommonStates& states,
    bool alpha,
    bool wireframe) const
{
    assert(deviceContext != nullptr);

    // Set the blend and depth stencil state.
    ID3D11BlendState* blendState;
    ID3D11DepthStencilState* depthStencilState;

    if (alpha)
    {
        depthStencilState = (s_reversez) ? states.DepthReadReverseZ() : states.DepthRead();
        if (pmalpha)
        {
            blendState = states.AlphaBlend();
        }
        else
        {
            blendState = states.NonPremultiplied();
        }
    }
    else
    {
        blendState = states.Opaque();
        depthStencilState = (s_reversez) ? states.DepthReverseZ() : states.DepthDefault();
    }

    deviceContext->OMSetBlendState(blendState, nullptr, 0xFFFFFFFF);
    deviceContext->OMSetDepthStencilState(depthStencilState, 0);

    // Set the rasterizer state.
    if (wireframe)
        deviceContext->RSSetState(states.Wireframe());
    else
        deviceContext->RSSetState(ccw ? states.CullCounterClockwise() : states.CullClockwise());

    // Set sampler state.
    ID3D11SamplerState* samplers[] =
    {
        states.LinearWrap(),
        states.LinearWrap(),
    };

    deviceContext->PSSetSamplers(0, 2, samplers);
}


// Draw mesh given worldViewProjection matrices.
_Use_decl_annotations_
void XM_CALLCONV ModelMesh::Draw(
    ID3D11DeviceContext* deviceContext,
    FXMMATRIX world,
    CXMMATRIX view,
    CXMMATRIX projection,
    bool alpha,
    std::function<void()> setCustomState) const
{
    assert(deviceContext != nullptr);

    for (const auto& it : meshParts)
    {
        auto part = it.get();
        assert(part != nullptr);

        if (part->isAlpha != alpha)
        {
            // Skip alpha parts when drawing opaque or skip opaque parts if drawing alpha
            continue;
        }

        auto imatrices = dynamic_cast<IEffectMatrices*>(part->effect.get());
        if (imatrices)
        {
            imatrices->SetMatrices(world, view, projection);
        }

        part->Draw(deviceContext, part->effect.get(), part->inputLayout.Get(), setCustomState);
    }
}


// Draw the mesh using model bones.
_Use_decl_annotations_
void XM_CALLCONV ModelMesh::Draw(
    ID3D11DeviceContext* deviceContext,
    size_t nbones, const XMMATRIX* boneTransforms,
    FXMMATRIX world,
    CXMMATRIX view,
    CXMMATRIX projection,
    bool alpha,
    std::function<void()> setCustomState) const
{
    assert(deviceContext != nullptr);

    if (!nbones || !boneTransforms)
    {
        throw std::invalid_argument("Bone transforms array required");
    }

    XMMATRIX local;
    if (boneIndex != ModelBone::c_Invalid && boneIndex < nbones)
    {
        local = XMMatrixMultiply(boneTransforms[boneIndex], world);
    }
    else
    {
        local = world;
    }

    for (const auto& it : meshParts)
    {
        auto part = it.get();
        assert(part != nullptr);

        if (part->isAlpha != alpha)
        {
            // Skip alpha parts when drawing opaque or skip opaque parts if drawing alpha
            continue;
        }

        auto imatrices = dynamic_cast<IEffectMatrices*>(part->effect.get());
        if (imatrices)
        {
            imatrices->SetMatrices(local, view, projection);
        }

        part->Draw(deviceContext, part->effect.get(), part->inputLayout.Get(), setCustomState);
    }
}


// Draw mesh using skinning given bone transform array.
_Use_decl_annotations_
void XM_CALLCONV ModelMesh::DrawSkinned(
    ID3D11DeviceContext* deviceContext,
    size_t nbones,
    const XMMATRIX* boneTransforms,
    FXMMATRIX world,
    CXMMATRIX view,
    CXMMATRIX projection,
    bool alpha,
    std::function<void()> setCustomState) const
{
    assert(deviceContext != nullptr);

    if (!nbones || !boneTransforms)
    {
        throw std::invalid_argument("Bone transforms array required");
    }

    ModelBone::TransformArray temp;

    for (const auto& mit : meshParts)
    {
        auto part = mit.get();
        assert(part != nullptr);

        if (part->isAlpha != alpha)
        {
            // Skip alpha parts when drawing opaque or skip opaque parts if drawing alpha
            continue;
        }

        auto imatrices = dynamic_cast<IEffectMatrices*>(part->effect.get());
        if (imatrices)
        {
            imatrices->SetMatrices(world, view, projection);
        }

        auto iskinning = dynamic_cast<IEffectSkinning*>(part->effect.get());
        if (iskinning)
        {
            if (boneInfluences.empty())
            {
                // Direct-mapping of vertex bone indices to our master bone array
                iskinning->SetBoneTransforms(boneTransforms, nbones);
            }
            else
            {
                if (!temp)
                {
                    // Create the influence mapped bones on-demand.
                    temp = ModelBone::MakeArray(IEffectSkinning::MaxBones);

                    size_t count = 0;
                    for (auto it : boneInfluences)
                    {
                        ++count;
                        if (count > IEffectSkinning::MaxBones)
                        {
                            throw std::runtime_error("Too many bones for skinning");
                        }

                        if (it >= nbones)
                        {
                            throw std::runtime_error("Invalid bone influence index");
                        }

                        temp[count - 1] = boneTransforms[it];
                    }

                    assert(count == boneInfluences.size());
                }

                iskinning->SetBoneTransforms(temp.get(), boneInfluences.size());
            }
        }
        else if (imatrices)
        {
            // Fallback for if we encounter a non-skinning effect in the model
            const XMMATRIX bm = (boneIndex != ModelBone::c_Invalid && boneIndex < nbones)
                ? boneTransforms[boneIndex] : XMMatrixIdentity();

            imatrices->SetWorld(XMMatrixMultiply(bm, world));
        }

        part->Draw(deviceContext, part->effect.get(), part->inputLayout.Get(), setCustomState);
    }
}


//--------------------------------------------------------------------------------------
// Model
//--------------------------------------------------------------------------------------

Model::~Model()
{
}

Model::Model(Model const& other) :
    meshes(other.meshes),
    bones(other.bones),
    name(other.name),
    mEffectCache(other.mEffectCache)
{
    const size_t nbones = other.bones.size();
    if (nbones > 0)
    {
        if (other.boneMatrices)
        {
            boneMatrices = ModelBone::MakeArray(nbones);
            memcpy(boneMatrices.get(), other.boneMatrices.get(), sizeof(XMMATRIX) * nbones);
        }
        if (other.invBindPoseMatrices)
        {
            invBindPoseMatrices = ModelBone::MakeArray(nbones);
            memcpy(invBindPoseMatrices.get(), other.invBindPoseMatrices.get(), sizeof(XMMATRIX) * nbones);
        }
    }
}

Model& Model::operator= (Model const& rhs)
{
    if (this != &rhs)
    {
        Model tmp(rhs);
        std::swap(meshes, tmp.meshes);
        std::swap(bones, tmp.bones);
        std::swap(boneMatrices, tmp.boneMatrices);
        std::swap(invBindPoseMatrices, tmp.invBindPoseMatrices);
        std::swap(name, tmp.name);
        std::swap(mEffectCache, tmp.mEffectCache);
    }
    return *this;
}


// Draw all meshes in model given worldViewProjection matrices.
_Use_decl_annotations_
void XM_CALLCONV Model::Draw(
    ID3D11DeviceContext* deviceContext,
    const CommonStates& states,
    FXMMATRIX world,
    CXMMATRIX view,
    CXMMATRIX projection,
    bool wireframe,
    std::function<void()> setCustomState) const
{
    assert(deviceContext != nullptr);

    // Draw opaque parts
    for (const auto& it : meshes)
    {
        const auto *mesh = it.get();
        assert(mesh != nullptr);

        mesh->PrepareForRendering(deviceContext, states, false, wireframe);

        mesh->Draw(deviceContext, world, view, projection, false, setCustomState);
    }

    // Draw alpha parts
    for (const auto& it : meshes)
    {
        const auto *mesh = it.get();
        assert(mesh != nullptr);

        mesh->PrepareForRendering(deviceContext, states, true, wireframe);

        mesh->Draw(deviceContext, world, view, projection, true, setCustomState);
    }
}


// Draw all meshes in model using rigid-body animation given bone transform array.
_Use_decl_annotations_
void XM_CALLCONV Model::Draw(
    ID3D11DeviceContext* deviceContext,
    const CommonStates& states,
    size_t nbones,
    const XMMATRIX* boneTransforms,
    FXMMATRIX world,
    CXMMATRIX view,
    CXMMATRIX projection,
    bool wireframe,
    std::function<void()> setCustomState) const
{
    assert(deviceContext != nullptr);

    // Draw opaque parts
    for (const auto& it : meshes)
    {
        const auto *mesh = it.get();
        assert(mesh != nullptr);

        mesh->PrepareForRendering(deviceContext, states, false, wireframe);

        mesh->Draw(deviceContext, nbones, boneTransforms, world, view, projection, false, setCustomState);
    }

    // Draw alpha parts
    for (const auto& it : meshes)
    {
        const auto *mesh = it.get();
        assert(mesh != nullptr);

        mesh->PrepareForRendering(deviceContext, states, true, wireframe);

        mesh->Draw(deviceContext, nbones, boneTransforms, world, view, projection, true, setCustomState);
    }
}


// Draw all meshes in model using skinning given bone transform array.
_Use_decl_annotations_
void XM_CALLCONV Model::DrawSkinned(
    ID3D11DeviceContext* deviceContext,
    const CommonStates& states,
    size_t nbones,
    const XMMATRIX* boneTransforms,
    FXMMATRIX world,
    CXMMATRIX view,
    CXMMATRIX projection,
    bool wireframe,
    std::function<void()> setCustomState) const
{
    assert(deviceContext != nullptr);

    // Draw opaque parts
    for (const auto& it : meshes)
    {
        const auto *mesh = it.get();
        assert(mesh != nullptr);

        mesh->PrepareForRendering(deviceContext, states, false, wireframe);

        mesh->DrawSkinned(deviceContext, nbones, boneTransforms, world, view, projection, false, setCustomState);
    }

    // Draw alpha parts
    for (const auto& it : meshes)
    {
        const auto *mesh = it.get();
        assert(mesh != nullptr);

        mesh->PrepareForRendering(deviceContext, states, true, wireframe);

        mesh->DrawSkinned(deviceContext, nbones, boneTransforms, world, view, projection, true, setCustomState);
    }
}


// Compute using bone hierarchy from model bone matrices to an array.
_Use_decl_annotations_
void Model::CopyAbsoluteBoneTransformsTo(
    size_t nbones,
    XMMATRIX* boneTransforms) const
{
    if (!nbones || !boneTransforms)
    {
        throw std::invalid_argument("Bone transforms array required");
    }

    if (nbones < bones.size())
    {
        throw std::invalid_argument("Bone transforms array is too small");
    }

    if (bones.empty() || !boneMatrices)
    {
        throw std::runtime_error("Model is missing bones");
    }

    memset(boneTransforms, 0, sizeof(XMMATRIX) * nbones);

    const XMMATRIX id = XMMatrixIdentity();
    size_t visited = 0;
    ComputeAbsolute(0, id, bones.size(), boneMatrices.get(), boneTransforms, visited);
}


// Compute using bone hierarchy from one array to another array.
_Use_decl_annotations_
void Model::CopyAbsoluteBoneTransforms(
    size_t nbones,
    const XMMATRIX* inBoneTransforms,
    XMMATRIX* outBoneTransforms) const
{
    if (!nbones || !inBoneTransforms || !outBoneTransforms)
    {
        throw std::invalid_argument("Bone transforms arrays required");
    }

    if (nbones < bones.size())
    {
        throw std::invalid_argument("Bone transforms arrays are too small");
    }

    if (bones.empty())
    {
        throw std::runtime_error("Model is missing bones");
    }

    memset(outBoneTransforms, 0, sizeof(XMMATRIX) * nbones);

    const XMMATRIX id = XMMatrixIdentity();
    size_t visited = 0;
    ComputeAbsolute(0, id, bones.size(), inBoneTransforms, outBoneTransforms, visited);
}


// Private helper for computing hierarchical transforms using bones via recursion.
_Use_decl_annotations_
void Model::ComputeAbsolute(
    uint32_t index,
    CXMMATRIX parent,
    size_t nbones,
    const XMMATRIX* inBoneTransforms,
    XMMATRIX* outBoneTransforms,
    size_t& visited) const
{
    if (index == ModelBone::c_Invalid || index >= nbones)
        return;

    assert(inBoneTransforms != nullptr && outBoneTransforms != nullptr);

    ++visited; // Cycle detection safety!
    if (visited > bones.size())
    {
        DebugTrace("ERROR: Model::CopyAbsoluteBoneTransformsTo encountered a cycle in the bones!\n");
        throw std::runtime_error("Model bones form an invalid graph");
    }

    XMMATRIX local = inBoneTransforms[index];
    local = XMMatrixMultiply(local, parent);
    outBoneTransforms[index] = local;

    if (bones[index].siblingIndex != ModelBone::c_Invalid)
    {
        ComputeAbsolute(bones[index].siblingIndex, parent, nbones,
            inBoneTransforms, outBoneTransforms, visited);
    }

    if (bones[index].childIndex != ModelBone::c_Invalid)
    {
        ComputeAbsolute(bones[index].childIndex, local, nbones,
            inBoneTransforms, outBoneTransforms, visited);
    }
}


// Copy the model bone matrices from an array.
_Use_decl_annotations_
void Model::CopyBoneTransformsFrom(size_t nbones, const XMMATRIX* boneTransforms)
{
    if (!nbones || !boneTransforms)
    {
        throw std::invalid_argument("Bone transforms array required");
    }

    if (nbones < bones.size())
    {
        throw std::invalid_argument("Bone transforms array is too small");
    }

    if (bones.empty())
    {
        throw std::runtime_error("Model is missing bones");
    }

    if (!boneMatrices)
    {
        boneMatrices = ModelBone::MakeArray(bones.size());
    }

    memcpy(boneMatrices.get(), boneTransforms, bones.size() * sizeof(XMMATRIX));
}


// Copy the model bone matrices to an array.
_Use_decl_annotations_
void Model::CopyBoneTransformsTo(size_t nbones, XMMATRIX* boneTransforms) const
{
    if (!nbones || !boneTransforms)
    {
        throw std::invalid_argument("Bone transforms array required");
    }

    if (nbones < bones.size())
    {
        throw std::invalid_argument("Bone transforms array is too small");
    }

    if (bones.empty())
    {
        throw std::runtime_error("Model is missing bones");
    }

    memcpy(boneTransforms, boneMatrices.get(), bones.size() * sizeof(XMMATRIX));
}


// Iterate through unique effect instances.
void Model::UpdateEffects(_In_ std::function<void(IEffect*)> setEffect)
{
    if (mEffectCache.empty())
    {
        // This cache ensures we only set each effect once (could be shared)
        for (const auto& mit : meshes)
        {
            auto mesh = mit.get();
            assert(mesh != nullptr);

            for (const auto& it : mesh->meshParts)
            {
                if (it->effect)
                    mEffectCache.insert(it->effect.get());
            }
        }
    }

    assert(setEffect != nullptr);

    for (const auto it : mEffectCache)
    {
        setEffect(it);
    }
}
