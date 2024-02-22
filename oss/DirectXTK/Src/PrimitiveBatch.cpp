//--------------------------------------------------------------------------------------
// File: PrimitiveBatch.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "PrimitiveBatch.h"
#include "DirectXHelpers.h"
#include "GraphicsMemory.h"
#include "PlatformHelpers.h"

using namespace DirectX;
using namespace DirectX::DX11::Private;
using Microsoft::WRL::ComPtr;


// Internal PrimitiveBatch implementation class.
class PrimitiveBatchBase::Impl
{
public:
    Impl(_In_ ID3D11DeviceContext* deviceContext, size_t maxIndices, size_t maxVertices, size_t vertexSize);

    void Begin();
    void End();

    void Draw(D3D11_PRIMITIVE_TOPOLOGY topology, bool isIndexed, _In_opt_count_(indexCount) uint16_t const* indices, size_t indexCount, size_t vertexCount, _Out_ void** pMappedVertices);

private:
    void FlushBatch();

#if defined(_XBOX_ONE) && defined(_TITLE)
    ComPtr<ID3D11DeviceContextX> mDeviceContext;
#else
    ComPtr<ID3D11DeviceContext> mDeviceContext;
#endif
    ComPtr<ID3D11Buffer> mIndexBuffer;
    ComPtr<ID3D11Buffer> mVertexBuffer;

    size_t mMaxIndices;
    size_t mMaxVertices;
    size_t mVertexSize;

    D3D11_PRIMITIVE_TOPOLOGY mCurrentTopology;
    bool mInBeginEndPair;
    bool mCurrentlyIndexed;

    size_t mCurrentIndex;
    size_t mCurrentVertex;

    size_t mBaseIndex;
    size_t mBaseVertex;

#if defined(_XBOX_ONE) && defined(_TITLE)
    void *grfxMemoryIB;
    void *grfxMemoryVB;
#else
    D3D11_MAPPED_SUBRESOURCE mMappedIndices;
    D3D11_MAPPED_SUBRESOURCE mMappedVertices;
#endif
};


namespace
{
    // Helper for creating a D3D vertex or index buffer.
#if defined(_XBOX_ONE) && defined(_TITLE)
    void CreateDynamicBuffer(_In_ ID3D11DeviceX* device, uint32_t bufferSize, D3D11_BIND_FLAG bindFlag, _Outptr_ ID3D11Buffer** pBuffer)
    {
        D3D11_BUFFER_DESC desc = {};

        desc.ByteWidth = bufferSize;
        desc.BindFlags = bindFlag;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        ThrowIfFailed(
            device->CreatePlacementBuffer(&desc, nullptr, pBuffer)
        );

        SetDebugObjectName(*pBuffer, "DirectXTK:PrimitiveBatch");
    }
#else
    void CreateDynamicBuffer(_In_ ID3D11Device* device, uint32_t bufferSize, D3D11_BIND_FLAG bindFlag, _Outptr_ ID3D11Buffer** pBuffer)
    {
        D3D11_BUFFER_DESC desc = {};

        desc.ByteWidth = bufferSize;
        desc.BindFlags = bindFlag;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        ThrowIfFailed(
            device->CreateBuffer(&desc, nullptr, pBuffer)
        );

        assert(pBuffer != nullptr && *pBuffer != nullptr);
        _Analysis_assume_(pBuffer != nullptr && *pBuffer != nullptr);

        SetDebugObjectName(*pBuffer, "DirectXTK:PrimitiveBatch");
    }
#endif
}


// Constructor.
PrimitiveBatchBase::Impl::Impl(_In_ ID3D11DeviceContext* deviceContext, size_t maxIndices, size_t maxVertices, size_t vertexSize)
    : mMaxIndices(maxIndices),
    mMaxVertices(maxVertices),
    mVertexSize(vertexSize),
    mCurrentTopology(D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED),
    mInBeginEndPair(false),
    mCurrentlyIndexed(false),
    mCurrentIndex(0),
    mCurrentVertex(0),
    mBaseIndex(0),
    mBaseVertex(0),
#if defined(_XBOX_ONE) && defined(_TITLE)
    grfxMemoryIB(nullptr),
    grfxMemoryVB(nullptr)
#else
    mMappedIndices
{
},
mMappedVertices{}
#endif
{
    ComPtr<ID3D11Device> device;
    deviceContext->GetDevice(&device);

    if (!maxVertices)
        throw std::invalid_argument("maxVertices must be greater than 0");

    if (vertexSize > D3D11_REQ_MULTI_ELEMENT_STRUCTURE_SIZE_IN_BYTES)
        throw std::invalid_argument("Vertex size is too large for DirectX 11");

    const uint64_t ibBytes = uint64_t(maxIndices) * sizeof(uint16_t);
    if (ibBytes > uint64_t(D3D11_REQ_RESOURCE_SIZE_IN_MEGABYTES_EXPRESSION_A_TERM * 1024u * 1024u)
        || ibBytes > UINT32_MAX)
        throw std::invalid_argument("IB too large for DirectX 11");

    const uint64_t vbBytes = uint64_t(maxVertices) * uint64_t(vertexSize);
    if (vbBytes > uint64_t(D3D11_REQ_RESOURCE_SIZE_IN_MEGABYTES_EXPRESSION_A_TERM * 1024u * 1024u)
        || vbBytes > UINT32_MAX)
        throw std::invalid_argument("VB too large for DirectX 11");

#if defined(_XBOX_ONE) && defined(_TITLE)
    ThrowIfFailed(deviceContext->QueryInterface(IID_GRAPHICS_PPV_ARGS(mDeviceContext.GetAddressOf())));

    ComPtr<ID3D11DeviceX> deviceX;
    ThrowIfFailed(device.As(&deviceX));

    // If you only intend to draw non-indexed geometry, specify maxIndices = 0 to skip creating the index buffer.
    if (maxIndices > 0)
    {
        CreateDynamicBuffer(deviceX.Get(), static_cast<uint32_t>(ibBytes), D3D11_BIND_INDEX_BUFFER, &mIndexBuffer);
    }

    // Create the vertex buffer.
    CreateDynamicBuffer(deviceX.Get(), static_cast<uint32_t>(vbBytes), D3D11_BIND_VERTEX_BUFFER, &mVertexBuffer);

    grfxMemoryIB = grfxMemoryVB = nullptr;
#else
    mDeviceContext = deviceContext;

    // If you only intend to draw non-indexed geometry, specify maxIndices = 0 to skip creating the index buffer.
    if (maxIndices > 0)
    {
        CreateDynamicBuffer(device.Get(), static_cast<uint32_t>(ibBytes), D3D11_BIND_INDEX_BUFFER, &mIndexBuffer);
    }

    // Create the vertex buffer.
    CreateDynamicBuffer(device.Get(), static_cast<uint32_t>(vbBytes), D3D11_BIND_VERTEX_BUFFER, &mVertexBuffer);
#endif
}


// Begins a batch of primitive drawing operations.
void PrimitiveBatchBase::Impl::Begin()
{
    if (mInBeginEndPair)
        throw std::logic_error("Cannot nest Begin calls");

#if defined(_XBOX_ONE) && defined(_TITLE)
    mDeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
#else
    // Bind the index buffer.
    if (mMaxIndices > 0)
    {
        mDeviceContext->IASetIndexBuffer(mIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
    }

    // Bind the vertex buffer.
    auto vertexBuffer = mVertexBuffer.Get();
    const UINT vertexStride = static_cast<UINT>(mVertexSize);
    constexpr UINT vertexOffset = 0;

    mDeviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &vertexStride, &vertexOffset);
#endif

    // If this is a deferred D3D context, reset position so the first Map calls will use D3D11_MAP_WRITE_DISCARD.
    if (mDeviceContext->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED)
    {
        mCurrentIndex = 0;
        mCurrentVertex = 0;
    }

    mInBeginEndPair = true;
}


// Ends a batch of primitive drawing operations.
void PrimitiveBatchBase::Impl::End()
{
    if (!mInBeginEndPair)
        throw std::logic_error("Begin must be called before End");

    FlushBatch();

    mInBeginEndPair = false;
}


namespace
{
    // Can we combine adjacent primitives using this topology into a single draw call?
    bool CanBatchPrimitives(D3D11_PRIMITIVE_TOPOLOGY topology) noexcept
    {
        switch (topology)
        {
        case D3D11_PRIMITIVE_TOPOLOGY_POINTLIST:
        case D3D11_PRIMITIVE_TOPOLOGY_LINELIST:
        case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
            // Lists can easily be merged.
            return true;

        default:
            // Strips cannot.
            return false;
        }

        // We could also merge indexed strips by inserting degenerates,
        // but that's not always a perf win, so let's keep things simple.
    }


#if !defined(_XBOX_ONE) || !defined(_TITLE)
    // Helper for locking a vertex or index buffer.
    void LockBuffer(_In_ ID3D11DeviceContext* deviceContext, _In_ ID3D11Buffer* buffer, size_t currentPosition, _Out_ size_t* basePosition, _Out_ D3D11_MAPPED_SUBRESOURCE* mappedResource)
    {
        const D3D11_MAP mapType = (currentPosition == 0) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;

        ThrowIfFailed(
            deviceContext->Map(buffer, 0, mapType, 0, mappedResource)
        );

        *basePosition = currentPosition;
    }
#endif
}


// Adds new geometry to the batch.
_Use_decl_annotations_
void PrimitiveBatchBase::Impl::Draw(D3D11_PRIMITIVE_TOPOLOGY topology, bool isIndexed, uint16_t const* indices, size_t indexCount, size_t vertexCount, void** pMappedVertices)
{
    if (isIndexed && !indices)
        throw std::invalid_argument("Indices cannot be null");

    if (indexCount >= mMaxIndices)
        throw std::out_of_range("Too many indices");

    if (vertexCount >= mMaxVertices)
        throw std::out_of_range("Too many vertices");

    if (!mInBeginEndPair)
        throw std::logic_error("Begin must be called before Draw");

    // Can we merge this primitive in with an existing batch, or must we flush first?
    const bool wrapIndexBuffer = (mCurrentIndex + indexCount > mMaxIndices);
    const bool wrapVertexBuffer = (mCurrentVertex + vertexCount > mMaxVertices);

    if ((topology != mCurrentTopology) ||
        (isIndexed != mCurrentlyIndexed) ||
        !CanBatchPrimitives(topology) ||
        wrapIndexBuffer || wrapVertexBuffer)
    {
        FlushBatch();
    }

#if defined(_XBOX_ONE) && defined(_TITLE)
    if (mCurrentTopology == D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
    {
        auto& grfxMem = GraphicsMemory::Get();

        if (isIndexed)
        {
            grfxMemoryIB = grfxMem.Allocate(mDeviceContext.Get(), mMaxIndices * sizeof(uint16_t), 64);
        }

        grfxMemoryVB = grfxMem.Allocate(mDeviceContext.Get(), mMaxVertices * mVertexSize, 64);

        mCurrentTopology = topology;
        mCurrentlyIndexed = isIndexed;
        mCurrentIndex = mCurrentVertex = 0;
    }

    // Copy over the index data.
    if (isIndexed)
    {
        assert(grfxMemoryIB != nullptr);
        auto outputIndices = reinterpret_cast<uint16_t*>(grfxMemoryIB) + mCurrentIndex;

        for (size_t i = 0; i < indexCount; i++)
        {
            outputIndices[i] = (uint16_t)(indices[i] + mCurrentVertex);
        }

        mCurrentIndex += indexCount;
    }

    // Return the output vertex data location.
    assert(grfxMemoryVB != nullptr);
    *pMappedVertices = reinterpret_cast<uint8_t*>(grfxMemoryVB) + (mCurrentVertex * mVertexSize);

    mCurrentVertex += vertexCount;
#else
    if (wrapIndexBuffer)
        mCurrentIndex = 0;

    if (wrapVertexBuffer)
        mCurrentVertex = 0;

    // If we are not already in a batch, lock the buffers.
    if (mCurrentTopology == D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
    {
        if (isIndexed)
        {
            LockBuffer(mDeviceContext.Get(), mIndexBuffer.Get(), mCurrentIndex, &mBaseIndex, &mMappedIndices);
        }

        LockBuffer(mDeviceContext.Get(), mVertexBuffer.Get(), mCurrentVertex, &mBaseVertex, &mMappedVertices);

        mCurrentTopology = topology;
        mCurrentlyIndexed = isIndexed;
    }

    // Copy over the index data.
    if (isIndexed)
    {
        auto outputIndices = static_cast<uint16_t*>(mMappedIndices.pData) + mCurrentIndex;

        for (size_t i = 0; i < indexCount; i++)
        {
            outputIndices[i] = static_cast<uint16_t>(indices[i] + mCurrentVertex - mBaseVertex);
        }

        mCurrentIndex += indexCount;
    }

    // Return the output vertex data location.
    *pMappedVertices = static_cast<uint8_t*>(mMappedVertices.pData) + (mCurrentVertex * mVertexSize);

    mCurrentVertex += vertexCount;
#endif
}


// Sends queued primitives to the graphics device.
void PrimitiveBatchBase::Impl::FlushBatch()
{
    // Early out if there is nothing to flush.
    if (mCurrentTopology == D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
        return;

    mDeviceContext->IASetPrimitiveTopology(mCurrentTopology);

#if defined(_XBOX_ONE) && defined(_TITLE)
    if (mCurrentlyIndexed)
    {
        // Draw indexed geometry.
        mDeviceContext->IASetPlacementIndexBuffer(mIndexBuffer.Get(), grfxMemoryIB, DXGI_FORMAT_R16_UINT);
        mDeviceContext->IASetPlacementVertexBuffer(0, mVertexBuffer.Get(), grfxMemoryVB, (UINT)mVertexSize);

        mDeviceContext->DrawIndexed((UINT)mCurrentIndex, 0, 0);
    }
    else
    {
        // Draw non-indexed geometry.
        mDeviceContext->IASetPlacementVertexBuffer(0, mVertexBuffer.Get(), grfxMemoryVB, (UINT)mVertexSize);

        mDeviceContext->Draw((UINT)mCurrentVertex, 0);
    }

    grfxMemoryIB = grfxMemoryVB = nullptr;
#else
    mDeviceContext->Unmap(mVertexBuffer.Get(), 0);

    if (mCurrentlyIndexed)
    {
        // Draw indexed geometry.
        mDeviceContext->Unmap(mIndexBuffer.Get(), 0);

        mDeviceContext->DrawIndexed(
            static_cast<UINT>(mCurrentIndex - mBaseIndex),
            static_cast<UINT>(mBaseIndex),
            static_cast<INT>(mBaseVertex));
    }
    else
    {
        // Draw non-indexed geometry.
        mDeviceContext->Draw(static_cast<UINT>(mCurrentVertex - mBaseVertex), static_cast<UINT>(mBaseVertex));
    }
#endif

    mCurrentTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
}


// Public constructor.
PrimitiveBatchBase::PrimitiveBatchBase(_In_ ID3D11DeviceContext* deviceContext, size_t maxIndices, size_t maxVertices, size_t vertexSize)
    : pImpl(std::make_unique<Impl>(deviceContext, maxIndices, maxVertices, vertexSize))
{
}


PrimitiveBatchBase::PrimitiveBatchBase(PrimitiveBatchBase&&) noexcept = default;
PrimitiveBatchBase& PrimitiveBatchBase::operator= (PrimitiveBatchBase&&) noexcept = default;
PrimitiveBatchBase::~PrimitiveBatchBase() = default;


// Begin a primitive batch.
void PrimitiveBatchBase::Begin()
{
    pImpl->Begin();
}


// End/draw a primitive batch.
void PrimitiveBatchBase::End()
{
    pImpl->End();
}


// Custom draw method.
_Use_decl_annotations_
void PrimitiveBatchBase::Draw(D3D11_PRIMITIVE_TOPOLOGY topology, bool isIndexed, uint16_t const* indices, size_t indexCount, size_t vertexCount, void** pMappedVertices)
{
    pImpl->Draw(topology, isIndexed, indices, indexCount, vertexCount, pMappedVertices);
}
