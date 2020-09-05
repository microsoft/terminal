# How to enable pixel shader effects in Windows Terminal

Due to the sheer amount of computing power in GPUs one can do awesome things with pixel shaders such as real-time fractal zoom, ray tracers and image processing.

Windows Terminal allows user to provide a pixel shader which will be applied on terminal. In order to try it out adding the setting to one of your console profiles:

```
"experimental.pixelShaderEffect": "RETROII"
```

This is a built-in pixel shader applies a retro look to the terminal:

![image](https://user-images.githubusercontent.com/2491891/88453750-39374480-ce6a-11ea-904f-e771d08920e8.png)

Note that if you specified the existing retro effect it takes precendence over any other pixel shader effects so be sure to disable it first.

```
"experimental.retroTerminalEffect": false,
```

The pixel shader effect also support the classic retro look as well:

```
"experimental.pixelShaderEffect": "RETRO"
```

## User provided pixel shaders

While `RETRO` and `RETROII` are cool what's even better is that a user can specify a user provided pixel shader.

In order to do save the following pixel shader code as a file with `.hlsl` file extension and note the path to the file:

```hlsl
// A minimal pixel shader that inverts the colors

// The terminal graphics as a texture
Texture2D shaderTexture;
SamplerState samplerState;

// Terminal settings such as the resolution of the texture
cbuffer PixelShaderSettings {
  // Time since pixel shader was enabled
  float  Time;
  // UI Scale
  float  Scale;
  // Resolution of the shaderTexture
  float2 Resolution;
  // Background color as rgba
  float4 Background;
};

// A pixel shader is a program that given a texture coordinate (tex) produces a color
//  Just ignore the pos parameter
float4 main(float4 pos : SV_POSITION, float2 tex : TEXCOORD) : SV_TARGET
{
    // Read the color value at the current texture coordinate (tex)
    //  float4 is tuple of 4 floats, rgba
    float4 color = shaderTexture.Sample(samplerState, tex);

    // Inverts the rgb values (xyz) but don't touch the alpha (w)
    color.xyz = 1.0 - color.xyz;

    // Return the final color
    return color;
}
```

If the path to pixel shader file is `C:\temp\invert.hlsl` then update a terminal profile with the setting:

```
"experimental.pixelShaderEffect": "C:\\temp\\invert.hlsl"
```

Because the settings file is a JSON document we need to use double slashes or "escape" in the file path to construct a valid JSON document.

Once the settings file is saved when you open a terminal with the changed profile it should now invert the colors of the screen.

Pretty neat.

If you do something wrong Windows Terminal to an error shader which shows red lines all over the terminal. It shows that it tried to apply your pixel shader but something went wrong, review the profile setting and the source code.

## HLSL aka C the good parts

The language we use to write pixel shaders is called `HLSL` and it is basically like `C` with all parts that are difficult with `C` taken out. That means you can't allocate memory, use pointers or recursion.

What you get access to is computing power in the teraflop range on decently recent GPUs. This means writing real-time raytracers or other cool effects are in the realm of possibility.

[shadertoy](https://shadertoy.com/) is a great site that show case what's possible with pixel shaders (albeit in `GLSL`). For example this [menger sponge](https://www.shadertoy.com/view/4scXzn). Converting from `GLSL` to `HLSL` isn't overly hard once you gotten the hang of it.

## Adding some retro raster bars

Raster bars was cool in 1980s so let's add that by modifying the invert shader like so:

```hlsl
// A minimal pixel shader that shows some raster bars

// The terminal graphics as a texture
Texture2D shaderTexture;
SamplerState samplerState;

// Terminal settings such as the resolution of the texture
cbuffer PixelShaderSettings {
  // Time since pixel shader was enabled
  float  Time;
  // UI Scale
  float  Scale;
  // Resolution of the shaderTexture
  float2 Resolution;
  // Background color as rgba
  float4 Background;
};

// A pixel shader is a program that given a texture coordinate (tex) produces a color
//  Just ignore the pos parameter
float4 main(float4 pos : SV_POSITION, float2 tex : TEXCOORD) : SV_TARGET
{
    // Read the color value at the current texture coordinate (tex)
    //  float4 is tuple of 4 floats, rgba
    float4 color = shaderTexture.Sample(samplerState, tex);

    // Read the color value at some offset, will be used as shadow
    float4 ocolor = shaderTexture.Sample(samplerState, tex+2.0*Scale*float2(-1.0, -1.0)/Resolution.y);

    // Thickness of raster
    const float thickness = 0.1;

    float ny = floor(tex.y/thickness);
    float my = tex.y%thickness;
    const float pi = 3.141592654;


    // ny is used to compute the rasterbar base color
    float cola = ny*2.0*pi;
    float3 col = 0.75+0.25*float3(sin(cola*0.111), sin(cola*0.222), sin(cola*0.333));

    // my is used to compute the rasterbar brightness
    //  smoothstep is a great little function: https://en.wikipedia.org/wiki/Smoothstep
    float brightness = 1.0-smoothstep(0.0, thickness*0.5, abs(my - 0.5*thickness));

    float3 rasterColor = col*brightness;

    // lerp(x, y, a) is another very useful function: https://en.wikipedia.org/wiki/Linear_interpolation
    float3 final = rasterColor;
    // Create the drop shadow of the terminal graphics
    //  .w is the alpha channel, 0 is fully transparent and 1 is fully opaque
    final = lerp(final, float(0.0), ocolor.w);
    // Draw the terminal graphics
    final = lerp(final, color.xyz, color.w);

    // Return the final color, set alpha to 1 (ie opaque)
    return float4(final, 1.0);
}
```

Once reloaded it shows some retro raster bars in the background + a drop shadow to make the text more readable.

## Make your own retro look

Of course, you are most likely somewhat disappointed with the look of `RETROII` and like to add your own touch to it so for you reference the source code for the `RETROII` is included below:

```hlsl
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
```

Have fun...