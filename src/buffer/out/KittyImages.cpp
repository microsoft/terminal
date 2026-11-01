// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "KittyImages.hpp"

#include "ImageSlice.hpp"
#include "KittyPlaceholder.hpp"
#include "Row.hpp"
#include "textBuffer.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace
{
    constexpr size_t MaxStripBytes = 64ull * 1024 * 1024;
    constexpr size_t MaxComposedRowEntries = 4096;

    void HashCombine(uint64_t& seed, const uint64_t value) noexcept
    {
        seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
    }
}

bool operator==(const KittyImages::StripKey& a, const KittyImages::StripKey& b) noexcept
{
    return a.imageGen == b.imageGen && a.imageId == b.imageId && a.placementId == b.placementId && a.row == b.row && a.cellW == b.cellW && a.cellH == b.cellH;
}

bool KittyImages::HasContent() const noexcept
{
    return !_placements.empty() || !_images.empty();
}

uint64_t KittyImages::Generation() const noexcept
{
    return _generation;
}

const KittyImages::Image* KittyImages::FindImage(const uint32_t id) const noexcept
{
    for (const auto& image : _images)
    {
        if (image.id == id)
        {
            return &image;
        }
    }
    return nullptr;
}

const KittyImages::Image* KittyImages::FindNewestImageByNumber(const uint32_t number) const noexcept
{
    for (auto it = _images.rbegin(); it != _images.rend(); ++it)
    {
        if (it->number == number)
        {
            return &*it;
        }
    }
    return nullptr;
}

const KittyImages::Image* KittyImages::StoreImage(const uint32_t id, const uint32_t number, const til::size size, std::vector<RGBQUAD>&& pixels, const uint32_t format, const char compressed)
{
    // Re-transmitting an id replaces the image and deletes its placements.
    const auto existing = std::find_if(_images.begin(), _images.end(), [&](const Image& image) {
        return image.id == id;
    });
    if (existing != _images.end())
    {
        std::erase_if(_placements, [&](const Placement& placement) {
            return placement.imageId == id;
        });
        _images.erase(existing);
    }

    auto image = Image{};
    image.id = id;
    image.number = number;
    image.size = size;
    image.pixels = std::move(pixels);
    image.format = format;
    image.compressed = compressed;
    image.gen = _nextImageGen++;
    _images.push_back(std::move(image));

    // Bound total image memory by evicting the oldest images, placements
    // and all.
    auto total = std::accumulate(_images.begin(), _images.end(), size_t{}, [](size_t sum, const Image& entry) {
        return sum + entry.pixels.size() * sizeof(RGBQUAD);
    });
    while (total > MaxTotalImageBytes && _images.size() > 1)
    {
        const auto& oldest = _images.front();
        total -= oldest.pixels.size() * sizeof(RGBQUAD);
        std::erase_if(_placements, [&](const Placement& placement) {
            return placement.imageId == oldest.id;
        });
        _images.erase(_images.begin());
    }

    _Bump();
    return &_images.back();
}

bool KittyImages::HasPlacements(const uint32_t imageId) const noexcept
{
    for (const auto& placement : _placements)
    {
        if (placement.imageId == imageId)
        {
            return true;
        }
    }
    return false;
}

size_t KittyImages::DeleteUnreferencedImages()
{
    auto removed = size_t{};
    for (size_t i = _images.size(); i-- > 0;)
    {
        if (!HasPlacements(_images[i].id))
        {
            _images.erase(_images.begin() + i);
            ++removed;
        }
    }
    if (removed)
    {
        _Bump();
    }
    return removed;
}

bool KittyImages::DeleteImage(const uint32_t id)
{
    const auto existing = std::find_if(_images.begin(), _images.end(), [&](const Image& image) {
        return image.id == id;
    });
    if (existing == _images.end())
    {
        return false;
    }
    _images.erase(existing);
    _Bump();
    return true;
}

KittyImages::Image* KittyImages::EnsureFrames(const uint32_t imageId, const uint32_t frame)
{
    if (frame == 0 || frame > MaxFrames)
    {
        return nullptr;
    }
    const auto image = std::find_if(_images.begin(), _images.end(), [&](const Image& entry) {
        return entry.id == imageId;
    });
    if (image == _images.end())
    {
        return nullptr;
    }
    if (image->frames.empty())
    {
        // The root frame is the image data as transmitted.
        image->frames.push_back(Frame{ image->pixels, 0 });
    }
    while (image->frames.size() < frame)
    {
        image->frames.push_back(Frame{ std::vector<RGBQUAD>(image->pixels.size()), 0 });
    }
    return &*image;
}

bool KittyImages::StoreFrame(const uint32_t imageId, const uint32_t frame, std::vector<RGBQUAD>&& pixels, const int32_t gap)
{
    auto target = frame;
    if (target == 0)
    {
        // Append after the last frame.
        const auto* existing = EnsureFrames(imageId, 1);
        if (!existing)
        {
            return false;
        }
        target = static_cast<uint32_t>(existing->frames.size()) + 1;
    }
    const auto image = EnsureFrames(imageId, target);
    if (!image || pixels.size() != image->pixels.size())
    {
        return false;
    }
    auto& slot = image->frames[target - 1];
    slot.pixels = std::move(pixels);
    slot.gap = gap;
    if (target == 1)
    {
        // A root-frame edit changes what is displayed whenever the root is
        // the current frame.
        image->pixels = image->frames[0].pixels;
        image->currentFrame = 1;
        image->gen = _nextImageGen++;
        _Bump();
    }
    return true;
}

bool KittyImages::SelectFrame(const uint32_t imageId, const uint32_t frame)
{
    const auto image = EnsureFrames(imageId, frame);
    if (!image)
    {
        return false;
    }
    if (image->currentFrame == frame)
    {
        return true;
    }
    image->pixels = image->frames[frame - 1].pixels;
    image->currentFrame = frame;
    image->gen = _nextImageGen++;
    _Bump();
    return true;
}

bool KittyImages::SetFrameGap(const uint32_t imageId, const uint32_t frame, const int32_t gap)
{
    const auto image = EnsureFrames(imageId, frame);
    if (!image)
    {
        return false;
    }
    image->frames[frame - 1].gap = gap;
    return true;
}

bool KittyImages::ComposeFrames(const uint32_t imageId, const uint32_t source, const uint32_t destination, const til::rect& sourceRect, const til::point& destinationOffset, const bool replace)
{
    const auto image = EnsureFrames(imageId, std::max(source, destination));
    if (!image)
    {
        return false;
    }
    const auto width = image->size.width;
    const auto height = image->size.height;
    if (sourceRect.left < 0 || sourceRect.top < 0 || sourceRect.right > width || sourceRect.bottom > height ||
        sourceRect.right <= sourceRect.left || sourceRect.bottom <= sourceRect.top)
    {
        return false;
    }
    // Clip the destination rectangle to the image; the matching source
    // region shrinks with it.
    auto copy = sourceRect;
    auto offsetX = destinationOffset.x;
    auto offsetY = destinationOffset.y;
    if (offsetX < 0)
    {
        copy.left -= offsetX;
        offsetX = 0;
    }
    if (offsetY < 0)
    {
        copy.top -= offsetY;
        offsetY = 0;
    }
    copy.right = std::min<til::CoordType>(copy.right, copy.left + (width - offsetX));
    copy.bottom = std::min<til::CoordType>(copy.bottom, copy.top + (height - offsetY));
    if (copy.right <= copy.left || copy.bottom <= copy.top)
    {
        return false;
    }

    auto& srcFrame = image->frames[source - 1];
    auto& dstFrame = image->frames[destination - 1];
    for (auto y = copy.top; y < copy.bottom; ++y)
    {
        for (auto x = copy.left; x < copy.right; ++x)
        {
            const auto src = srcFrame.pixels[static_cast<size_t>(y) * width + x];
            auto& dst = dstFrame.pixels[static_cast<size_t>(y + offsetY - sourceRect.top) * width + (x + offsetX - sourceRect.left)];
            if (replace || src.rgbReserved == 255)
            {
                dst = src;
                continue;
            }
            if (src.rgbReserved == 0)
            {
                continue;
            }
            // Straight-alpha "over".
            const auto srcA = src.rgbReserved / 255.0f;
            const auto dstA = dst.rgbReserved / 255.0f;
            const auto outA = srcA + dstA * (1.0f - srcA);
            const auto blend = [srcA, dstA, outA](const float s, const float d) {
                return static_cast<BYTE>((s * srcA + d * dstA * (1.0f - srcA)) / (outA > 0.0f ? outA : 1.0f) + 0.5f);
            };
            dst = RGBQUAD{
                blend(src.rgbBlue, dst.rgbBlue),
                blend(src.rgbGreen, dst.rgbGreen),
                blend(src.rgbRed, dst.rgbRed),
                static_cast<BYTE>(outA * 255.0f + 0.5f),
            };
        }
    }
    if (image->currentFrame == destination)
    {
        image->pixels = dstFrame.pixels;
        image->gen = _nextImageGen++;
        _Bump();
    }
    return true;
}

bool KittyImages::SetAnimationState(const uint32_t imageId, const uint32_t state, const uint32_t loops)
{
    const auto image = std::find_if(_images.begin(), _images.end(), [&](const Image& entry) {
        return entry.id == imageId;
    });
    if (image == _images.end())
    {
        return false;
    }
    image->animState = state;
    image->loops = loops;
    return true;
}

KittyImages::Placement* KittyImages::FindPlacement(const uint32_t imageId, const uint32_t placementId) noexcept{
    for (auto& placement : _placements)
    {
        if (placement.imageId == imageId && placement.placementId == placementId)
        {
            return &placement;
        }
    }
    return nullptr;
}

const std::vector<KittyImages::Placement>& KittyImages::Placements() const noexcept
{
    return _placements;
}

void KittyImages::AddPlacement(Placement placement)
{
    // A placement id of zero means "unnamed": every command adds a new
    // placement. A non-zero id replaces the placement with the same
    // (image id, placement id) pair.
    if (placement.placementId != 0)
    {
        std::erase_if(_placements, [&](const Placement& existing) {
            return existing.imageId == placement.imageId && existing.placementId == placement.placementId;
        });
    }
    if (placement.virtual_)
    {
        _hasVirtualPlacements = true;
    }
    _placements.push_back(std::move(placement));
    _Bump();
}

void KittyImages::OnCircularIncrement() noexcept
{
    for (auto& placement : _placements)
    {
        if (!placement.virtual_)
        {
            placement.y -= 1;
        }
    }
    std::erase_if(_placements, [](const Placement& placement) {
        return !placement.virtual_ && placement.y < 0;
    });
    _composedRows.clear();
    _Bump();
}

void KittyImages::OnScrollRows(const til::CoordType firstRow, const til::CoordType size, const til::CoordType delta) noexcept
{
    if (delta == 0)
    {
        return;
    }
    const auto lastRow = firstRow + size;
    for (auto& placement : _placements)
    {
        if (!placement.virtual_ && placement.y >= firstRow && placement.y < lastRow)
        {
            placement.y += delta;
        }
    }
    std::erase_if(_placements, [](const Placement& placement) {
        return !placement.virtual_ && placement.y < 0;
    });
    _composedRows.clear();
    _Bump();
}

void KittyImages::OnResize(const til::CoordType height) noexcept
{
    for (auto& placement : _placements)
    {
        if (!placement.virtual_)
        {
            placement.y = std::max<til::CoordType>(0, std::min(placement.y, height - placement.rows));
        }
    }
    _strips.clear();
    _stripBytes = 0;
    _composedRows.clear();
    _Bump();
}

void KittyImages::Clear() noexcept
{
    _images.clear();
    _placements.clear();
    _strips.clear();
    _stripBytes = 0;
    _composedRows.clear();
    _hasVirtualPlacements = false;
    _Bump();
}

void KittyImages::_Bump() noexcept
{
    ++_generation;
}

const std::vector<RGBQUAD>& KittyImages::_Strip(const Placement& placement, const Image& image, const til::CoordType row, const til::size cellSize)
{
    const auto key = StripKey{
        .imageGen = image.gen,
        .imageId = image.id,
        .placementId = placement.placementId,
        .row = row,
        .cellW = cellSize.width,
        .cellH = cellSize.height,
    };
    for (const auto& entry : _strips)
    {
        if (entry.first == key)
        {
            return entry.second;
        }
    }

    const auto srcX = placement.srcRect.left;
    const auto srcY = placement.srcRect.top;
    const auto srcW = placement.srcRect.right - placement.srcRect.left;
    const auto srcH = placement.srcRect.bottom - placement.srcRect.top;
    const auto dstW = placement.cols * cellSize.width;
    const auto dstH = placement.rows * cellSize.height;

    // The source rectangle is scaled to fit the destination rectangle with
    // the aspect ratio preserved, letterboxed inside it.
    const auto scaleX = static_cast<double>(dstW) / srcW;
    const auto scaleY = static_cast<double>(dstH) / srcH;
    const auto scale = std::min(scaleX, scaleY);
    const auto visibleW = std::min(dstW, static_cast<til::CoordType>(std::lround(srcW * scale)));
    const auto visibleH = std::min(dstH, static_cast<til::CoordType>(std::lround(srcH * scale)));
    const auto offX = (dstW - visibleW) / 2;
    const auto offY = (dstH - visibleH) / 2;

    auto strip = std::vector<RGBQUAD>(static_cast<size_t>(dstW) * cellSize.height);

    // This strip covers destination pixel rows [row*cellH, row*cellH+cellH).
    for (til::CoordType y = 0; y < cellSize.height; ++y)
    {
        const auto dstY = row * cellSize.height + y;
        const auto inVisibleY = dstY >= offY && dstY < offY + visibleH;
        for (til::CoordType x = 0; x < dstW; ++x)
        {
            auto& out = strip[static_cast<size_t>(y) * dstW + x];
            out = RGBQUAD{ 0, 0, 0, 0 };
            if (!inVisibleY || x < offX || x >= offX + visibleW)
            {
                continue;
            }
            // Box filter: a destination pixel averages the source pixels
            // that map to it, so downscales stay smooth. Upscales fall
            // back to the nearest source pixel.
            const auto sx0 = static_cast<til::CoordType>(std::floor(srcX + (x - offX) / scale));
            const auto sx1 = std::max(sx0 + 1, static_cast<til::CoordType>(std::ceil(srcX + (x - offX + 1) / scale)));
            const auto sy0 = static_cast<til::CoordType>(std::floor(srcY + (dstY - offY) / scale));
            const auto sy1 = std::max(sy0 + 1, static_cast<til::CoordType>(std::ceil(srcY + (dstY - offY + 1) / scale)));
            uint32_t r = 0, g = 0, b = 0, a = 0, count = 0;
            for (auto sy = std::max(sy0, srcY); sy < std::min(sy1, srcY + srcH); ++sy)
            {
                for (auto sx = std::max(sx0, srcX); sx < std::min(sx1, srcX + srcW); ++sx)
                {
                    const auto& pixel = image.pixels[static_cast<size_t>(sy) * image.size.width + sx];
                    r += pixel.rgbRed;
                    g += pixel.rgbGreen;
                    b += pixel.rgbBlue;
                    a += pixel.rgbReserved;
                    ++count;
                }
            }
            if (count)
            {
                out = RGBQUAD{
                    static_cast<BYTE>(b / count),
                    static_cast<BYTE>(g / count),
                    static_cast<BYTE>(r / count),
                    static_cast<BYTE>(a / count),
                };
            }
        }
    }

    _stripBytes += strip.size() * sizeof(RGBQUAD);
    if (_stripBytes > MaxStripBytes)
    {
        _strips.clear();
        _stripBytes = strip.size() * sizeof(RGBQUAD);
    }
    _strips.emplace_back(key, std::move(strip));
    return _strips.back().second;
}

namespace
{
    struct Op
    {
        const KittyImages::Placement* placement;
        const KittyImages::Image* image;
        // The placement-relative strip row to sample.
        til::CoordType stripRow;
        // Destination columns, in cells.
        til::CoordType columnBegin;
        til::CoordType columnEnd;
        // Pixel offsets applied to the destination, in pixels.
        til::CoordType pixelOffsetX;
        til::CoordType pixelOffsetY;
        // For placeholder cells: the grid position the cell encodes.
        int gridRow;
        int gridColumn;
        // Column in the strip to sample for this op's left edge, in
        // pixels. Normally 0; gridRow/gridColumn cells use grid coords.
        til::CoordType stripOffsetX;
    };
}

void KittyImages::ComposeRow(const TextBuffer& buffer, ROW& row, const til::CoordType rowY, const til::size cellSize)
{
    if (!HasContent() && _composedRows.empty())
    {
        return;
    }
    if (cellSize.width <= 0 || cellSize.height <= 0)
    {
        return;
    }

    til::small_vector<Op, 4> ops;

    for (const auto& placement : _placements)
    {
        if (placement.virtual_)
        {
            continue;
        }
        if (rowY < placement.y - 1 || rowY >= placement.y + placement.rows)
        {
            continue;
        }
        const auto image = FindImage(placement.imageId);
        if (!image)
        {
            continue;
        }
        // A placement can contribute to this row through more than one of
        // its strips when offset pixels shift content across row bounds.
        for (auto r = std::max<til::CoordType>(0, rowY - placement.y - 1); r < rowY - placement.y + 1 && r < placement.rows; ++r)
        {
            ops.push_back(Op{
                .placement = &placement,
                .image = image,
                .stripRow = r,
                .columnBegin = placement.x,
                .columnEnd = placement.x + placement.cols,
                .pixelOffsetX = placement.offsetX,
                .pixelOffsetY = placement.offsetY,
                .gridRow = -1,
                .gridColumn = -1,
                .stripOffsetX = 0,
            });
        }
    }

    if (_hasVirtualPlacements)
    {
        static thread_local std::vector<KittyPlaceholder::Cell> cells;
        if (KittyPlaceholder::DecodeRow(buffer, row, rowY, cells))
        {
            for (const auto& cell : cells)
            {
                if (cell.imageIdLow < 0 || cell.row < 0 || cell.columnInGrid < 0)
                {
                    continue;
                }
                const auto imageId = static_cast<uint32_t>(cell.imageIdLow) | (cell.imageIdHigh < 0 ? 0u : static_cast<uint32_t>(cell.imageIdHigh) << 24);
                const auto image = FindImage(imageId);
                if (!image)
                {
                    continue;
                }
                // The underline color carries the placement id, if any. With
                // none given, the first virtual placement of the image is
                // used.
                const KittyImages::Placement* virtualPlacement = nullptr;
                for (const auto& placement : _placements)
                {
                    if (placement.virtual_ && placement.imageId == imageId)
                    {
                        virtualPlacement = &placement;
                        break;
                    }
                }
                if (!virtualPlacement)
                {
                    continue;
                }
                if (cell.row >= virtualPlacement->rows || cell.columnInGrid >= virtualPlacement->cols)
                {
                    continue;
                }
                ops.push_back(Op{
                    .placement = virtualPlacement,
                    .image = image,
                    .stripRow = cell.row,
                    .columnBegin = cell.column,
                    .columnEnd = cell.column + 1,
                    .pixelOffsetX = 0,
                    .pixelOffsetY = 0,
                    .gridRow = cell.row,
                    .gridColumn = cell.columnInGrid,
                    .stripOffsetX = cell.columnInGrid * cellSize.width,
                });
            }
        }
    }

    // Signature of everything that affects this row's composed pixels.
    auto key = _generation;
    HashCombine(key, static_cast<uint64_t>(cellSize.width) << 32 | static_cast<uint32_t>(cellSize.height));
    for (const auto& op : ops)
    {
        HashCombine(key, op.placement->imageId);
        HashCombine(key, op.placement->placementId);
        HashCombine(key, static_cast<uint64_t>(op.placement->z));
        HashCombine(key, (static_cast<uint64_t>(op.stripRow) << 32) | static_cast<uint32_t>(op.columnBegin));
        HashCombine(key, (static_cast<uint64_t>(op.gridRow + 1) << 32) | static_cast<uint32_t>(op.gridColumn + 1));
    }
    const auto composed = _composedRows.find(rowY);
    if (composed != _composedRows.end() && composed->second == key)
    {
        return;
    }

    if (ops.empty())
    {
        // Nothing to show anymore. If we composed this row before, clear
        // the pixels we wrote. This can also clear Sixel pixels on the
        // same row; mixing Sixel and Kitty images on one row is not
        // supported.
        if (composed != _composedRows.end())
        {
            ImageSlice::EraseCells(row, 0, row.size());
            _composedRows.erase(composed);
        }
        return;
    }

    // The composed pixels changed. Drop whatever we composed before,
    // including a layer that no longer has any placements, and start over.
    if (composed != _composedRows.end())
    {
        ImageSlice::EraseCells(row, 0, row.size());
    }

    // Draw in (z, image id, placement id) order so overlaps behave
    // deterministically: the higher z, then the higher id, ends on top.
    // Negative-z placements compose into the below-text layer, the rest
    // into the above-text one; the renderer paints the former under the
    // glyphs and the latter over them.
    std::sort(ops.begin(), ops.end(), [](const Op& a, const Op& b) {
        if (a.placement->z != b.placement->z)
        {
            return a.placement->z < b.placement->z;
        }
        if (a.placement->imageId != b.placement->imageId)
        {
            return a.placement->imageId < b.placement->imageId;
        }
        return a.placement->placementId < b.placement->placementId;
    });

    ImageSlice* slices[2] = {};
    const auto rowWidth = row.size();
    for (const auto& op : ops)
    {
        const auto below = op.placement->z < 0;
        auto& slice = slices[below ? 1 : 0];
        if (!slice)
        {
            const auto layer = below ? ImageSliceLayer::BelowText : ImageSliceLayer::AboveText;
            slice = row.GetMutableImageSlice(layer);
            if (!slice || slice->CellSize() != cellSize)
            {
                slice = row.SetImageSlice(layer, std::make_unique<ImageSlice>(cellSize));
            }
        }
        const auto columnBegin = std::max<til::CoordType>(0, op.columnBegin);
        const auto columnEnd = std::min<til::CoordType>(rowWidth, op.columnEnd);
        if (columnBegin >= columnEnd)
        {
            continue;
        }
        const auto& strip = _Strip(*op.placement, *op.image, op.stripRow, cellSize);
        const auto stripWidth = op.placement->cols * cellSize.width;
        auto slicePixels = slice->MutablePixels(columnBegin, columnEnd);
        const auto sliceStride = slice->PixelWidth();

        for (til::CoordType y = 0; y < cellSize.height; ++y)
        {
            // The destination pixel row within this row's slice. Direct
            // placements derive it from the placement's own position,
            // which can be shifted into neighboring rows by pixel offsets.
            // Placeholder cells draw into the very row that holds their
            // text, sampling the strip of the grid row they encode.
            const auto localY = op.gridRow < 0
                ? (op.placement->y + op.stripRow) * cellSize.height + op.pixelOffsetY + y - rowY * cellSize.height
                : y;
            if (localY < 0 || localY >= cellSize.height)
            {
                continue;
            }
            const auto stripLine = &strip[static_cast<size_t>(y) * stripWidth];
            auto sliceLine = slicePixels + static_cast<size_t>(localY) * sliceStride;
            // Direct placements sample the whole strip, starting at pixel
            // 0. Placeholder cells sample the strip columns of the grid
            // cell they encode.
            const auto sampleBegin = op.gridRow < 0 ? 0 : op.stripOffsetX;
            const auto sampleCount = op.gridRow < 0 ? stripWidth : cellSize.width;
            for (til::CoordType x = 0; x < sampleCount; ++x)
            {
                const auto sliceX = op.pixelOffsetX + (columnBegin - op.columnBegin) * cellSize.width + x;
                if (sliceX < 0 || sliceX >= (columnEnd - columnBegin) * cellSize.width)
                {
                    continue;
                }
                const auto& pixel = stripLine[sampleBegin + x];
                if (pixel.rgbReserved == 0)
                {
                    continue;
                }
                sliceLine[sliceX] = pixel;
            }
        }
    }

    for (auto* const composedSlice : slices)
    {
        if (composedSlice)
        {
            composedSlice->BumpRevision();
        }
    }
    if (_composedRows.size() >= MaxComposedRowEntries)
    {
        _composedRows.clear();
    }
    _composedRows[rowY] = key;
}
