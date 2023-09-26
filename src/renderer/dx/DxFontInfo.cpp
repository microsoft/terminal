// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "DxFontInfo.h"

#include <unicode.hpp>
#include <VersionHelpers.h>

#include "../base/FontCache.h"

static constexpr std::wstring_view FALLBACK_FONT_FACES[] = { L"Consolas", L"Lucida Console", L"Courier New" };

using namespace Microsoft::Console::Render;

DxFontInfo::DxFontInfo(IDWriteFactory1* dwriteFactory) :
    DxFontInfo{ dwriteFactory, {}, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL }
{
}

DxFontInfo::DxFontInfo(
    IDWriteFactory1* dwriteFactory,
    std::wstring_view familyName,
    DWRITE_FONT_WEIGHT weight,
    DWRITE_FONT_STYLE style,
    DWRITE_FONT_STRETCH stretch) :
    _familyName(familyName),
    _weight(weight),
    _style(style),
    _stretch(stretch),
    _didFallback(false)
{
    __assume(dwriteFactory != nullptr);
    THROW_IF_FAILED(dwriteFactory->GetSystemFontCollection(_fontCollection.addressof(), FALSE));
}

bool DxFontInfo::operator==(const DxFontInfo& other) const noexcept
{
    return (_familyName == other._familyName &&
            _weight == other._weight &&
            _style == other._style &&
            _stretch == other._stretch &&
            _didFallback == other._didFallback);
}

std::wstring_view DxFontInfo::GetFamilyName() const noexcept
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

bool DxFontInfo::GetFallback() const noexcept
{
    return _didFallback;
}

IDWriteFontCollection* DxFontInfo::GetFontCollection() const noexcept
{
    return _fontCollection.get();
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
// - dwriteFactory - The DWrite factory to use
// - localeName - Locale to search for appropriate fonts
// Return Value:
// - Smart pointer holding interface reference for queryable font data.
[[nodiscard]] Microsoft::WRL::ComPtr<IDWriteFontFace1> DxFontInfo::ResolveFontFaceWithFallback(std::wstring& localeName)
{
    // First attempt to find exactly what the user asked for.
    _didFallback = false;
    Microsoft::WRL::ComPtr<IDWriteFontFace1> face{ nullptr };

    // GH#10211 - wrap this all up in a try/catch. If the nearby fonts are
    // corrupted, then we don't want to throw out of this top half of this
    // method. We still want to fall back to a font that's reasonable, below.
    try
    {
        face = _FindFontFace(localeName);

        if constexpr (Feature_NearbyFontLoading::IsEnabled())
        {
            if (!face)
            {
                _fontCollection = FontCache::GetCached();
                face = _FindFontFace(localeName);
            }
        }

        if (!face)
        {
            // If we missed, try looking a little more by trimming the last word off the requested family name a few times.
            // Quite often, folks are specifying weights or something in the familyName and it causes failed resolution and
            // an unexpected error dialog. We theoretically could detect the weight words and convert them, but this
            // is the quick fix for the majority scenario.
            // The long/full fix is backlogged to GH#9744
            // Also this doesn't count as a fallback because we don't want to annoy folks with the warning dialog over
            // this resolution.
            while (!face && !_familyName.empty())
            {
                const auto lastSpace = _familyName.find_last_of(UNICODE_SPACE);

                // value is unsigned and npos will be greater than size.
                // if we didn't find anything to trim, leave.
                if (lastSpace >= _familyName.size())
                {
                    break;
                }

                // trim string down to just before the found space
                // (space found at 6... trim from 0 for 6 length will give us 0-5 as the new string)
                _familyName = _familyName.substr(0, lastSpace);

                // Try to find it with the shortened family name
                face = _FindFontFace(localeName);
            }
        }
    }
    CATCH_LOG();

    // Alright, if our quick shot at trimming didn't work either...
    // move onto looking up a font from our hard-coded list of fonts
    // that should really always be available.
    if (!face)
    {
        for (const auto fallbackFace : FALLBACK_FONT_FACES)
        {
            _familyName = fallbackFace;

            face = _FindFontFace(localeName);
            if (face)
            {
                _didFallback = true;
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
// - dwriteFactory - The DWrite factory to use
// - localeName - Locale to search for appropriate fonts
// Return Value:
// - Smart pointer holding interface reference for queryable font data.
#pragma warning(suppress : 26429) // C26429: Symbol 'fontCollection' is never tested for nullness, it can be marked as not_null (f.23).
[[nodiscard]] Microsoft::WRL::ComPtr<IDWriteFontFace1> DxFontInfo::_FindFontFace(std::wstring& localeName)
{
    Microsoft::WRL::ComPtr<IDWriteFontFace1> fontFace;

    UINT32 familyIndex;
    BOOL familyExists;

    THROW_IF_FAILED(_fontCollection->FindFamilyName(_familyName.data(), &familyIndex, &familyExists));

    if (familyExists)
    {
        Microsoft::WRL::ComPtr<IDWriteFontFamily> fontFamily;
        THROW_IF_FAILED(_fontCollection->GetFontFamily(familyIndex, &fontFamily));

        Microsoft::WRL::ComPtr<IDWriteFont> font;
        THROW_IF_FAILED(fontFamily->GetFirstMatchingFont(GetWeight(), GetStretch(), GetStyle(), &font));

        Microsoft::WRL::ComPtr<IDWriteFontFace> fontFace0;
        THROW_IF_FAILED(font->CreateFontFace(&fontFace0));

        THROW_IF_FAILED(fontFace0.As(&fontFace));

        // Retrieve metrics in case the font we created was different than what was requested.
        _weight = font->GetWeight();
        _stretch = font->GetStretch();
        _style = font->GetStyle();

        // Dig the family name out at the end to return it.
        _familyName = _GetFontFamilyName(fontFamily.Get(), localeName);
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
[[nodiscard]] std::wstring DxFontInfo::_GetFontFamilyName(const gsl::not_null<IDWriteFontFamily*> fontFamily,
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
