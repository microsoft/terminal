/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- renderData.hpp

Abstract:
- This method provides an interface for rendering the final display based on the current console state

Author(s):
- Michael Niksa (miniksa) Nov 2015
--*/

#pragma once

#include "../renderer/inc/IRenderData.hpp"

class RenderData final :
    public Microsoft::Console::Render::IRenderData
{
public:
    Microsoft::Console::Types::Viewport GetViewport() noexcept override;
    til::point GetTextBufferEndPosition() const noexcept override;
    const TextBuffer& GetTextBuffer() const noexcept override;
    const FontInfo& GetFontInfo() const noexcept override;

    std::vector<Microsoft::Console::Types::Viewport> GetSelectionRects() noexcept override;

    void LockConsole() noexcept override;
    void UnlockConsole() noexcept override;

    til::point GetCursorPosition() const noexcept override;
    bool IsCursorVisible() const noexcept override;
    bool IsCursorOn() const noexcept override;
    ULONG GetCursorHeight() const noexcept override;
    CursorType GetCursorStyle() const noexcept override;
    ULONG GetCursorPixelWidth() const noexcept override;
    bool IsCursorDoubleWidth() const override;

    const std::vector<Microsoft::Console::Render::RenderOverlay> GetOverlays() const noexcept override;

    const bool IsGridLineDrawingAllowed() noexcept override;

    const std::wstring_view GetConsoleTitle() const noexcept override;

    const std::wstring GetHyperlinkUri(uint16_t id) const override;
    const std::wstring GetHyperlinkCustomId(uint16_t id) const override;

    const std::vector<size_t> GetPatternId(const til::point location) const override;

    std::pair<COLORREF, COLORREF> GetAttributeColors(const TextAttribute& attr) const noexcept override;
    const bool IsSelectionActive() const override;
    const bool IsBlockSelection() const noexcept override;
    void ClearSelection() override;
    void SelectNewRegion(const til::point coordStart, const til::point coordEnd) override;
    const til::point GetSelectionAnchor() const noexcept override;
    const til::point GetSelectionEnd() const noexcept override;
    void ColorSelection(const til::point coordSelectionStart, const til::point coordSelectionEnd, const TextAttribute attr) override;
    const bool IsUiaDataInitialized() const noexcept override { return true; }
};
