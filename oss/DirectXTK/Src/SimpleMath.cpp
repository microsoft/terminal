//-------------------------------------------------------------------------------------
// SimpleMath.cpp -- Simplified C++ Math wrapper for DirectXMath
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
// http://go.microsoft.com/fwlink/?LinkID=615561
//-------------------------------------------------------------------------------------

#include "pch.h"
#include "SimpleMath.h"

/****************************************************************************
 *
 * Constants
 *
 ****************************************************************************/

namespace DirectX
{
    namespace SimpleMath
    {
        const Vector2 Vector2::Zero = { 0.f, 0.f };
        const Vector2 Vector2::One = { 1.f, 1.f };
        const Vector2 Vector2::UnitX = { 1.f, 0.f };
        const Vector2 Vector2::UnitY = { 0.f, 1.f };

        const Vector3 Vector3::Zero = { 0.f, 0.f, 0.f };
        const Vector3 Vector3::One = { 1.f, 1.f, 1.f };
        const Vector3 Vector3::UnitX = { 1.f, 0.f, 0.f };
        const Vector3 Vector3::UnitY = { 0.f, 1.f, 0.f };
        const Vector3 Vector3::UnitZ = { 0.f, 0.f, 1.f };
        const Vector3 Vector3::Up = { 0.f, 1.f, 0.f };
        const Vector3 Vector3::Down = { 0.f, -1.f, 0.f };
        const Vector3 Vector3::Right = { 1.f, 0.f, 0.f };
        const Vector3 Vector3::Left = { -1.f, 0.f, 0.f };
        const Vector3 Vector3::Forward = { 0.f, 0.f, -1.f };
        const Vector3 Vector3::Backward = { 0.f, 0.f, 1.f };

        const Vector4 Vector4::Zero = { 0.f, 0.f, 0.f, 0.f };
        const Vector4 Vector4::One = { 1.f, 1.f, 1.f, 1.f };
        const Vector4 Vector4::UnitX = { 1.f, 0.f, 0.f, 0.f };
        const Vector4 Vector4::UnitY = { 0.f, 1.f, 0.f, 0.f };
        const Vector4 Vector4::UnitZ = { 0.f, 0.f, 1.f, 0.f };
        const Vector4 Vector4::UnitW = { 0.f, 0.f, 0.f, 1.f };

        const Matrix Matrix::Identity = { 1.f, 0.f, 0.f, 0.f,
                                          0.f, 1.f, 0.f, 0.f,
                                          0.f, 0.f, 1.f, 0.f,
                                          0.f, 0.f, 0.f, 1.f };

        const Quaternion Quaternion::Identity = { 0.f, 0.f, 0.f, 1.f };
    }
}

using namespace DirectX;
using namespace DirectX::SimpleMath;

/****************************************************************************
 *
 * Quaternion
 *
 ****************************************************************************/

void Quaternion::RotateTowards(const Quaternion& target, float maxAngle, Quaternion& result) const noexcept
{
    const XMVECTOR T = XMLoadFloat4(this);

    // We can use the conjugate here instead of inverse assuming q1 & q2 are normalized.
    const XMVECTOR R = XMQuaternionMultiply(XMQuaternionConjugate(T), target);

    const float rs = XMVectorGetW(R);
    const XMVECTOR L = XMVector3Length(R);
    const float angle = 2.f * atan2f(XMVectorGetX(L), rs);
    if (angle > maxAngle)
    {
        const XMVECTOR delta = XMQuaternionRotationAxis(R, maxAngle);
        const XMVECTOR Q = XMQuaternionMultiply(delta, T);
        XMStoreFloat4(&result, Q);
    }
    else
    {
        // Don't overshoot.
        result = target;
    }
}

void Quaternion::FromToRotation(const Vector3& fromDir, const Vector3& toDir, Quaternion& result) noexcept
{
    // Melax, "The Shortest Arc Quaternion", Game Programming Gems, Charles River Media (2000).

    const XMVECTOR F = XMVector3Normalize(fromDir);
    const XMVECTOR T = XMVector3Normalize(toDir);

    const float dot = XMVectorGetX(XMVector3Dot(F, T));
    if (dot >= 1.f)
    {
        result = Identity;
    }
    else if (dot <= -1.f)
    {
        XMVECTOR axis = XMVector3Cross(F, Vector3::Right);
        if (XMVector3NearEqual(XMVector3LengthSq(axis), g_XMZero, g_XMEpsilon))
        {
            axis = XMVector3Cross(F, Vector3::Up);
        }

        const XMVECTOR Q = XMQuaternionRotationAxis(axis, XM_PI);
        XMStoreFloat4(&result, Q);
    }
    else
    {
        const XMVECTOR C = XMVector3Cross(F, T);
        XMStoreFloat4(&result, C);

        const float s = sqrtf((1.f + dot) * 2.f);
        result.x /= s;
        result.y /= s;
        result.z /= s;
        result.w = s * 0.5f;
    }
}

void Quaternion::LookRotation(const Vector3& forward, const Vector3& up, Quaternion& result) noexcept
{
    Quaternion q1;
    FromToRotation(Vector3::Forward, forward, q1);

    const XMVECTOR C = XMVector3Cross(forward, up);
    if (XMVector3NearEqual(XMVector3LengthSq(C), g_XMZero, g_XMEpsilon))
    {
        // forward and up are co-linear
        result = q1;
        return;
    }

    const XMVECTOR U = XMQuaternionMultiply(q1, Vector3::Up);

    Quaternion q2;
    FromToRotation(U, up, q2);

    XMStoreFloat4(&result, XMQuaternionMultiply(q2, q1));
}


 /****************************************************************************
 *
 * Viewport
 *
 ****************************************************************************/

#if defined(__d3d11_h__) || defined(__d3d11_x_h__)
static_assert(sizeof(DirectX::SimpleMath::Viewport) == sizeof(D3D11_VIEWPORT), "Size mismatch");
static_assert(offsetof(DirectX::SimpleMath::Viewport, x) == offsetof(D3D11_VIEWPORT, TopLeftX), "Layout mismatch");
static_assert(offsetof(DirectX::SimpleMath::Viewport, y) == offsetof(D3D11_VIEWPORT, TopLeftY), "Layout mismatch");
static_assert(offsetof(DirectX::SimpleMath::Viewport, width) == offsetof(D3D11_VIEWPORT, Width), "Layout mismatch");
static_assert(offsetof(DirectX::SimpleMath::Viewport, height) == offsetof(D3D11_VIEWPORT, Height), "Layout mismatch");
static_assert(offsetof(DirectX::SimpleMath::Viewport, minDepth) == offsetof(D3D11_VIEWPORT, MinDepth), "Layout mismatch");
static_assert(offsetof(DirectX::SimpleMath::Viewport, maxDepth) == offsetof(D3D11_VIEWPORT, MaxDepth), "Layout mismatch");
#endif

#if defined(__d3d12_h__) || defined(__d3d12_x_h__) || defined(__XBOX_D3D12_X__)
static_assert(sizeof(DirectX::SimpleMath::Viewport) == sizeof(D3D12_VIEWPORT), "Size mismatch");
static_assert(offsetof(DirectX::SimpleMath::Viewport, x) == offsetof(D3D12_VIEWPORT, TopLeftX), "Layout mismatch");
static_assert(offsetof(DirectX::SimpleMath::Viewport, y) == offsetof(D3D12_VIEWPORT, TopLeftY), "Layout mismatch");
static_assert(offsetof(DirectX::SimpleMath::Viewport, width) == offsetof(D3D12_VIEWPORT, Width), "Layout mismatch");
static_assert(offsetof(DirectX::SimpleMath::Viewport, height) == offsetof(D3D12_VIEWPORT, Height), "Layout mismatch");
static_assert(offsetof(DirectX::SimpleMath::Viewport, minDepth) == offsetof(D3D12_VIEWPORT, MinDepth), "Layout mismatch");
static_assert(offsetof(DirectX::SimpleMath::Viewport, maxDepth) == offsetof(D3D12_VIEWPORT, MaxDepth), "Layout mismatch");
#endif

#if defined(__dxgi1_2_h__) || defined(__d3d11_x_h__) || defined(__d3d12_x_h__) || defined(__XBOX_D3D12_X__)
RECT Viewport::ComputeDisplayArea(DXGI_SCALING scaling, UINT backBufferWidth, UINT backBufferHeight, int outputWidth, int outputHeight) noexcept
{
    RECT rct = {};

    switch (int(scaling))
    {
    case DXGI_SCALING_STRETCH:
        // Output fills the entire window area
        rct.top = 0;
        rct.left = 0;
        rct.right = outputWidth;
        rct.bottom = outputHeight;
        break;

    case 2 /*DXGI_SCALING_ASPECT_RATIO_STRETCH*/:
        // Output fills the window area but respects the original aspect ratio, using pillar boxing or letter boxing as required
        // Note: This scaling option is not supported for legacy Win32 windows swap chains
        {
            assert(backBufferHeight > 0);
            const float aspectRatio = float(backBufferWidth) / float(backBufferHeight);

            // Horizontal fill
            float scaledWidth = float(outputWidth);
            float scaledHeight = float(outputWidth) / aspectRatio;
            if (scaledHeight >= float(outputHeight))
            {
                // Do vertical fill
                scaledWidth = float(outputHeight) * aspectRatio;
                scaledHeight = float(outputHeight);
            }

            const float offsetX = (float(outputWidth) - scaledWidth) * 0.5f;
            const float offsetY = (float(outputHeight) - scaledHeight) * 0.5f;

            rct.left = static_cast<LONG>(offsetX);
            rct.top = static_cast<LONG>(offsetY);
            rct.right = static_cast<LONG>(offsetX + scaledWidth);
            rct.bottom = static_cast<LONG>(offsetY + scaledHeight);

            // Clip to display window
            rct.left = std::max<LONG>(0, rct.left);
            rct.top = std::max<LONG>(0, rct.top);
            rct.right = std::min<LONG>(outputWidth, rct.right);
            rct.bottom = std::min<LONG>(outputHeight, rct.bottom);
        }
        break;

    case DXGI_SCALING_NONE:
    default:
        // Output is displayed in the upper left corner of the window area
        rct.top = 0;
        rct.left = 0;
        rct.right = std::min<LONG>(static_cast<LONG>(backBufferWidth), outputWidth);
        rct.bottom = std::min<LONG>(static_cast<LONG>(backBufferHeight), outputHeight);
        break;
    }

    return rct;
}
#endif

RECT Viewport::ComputeTitleSafeArea(UINT backBufferWidth, UINT backBufferHeight) noexcept
{
    const float safew = (float(backBufferWidth) + 19.f) / 20.f;
    const float safeh = (float(backBufferHeight) + 19.f) / 20.f;

    RECT rct;
    rct.left = static_cast<LONG>(safew);
    rct.top = static_cast<LONG>(safeh);
    rct.right = static_cast<LONG>(float(backBufferWidth) - safew + 0.5f);
    rct.bottom = static_cast<LONG>(float(backBufferHeight) - safeh + 0.5f);

    return rct;
}
