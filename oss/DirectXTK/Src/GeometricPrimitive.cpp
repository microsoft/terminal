//--------------------------------------------------------------------------------------
// File: GeometricPrimitive.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "GeometricPrimitive.h"
#include "BufferHelpers.h"
#include "CommonStates.h"
#include "DirectXHelpers.h"
#include "Effects.h"
#include "Geometry.h"
#include "SharedResourcePool.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;


// Internal GeometricPrimitive implementation class.
class GeometricPrimitive::Impl
{
public:
    Impl() noexcept : mIndexCount(0) {}

    void Initialize(_In_ ID3D11DeviceContext* deviceContext, const VertexCollection& vertices, const IndexCollection& indices);

    void XM_CALLCONV Draw(FXMMATRIX world, CXMMATRIX view, CXMMATRIX projection,
        FXMVECTOR color,
        _In_opt_ ID3D11ShaderResourceView* texture,
        bool wireframe,
        const std::function<void()>& setCustomState) const;

    void Draw(_In_ IEffect* effect,
        _In_ ID3D11InputLayout* inputLayout,
        bool alpha, bool wireframe,
        const std::function<void()>& setCustomState) const;

    void DrawInstanced(_In_ IEffect* effect,
        _In_ ID3D11InputLayout* inputLayout,
        uint32_t instanceCount,
        bool alpha, bool wireframe,
        uint32_t startInstanceLocation,
        const std::function<void()>& setCustomState) const;

    void CreateInputLayout(_In_ IEffect* effect, _Outptr_ ID3D11InputLayout** inputLayout) const;

private:
    ComPtr<ID3D11Buffer> mVertexBuffer;
    ComPtr<ID3D11Buffer> mIndexBuffer;

    UINT mIndexCount;

    // Only one of these helpers is allocated per D3D device context, even if there are multiple GeometricPrimitive instances.
    class SharedResources
    {
    public:
        SharedResources(_In_ ID3D11DeviceContext* deviceContext);

        void PrepareForRendering(bool alpha, bool wireframe) const;

        ComPtr<ID3D11DeviceContext> mDeviceContext;
        std::unique_ptr<BasicEffect> effect;

        ComPtr<ID3D11InputLayout> inputLayoutTextured;
        ComPtr<ID3D11InputLayout> inputLayoutUntextured;

        std::unique_ptr<CommonStates> stateObjects;
    };


    // Per-device-context data.
    std::shared_ptr<SharedResources> mResources;

    static SharedResourcePool<ID3D11DeviceContext*, SharedResources> sharedResourcesPool;
};


// Global pool of per-device-context GeometricPrimitive resources.
SharedResourcePool<ID3D11DeviceContext*, GeometricPrimitive::Impl::SharedResources> GeometricPrimitive::Impl::sharedResourcesPool;


// Per-device-context constructor.
GeometricPrimitive::Impl::SharedResources::SharedResources(_In_ ID3D11DeviceContext* deviceContext)
    : mDeviceContext(deviceContext)
{
    ComPtr<ID3D11Device> device;
    deviceContext->GetDevice(&device);

    // Create the BasicEffect.
    effect = std::make_unique<BasicEffect>(device.Get());

    effect->EnableDefaultLighting();

    // Create state objects.
    stateObjects = std::make_unique<CommonStates>(device.Get());

    // Create input layouts.
    effect->SetTextureEnabled(true);
    ThrowIfFailed(
        CreateInputLayoutFromEffect<VertexType>(device.Get(), effect.get(), &inputLayoutTextured)
    );

    SetDebugObjectName(inputLayoutTextured.Get(), "DirectXTK:GeometricPrimitive");

    effect->SetTextureEnabled(false);
    ThrowIfFailed(
        CreateInputLayoutFromEffect<VertexType>(device.Get(), effect.get(), &inputLayoutUntextured)
    );

    SetDebugObjectName(inputLayoutUntextured.Get(), "DirectXTK:GeometricPrimitive");
}


// Sets up D3D device state ready for drawing a primitive.
void GeometricPrimitive::Impl::SharedResources::PrepareForRendering(bool alpha, bool wireframe) const
{
    // Set the blend and depth stencil state.
    ID3D11BlendState* blendState;
    ID3D11DepthStencilState* depthStencilState;

    if (alpha)
    {
        // Alpha blended rendering.
        blendState = stateObjects->AlphaBlend();
        depthStencilState = s_reversez ? stateObjects->DepthReadReverseZ() : stateObjects->DepthRead();
    }
    else
    {
        // Opaque rendering.
        blendState = stateObjects->Opaque();
        depthStencilState = s_reversez ? stateObjects->DepthReverseZ() : stateObjects->DepthDefault();
    }

    mDeviceContext->OMSetBlendState(blendState, nullptr, 0xFFFFFFFF);
    mDeviceContext->OMSetDepthStencilState(depthStencilState, 0);

    // Set the rasterizer state.
    if (wireframe)
        mDeviceContext->RSSetState(stateObjects->Wireframe());
    else
        mDeviceContext->RSSetState(stateObjects->CullCounterClockwise());

    ID3D11SamplerState* samplerState = stateObjects->LinearWrap();

    mDeviceContext->PSSetSamplers(0, 1, &samplerState);
}


// Initializes a geometric primitive instance that will draw the specified vertex and index data.
_Use_decl_annotations_
void GeometricPrimitive::Impl::Initialize(ID3D11DeviceContext* deviceContext, const VertexCollection& vertices, const IndexCollection& indices)
{
    if (vertices.size() >= USHRT_MAX)
        throw std::out_of_range("Too many vertices for 16-bit index buffer");

    if (indices.size() > UINT32_MAX)
        throw std::out_of_range("Too many indices");

    mResources = sharedResourcesPool.DemandCreate(deviceContext);

    ComPtr<ID3D11Device> device;
    deviceContext->GetDevice(&device);

    ThrowIfFailed(
        CreateStaticBuffer(device.Get(), vertices, D3D11_BIND_VERTEX_BUFFER, mVertexBuffer.ReleaseAndGetAddressOf())
    );

    ThrowIfFailed(
        CreateStaticBuffer(device.Get(), indices, D3D11_BIND_INDEX_BUFFER, mIndexBuffer.ReleaseAndGetAddressOf())
    );

    SetDebugObjectName(mVertexBuffer.Get(), "DirectXTK:GeometricPrimitive");
    SetDebugObjectName(mIndexBuffer.Get(), "DirectXTK:GeometricPrimitive");

    mIndexCount = static_cast<UINT>(indices.size());
}


// Draws the primitive.
_Use_decl_annotations_
void XM_CALLCONV GeometricPrimitive::Impl::Draw(
    FXMMATRIX world,
    CXMMATRIX view,
    CXMMATRIX projection,
    FXMVECTOR color,
    ID3D11ShaderResourceView* texture,
    bool wireframe,
    const std::function<void()>& setCustomState) const
{
    assert(mResources);
    auto effect = mResources->effect.get();
    assert(effect != nullptr);

    ID3D11InputLayout *inputLayout;
    if (texture)
    {
        effect->SetTextureEnabled(true);
        effect->SetTexture(texture);

        inputLayout = mResources->inputLayoutTextured.Get();
    }
    else
    {
        effect->SetTextureEnabled(false);

        inputLayout = mResources->inputLayoutUntextured.Get();
    }

    // Set effect parameters.
    effect->SetMatrices(world, view, projection);

    effect->SetColorAndAlpha(color);

    const float alpha = XMVectorGetW(color);
    Draw(effect, inputLayout, (alpha < 1.f), wireframe, setCustomState);
}


// Draw the primitive using a custom effect.
_Use_decl_annotations_
void GeometricPrimitive::Impl::Draw(
    IEffect* effect,
    ID3D11InputLayout* inputLayout,
    bool alpha,
    bool wireframe,
    const std::function<void()>& setCustomState) const
{
    assert(mResources);
    auto deviceContext = mResources->mDeviceContext.Get();
    assert(deviceContext != nullptr);

    // Set state objects.
    mResources->PrepareForRendering(alpha, wireframe);

    // Set input layout.
    assert(inputLayout != nullptr);
    deviceContext->IASetInputLayout(inputLayout);

    // Activate our shaders, constant buffers, texture, etc.
    assert(effect != nullptr);
    effect->Apply(deviceContext);

    // Set the vertex and index buffer.
    auto vertexBuffer = mVertexBuffer.Get();
    constexpr UINT vertexStride = sizeof(VertexType);
    constexpr UINT vertexOffset = 0;

    deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &vertexStride, &vertexOffset);

    deviceContext->IASetIndexBuffer(mIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

    // Hook lets the caller replace our shaders or state settings with whatever else they see fit.
    if (setCustomState)
    {
        setCustomState();
    }

    // Draw the primitive.
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    deviceContext->DrawIndexed(mIndexCount, 0, 0);
}

_Use_decl_annotations_
void GeometricPrimitive::Impl::DrawInstanced(
    IEffect* effect,
    ID3D11InputLayout* inputLayout,
    uint32_t instanceCount,
    bool alpha,
    bool wireframe,
    uint32_t startInstanceLocation,
    const std::function<void()>& setCustomState) const
{
    assert(mResources);
    auto deviceContext = mResources->mDeviceContext.Get();
    assert(deviceContext != nullptr);

    // Set state objects.
    mResources->PrepareForRendering(alpha, wireframe);

    // Set input layout.
    assert(inputLayout != nullptr);
    deviceContext->IASetInputLayout(inputLayout);

    // Activate our shaders, constant buffers, texture, etc.
    assert(effect != nullptr);
    effect->Apply(deviceContext);

    // Set the vertex and index buffer.
    auto vertexBuffer = mVertexBuffer.Get();
    constexpr UINT vertexStride = sizeof(VertexType);
    constexpr UINT vertexOffset = 0;

    deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &vertexStride, &vertexOffset);

    deviceContext->IASetIndexBuffer(mIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

    // Hook lets the caller replace our shaders or state settings with whatever else they see fit.
    if (setCustomState)
    {
        setCustomState();
    }

    // Draw the primitive.
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    deviceContext->DrawIndexedInstanced(mIndexCount, instanceCount, 0, 0, startInstanceLocation);
}


// Create input layout for drawing with a custom effect.
_Use_decl_annotations_
void GeometricPrimitive::Impl::CreateInputLayout(IEffect* effect, ID3D11InputLayout** inputLayout) const
{
    assert(effect != nullptr);
    assert(inputLayout != nullptr);

    assert(mResources);
    auto deviceContext = mResources->mDeviceContext.Get();
    assert(deviceContext != nullptr);

    ComPtr<ID3D11Device> device;
    deviceContext->GetDevice(&device);

    ThrowIfFailed(
        CreateInputLayoutFromEffect<VertexType>(device.Get(), effect, inputLayout)
    );

    assert(inputLayout != nullptr && *inputLayout != nullptr);
    _Analysis_assume_(inputLayout != nullptr && *inputLayout != nullptr);

    SetDebugObjectName(*inputLayout, "DirectXTK:GeometricPrimitive");
}


//--------------------------------------------------------------------------------------
// GeometricPrimitive
//--------------------------------------------------------------------------------------

bool GeometricPrimitive::s_reversez = false;

// Constructor.
GeometricPrimitive::GeometricPrimitive() noexcept(false)
    : pImpl(std::make_unique<Impl>())
{
}


// Destructor.
GeometricPrimitive::~GeometricPrimitive()
{
}


// Public entrypoints.
_Use_decl_annotations_
void XM_CALLCONV GeometricPrimitive::Draw(
    FXMMATRIX world,
    CXMMATRIX view,
    CXMMATRIX projection,
    FXMVECTOR color,
    ID3D11ShaderResourceView* texture,
    bool wireframe,
    std::function<void()> setCustomState) const
{
    pImpl->Draw(world, view, projection, color, texture, wireframe, setCustomState);
}


_Use_decl_annotations_
void GeometricPrimitive::Draw(
    IEffect* effect,
    ID3D11InputLayout* inputLayout,
    bool alpha,
    bool wireframe,
    std::function<void()> setCustomState) const
{
    pImpl->Draw(effect, inputLayout, alpha, wireframe, setCustomState);
}


_Use_decl_annotations_
void GeometricPrimitive::DrawInstanced(
    IEffect* effect,
    ID3D11InputLayout* inputLayout,
    uint32_t instanceCount,
    bool alpha,
    bool wireframe,
    uint32_t startInstanceLocation,
    std::function<void()> setCustomState) const
{
    pImpl->DrawInstanced(effect, inputLayout, instanceCount, alpha, wireframe, startInstanceLocation, setCustomState);
}


_Use_decl_annotations_
void GeometricPrimitive::CreateInputLayout(IEffect* effect, ID3D11InputLayout** inputLayout) const
{
    pImpl->CreateInputLayout(effect, inputLayout);
}


//--------------------------------------------------------------------------------------
// Cube (aka a Hexahedron) or Box
//--------------------------------------------------------------------------------------

_Use_decl_annotations_
std::unique_ptr<GeometricPrimitive> GeometricPrimitive::CreateCube(
    ID3D11DeviceContext* deviceContext,
    float size,
    bool rhcoords)
{
    VertexCollection vertices;
    IndexCollection indices;
    ComputeBox(vertices, indices, XMFLOAT3(size, size, size), rhcoords, false);

    // Create the primitive object.
    std::unique_ptr<GeometricPrimitive> primitive(new GeometricPrimitive());

    primitive->pImpl->Initialize(deviceContext, vertices, indices);

    return primitive;
}

void GeometricPrimitive::CreateCube(
    VertexCollection& vertices,
    IndexCollection& indices,
    float size,
    bool rhcoords)
{
    ComputeBox(vertices, indices, XMFLOAT3(size, size, size), rhcoords, false);
}


// Creates a box primitive.
_Use_decl_annotations_
std::unique_ptr<GeometricPrimitive> GeometricPrimitive::CreateBox(
    ID3D11DeviceContext* deviceContext,
    const XMFLOAT3& size,
    bool rhcoords,
    bool invertn)
{
    VertexCollection vertices;
    IndexCollection indices;
    ComputeBox(vertices, indices, size, rhcoords, invertn);

    // Create the primitive object.
    std::unique_ptr<GeometricPrimitive> primitive(new GeometricPrimitive());

    primitive->pImpl->Initialize(deviceContext, vertices, indices);

    return primitive;
}

void GeometricPrimitive::CreateBox(
    VertexCollection& vertices,
    IndexCollection& indices,
    const XMFLOAT3& size,
    bool rhcoords,
    bool invertn)
{
    ComputeBox(vertices, indices, size, rhcoords, invertn);
}


//--------------------------------------------------------------------------------------
// Sphere
//--------------------------------------------------------------------------------------

_Use_decl_annotations_
std::unique_ptr<GeometricPrimitive> GeometricPrimitive::CreateSphere(
    ID3D11DeviceContext* deviceContext,
    float diameter,
    size_t tessellation,
    bool rhcoords,
    bool invertn)
{
    VertexCollection vertices;
    IndexCollection indices;
    ComputeSphere(vertices, indices, diameter, tessellation, rhcoords, invertn);

    // Create the primitive object.
    std::unique_ptr<GeometricPrimitive> primitive(new GeometricPrimitive());

    primitive->pImpl->Initialize(deviceContext, vertices, indices);

    return primitive;
}

void GeometricPrimitive::CreateSphere(
    VertexCollection& vertices,
    IndexCollection& indices,
    float diameter,
    size_t tessellation,
    bool rhcoords,
    bool invertn)
{
    ComputeSphere(vertices, indices, diameter, tessellation, rhcoords, invertn);
}


//--------------------------------------------------------------------------------------
// Geodesic sphere
//--------------------------------------------------------------------------------------

_Use_decl_annotations_
std::unique_ptr<GeometricPrimitive> GeometricPrimitive::CreateGeoSphere(
    ID3D11DeviceContext* deviceContext,
    float diameter,
    size_t tessellation,
    bool rhcoords)
{
    VertexCollection vertices;
    IndexCollection indices;
    ComputeGeoSphere(vertices, indices, diameter, tessellation, rhcoords);

    // Create the primitive object.
    std::unique_ptr<GeometricPrimitive> primitive(new GeometricPrimitive());

    primitive->pImpl->Initialize(deviceContext, vertices, indices);

    return primitive;
}

void GeometricPrimitive::CreateGeoSphere(
    VertexCollection& vertices,
    IndexCollection& indices,
    float diameter,
    size_t tessellation, bool rhcoords)
{
    ComputeGeoSphere(vertices, indices, diameter, tessellation, rhcoords);
}


//--------------------------------------------------------------------------------------
// Cylinder / Cone
//--------------------------------------------------------------------------------------

// Creates a cylinder primitive.
_Use_decl_annotations_
std::unique_ptr<GeometricPrimitive> GeometricPrimitive::CreateCylinder(
    ID3D11DeviceContext* deviceContext,
    float height,
    float diameter,
    size_t tessellation,
    bool rhcoords)
{
    VertexCollection vertices;
    IndexCollection indices;
    ComputeCylinder(vertices, indices, height, diameter, tessellation, rhcoords);

    // Create the primitive object.
    std::unique_ptr<GeometricPrimitive> primitive(new GeometricPrimitive());

    primitive->pImpl->Initialize(deviceContext, vertices, indices);

    return primitive;
}

void GeometricPrimitive::CreateCylinder(
    VertexCollection& vertices,
    IndexCollection& indices,
    float height,
    float diameter,
    size_t tessellation,
    bool rhcoords)
{
    ComputeCylinder(vertices, indices, height, diameter, tessellation, rhcoords);
}


// Creates a cone primitive.
_Use_decl_annotations_
std::unique_ptr<GeometricPrimitive> GeometricPrimitive::CreateCone(
    ID3D11DeviceContext* deviceContext,
    float diameter,
    float height,
    size_t tessellation,
    bool rhcoords)
{
    VertexCollection vertices;
    IndexCollection indices;
    ComputeCone(vertices, indices, diameter, height, tessellation, rhcoords);

    // Create the primitive object.
    std::unique_ptr<GeometricPrimitive> primitive(new GeometricPrimitive());

    primitive->pImpl->Initialize(deviceContext, vertices, indices);

    return primitive;
}

void GeometricPrimitive::CreateCone(
    VertexCollection& vertices,
    IndexCollection& indices,
    float diameter,
    float height,
    size_t tessellation,
    bool rhcoords)
{
    ComputeCone(vertices, indices, diameter, height, tessellation, rhcoords);
}


//--------------------------------------------------------------------------------------
// Torus
//--------------------------------------------------------------------------------------

_Use_decl_annotations_
std::unique_ptr<GeometricPrimitive> GeometricPrimitive::CreateTorus(
    ID3D11DeviceContext* deviceContext,
    float diameter,
    float thickness,
    size_t tessellation,
    bool rhcoords)
{
    VertexCollection vertices;
    IndexCollection indices;
    ComputeTorus(vertices, indices, diameter, thickness, tessellation, rhcoords);

    // Create the primitive object.
    std::unique_ptr<GeometricPrimitive> primitive(new GeometricPrimitive());

    primitive->pImpl->Initialize(deviceContext, vertices, indices);

    return primitive;
}

void GeometricPrimitive::CreateTorus(
    VertexCollection& vertices,
    IndexCollection& indices,
    float diameter,
    float thickness,
    size_t tessellation,
    bool rhcoords)
{
    ComputeTorus(vertices, indices, diameter, thickness, tessellation, rhcoords);
}


//--------------------------------------------------------------------------------------
// Tetrahedron
//--------------------------------------------------------------------------------------

_Use_decl_annotations_
std::unique_ptr<GeometricPrimitive> GeometricPrimitive::CreateTetrahedron(
    ID3D11DeviceContext* deviceContext,
    float size,
    bool rhcoords)
{
    VertexCollection vertices;
    IndexCollection indices;
    ComputeTetrahedron(vertices, indices, size, rhcoords);

    // Create the primitive object.
    std::unique_ptr<GeometricPrimitive> primitive(new GeometricPrimitive());

    primitive->pImpl->Initialize(deviceContext, vertices, indices);

    return primitive;
}

void GeometricPrimitive::CreateTetrahedron(
    VertexCollection& vertices,
    IndexCollection& indices,
    float size,
    bool rhcoords)
{
    ComputeTetrahedron(vertices, indices, size, rhcoords);
}


//--------------------------------------------------------------------------------------
// Octahedron
//--------------------------------------------------------------------------------------

_Use_decl_annotations_
std::unique_ptr<GeometricPrimitive> GeometricPrimitive::CreateOctahedron(
    ID3D11DeviceContext* deviceContext,
    float size,
    bool rhcoords)
{
    VertexCollection vertices;
    IndexCollection indices;
    ComputeOctahedron(vertices, indices, size, rhcoords);

    // Create the primitive object.
    std::unique_ptr<GeometricPrimitive> primitive(new GeometricPrimitive());

    primitive->pImpl->Initialize(deviceContext, vertices, indices);

    return primitive;
}

void GeometricPrimitive::CreateOctahedron(
    VertexCollection& vertices,
    IndexCollection& indices,
    float size,
    bool rhcoords)
{
    ComputeOctahedron(vertices, indices, size, rhcoords);
}


//--------------------------------------------------------------------------------------
// Dodecahedron
//--------------------------------------------------------------------------------------

_Use_decl_annotations_
std::unique_ptr<GeometricPrimitive> GeometricPrimitive::CreateDodecahedron(
    ID3D11DeviceContext* deviceContext,
    float size,
    bool rhcoords)
{
    VertexCollection vertices;
    IndexCollection indices;
    ComputeDodecahedron(vertices, indices, size, rhcoords);

    // Create the primitive object.
    std::unique_ptr<GeometricPrimitive> primitive(new GeometricPrimitive());

    primitive->pImpl->Initialize(deviceContext, vertices, indices);

    return primitive;
}

void GeometricPrimitive::CreateDodecahedron(
    VertexCollection& vertices,
    IndexCollection& indices,
    float size,
    bool rhcoords)
{
    ComputeDodecahedron(vertices, indices, size, rhcoords);
}


//--------------------------------------------------------------------------------------
// Icosahedron
//--------------------------------------------------------------------------------------

_Use_decl_annotations_
std::unique_ptr<GeometricPrimitive> GeometricPrimitive::CreateIcosahedron(
    ID3D11DeviceContext* deviceContext,
    float size,
    bool rhcoords)
{
    VertexCollection vertices;
    IndexCollection indices;
    ComputeIcosahedron(vertices, indices, size, rhcoords);

    // Create the primitive object.
    std::unique_ptr<GeometricPrimitive> primitive(new GeometricPrimitive());

    primitive->pImpl->Initialize(deviceContext, vertices, indices);

    return primitive;
}

void GeometricPrimitive::CreateIcosahedron(
    VertexCollection& vertices,
    IndexCollection& indices,
    float size,
    bool rhcoords)
{
    ComputeIcosahedron(vertices, indices, size, rhcoords);
}


//--------------------------------------------------------------------------------------
// Teapot
//--------------------------------------------------------------------------------------

_Use_decl_annotations_
std::unique_ptr<GeometricPrimitive> GeometricPrimitive::CreateTeapot(
    ID3D11DeviceContext* deviceContext,
    float size,
    size_t tessellation,
    bool rhcoords)
{
    VertexCollection vertices;
    IndexCollection indices;
    ComputeTeapot(vertices, indices, size, tessellation, rhcoords);

    // Create the primitive object.
    std::unique_ptr<GeometricPrimitive> primitive(new GeometricPrimitive());

    primitive->pImpl->Initialize(deviceContext, vertices, indices);

    return primitive;
}

void GeometricPrimitive::CreateTeapot(
    VertexCollection& vertices,
    IndexCollection& indices,
    float size,
    size_t tessellation,
    bool rhcoords)
{
    ComputeTeapot(vertices, indices, size, tessellation, rhcoords);
}


//--------------------------------------------------------------------------------------
// Custom
//--------------------------------------------------------------------------------------

_Use_decl_annotations_
std::unique_ptr<GeometricPrimitive> GeometricPrimitive::CreateCustom(
    ID3D11DeviceContext* deviceContext,
    const VertexCollection& vertices,
    const IndexCollection& indices)
{
    // Extra validation
    if (vertices.empty() || indices.empty())
        throw std::invalid_argument("Requires both vertices and indices");

    if (indices.size() % 3)
        throw std::invalid_argument("Expected triangular faces");

    const size_t nVerts = vertices.size();
    if (nVerts >= USHRT_MAX)
        throw std::out_of_range("Too many vertices for 16-bit index buffer");

    for (const auto it : indices)
    {
        if (it >= nVerts)
        {
            throw std::out_of_range("Index not in vertices list");
        }
    }

    // Create the primitive object.
    std::unique_ptr<GeometricPrimitive> primitive(new GeometricPrimitive());

    primitive->pImpl->Initialize(deviceContext, vertices, indices);

    return primitive;
}
