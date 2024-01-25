/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- PageManager.hpp

Abstract:
- This manages the text buffers required by the VT paging operations.
--*/

#pragma once

#include "ITerminalApi.hpp"
#include "til.h"

namespace Microsoft::Console::VirtualTerminal
{
    class Page
    {
    public:
        Page(TextBuffer& buffer, const til::rect& viewport, const til::CoordType number) noexcept;
        TextBuffer& Buffer() const noexcept;
        til::rect Viewport() const noexcept;
        til::CoordType Number() const noexcept;

    private:
        TextBuffer& _buffer;
        til::rect _viewport;
        til::CoordType _number;
    };

    class PageManager
    {
        using Renderer = Microsoft::Console::Render::Renderer;

    public:
        PageManager(ITerminalApi& api, Renderer& renderer) noexcept;
        void Reset();
        Page Get(const til::CoordType pageNumber) const;
        Page ActivePage() const;
        Page VisiblePage() const;
        void MoveTo(const til::CoordType pageNumber);
        void MoveRelative(const til::CoordType pageCount);

    private:
        TextBuffer& _getBuffer(const til::CoordType pageNumber, const til::size pageSize) const;

        ITerminalApi& _api;
        Renderer& _renderer;
        til::CoordType _activePageNumber = 1;
        til::CoordType _visiblePageNumber = 1;
        static constexpr til::CoordType MAX_PAGES = 6;
        mutable std::array<std::unique_ptr<TextBuffer>, MAX_PAGES> _buffers;
    };
}
