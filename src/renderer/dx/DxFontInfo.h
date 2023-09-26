// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <dwrite.h>
#include <dwrite_1.h>
#include <dwrite_2.h>
#include <dwrite_3.h>

namespace Microsoft::Console::Render
{
    class DxFontInfo
    {
    public:
        DxFontInfo(IDWriteFactory1* dwriteFactory);

        DxFontInfo(
            IDWriteFactory1* dwriteFactory,
            std::wstring_view familyName,
            DWRITE_FONT_WEIGHT weight,
            DWRITE_FONT_STYLE style,
            DWRITE_FONT_STRETCH stretch);

        bool operator==(const DxFontInfo& other) const noexcept;

        std::wstring_view GetFamilyName() const noexcept;
        void SetFamilyName(const std::wstring_view familyName);

        DWRITE_FONT_WEIGHT GetWeight() const noexcept;
        void SetWeight(const DWRITE_FONT_WEIGHT weight) noexcept;

        DWRITE_FONT_STYLE GetStyle() const noexcept;
        void SetStyle(const DWRITE_FONT_STYLE style) noexcept;

        DWRITE_FONT_STRETCH GetStretch() const noexcept;
        void SetStretch(const DWRITE_FONT_STRETCH stretch) noexcept;

        bool GetFallback() const noexcept;
        IDWriteFontCollection* GetFontCollection() const noexcept;

        void SetFromEngine(const std::wstring_view familyName,
                           const DWRITE_FONT_WEIGHT weight,
                           const DWRITE_FONT_STYLE style,
                           const DWRITE_FONT_STRETCH stretch);

        [[nodiscard]] ::Microsoft::WRL::ComPtr<IDWriteFontFace1> ResolveFontFaceWithFallback(std::wstring& localeName);

    private:
        [[nodiscard]] ::Microsoft::WRL::ComPtr<IDWriteFontFace1> _FindFontFace(std::wstring& localeName);

        [[nodiscard]] std::wstring _GetFontFamilyName(const gsl::not_null<IDWriteFontFamily*> fontFamily,
                                                      std::wstring& localeName);

        // The font name we should be looking for
        std::wstring _familyName;

        // The weight (bold, light, etc.)
        DWRITE_FONT_WEIGHT _weight;

        // Normal, italic, etc.
        DWRITE_FONT_STYLE _style;

        // The stretch of the font is the spacing between each letter
        DWRITE_FONT_STRETCH _stretch;

        wil::com_ptr<IDWriteFontCollection> _fontCollection;

        // Indicates whether we couldn't match the user request and had to choose from a hard-coded default list.
        bool _didFallback;
    };
}
