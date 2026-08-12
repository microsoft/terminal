// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "PageManager.hpp"
#include "../../renderer/base/renderer.hpp"

using namespace Microsoft::Console::VirtualTerminal;

namespace
{
    til::inclusive_rect getEffectiveMargins(const TextBuffer& buffer, const til::size pageSize) noexcept
    {
        const auto scrollMargins = buffer.GetScrollMargins();
        const auto rightmostColumn = buffer.GetSize().Width() - 1;
        const auto bottommostRow = pageSize.height - 1;
        const auto horizontalMarginsSet = scrollMargins.left < scrollMargins.right && scrollMargins.left < rightmostColumn;
        const auto verticalMarginsSet = scrollMargins.top < scrollMargins.bottom && scrollMargins.top < bottommostRow;
        return {
            horizontalMarginsSet ? scrollMargins.left : 0,
            verticalMarginsSet ? scrollMargins.top : 0,
            horizontalMarginsSet ? std::min(scrollMargins.right, rightmostColumn) : rightmostColumn,
            verticalMarginsSet ? std::min(scrollMargins.bottom, bottommostRow) : bottommostRow,
        };
    }
}

Page::Page(TextBuffer& buffer, const til::rect& viewport, const til::CoordType number) noexcept :
    _buffer{ buffer },
    _viewport{ viewport },
    _number(number)
{
}

TextBuffer& Page::Buffer() const noexcept
{
    return _buffer;
}

til::rect Page::Viewport() const noexcept
{
    return _viewport;
}

til::CoordType Page::Number() const noexcept
{
    return _number;
}

Cursor& Page::Cursor() const noexcept
{
    return _buffer.GetCursor();
}

const TextAttribute& Page::Attributes() const noexcept
{
    return _buffer.GetCurrentAttributes();
}

void Page::SetAttributes(const TextAttribute& attr) const noexcept
{
    _buffer.SetCurrentAttributes(attr);
}

til::size Page::Size() const noexcept
{
    return { Width(), Height() };
}

til::CoordType Page::Top() const noexcept
{
    // If we ever support vertical window panning, the page top won't
    // necessarily align with the viewport top, so it's best we always
    // treat them as distinct properties.
    return _viewport.top;
}

til::CoordType Page::Bottom() const noexcept
{
    // Similarly, the page bottom won't always match the viewport bottom.
    return _viewport.bottom;
}

til::CoordType Page::Width() const noexcept
{
    // The page width could also one day be different from the buffer width,
    // so again it's best treated as a distinct property.
    return _buffer.GetSize().Width();
}

til::CoordType Page::Height() const noexcept
{
    return Bottom() - Top();
}

til::CoordType Page::BufferHeight() const noexcept
{
    return _buffer.GetSize().Height();
}

til::CoordType Page::XPanOffset() const noexcept
{
    return _viewport.left;
}

til::CoordType Page::YPanOffset() const noexcept
{
    return 0; // Vertical panning is not yet supported
}

void Page::MoveViewportDown() noexcept
{
    _viewport.top++;
    _viewport.bottom++;
}

PageManager::PageManager(ITerminalApi& api, Renderer* renderer) noexcept :
    _api{ api },
    _renderer{ renderer }
{
}

void PageManager::Reset()
{
    _activePageNumber = 1;
    _visiblePageNumber = 1;
    _buffers = {};
}

Page PageManager::Get(const til::CoordType pageNumber) const
{
    const auto requestedPageNumber = std::min(std::max(pageNumber, 1), MAX_PAGES);
    auto [visibleBuffer, visibleViewport, isMainBuffer] = _api.GetBufferAndViewport();

    // If we're not in the main buffer (either because an app has enabled the
    // alternate buffer mode, or switched the conhost screen buffer), then VT
    // paging doesn't apply, so we disregard the requested page number and just
    // use the visible buffer (with a fixed page number of 1).
    if (!isMainBuffer)
    {
        return { visibleBuffer, visibleViewport, 1 };
    }

    // If the requested page number happens to be the visible page, then we
    // can also just use the visible buffer as is.
    if (requestedPageNumber == _visiblePageNumber)
    {
        return { visibleBuffer, visibleViewport, _visiblePageNumber };
    }

    // Otherwise we're working with a background buffer, so we need to
    // retrieve that from the buffer array, and resize it to match the
    // active page size.
    const auto pageSize = visibleViewport.size();
    auto& pageBuffer = _getBuffer(requestedPageNumber, pageSize);
    return { pageBuffer, til::rect{ pageSize }, requestedPageNumber };
}

Page PageManager::ActivePage() const
{
    return Get(_activePageNumber);
}

Page PageManager::VisiblePage() const
{
    return Get(_visiblePageNumber);
}

void PageManager::MoveTo(const til::CoordType pageNumber, const bool makeVisible, const bool originMode)
{
    auto [visibleBuffer, visibleViewport, isMainBuffer] = _api.GetBufferAndViewport();
    if (!isMainBuffer)
    {
        return;
    }

    const auto pageSize = visibleViewport.size();
    const auto visibleTop = visibleViewport.top;
    const auto wasVisible = _activePageNumber == _visiblePageNumber;
    const auto newPageNumber = std::min(std::max(pageNumber, 1), MAX_PAGES);
    // This must be captured before a visible page swap replaces the margins in visibleBuffer.
    const auto oldMargins = getEffectiveMargins(wasVisible ? visibleBuffer : _getBuffer(_activePageNumber, pageSize), pageSize);
    auto redrawRequired = false;

    // If we're changing the visible page, what we do is swap out the current
    // visible page into its backing buffer, and swap in the new page from the
    // backing buffer to the main buffer. That way the rest of the system only
    // ever has to deal with the main buffer.
    if (makeVisible && _visiblePageNumber != newPageNumber)
    {
        const auto& newBuffer = _getBuffer(newPageNumber, pageSize);
        auto& saveBuffer = _getBuffer(_visiblePageNumber, pageSize);
        for (auto i = 0; i < pageSize.height; i++)
        {
            visibleBuffer.CopyRow(visibleTop + i, i, saveBuffer);
        }
        for (auto i = 0; i < pageSize.height; i++)
        {
            newBuffer.CopyRow(i, visibleTop + i, visibleBuffer);
        }
        saveBuffer.SetScrollMargins(visibleBuffer.GetScrollMargins());
        visibleBuffer.SetScrollMargins(newBuffer.GetScrollMargins());
        _visiblePageNumber = newPageNumber;
        redrawRequired = true;
    }

    // If the active page was previously visible, and is now still visible,
    // there is no need to update any buffer properties, because we'll have
    // been using the main buffer in both cases.
    const auto isVisible = newPageNumber == _visiblePageNumber;
    auto& oldBuffer = wasVisible ? visibleBuffer : _getBuffer(_activePageNumber, pageSize);
    auto& newBuffer = isVisible ? visibleBuffer : _getBuffer(newPageNumber, pageSize);
    if (!wasVisible || !isVisible)
    {
        // Otherwise we need to copy the properties from the old buffer to the
        // new, so we retain the current attributes and cursor position. This
        // is only needed if they are actually different.
        if (&oldBuffer != &newBuffer)
        {
            newBuffer.SetCurrentAttributes(oldBuffer.GetCurrentAttributes());
            newBuffer.GetCursor().CopyProperties(oldBuffer.GetCursor());
        }
        // If we moved from the visible buffer to a background buffer we need
        // to hide the cursor in the visible buffer. This is because the page
        // number is like a third dimension in the cursor coordinate system.
        // If the cursor isn't on the visible page, it's the same as if its
        // x/y coordinates are outside the visible viewport.
        if (wasVisible && !isVisible)
        {
            visibleBuffer.GetCursor().SetIsVisible(false);
        }
    }

    // Adjust the cursor when changing the active page, or when transferring an
    // unchanged active page from its backing buffer into the visible buffer.
    if (_activePageNumber != newPageNumber || (makeVisible && !wasVisible))
    {
        const auto oldTop = wasVisible ? visibleTop : 0;
        const auto newTop = isVisible ? visibleTop : 0;
        auto position = oldBuffer.GetCursor().GetPosition();
        position.y = position.y - oldTop + newTop;
        if (originMode)
        {
            const auto newMargins = getEffectiveMargins(newBuffer, pageSize);
            position.x += newMargins.left - oldMargins.left;
            position.y += newMargins.top - oldMargins.top;
            position.x = std::min(position.x, newMargins.right);
            position.y = std::min(position.y, newTop + newMargins.bottom);
        }
        newBuffer.GetCursor().SetPosition(position);
    }

    _activePageNumber = newPageNumber;
    if (redrawRequired && _renderer)
    {
        _renderer->TriggerRedrawAll();
    }
}

void PageManager::MoveRelative(const til::CoordType pageCount, const bool makeVisible, const bool originMode)
{
    MoveTo(_activePageNumber + pageCount, makeVisible, originMode);
}

void PageManager::MakeActivePageVisible(const bool originMode)
{
    if (_activePageNumber != _visiblePageNumber)
    {
        MoveTo(_activePageNumber, true, originMode);
    }
}

TextBuffer& PageManager::_getBuffer(const til::CoordType pageNumber, const til::size pageSize) const
{
    auto& buffer = til::at(_buffers, pageNumber - 1);
    if (buffer == nullptr)
    {
        // Page buffers are created on demand, and are sized to match the active
        // page dimensions without any scrollback rows.
        buffer = std::make_unique<TextBuffer>(pageSize, TextAttribute{}, 0, false, _renderer);
    }
    else if (buffer->GetSize().Dimensions() != pageSize)
    {
        // If a buffer already exists for the page, and the page dimensions have
        // changed while it was inactive, it will need to be resized.
        // TODO: We don't currently reflow the existing content in this case, but
        // that may be something we want to reconsider.
        buffer->ResizeTraditional(pageSize);
    }
    return *buffer;
}
