/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IRenderData.hpp

Abstract:
- This serves as the interface defining all information needed to render to the screen.

Author(s):
- Michael Niksa (MiNiksa) 17-Nov-2015
--*/

#pragma once

#include "../../host/conimeinfo.h"
#include "../../buffer/out/textBuffer.hpp"
#include "../../types/IBaseData.h"

namespace Microsoft::Console::Render
{
    struct RenderOverlay final
    {
        // This is where the data is stored
        const TextBuffer* buffer;

        // This is where the top left of the stored buffer should be overlaid on the screen
        // (relative to the current visible viewport)
        til::point origin;

        // This is the area of the buffer that is actually used for overlay.
        // Anything outside of this is considered empty by the overlay and shouldn't be used
        // for painting purposes.
        Microsoft::Console::Types::Viewport region;
    };

    class IRenderData : public Microsoft::Console::Types::IBaseData
    {
    public:
        virtual til::point GetCursorPosition() const noexcept = 0;
        virtual bool IsCursorVisible() const noexcept = 0;
        virtual bool IsCursorOn() const noexcept = 0;
        virtual ULONG GetCursorHeight() const noexcept = 0;
        virtual CursorType GetCursorStyle() const noexcept = 0;
        virtual ULONG GetCursorPixelWidth() const noexcept = 0;
        virtual bool IsCursorDoubleWidth() const = 0;

        virtual const std::vector<RenderOverlay> GetOverlays() const noexcept = 0;

        virtual const bool IsGridLineDrawingAllowed() noexcept = 0;
        virtual const std::wstring_view GetConsoleTitle() const noexcept = 0;

        virtual const std::wstring GetHyperlinkUri(uint16_t id) const noexcept = 0;
        virtual const std::wstring GetHyperlinkCustomId(uint16_t id) const noexcept = 0;

        virtual const std::vector<size_t> GetPatternId(const til::point location) const noexcept = 0;
    };

    struct RenderData : IRenderData
    {
        Types::Viewport viewport;
        TextBuffer textBuffer;
        FontInfo fontInfo;
        std::vector<Types::Viewport> selectionRects;

        til::point cursorPosition;
        bool cursorVisible = false;
        bool cursorOn = false;
        ULONG cursorHeight = 0;
        CursorType cursorStyle = CursorType::Legacy;
        ULONG cursorPixelWidth = 0;
        bool cursorDoubleWidth = false;
        std::vector<RenderOverlay> overlays;
        bool gridLineDrawingAllowed = false;
        std::wstring consoleTitle;

        void Snapshot(IRenderData* other);

        Types::Viewport GetViewport() noexcept override;
        til::point GetTextBufferEndPosition() const noexcept override;
        const TextBuffer& GetTextBuffer() const noexcept override;
        const FontInfo& GetFontInfo() const noexcept override;
        std::vector<Types::Viewport> GetSelectionRects() noexcept override;
        void LockConsole() noexcept override;
        void UnlockConsole() noexcept override;

        til::point GetCursorPosition() const noexcept override;
        bool IsCursorVisible() const noexcept override;
        bool IsCursorOn() const noexcept override;
        ULONG GetCursorHeight() const noexcept override;
        CursorType GetCursorStyle() const noexcept override;
        ULONG GetCursorPixelWidth() const noexcept override;
        bool IsCursorDoubleWidth() const override;
        const std::vector<RenderOverlay> GetOverlays() const noexcept override;
        const bool IsGridLineDrawingAllowed() noexcept override;
        const std::wstring_view GetConsoleTitle() const noexcept override;
        const std::wstring GetHyperlinkUri(uint16_t id) const noexcept override;
        const std::wstring GetHyperlinkCustomId(uint16_t id) const noexcept override;
        const std::vector<size_t> GetPatternId(const til::point location) const noexcept override;
    };
}
