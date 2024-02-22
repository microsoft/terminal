//--------------------------------------------------------------------------------------
// File: Geometry.h
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#include "VertexTypes.h"

namespace DirectX
{
    using VertexCollection = std::vector<DirectX::VertexPositionNormalTexture>;
    using IndexCollection = std::vector<uint16_t>;

    void ComputeBox(VertexCollection& vertices, IndexCollection& indices, const XMFLOAT3& size, bool rhcoords, bool invertn);
    void ComputeSphere(VertexCollection& vertices, IndexCollection& indices, float diameter, size_t tessellation, bool rhcoords, bool invertn);
    void ComputeGeoSphere(VertexCollection& vertices, IndexCollection& indices, float diameter, size_t tessellation, bool rhcoords);
    void ComputeCylinder(VertexCollection& vertices, IndexCollection& indices, float height, float diameter, size_t tessellation, bool rhcoords);
    void ComputeCone(VertexCollection& vertices, IndexCollection& indices, float diameter, float height, size_t tessellation, bool rhcoords);
    void ComputeTorus(VertexCollection& vertices, IndexCollection& indices, float diameter, float thickness, size_t tessellation, bool rhcoords);
    void ComputeTetrahedron(VertexCollection& vertices, IndexCollection& indices, float size, bool rhcoords);
    void ComputeOctahedron(VertexCollection& vertices, IndexCollection& indices, float size, bool rhcoords);
    void ComputeDodecahedron(VertexCollection& vertices, IndexCollection& indices, float size, bool rhcoords);
    void ComputeIcosahedron(VertexCollection& vertices, IndexCollection& indices, float size, bool rhcoords);
    void ComputeTeapot(VertexCollection& vertices, IndexCollection& indices, float size, size_t tessellation, bool rhcoords);
}
