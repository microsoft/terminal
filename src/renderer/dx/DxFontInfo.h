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
        DxFontInfo() noexcept;

        DxFontInfo(std::wstring_view familyName,
                   unsigned int weight,
                   DWRITE_FONT_STYLE style,
                   DWRITE_FONT_STRETCH stretch);

        DxFontInfo(std::wstring_view familyName,
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

        void SetFromEngine(const std::wstring_view familyName,
                           const DWRITE_FONT_WEIGHT weight,
                           const DWRITE_FONT_STYLE style,
                           const DWRITE_FONT_STRETCH stretch);

        [[nodiscard]] ::Microsoft::WRL::ComPtr<IDWriteFontFace1> ResolveFontFaceWithFallback(gsl::not_null<IDWriteFactory1*> dwriteFactory,
                                                                                             std::wstring& localeName);

    private:
        [[nodiscard]] ::Microsoft::WRL::ComPtr<IDWriteFontFace1> _FindFontFace(gsl::not_null<IDWriteFactory1*> dwriteFactory,
                                                                               std::wstring& localeName,
                                                                               const bool withNearbyLookup);

        [[nodiscard]] std::wstring _GetFontFamilyName(gsl::not_null<IDWriteFontFamily*> const fontFamily,
                                                      std::wstring& localeName);

        [[nodiscard]] const Microsoft::WRL::ComPtr<IDWriteFontCollection1>& _NearbyCollection(gsl::not_null<IDWriteFactory1*> dwriteFactory) const;

        [[nodiscard]] static std::vector<std::filesystem::path> s_GetNearbyFonts();

        mutable ::Microsoft::WRL::ComPtr<IDWriteFontCollection1> _nearbyCollection;

        // The font name we should be looking for
        std::wstring _familyName;

        // The weight (bold, light, etc.)
        DWRITE_FONT_WEIGHT _weight;

        // Normal, italic, etc.
        DWRITE_FONT_STYLE _style;

        // The stretch of the font is the spacing between each letter
        DWRITE_FONT_STRETCH _stretch;

        // Indicates whether we couldn't match the user request and had to choose from a hardcoded default list.
        bool _didFallback;
    };
}

namespace std
{
    template<>
    struct hash<Microsoft::Console::Render::DxFontInfo>
    {
        size_t operator()(const Microsoft::Console::Render::DxFontInfo& fontInfo) const noexcept
        {
            const size_t h1 = std::hash<std::wstring_view>{}(fontInfo.GetFamilyName());
            const size_t h2 = std::hash<DWRITE_FONT_WEIGHT>{}(fontInfo.GetWeight());
            const size_t h3 = std::hash<DWRITE_FONT_STYLE>{}(fontInfo.GetStyle());
            const size_t h4 = std::hash<DWRITE_FONT_STRETCH>{}(fontInfo.GetStretch());
            const size_t h5 = std::hash<bool>{}(fontInfo.GetFallback());

            static const auto combine = [](std::initializer_list<size_t> list) {
                size_t seed = 0;
                for (auto hash : list)
                {
                    seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
                }
                return seed;
            };

            return combine({ h1, h2, h3, h4, h5 });
        }
    };
}
