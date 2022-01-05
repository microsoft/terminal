// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

// clang-format off
float4 main(uint id: SV_VERTEXID): SV_POSITION
// clang-format on
{
    // The algorithm below is a fast way to generate a full screen triangle,
    // published by Bill Bilodeau "Vertex Shader Tricks" at GDC14.
    // It covers the entire viewport and is faster for the GPU than a quad/rectangle.
    return float4(
        float(id / 2) * 4.0 - 1.0,
        float(id % 2) * 4.0 - 1.0,
        0.0,
        1.0
    );
}
