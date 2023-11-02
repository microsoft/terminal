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
            BlinkAllowed,
            IndexedDistinguishableColors,
            AlwaysDistinguishableColors,
            IntenseIsBold,
            IntenseIsBright,
            ScreenReversed
        };

        RenderSettings() noexcept;
        void SetRenderMode(const Mode mode, const bool enabled) noexcept;
        bool GetRenderMode(const Mode mode) const noexcept;
        const std::array<COLORREF, TextColor::TABLE_SIZE>& GetColorTable() const noexcept;
        void ResetColorTable() noexcept;
        void SetColorTableEntry(const size_t tableIndex, const COLORREF color);
        COLORREF GetColorTableEntry(const size_t tableIndex) const;
        void SetColorAlias(const ColorAlias alias, const size_t tableIndex, const COLORREF color);
        COLORREF GetColorAlias(const ColorAlias alias) const;
        void SetColorAliasIndex(const ColorAlias alias, const size_t tableIndex) noexcept;
        size_t GetColorAliasIndex(const ColorAlias alias) const noexcept;
        std::pair<COLORREF, COLORREF> GetAttributeColors(const TextAttribute& attr) const noexcept;
        std::pair<COLORREF, COLORREF> GetAttributeColorsWithAlpha(const TextAttribute& attr) const noexcept;
        void ToggleBlinkRendition(class Renderer& renderer) noexcept;

    private:
        til::enumset<Mode> _renderMode{ Mode::BlinkAllowed, Mode::IntenseIsBright };
        std::array<COLORREF, TextColor::TABLE_SIZE> _colorTable;
        std::array<size_t, static_cast<size_t>(ColorAlias::ENUM_COUNT)> _colorAliasIndices;
        size_t _blinkCycle = 0;
        mutable bool _blinkIsInUse = false;
        bool _blinkShouldBeFaint = false;
    };
}
