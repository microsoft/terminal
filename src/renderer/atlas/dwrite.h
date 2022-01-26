// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

// Exclude stuff from <Windows.h> we don't need.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <dwrite_1.h>

// See .cpp file for documentation.
void DWrite_GetRenderParams(IDWriteFactory1* factory, float* gamma, float* cleartypeEnhancedContrast, float* grayscaleEnhancedContrast, IDWriteRenderingParams1** linearParams);
void DWrite_GetGammaRatios(float gamma, float (&out)[4]) noexcept;
bool DWrite_IsThinFontFamily(const wchar_t* canonicalFamilyName) noexcept;
bool DWrite_IsThinFontFamily(IDWriteFontCollection* fontCollection, const wchar_t* familyName);
