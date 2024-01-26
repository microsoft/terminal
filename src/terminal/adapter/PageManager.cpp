// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "PageManager.hpp"
#include "../../renderer/base/renderer.hpp"

using namespace Microsoft::Console::VirtualTerminal;

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

void Page::SetAttributes(const TextAttribute& attr, ITerminalApi* api) const
{
    _buffer.SetCurrentAttributes(attr);
    // If the api parameter was specified, we need to pass the new attributes
    // through to the api. This occurs when there's a potential for the colors
    // to be changed, which may require some legacy remapping in conhost.
    if (api)
    {
        api->SetTextAttributes(attr);
    }
}

til::CoordType Page::Top() const noexcept
{
    return _viewport.top;
}

til::CoordType Page::Bottom() const noexcept
{
    return _viewport.bottom;
}

til::CoordType Page::Width() const noexcept
{
    return _buffer.GetSize().Width();
}

til::CoordType Page::Height() const noexcept
{
    return _viewport.bottom - _viewport.top;
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

PageManager::PageManager(ITerminalApi& api, Renderer& renderer) noexcept :
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
    if (!isMainBuffer)
    {
        return { visibleBuffer, visibleViewport, 1 };
    }
    else if (requestedPageNumber == _visiblePageNumber)
    {
        return { visibleBuffer, visibleViewport, _visiblePageNumber };
    }
    else
    {
        const auto pageSize = visibleViewport.size();
        auto& pageBuffer = _getBuffer(requestedPageNumber, pageSize);
        return { pageBuffer, til::rect{ pageSize }, requestedPageNumber };
    }
}

Page PageManager::ActivePage() const
{
    return Get(_activePageNumber);
}

Page PageManager::VisiblePage() const
{
    return Get(_visiblePageNumber);
}

void PageManager::MoveTo(const til::CoordType pageNumber, const bool makeVisible)
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
            saveBuffer.GetMutableRowByOffset(i).CopyFrom(visibleBuffer.GetRowByOffset(visibleTop + i));
        }
        for (auto i = 0; i < pageSize.height; i++)
        {
            visibleBuffer.GetMutableRowByOffset(visibleTop + i).CopyFrom(newBuffer.GetRowByOffset(i));
        }
        _visiblePageNumber = newPageNumber;
        redrawRequired = true;
    }

    // If the active page was previously visible, and is now still visible,
    // there is no need to update any buffer properties, because we'll have
    // been using the main buffer in both cases.
    const auto isVisible = newPageNumber == _visiblePageNumber;
    if (!wasVisible || !isVisible)
    {
        // Otherwise we need to copy the properties from the old buffer to the
        // new, so we retain the current attributes and cursor position. This
        // is only needed if they are actually different.
        auto& oldBuffer = wasVisible ? visibleBuffer : _getBuffer(_activePageNumber, pageSize);
        auto& newBuffer = isVisible ? visibleBuffer : _getBuffer(newPageNumber, pageSize);
        if (&oldBuffer != &newBuffer)
        {
            // When copying the cursor position, we need to adjust the y
            // coordinate to account for scrollback in the visible buffer.
            const auto oldTop = wasVisible ? visibleTop : 0;
            const auto newTop = isVisible ? visibleTop : 0;
            auto position = oldBuffer.GetCursor().GetPosition();
            position.y = position.y - oldTop + newTop;
            newBuffer.SetCurrentAttributes(oldBuffer.GetCurrentAttributes());
            newBuffer.CopyProperties(oldBuffer);
            newBuffer.GetCursor().SetPosition(position);
        }
        // If we moved from the visible buffer to a background buffer
        // we need to hide the cursor in the visible buffer. It should
        // be restored when moving back.
        if (wasVisible && !isVisible)
        {
            visibleBuffer.GetCursor().SetIsVisible(false);
        }
    }

    _activePageNumber = newPageNumber;
    if (redrawRequired)
    {
        _renderer.TriggerRedrawAll();
    }
}

void PageManager::MoveRelative(const til::CoordType pageCount, const bool makeVisible)
{
    MoveTo(_activePageNumber + pageCount, makeVisible);
}

void PageManager::MakeActivePageVisible()
{
    if (_activePageNumber != _visiblePageNumber)
    {
        MoveTo(_activePageNumber, true);
    }
}

TextBuffer& PageManager::_getBuffer(const til::CoordType pageNumber, const til::size pageSize) const
{
    auto& buffer = til::at(_buffers, pageNumber - 1);
    if (buffer == nullptr)
    {
        buffer = std::make_unique<TextBuffer>(pageSize, TextAttribute{}, 0, false, _renderer);
    }
    else if (buffer->GetSize().Dimensions() != pageSize)
    {
        buffer->ResizeTraditional(pageSize);
    }
    return *buffer;
}
