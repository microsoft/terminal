/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- GraphicsImage.hpp

Abstract:
- Image data and placements used by the kitty graphics protocol. Images are
  immutable once created, so they can be shared between the buffer rows that
  display them and the renderer.
--*/

#pragma once

#include "til.h"
#include <deque>
#include <memory>
#include <vector>

struct GraphicsImage
{
    uint32_t id = 0;
    uint32_t number = 0;
    til::size size;
    // Premultiplied BGRA pixels, size.width * size.height entries.
    std::vector<uint32_t> pixels;
    // A unique non-zero value identifying this pixel data.
    uint64_t revision = 0;

    size_t ByteSize() const noexcept;
    static uint64_t NextRevision() noexcept;
};

struct GraphicsRect
{
    float left = 0;
    float top = 0;
    float right = 0;
    float bottom = 0;
};

struct GraphicsGeometry
{
    // The source rectangle in image pixels.
    til::rect source;
    // The offset within the first cell, in virtual pixels.
    til::point offset;
    // The requested size in cells, where 0 means automatic.
    til::CoordType columns = 0;
    til::CoordType rows = 0;
    // The cell size that virtual pixel values are relative to.
    til::size virtualCellSize;

    GraphicsRect Destination(til::size cellSize) const noexcept;
};

struct GraphicsPlacement
{
    std::shared_ptr<const GraphicsImage> image;
    uint32_t placementId = 0;
    GraphicsGeometry geometry;
    // The anchor column and the number of cells covered.
    til::CoordType column = 0;
    til::CoordType columns = 0;
    til::CoordType rows = 0;
    int32_t z = 0;
    uint64_t revision = 0;
};

// Attached to every buffer row that a placement covers.
struct GraphicsRowRef
{
    std::shared_ptr<const GraphicsPlacement> placement;
    til::CoordType row = 0;
};

struct GraphicsVirtualPlacement
{
    uint32_t imageId = 0;
    uint32_t placementId = 0;
    til::CoordType columns = 0;
    til::CoordType rows = 0;
};

namespace GraphicsPlaceholder
{
    // Cells containing U+10EEEE display a part of an image. The image id is
    // encoded in the foreground color and the placement id in the underline
    // color, while combining diacritics encode the row, column and the most
    // significant byte of the image id.
    inline constexpr std::wstring_view Character{ L"􎻮" };

    // Returns the number encoded by a diacritic, or -1 if it isn't one.
    int DiacriticValue(char32_t ch) noexcept;
}

class GraphicsStore
{
public:
    static constexpr size_t Quota = 320 * 1024 * 1024;

    std::shared_ptr<const GraphicsImage> Find(uint32_t id) const noexcept;
    std::shared_ptr<const GraphicsImage> FindByNumber(uint32_t number) const noexcept;
    void Add(std::shared_ptr<const GraphicsImage> image);
    void Remove(uint32_t id) noexcept;
    uint32_t NextFreeId() noexcept;
    std::vector<uint32_t> Ids() const;

    void SetVirtualPlacement(const GraphicsVirtualPlacement& placement);
    const GraphicsVirtualPlacement* FindVirtualPlacement(uint32_t imageId, uint32_t placementId) const noexcept;
    void RemoveVirtualPlacements(uint32_t imageId, uint32_t placementId) noexcept;

    void Clear() noexcept;

private:
    void _enforceQuota(size_t incoming) noexcept;

    // Ordered from oldest to newest.
    std::deque<std::shared_ptr<const GraphicsImage>> _images;
    std::vector<GraphicsVirtualPlacement> _virtualPlacements;
    size_t _totalBytes = 0;
    uint32_t _nextId = 0x7f000000;
};
