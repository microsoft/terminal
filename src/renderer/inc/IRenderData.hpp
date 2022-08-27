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
#include "../../buffer/out/TextAttribute.hpp"
#include "../../types/IBaseData.h"

class Cursor;

namespace Microsoft::Console::Render
{
    struct RenderOverlay final
    {
        // This is where the data is stored
        const TextBuffer& buffer;

        // This is where the top left of the stored buffer should be overlaid on the screen
        // (relative to the current visible viewport)
        const til::point origin;

        // This is the area of the buffer that is actually used for overlay.
        // Anything outside of this is considered empty by the overlay and shouldn't be used
        // for painting purposes.
        const Microsoft::Console::Types::Viewport region;
    };

    class IRenderData : public Microsoft::Console::Types::IBaseData
    {
    public:
        ~IRenderData() = 0;
        IRenderData(const IRenderData&) = default;
        IRenderData(IRenderData&&) = default;
        IRenderData& operator=(const IRenderData&) = default;
        IRenderData& operator=(IRenderData&&) = default;

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

    protected:
        IRenderData() = default;
    };

    // See docs/virtual-dtors.md for an explanation of why this is weird.
    inline IRenderData::~IRenderData() {}
}
