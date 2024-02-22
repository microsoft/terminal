//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard 
//

#ifndef __PIXEL_PACKING_VELOCITY_HLSLI__
#define __PIXEL_PACKING_VELOCITY_HLSLI__

#if 1
// This is a custom packing that devotes 10 bits each to X and Y velocity but 12 bits to Z velocity.  Floats
// are used instead of SNORM to increase precision around small deltas, which are the majority of deltas.
// With TAA and Motion Blur, velocities are clamped, giving little reason to express them precisely in terms
// of the size of the screen.
#define packed_velocity_t uint

// Designed to compress (-256.0, +256.0) with a signed 6e3 float
uint PackXY(float x)
{
    uint signbit = asuint(x) >> 31;
    x = clamp(abs(x / 32768.0), 0, asfloat(0x3BFFE000));
    return (f32tof16(x) + 8) >> 4 | signbit << 9;
}

float UnpackXY(uint x)
{
    return f16tof32((x & 0x1FF) << 4 | (x >> 9) << 15) * 32768.0;
}

// Designed to compress (-1.0, 1.0) with a signed 8e3 float
uint PackZ(float x)
{
    uint signbit = asuint(x) >> 31;
    x = clamp(abs(x / 128.0), 0, asfloat(0x3BFFE000));
    return (f32tof16(x) + 2) >> 2 | signbit << 11;
}

float UnpackZ(uint x)
{
    return f16tof32((x & 0x7FF) << 2 | (x >> 11) << 15) * 128.0;
}

// Pack the velocity to write to R10G10B10A2_UNORM
packed_velocity_t PackVelocity(float3 Velocity)
{
    return PackXY(Velocity.x) | PackXY(Velocity.y) << 10 | PackZ(Velocity.z) << 20;
}

// Unpack the velocity from R10G10B10A2_UNORM
float3 UnpackVelocity(packed_velocity_t Velocity)
{
    return float3(UnpackXY(Velocity & 0x3FF), UnpackXY((Velocity >> 10) & 0x3FF), UnpackZ(Velocity >> 20));
}

#elif 1
#define packed_velocity_t float4

// Pack the velocity to write to R10G10B10A2_UNORM
packed_velocity_t PackVelocity(float3 Velocity)
{
    // Stretch dx,dy from [-64, 63.875] to [-512, 511] to [-0.5, 0.5) to [0, 1)
    // Velocity.xy = (0,0) must be representable.
    return float4(Velocity * float3(8, 8, 4096) / 1024.0 + 512 / 1023.0, 0);
}

// Unpack the velocity from R10G10B10A2_UNORM
float3 UnpackVelocity(packed_velocity_t Velocity)
{
    return (Velocity.xyz - 512.0 / 1023.0) * float3(1024, 1024, 2) / 8.0;
}
#else
#define packed_velocity_t float4

// Pack the velocity to write to R16G16B16A16_FLOAT
packed_velocity_t PackVelocity(float3 Velocity)
{
    return float4(Velocity * float3(16, 16, 32*1024), 0);
}

// Unpack the velocity from R10G10B10A2_UNORM
float3 UnpackVelocity(packed_velocity_t Velocity)
{
    return Velocity.xyz / float3(16, 16, 32*1024);
}

#endif

#endif // __PIXEL_PACKING_HLSLI__
