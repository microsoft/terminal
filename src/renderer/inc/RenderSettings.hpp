/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- RenderSettings.hpp

Abstract:
- This class manages the runtime settings that are relevant to the renderer.
--*/

#pragma once

#include "../../buffer/out/TextAttribute.hpp"

namespace Microsoft::Console::Render
{
    class RenderSettings
    {
    public:
        enum class Mode : size_t
        {
            // Generate256Colors is first just to make test(Generate256Colors)
            // ever so slightly cheaper. It's the most widely accessed one.
            Generate256Colors,
            IndexedDistinguishableColors,
            AlwaysDistinguishableColors,
            IntenseIsBold,
            IntenseIsBright,
            ScreenReversed,
            SynchronizedOutput,
        };

        RenderSettings() noexcept;

        void SaveDefaultSettings() noexcept;
        void RestoreDefaultSettings() noexcept;

        void SetRenderMode(const Mode mode, const bool enabled) noexcept;
        bool GetRenderMode(const Mode mode) const noexcept;

        const std::array<COLORREF, TextColor::TABLE_SIZE>& GetColorTable() noexcept;
        void RestoreDefaultIndexed256ColorTable();
        void RestoreDefaultColorTableEntry(const size_t tableIndex);

        void SetColorTableEntry(const size_t tableIndex, const COLORREF color);
        COLORREF GetColorTableEntry(const size_t tableIndex);

        void SetColorAlias(const ColorAlias alias, const size_t tableIndex, const COLORREF color);
        COLORREF GetColorAlias(const ColorAlias alias);
        void SetColorAliasIndex(const ColorAlias alias, const size_t tableIndex) noexcept;
        size_t GetColorAliasIndex(const ColorAlias alias) noexcept;
        void RestoreDefaultColorAliasIndex(const ColorAlias alias) noexcept;

        std::pair<COLORREF, COLORREF> GetAttributeColors(const TextAttribute& attr) noexcept;
        std::pair<COLORREF, COLORREF> GetAttributeColorsWithAlpha(const TextAttribute& attr) noexcept;
        COLORREF GetAttributeUnderlineColor(const TextAttribute& attr) noexcept;
        void ToggleBlinkRendition() noexcept;

    private:
        const std::array<COLORREF, TextColor::TABLE_SIZE>& _getColorTable() noexcept;
        void _flagColorTableDirty() noexcept;
        void _generate256ColorTable() noexcept;

        // NOTE: Reads of _colorTable MUST occur through _getColorTable()!
        //       Writers to one of the following 3 MUST call _flagColorTableDirty()!
        til::enumset<Mode> _renderMode{ Mode::IntenseIsBright };
        std::array<COLORREF, TextColor::TABLE_SIZE> _colorTable;
        std::array<size_t, static_cast<size_t>(ColorAlias::ENUM_COUNT)> _colorAliasIndices;

        std::array<COLORREF, TextColor::TABLE_SIZE> _defaultColorTable;
        std::array<size_t, static_cast<size_t>(ColorAlias::ENUM_COUNT)> _defaultColorAliasIndices;

        bool _colorTableDirty = true;
        bool _blinkShouldBeFaint = false;
    };
}
