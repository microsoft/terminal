// In order to us Kodelife (a shader IDE), uncomment the following line:
// #define KODELIFE

#ifdef KODELIFE
#define DOWNSCALE   3.0
#define MAIN        ps_main
#else
#define DOWNSCALE   (2.0*Scale)
#define MAIN        main
#endif

#define PI          3.141592654
#define TAU         (2.0*PI)

Texture2D shaderTexture;
SamplerState samplerState;

cbuffer PixelShaderSettings {
  float  Time;
  float  Scale;
  float2 Resolution;
  float4 Background;
};

// ----------------------------------------------------------------------------
// Ray sphere intersection
//  From: https://www.shadertoy.com/view/4d2XWV
//  Blog: https://www.iquilezles.org/www/articles/intersectors/intersectors.htm
// The MIT License
// Copyright (C) 2014 Inigo Quilez
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
float raySphere(float3 ro, float3 rd, float4 sph) {
  float3 oc = ro - sph.xyz;
  float b = dot(oc, rd);
  float c = dot(oc, oc) - sph.w*sph.w;
  float h = b*b - c;
  if (h<0.0) return -1.0;
  h = sqrt(h);
  return -b - h;
}
// ----------------------------------------------------------------------------

float psin(float a) {
  return 0.5+0.5*sin(a);
}

float3 sample(float2 p) {
  float2 cp = abs(p - 0.5);
  float4 s = shaderTexture.Sample(samplerState, p);
  float3 col = lerp(Background.xyz, s.xyz, s.w);
  return col*step(cp.x, 0.5)*step(cp.y, 0.5);
}

float3 screen(float2 reso, float2 p, float diff, float spe) {
  float sr = reso.y/reso.x;
  float res=reso.y/DOWNSCALE;

  // Lots of experimentation lead to these rows

  float2 ap = p;
  ap.x *= sr;

  // tanh is such a great function!

  // Viginetting
  float2 vp = ap + 0.5;
  float vig = tanh(pow(max(100.0*vp.x*vp.y*(1.0-vp.x)*(1.0-vp.y), 0.0), 0.35));

  ap *= 1.025;

  // Screen at coord
  float2 sp = ap;
  sp += 0.5;
  float3 scol = sample(sp);
  // Compute brightness
  float sbri0 = max(max(scol.x, scol.y), scol.z);
  float sbri1 = sbri0;

  // Scan line brightness
  float scanbri = lerp(0.25, 2.0, psin(PI*res*p.y));

  // Compute new brightness
  sbri1 *= scanbri;
  sbri1 = tanh(1.5*sbri1);
  sbri1 *= vig;

  float3 col = float3(0.0, 0.0, 0.0);
  col += scol*sbri1/sbri0;
  col += (0.35*spe+0.25*diff)*vig;

  return col;
}

// Computes the color given the ray origin and texture coord p [-1, 1]
float3 color(float2 reso, float3 ro, float2 p) {
  // Quick n dirty way to get ray direction
  float3 rd = normalize(float3(p, 2.0));

  // The screen is imagined to be projected on a large sphere to give it a curve
  const float radius = 20.0;
  const float4 center = float4(0.0, 0.0, radius, radius-1.0);
  float3 lightPos = 0.95*float3(-1.0, -1.0, 0.0);

  // Find the ray sphere intersection, basically a single ray tracing step
  float sd = raySphere(ro, rd, center);

  if (sd > 0.0) {
    // sp is the point on sphere where the ray intersected
    float3 sp = ro + sd*rd;
    // Normal of the sphere allows to compute lighting
    float3 nor = normalize(sp - center.xyz);
    float3 ld = normalize(lightPos - sp);

    // Diffuse lighting
    float diff = max(dot(ld, nor), 0.0);
    // Specular lighting
    float spe = pow(max(dot(reflect(rd, nor), ld), 0.0),30.0);

    // Due to how the scene is setup up we cheat and use sp.xy as the screen coord
    return screen(reso, sp.xy, diff, spe);
  } else {
    return float3(0.0, 0.0, 0.0);
  }
}

float4 MAIN(float4 pos : SV_POSITION, float2 tex : TEXCOORD) : SV_TARGET {
  float2 reso = Resolution;
  float2 q = tex;
  float2 p = -1. + 2. * q;
  p.x *= reso.x/reso.y;

  float3 ro = float3(0.0, 0.0, 0.0);
  float3 col = color(reso, ro, p);

  return float4(col, 1.0);
}
