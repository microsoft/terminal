// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "dwrite.h"

#pragma warning(disable : 26429) // Symbol '...' is never tested for nullness, it can be marked as not_null (f.23).

template<typename T>
constexpr T clamp(T v, T min, T max) noexcept
{
    return std::max(min, std::min(max, v));
}

// The gamma and grayscaleEnhancedContrast values are required for DWrite_GetGrayscaleCorrectedAlpha().
// in shader.hlsl later and can be passed in your cbuffer for instance.
// The returned linearParams object can be passed to various DirectWrite/D2D
// methods, like ID2D1RenderTarget::SetTextRenderingParams for instance.
//
// DirectWrite's alpha blending is gamma corrected and thus text color dependent.
// In order to do such blending in our shader we have to disable gamma compensation inside DirectWrite/Direct2D.
// If we didn't we'd apply the correction twice and the result would look wrong.
//
// Under Windows applications aren't expected to refresh the rendering params after startup,
// allowing you to cache these values for the lifetime of your application.
void DWrite_GetRenderParams(IDWriteFactory1* factory, float* gamma, float* cleartypeEnhancedContrast, float* grayscaleEnhancedContrast, IDWriteRenderingParams1** linearParams)
{
    // If you're concerned with crash resilience don't use reinterpret_cast
    // and use .query<IDWriteRenderingParams1>() or ->QueryInterface() instead.
    wil::com_ptr<IDWriteRenderingParams1> defaultParams;
    THROW_IF_FAILED(factory->CreateRenderingParams(reinterpret_cast<IDWriteRenderingParams**>(defaultParams.addressof())));

    *gamma = defaultParams->GetGamma();
    *cleartypeEnhancedContrast = defaultParams->GetEnhancedContrast();
    *grayscaleEnhancedContrast = defaultParams->GetGrayscaleEnhancedContrast();

    THROW_IF_FAILED(factory->CreateCustomRenderingParams(1.0f, 0.0f, 0.0f, defaultParams->GetClearTypeLevel(), defaultParams->GetPixelGeometry(), defaultParams->GetRenderingMode(), linearParams));
}

// This function produces 4 magic constants for DWrite_ApplyAlphaCorrection() in dwrite.hlsl
// and are required as an argument for DWrite_GetGrayscaleCorrectedAlpha().
// gamma should be set to the return value of DWrite_GetRenderParams() or (pseudo-code):
//   IDWriteRenderingParams* defaultParams;
//   dwriteFactory->CreateRenderingParams(&defaultParams);
//   gamma = defaultParams->GetGamma();
//
// gamma is chosen using the gamma value you pick in the "Adjust ClearType text" application.
// The default value for this are the 1.8 gamma ratios, which equates to:
//   0.148054421f, -0.894594550f, 1.47590804f, -0.324668258f
void DWrite_GetGammaRatios(float gamma, float (&out)[4]) noexcept
{
    static constexpr float gammaIncorrectTargetRatios[13][4]{
        { 0.0000f / 4.f, 0.0000f / 4.f, 0.0000f / 4.f, 0.0000f / 4.f }, // gamma = 1.0
        { 0.0166f / 4.f, -0.0807f / 4.f, 0.2227f / 4.f, -0.0751f / 4.f }, // gamma = 1.1
        { 0.0350f / 4.f, -0.1760f / 4.f, 0.4325f / 4.f, -0.1370f / 4.f }, // gamma = 1.2
        { 0.0543f / 4.f, -0.2821f / 4.f, 0.6302f / 4.f, -0.1876f / 4.f }, // gamma = 1.3
        { 0.0739f / 4.f, -0.3963f / 4.f, 0.8167f / 4.f, -0.2287f / 4.f }, // gamma = 1.4
        { 0.0933f / 4.f, -0.5161f / 4.f, 0.9926f / 4.f, -0.2616f / 4.f }, // gamma = 1.5
        { 0.1121f / 4.f, -0.6395f / 4.f, 1.1588f / 4.f, -0.2877f / 4.f }, // gamma = 1.6
        { 0.1300f / 4.f, -0.7649f / 4.f, 1.3159f / 4.f, -0.3080f / 4.f }, // gamma = 1.7
        { 0.1469f / 4.f, -0.8911f / 4.f, 1.4644f / 4.f, -0.3234f / 4.f }, // gamma = 1.8
        { 0.1627f / 4.f, -1.0170f / 4.f, 1.6051f / 4.f, -0.3347f / 4.f }, // gamma = 1.9
        { 0.1773f / 4.f, -1.1420f / 4.f, 1.7385f / 4.f, -0.3426f / 4.f }, // gamma = 2.0
        { 0.1908f / 4.f, -1.2652f / 4.f, 1.8650f / 4.f, -0.3476f / 4.f }, // gamma = 2.1
        { 0.2031f / 4.f, -1.3864f / 4.f, 1.9851f / 4.f, -0.3501f / 4.f }, // gamma = 2.2
    };
    static constexpr auto norm13 = static_cast<float>(static_cast<double>(0x10000) / (255 * 255) * 4);
    static constexpr auto norm24 = static_cast<float>(static_cast<double>(0x100) / (255) * 4);

#pragma warning(suppress : 26451) // Arithmetic overflow: Using operator '+' on a 4 byte value and then casting the result to a 8 byte value.
    const auto index = clamp<ptrdiff_t>(static_cast<ptrdiff_t>(gamma * 10.0f + 0.5f), 10, 22) - 10;
#pragma warning(suppress : 26446) // Prefer to use gsl::at() instead of unchecked subscript operator (bounds.4).
#pragma warning(suppress : 26482) // Only index into arrays using constant expressions (bounds.2).
    const auto& ratios = gammaIncorrectTargetRatios[index];

    out[0] = norm13 * ratios[0];
    out[1] = norm24 * ratios[1];
    out[2] = norm13 * ratios[2];
    out[3] = norm24 * ratios[3];
}

// This belongs to isThinFontFamily().
// Keep this in alphabetical order, or the loop will break.
// Keep thinFontFamilyNamesMaxWithNull updated.
static constexpr std::array thinFontFamilyNames{
    L"Courier New",
    L"Fixed Miriam Transparent",
    L"Miriam Fixed",
    L"Rod",
    L"Rod Transparent",
    L"Simplified Arabic Fixed"
};
static constexpr size_t thinFontFamilyNamesMaxLengthWithNull = 25;

// DWrite_IsThinFontFamily returns true if the specified family name is in our hard-coded list of "thin fonts".
// These are fonts that require special rendering because their strokes are too thin.
//
// The history of these fonts is interesting. The glyph outlines were originally created by digitizing the typeballs of
// IBM Selectric typewriters. Digitizing the metal typeballs yielded very precise outlines. However, the strokes are
// consistently too thin in comparison with the corresponding typewritten characters because the thickness of the
// typewriter ribbon was not accounted for. This didn't matter in the earliest versions of Windows because the screen
// resolution was not that high and you could not have a stroke thinner than one pixel. However, with the introduction
// of anti-aliasing the thin strokes manifested in text that was too light. By this time, it was too late to change
// the fonts so instead a special case was added to render these fonts differently.
//
// ---
//
// The canonical family name is a font's family English name, when
// * There's a corresponding font face name with the same language ID
// * If multiple such pairs exist, en-us is preferred
// * Otherwise (if en-us is not a translation) it's the lowest LCID
//
// However my (lhecker) understanding is that none of the thinFontFamilyNames come without an en-us translation.
// As such you can simply get the en-us name of the font from a IDWriteFontCollection for instance.
// See the overloaded alternative version of isThinFontFamily.
bool DWrite_IsThinFontFamily(const wchar_t* canonicalFamilyName) noexcept
{
    auto n = 0;

    // Check if the given canonicalFamilyName is a member of the set of thinFontFamilyNames.
    // Binary search isn't helpful here, as it doesn't really reduce the number of average comparisons.
    for (const auto familyName : thinFontFamilyNames)
    {
        n = wcscmp(canonicalFamilyName, familyName);
        if (n <= 0)
        {
            break;
        }
    }

    return n == 0;
}

// The actual DWrite_IsThinFontFamily() expects you to pass a "canonical" family name,
// which technically isn't that trivial to determine. This function might help you with that.
// Just give it the font collection you use and any family name from that collection.
// (For instance from IDWriteFactory::GetSystemFontCollection.)
bool DWrite_IsThinFontFamily(IDWriteFontCollection* fontCollection, const wchar_t* familyName)
{
    UINT32 index;
    BOOL exists;
    if (FAILED(fontCollection->FindFamilyName(familyName, &index, &exists)) || !exists)
    {
        return false;
    }

    wil::com_ptr<IDWriteFontFamily> fontFamily;
    THROW_IF_FAILED(fontCollection->GetFontFamily(index, fontFamily.addressof()));

    wil::com_ptr<IDWriteLocalizedStrings> localizedFamilyNames;
    THROW_IF_FAILED(fontFamily->GetFamilyNames(localizedFamilyNames.addressof()));

    THROW_IF_FAILED(localizedFamilyNames->FindLocaleName(L"en-US", &index, &exists));
    if (!exists)
    {
        return false;
    }

    UINT32 length;
    THROW_IF_FAILED(localizedFamilyNames->GetStringLength(index, &length));

    if (length >= thinFontFamilyNamesMaxLengthWithNull)
    {
        return false;
    }

    wchar_t enUsFamilyName[thinFontFamilyNamesMaxLengthWithNull];
    THROW_IF_FAILED(localizedFamilyNames->GetString(index, &enUsFamilyName[0], thinFontFamilyNamesMaxLengthWithNull));

    return DWrite_IsThinFontFamily(&enUsFamilyName[0]);
}
