// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "DxFontInfo.h"

static constexpr std::wstring_view FALLBACK_FONT_FACES[] = { L"Consolas", L"Lucida Console", L"Courier New" };

using namespace Microsoft::Console::Render;

DxFontInfo::DxFontInfo() noexcept :
    _familyName(),
    _weight(DWRITE_FONT_WEIGHT_NORMAL),
    _style(DWRITE_FONT_STYLE_NORMAL),
    _stretch(DWRITE_FONT_STRETCH_NORMAL)
{
}

DxFontInfo::DxFontInfo(std::wstring_view familyName,
                       DWRITE_FONT_WEIGHT weight,
                       DWRITE_FONT_STYLE style,
                       DWRITE_FONT_STRETCH stretch) :
    _familyName(familyName),
    _weight(weight),
    _style(style),
    _stretch(stretch)
{
}

DxFontInfo::DxFontInfo(std::wstring_view familyName,
                       unsigned int weight,
                       DWRITE_FONT_STYLE style,
                       DWRITE_FONT_STRETCH stretch) :
    _familyName(familyName),
    _weight(static_cast<DWRITE_FONT_WEIGHT>(weight)),
    _style(style),
    _stretch(stretch)
{
}

std::wstring DxFontInfo::GetFamilyName() const
{
    return _familyName;
}

void DxFontInfo::SetFamilyName(const std::wstring_view familyName)
{
    _familyName = familyName;
}

DWRITE_FONT_WEIGHT DxFontInfo::GetWeight() const noexcept
{
    return _weight;
}

void DxFontInfo::SetWeight(const DWRITE_FONT_WEIGHT weight) noexcept
{
    _weight = weight;
}

DWRITE_FONT_STYLE DxFontInfo::GetStyle() const noexcept
{
    return _style;
}

void DxFontInfo::SetStyle(const DWRITE_FONT_STYLE style) noexcept
{
    _style = style;
}

DWRITE_FONT_STRETCH DxFontInfo::GetStretch() const noexcept
{
    return _stretch;
}

void DxFontInfo::SetStretch(const DWRITE_FONT_STRETCH stretch) noexcept
{
    _stretch = stretch;
}

void DxFontInfo::SetFromEngine(const std::wstring_view familyName,
                               const DWRITE_FONT_WEIGHT weight,
                               const DWRITE_FONT_STYLE style,
                               const DWRITE_FONT_STRETCH stretch)
{
    _familyName = familyName;
    _weight = weight;
    _style = style;
    _stretch = stretch;
}

// Routine Description:
// - Attempts to locate the font given, but then begins falling back if we cannot find it.
// - We'll try to fall back to Consolas with the given weight/stretch/style first,
//   then try Consolas again with normal weight/stretch/style,
//   and if nothing works, then we'll throw an error.
// Arguments:
// - familyName - The font name we should be looking for
// - weight - The weight (bold, light, etc.)
// - stretch - The stretch of the font is the spacing between each letter
// - style - Normal, italic, etc.
// Return Value:
// - Smart pointer holding interface reference for queryable font data.
[[nodiscard]] Microsoft::WRL::ComPtr<IDWriteFontFace1> DxFontInfo::ResolveFontFaceWithFallback(gsl::not_null<IDWriteFactory1*> dwriteFactory,
                                                                                               std::wstring& localeName)
{
    auto face = _FindFontFace(dwriteFactory, localeName);

    if (!face)
    {
        for (const auto fallbackFace : FALLBACK_FONT_FACES)
        {
            _familyName = fallbackFace;
            face = _FindFontFace(dwriteFactory, localeName);

            if (face)
            {
                break;
            }

            SetFromEngine(fallbackFace,
                          DWRITE_FONT_WEIGHT_NORMAL,
                          DWRITE_FONT_STYLE_NORMAL,
                          DWRITE_FONT_STRETCH_NORMAL);
            face = _FindFontFace(dwriteFactory, localeName);

            if (face)
            {
                break;
            }
        }
    }

    THROW_HR_IF_NULL(E_FAIL, face);

    return face;
}

// Routine Description:
// - Locates a suitable font face from the given information
// Arguments:
// - familyName - The font name we should be looking for
// - weight - The weight (bold, light, etc.)
// - stretch - The stretch of the font is the spacing between each letter
// - style - Normal, italic, etc.
// Return Value:
// - Smart pointer holding interface reference for queryable font data.
[[nodiscard]] Microsoft::WRL::ComPtr<IDWriteFontFace1> DxFontInfo::_FindFontFace(gsl::not_null<IDWriteFactory1*> dwriteFactory, std::wstring& localeName)
{
    Microsoft::WRL::ComPtr<IDWriteFontFace1> fontFace;

    Microsoft::WRL::ComPtr<IDWriteFontCollection> fontCollection;
    THROW_IF_FAILED(dwriteFactory->GetSystemFontCollection(&fontCollection, false));

    UINT32 familyIndex;
    BOOL familyExists;
    THROW_IF_FAILED(fontCollection->FindFamilyName(GetFamilyName().data(), &familyIndex, &familyExists));

    if (familyExists)
    {
        Microsoft::WRL::ComPtr<IDWriteFontFamily> fontFamily;
        THROW_IF_FAILED(fontCollection->GetFontFamily(familyIndex, &fontFamily));

        Microsoft::WRL::ComPtr<IDWriteFont> font;
        THROW_IF_FAILED(fontFamily->GetFirstMatchingFont(GetWeight(), GetStretch(), GetStyle(), &font));

        Microsoft::WRL::ComPtr<IDWriteFontFace> fontFace0;
        THROW_IF_FAILED(font->CreateFontFace(&fontFace0));

        THROW_IF_FAILED(fontFace0.As(&fontFace));

        // Retrieve metrics in case the font we created was different than what was requested.
        SetWeight(font->GetWeight());
        SetStretch(font->GetStretch());
        SetStyle(font->GetStyle());

        // Dig the family name out at the end to return it.
        SetFamilyName(_GetFontFamilyName(fontFamily.Get(), localeName));
    }

    return fontFace;
}

// Routine Description:
// - Retrieves the font family name out of the given object in the given locale.
// - If we can't find a valid name for the given locale, we'll fallback and report it back.
// Arguments:
// - fontFamily - DirectWrite font family object
// - localeName - The locale in which the name should be retrieved.
//              - If fallback occurred, this is updated to what we retrieved instead.
// Return Value:
// - Localized string name of the font family
[[nodiscard]] std::wstring DxFontInfo::_GetFontFamilyName(gsl::not_null<IDWriteFontFamily*> const fontFamily,
                                                          std::wstring& localeName)
{
    // See: https://docs.microsoft.com/en-us/windows/win32/api/dwrite/nn-dwrite-idwritefontcollection
    Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> familyNames;
    THROW_IF_FAILED(fontFamily->GetFamilyNames(&familyNames));

    // First we have to find the right family name for the locale. We're going to bias toward what the caller
    // requested, but fallback if we need to and reply with the locale we ended up choosing.
    UINT32 index = 0;
    BOOL exists = false;

    // This returns S_OK whether or not it finds a locale name. Check exists field instead.
    // If it returns an error, it's a real problem, not an absence of this locale name.
    // https://docs.microsoft.com/en-us/windows/win32/api/dwrite/nf-dwrite-idwritelocalizedstrings-findlocalename
    THROW_IF_FAILED(familyNames->FindLocaleName(localeName.data(), &index, &exists));

    // If we tried and it still doesn't exist, try with the fallback locale.
    if (!exists)
    {
        localeName = L"en-us";
        THROW_IF_FAILED(familyNames->FindLocaleName(localeName.data(), &index, &exists));
    }

    // If it still doesn't exist, we're going to try index 0.
    if (!exists)
    {
        index = 0;

        // Get the locale name out so at least the caller knows what locale this name goes with.
        UINT32 length = 0;
        THROW_IF_FAILED(familyNames->GetLocaleNameLength(index, &length));
        localeName.resize(length);

        // https://docs.microsoft.com/en-us/windows/win32/api/dwrite/nf-dwrite-idwritelocalizedstrings-getlocalenamelength
        // https://docs.microsoft.com/en-us/windows/win32/api/dwrite/nf-dwrite-idwritelocalizedstrings-getlocalename
        // GetLocaleNameLength does not include space for null terminator, but GetLocaleName needs it so add one.
        THROW_IF_FAILED(familyNames->GetLocaleName(index, localeName.data(), length + 1));
    }

    // OK, now that we've decided which family name and the locale that it's in... let's go get it.
    UINT32 length = 0;
    THROW_IF_FAILED(familyNames->GetStringLength(index, &length));

    // Make our output buffer and resize it so it is allocated.
    std::wstring retVal;
    retVal.resize(length);

    // FINALLY, go fetch the string name.
    // https://docs.microsoft.com/en-us/windows/win32/api/dwrite/nf-dwrite-idwritelocalizedstrings-getstringlength
    // https://docs.microsoft.com/en-us/windows/win32/api/dwrite/nf-dwrite-idwritelocalizedstrings-getstring
    // Once again, GetStringLength is without the null, but GetString needs the null. So add one.
    THROW_IF_FAILED(familyNames->GetString(index, retVal.data(), length + 1));

    // and return it.
    return retVal;
}
