//--------------------------------------------------------------------------------------
// File: Bezier.h
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#pragma once

#include <array>
#include <algorithm>
#include <DirectXMath.h>


namespace Bezier
{
    // Performs a cubic bezier interpolation between four control points,
    // returning the value at the specified time (t ranges 0 to 1).
    template<typename T>
    inline T CubicInterpolate(T const& p1, T const& p2, T const& p3, T const& p4, float t) noexcept
    {
        return p1 * (1 - t) * (1 - t) * (1 - t) +
            p2 * 3 * t * (1 - t) * (1 - t) +
            p3 * 3 * t * t * (1 - t) +
            p4 * t * t * t;
    }

    template<>
    inline DirectX::XMVECTOR CubicInterpolate(DirectX::XMVECTOR const& p1, DirectX::XMVECTOR const& p2, DirectX::XMVECTOR const& p3, DirectX::XMVECTOR const& p4, float t) noexcept
    {
        using namespace DirectX;

        const XMVECTOR T0 = XMVectorReplicate((1 - t) * (1 - t) * (1 - t));
        const XMVECTOR T1 = XMVectorReplicate(3 * t * (1 - t) * (1 - t));
        const XMVECTOR T2 = XMVectorReplicate(3 * t * t * (1 - t));
        const XMVECTOR T3 = XMVectorReplicate(t * t * t);

        XMVECTOR Result = XMVectorMultiply(p1, T0);
        Result = XMVectorMultiplyAdd(p2, T1, Result);
        Result = XMVectorMultiplyAdd(p3, T2, Result);
        Result = XMVectorMultiplyAdd(p4, T3, Result);

        return Result;
    }


    // Computes the tangent of a cubic bezier curve at the specified time.
    template<typename T>
    inline T CubicTangent(T const& p1, T const& p2, T const& p3, T const& p4, float t) noexcept
    {
        return p1 * (-1 + 2 * t - t * t) +
            p2 * (1 - 4 * t + 3 * t * t) +
            p3 * (2 * t - 3 * t * t) +
            p4 * (t * t);
    }

    template<>
    inline DirectX::XMVECTOR CubicTangent(DirectX::XMVECTOR const& p1, DirectX::XMVECTOR const& p2, DirectX::XMVECTOR const& p3, DirectX::XMVECTOR const& p4, float t) noexcept
    {
        using namespace DirectX;

        const XMVECTOR T0 = XMVectorReplicate(-1 + 2 * t - t * t);
        const XMVECTOR T1 = XMVectorReplicate(1 - 4 * t + 3 * t * t);
        const XMVECTOR T2 = XMVectorReplicate(2 * t - 3 * t * t);
        const XMVECTOR T3 = XMVectorReplicate(t * t);

        XMVECTOR Result = XMVectorMultiply(p1, T0);
        Result = XMVectorMultiplyAdd(p2, T1, Result);
        Result = XMVectorMultiplyAdd(p3, T2, Result);
        Result = XMVectorMultiplyAdd(p4, T3, Result);

        return Result;
    }


    // Creates vertices for a patch that is tessellated at the specified level.
    // Calls the specified outputVertex function for each generated vertex,
    // passing the position, normal, and texture coordinate as parameters.
    template<typename TOutputFunc>
    void CreatePatchVertices(_In_reads_(16) const DirectX::XMVECTOR patch[16], size_t tessellation, bool isMirrored, TOutputFunc outputVertex)
    {
        using namespace DirectX;

        for (size_t i = 0; i <= tessellation; i++)
        {
            const float u = float(i) / float(tessellation);

            for (size_t j = 0; j <= tessellation; j++)
            {
                const float v = float(j) / float(tessellation);

                // Perform four horizontal bezier interpolations
                // between the control points of this patch.
                const XMVECTOR p1 = CubicInterpolate(patch[0], patch[1], patch[2], patch[3], u);
                const XMVECTOR p2 = CubicInterpolate(patch[4], patch[5], patch[6], patch[7], u);
                const XMVECTOR p3 = CubicInterpolate(patch[8], patch[9], patch[10], patch[11], u);
                const XMVECTOR p4 = CubicInterpolate(patch[12], patch[13], patch[14], patch[15], u);

                // Perform a vertical interpolation between the results of the
                // previous horizontal interpolations, to compute the position.
                const XMVECTOR position = CubicInterpolate(p1, p2, p3, p4, v);

                // Perform another four bezier interpolations between the control
                // points, but this time vertically rather than horizontally.
                const XMVECTOR q1 = CubicInterpolate(patch[0], patch[4], patch[8], patch[12], v);
                const XMVECTOR q2 = CubicInterpolate(patch[1], patch[5], patch[9], patch[13], v);
                const XMVECTOR q3 = CubicInterpolate(patch[2], patch[6], patch[10], patch[14], v);
                const XMVECTOR q4 = CubicInterpolate(patch[3], patch[7], patch[11], patch[15], v);

                // Compute vertical and horizontal tangent vectors.
                const XMVECTOR tangent1 = CubicTangent(p1, p2, p3, p4, v);
                const XMVECTOR tangent2 = CubicTangent(q1, q2, q3, q4, u);

                // Cross the two tangent vectors to compute the normal.
                XMVECTOR normal = XMVector3Cross(tangent1, tangent2);

                if (!XMVector3NearEqual(normal, XMVectorZero(), g_XMEpsilon))
                {
                    normal = XMVector3Normalize(normal);

                    // If this patch is mirrored, we must invert the normal.
                    if (isMirrored)
                    {
                        normal = XMVectorNegate(normal);
                    }
                }
                else
                {
                    // In a tidy and well constructed bezier patch, the preceding
                    // normal computation will always work. But the classic teapot
                    // model is not tidy or well constructed! At the top and bottom
                    // of the teapot, it contains degenerate geometry where a patch
                    // has several control points in the same place, which causes
                    // the tangent computation to fail and produce a zero normal.
                    // We 'fix' these cases by just hard-coding a normal that points
                    // either straight up or straight down, depending on whether we
                    // are on the top or bottom of the teapot. This is not a robust
                    // solution for all possible degenerate bezier patches, but hey,
                    // it's good enough to make the teapot work correctly!

                    normal = XMVectorSelect(g_XMIdentityR1, g_XMNegIdentityR1, XMVectorLess(position, XMVectorZero()));
                }

                // Compute the texture coordinate.
                const float mirroredU = isMirrored ? 1 - u : u;

                const XMVECTOR textureCoordinate = XMVectorSet(mirroredU, v, 0, 0);

                // Output this vertex.
                outputVertex(position, normal, textureCoordinate);
            }
        }
    }


    // Creates indices for a patch that is tessellated at the specified level.
    // Calls the specified outputIndex function for each generated index value.
    template<typename TOutputFunc>
    void CreatePatchIndices(size_t tessellation, bool isMirrored, TOutputFunc outputIndex)
    {
        size_t stride = tessellation + 1;

        for (size_t i = 0; i < tessellation; i++)
        {
            for (size_t j = 0; j < tessellation; j++)
            {
                // Make a list of six index values (two triangles).
                std::array<size_t, 6> indices =
                {
                    i * stride + j,
                    (i + 1) * stride + j,
                    (i + 1) * stride + j + 1,

                    i * stride + j,
                    (i + 1) * stride + j + 1,
                    i * stride + j + 1,
                };

                // If this patch is mirrored, reverse indices to fix the winding order.
                if (isMirrored)
                {
                    std::reverse(indices.begin(), indices.end());
                }

                // Output these index values.
                std::for_each(indices.begin(), indices.end(), outputIndex);
            }
        }
    }
}
