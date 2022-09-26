// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

struct VSData {
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD;
    nointerpolation float4 color : COLOR; // This is in straight alpha!
    nointerpolation uint shadingType : ShadingType;
};
