// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "../renderer/inc/IRenderData.hpp"

class RenderData final :
    public Microsoft::Console::Render::IRenderData
{
public:
    void UpdateSystemMetrics();

    //
    // BEGIN IRenderData
    //

    Microsoft::Console::Types::Viewport GetViewport() noexcept override;
    til::point GetTextBufferEndPosition() const noexcept override;
    TextBuffer& GetTextBuffer() const noexcept override;
    const FontInfo& GetFontInfo() const noexcept override;
    std::span<const til::point_span> GetSearchHighlights() const noexcept override;
    const til::point_span* GetSearchHighlightFocused() const noexcept override;
    std::span<const til::point_span> GetSelectionSpans() const noexcept override;
    void LockConsole() noexcept override;
    void UnlockConsole() noexcept override;

    Microsoft::Console::Render::TimerDuration GetBlinkInterval() noexcept override;
    ULONG GetCursorPixelWidth() const noexcept override;
    bool IsGridLineDrawingAllowed() noexcept override;
    std::wstring_view GetConsoleTitle() const noexcept override;
    std::wstring GetHyperlinkUri(uint16_t id) const override;
    std::wstring GetHyperlinkCustomId(uint16_t id) const override;
    std::vector<size_t> GetPatternId(const til::point location) const override;

    std::pair<COLORREF, COLORREF> GetAttributeColors(const TextAttribute& attr) const noexcept override;
    bool IsSelectionActive() const override;
    bool IsBlockSelection() const override;
    void ClearSelection() override;
    void SelectNewRegion(const til::point coordStart, const til::point coordEnd) override;
    til::point GetSelectionAnchor() const noexcept override;
    til::point GetSelectionEnd() const noexcept override;
    bool IsUiaDataInitialized() const noexcept override;

    //
    // END IRenderData
    //

private:
    std::optional<Microsoft::Console::Render::TimerDuration> _cursorBlinkInterval;
};
