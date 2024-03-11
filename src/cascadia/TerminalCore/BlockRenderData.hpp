// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <conattrs.hpp>

#include "../../inc/DefaultSettings.h"
#include "../../buffer/out/textBuffer.hpp"
#include "../../renderer/inc/IRenderData.hpp"
#include "../../terminal/adapter/ITerminalApi.hpp"
#include "../../terminal/parser/StateMachine.hpp"
#include "../../terminal/input/terminalInput.hpp"
#include "../../types/inc/Viewport.hpp"
#include "../../types/inc/GlyphWidth.hpp"
#include "../../cascadia/terminalcore/ITerminalInput.hpp"
#include "../../cascadia/terminalcore/Terminal.hpp"

#include <til/ticket_lock.h>
#include <til/winrt.h>

namespace Microsoft::Console::VirtualTerminal
{
    class AdaptDispatch;
}

namespace Microsoft::Terminal::Core
{
    class BlockRenderData;
}

class Microsoft::Terminal::Core::BlockRenderData final :
    public Microsoft::Console::Render::IRenderData
{
    using RenderSettings = Microsoft::Console::Render::RenderSettings;

public:
    BlockRenderData(Terminal& _terminal, til::CoordType virtualTop);
    void SetBottom(til::CoordType bottom);

#pragma region IRenderData
    Microsoft::Console::Types::Viewport GetViewport() noexcept override;
    til::point GetTextBufferEndPosition() const noexcept override;
    const TextBuffer& GetTextBuffer() const noexcept override;
    const FontInfo& GetFontInfo() const noexcept override;

    void LockConsole() noexcept override;
    void UnlockConsole() noexcept override;

    til::point GetCursorPosition() const noexcept override;
    bool IsCursorVisible() const noexcept override;
    bool IsCursorOn() const noexcept override;
    ULONG GetCursorHeight() const noexcept override;
    ULONG GetCursorPixelWidth() const noexcept override;
    CursorType GetCursorStyle() const noexcept override;
    bool IsCursorDoubleWidth() const override;
    const std::vector<Microsoft::Console::Render::RenderOverlay> GetOverlays() const noexcept override;
    const bool IsGridLineDrawingAllowed() noexcept override;
    const std::wstring GetHyperlinkUri(uint16_t id) const override;
    const std::wstring GetHyperlinkCustomId(uint16_t id) const override;
    const std::vector<size_t> GetPatternId(const til::point location) const override;

    std::pair<COLORREF, COLORREF> GetAttributeColors(const TextAttribute& attr) const noexcept override;
    std::vector<Microsoft::Console::Types::Viewport> GetSelectionRects() noexcept override;
    std::vector<Microsoft::Console::Types::Viewport> GetSearchSelectionRects() noexcept override;
    const bool IsSelectionActive() const noexcept override;
    const bool IsBlockSelection() const noexcept override;
    void ClearSelection() override;
    void SelectNewRegion(const til::point coordStart, const til::point coordEnd) override;
    void SelectSearchRegions(std::vector<til::inclusive_rect> source) override;
    const til::point GetSelectionAnchor() const noexcept override;
    const til::point GetSelectionEnd() const noexcept override;
    const std::wstring_view GetConsoleTitle() const noexcept override;
    const bool IsUiaDataInitialized() const noexcept override;

    til::CoordType GetBufferHeight() const noexcept override;
    void UserScrollViewport(const int viewTop) override;

#pragma endregion

private:
    Terminal& _terminal;

    til::CoordType _virtualTop{ 0 };
    til::CoordType _scrollOffset{ 0 };
    std::optional<til::CoordType> _virtualBottom{ std::nullopt };
};
