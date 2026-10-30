// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "GraphicsImage.hpp"

static std::atomic<uint64_t> s_graphicsRevision{ 0 };

// The combining characters used as placeholder row and column numbers, in the
// order of their values. These are the characters of combining class 230 in
// Unicode 6.0.0 without decomposition mappings, as defined by the protocol.
static constexpr char32_t s_placeholderDiacritics[]{
    0x0305, 0x030D, 0x030E, 0x0310, 0x0312, 0x033D, 0x033E, 0x033F, 0x0346, 0x034A, 0x034B, 0x034C,
    0x0350, 0x0351, 0x0352, 0x0357, 0x035B, 0x0363, 0x0364, 0x0365, 0x0366, 0x0367, 0x0368, 0x0369,
    0x036A, 0x036B, 0x036C, 0x036D, 0x036E, 0x036F, 0x0483, 0x0484, 0x0485, 0x0486, 0x0487, 0x0592,
    0x0593, 0x0594, 0x0595, 0x0597, 0x0598, 0x0599, 0x059C, 0x059D, 0x059E, 0x059F, 0x05A0, 0x05A1,
    0x05A8, 0x05A9, 0x05AB, 0x05AC, 0x05AF, 0x05C4, 0x0610, 0x0611, 0x0612, 0x0613, 0x0614, 0x0615,
    0x0616, 0x0617, 0x0657, 0x0658, 0x0659, 0x065A, 0x065B, 0x065D, 0x065E, 0x06D6, 0x06D7, 0x06D8,
    0x06D9, 0x06DA, 0x06DB, 0x06DC, 0x06DF, 0x06E0, 0x06E1, 0x06E2, 0x06E4, 0x06E7, 0x06E8, 0x06EB,
    0x06EC, 0x0730, 0x0732, 0x0733, 0x0735, 0x0736, 0x073A, 0x073D, 0x073F, 0x0740, 0x0741, 0x0743,
    0x0745, 0x0747, 0x0749, 0x074A, 0x07EB, 0x07EC, 0x07ED, 0x07EE, 0x07EF, 0x07F0, 0x07F1, 0x07F3,
    0x0816, 0x0817, 0x0818, 0x0819, 0x081B, 0x081C, 0x081D, 0x081E, 0x081F, 0x0820, 0x0821, 0x0822,
    0x0823, 0x0825, 0x0826, 0x0827, 0x0829, 0x082A, 0x082B, 0x082C, 0x082D, 0x0951, 0x0953, 0x0954,
    0x0F82, 0x0F83, 0x0F86, 0x0F87, 0x135D, 0x135E, 0x135F, 0x17DD, 0x193A, 0x1A17, 0x1A75, 0x1A76,
    0x1A77, 0x1A78, 0x1A79, 0x1A7A, 0x1A7B, 0x1A7C, 0x1B6B, 0x1B6D, 0x1B6E, 0x1B6F, 0x1B70, 0x1B71,
    0x1B72, 0x1B73, 0x1CD0, 0x1CD1, 0x1CD2, 0x1CDA, 0x1CDB, 0x1CE0, 0x1DC0, 0x1DC1, 0x1DC3, 0x1DC4,
    0x1DC5, 0x1DC6, 0x1DC7, 0x1DC8, 0x1DC9, 0x1DCB, 0x1DCC, 0x1DD1, 0x1DD2, 0x1DD3, 0x1DD4, 0x1DD5,
    0x1DD6, 0x1DD7, 0x1DD8, 0x1DD9, 0x1DDA, 0x1DDB, 0x1DDC, 0x1DDD, 0x1DDE, 0x1DDF, 0x1DE0, 0x1DE1,
    0x1DE2, 0x1DE3, 0x1DE4, 0x1DE5, 0x1DE6, 0x1DFE, 0x20D0, 0x20D1, 0x20D4, 0x20D5, 0x20D6, 0x20D7,
    0x20DB, 0x20DC, 0x20E1, 0x20E7, 0x20E9, 0x20F0, 0x2CEF, 0x2CF0, 0x2CF1, 0x2DE0, 0x2DE1, 0x2DE2,
    0x2DE3, 0x2DE4, 0x2DE5, 0x2DE6, 0x2DE7, 0x2DE8, 0x2DE9, 0x2DEA, 0x2DEB, 0x2DEC, 0x2DED, 0x2DEE,
    0x2DEF, 0x2DF0, 0x2DF1, 0x2DF2, 0x2DF3, 0x2DF4, 0x2DF5, 0x2DF6, 0x2DF7, 0x2DF8, 0x2DF9, 0x2DFA,
    0x2DFB, 0x2DFC, 0x2DFD, 0x2DFE, 0x2DFF, 0xA66F, 0xA67C, 0xA67D, 0xA6F0, 0xA6F1, 0xA8E0, 0xA8E1,
    0xA8E2, 0xA8E3, 0xA8E4, 0xA8E5, 0xA8E6, 0xA8E7, 0xA8E8, 0xA8E9, 0xA8EA, 0xA8EB, 0xA8EC, 0xA8ED,
    0xA8EE, 0xA8EF, 0xA8F0, 0xA8F1, 0xAAB0, 0xAAB2, 0xAAB3, 0xAAB7, 0xAAB8, 0xAABE, 0xAABF, 0xAAC1,
    0xFE20, 0xFE21, 0xFE22, 0xFE23, 0xFE24, 0xFE25, 0xFE26, 0x10A0F, 0x10A38, 0x1D185, 0x1D186, 0x1D187,
    0x1D188, 0x1D189, 0x1D1AA, 0x1D1AB, 0x1D1AC, 0x1D1AD, 0x1D242, 0x1D243, 0x1D244,
};

int GraphicsPlaceholder::DiacriticValue(const char32_t ch) noexcept
{
    const auto it = std::lower_bound(std::begin(s_placeholderDiacritics), std::end(s_placeholderDiacritics), ch);
    return it != std::end(s_placeholderDiacritics) && *it == ch ? gsl::narrow_cast<int>(it - std::begin(s_placeholderDiacritics)) : -1;
}

size_t GraphicsImage::ByteSize() const noexcept
{
    return pixels.size() * sizeof(uint32_t);
}

uint64_t GraphicsImage::NextRevision() noexcept
{
    // Revisions are never 0, so that the renderer can use it as a sentinel.
    return s_graphicsRevision.fetch_add(1, std::memory_order_relaxed) + 1;
}

GraphicsRect GraphicsGeometry::Destination(const til::size cellSize) const noexcept
{
    const auto scaleX = static_cast<float>(cellSize.width) / std::max(1, virtualCellSize.width);
    const auto scaleY = static_cast<float>(cellSize.height) / std::max(1, virtualCellSize.height);
    const auto srcWidth = static_cast<float>(std::max(1, source.width()));
    const auto srcHeight = static_cast<float>(std::max(1, source.height()));
    const auto left = offset.x * scaleX;
    const auto top = offset.y * scaleY;

    if (columns > 0 && rows > 0)
    {
        // The image is letterboxed within the requested area to avoid distortion.
        const auto boxWidth = std::max(0.0f, static_cast<float>(columns * cellSize.width) - left);
        const auto boxHeight = std::max(0.0f, static_cast<float>(rows * cellSize.height) - top);
        const auto scale = std::min(boxWidth / srcWidth, boxHeight / srcHeight);
        const auto width = srcWidth * scale;
        const auto height = srcHeight * scale;
        const auto l = left + (boxWidth - width) / 2;
        const auto t = top + (boxHeight - height) / 2;
        return { l, t, l + width, t + height };
    }
    if (rows > 0)
    {
        const auto bottom = static_cast<float>(rows * cellSize.height);
        const auto width = (bottom - top) * srcWidth / srcHeight;
        return { left, top, left + width, bottom };
    }
    if (columns > 0)
    {
        const auto right = static_cast<float>(columns * cellSize.width);
        const auto height = (right - left) * srcHeight / srcWidth;
        return { left, top, right, top + height };
    }
    return { left, top, left + srcWidth * scaleX, top + srcHeight * scaleY };
}

std::shared_ptr<const GraphicsImage> GraphicsStore::Find(const uint32_t id) const noexcept
{
    for (auto it = _images.rbegin(); it != _images.rend(); ++it)
    {
        if ((*it)->id == id)
        {
            return *it;
        }
    }
    return nullptr;
}

std::shared_ptr<const GraphicsImage> GraphicsStore::FindByNumber(const uint32_t number) const noexcept
{
    for (auto it = _images.rbegin(); it != _images.rend(); ++it)
    {
        if ((*it)->number == number)
        {
            return *it;
        }
    }
    return nullptr;
}

void GraphicsStore::Add(std::shared_ptr<const GraphicsImage> image)
{
    Remove(image->id);
    _enforceQuota(image->ByteSize());
    _totalBytes += image->ByteSize();
    _images.emplace_back(std::move(image));
}

void GraphicsStore::Remove(const uint32_t id) noexcept
{
    for (auto it = _images.begin(); it != _images.end(); ++it)
    {
        if ((*it)->id == id)
        {
            _totalBytes -= (*it)->ByteSize();
            _images.erase(it);
            break;
        }
    }
    RemoveVirtualPlacements(id, 0);
}

uint32_t GraphicsStore::NextFreeId() noexcept
{
    for (;;)
    {
        const auto id = ++_nextId;
        if (id != 0 && !Find(id))
        {
            return id;
        }
    }
}

// Returns the ids of all stored images and virtual placements.
std::vector<uint32_t> GraphicsStore::Ids() const
{
    std::vector<uint32_t> ids;
    for (const auto& image : _images)
    {
        ids.push_back(image->id);
    }
    for (const auto& placement : _virtualPlacements)
    {
        ids.push_back(placement.imageId);
    }
    return ids;
}

void GraphicsStore::SetVirtualPlacement(const GraphicsVirtualPlacement& placement)
{
    for (auto& p : _virtualPlacements)
    {
        if (p.imageId == placement.imageId && p.placementId == placement.placementId)
        {
            p = placement;
            return;
        }
    }
    _virtualPlacements.emplace_back(placement);
}

const GraphicsVirtualPlacement* GraphicsStore::FindVirtualPlacement(const uint32_t imageId, const uint32_t placementId) const noexcept
{
    for (const auto& p : _virtualPlacements)
    {
        if (p.imageId == imageId && (placementId == 0 || p.placementId == placementId))
        {
            return &p;
        }
    }
    return nullptr;
}

void GraphicsStore::RemoveVirtualPlacements(const uint32_t imageId, const uint32_t placementId) noexcept
{
    std::erase_if(_virtualPlacements, [&](const auto& p) {
        return p.imageId == imageId && (placementId == 0 || p.placementId == placementId);
    });
}

void GraphicsStore::Clear() noexcept
{
    _images.clear();
    _virtualPlacements.clear();
    _totalBytes = 0;
}

void GraphicsStore::_enforceQuota(const size_t incoming) noexcept
{
    // Images that aren't displayed anywhere are evicted first, oldest first.
    for (auto it = _images.begin(); it != _images.end() && _totalBytes + incoming > Quota;)
    {
        if (it->use_count() == 1)
        {
            _totalBytes -= (*it)->ByteSize();
            RemoveVirtualPlacements((*it)->id, 0);
            it = _images.erase(it);
        }
        else
        {
            ++it;
        }
    }
    while (!_images.empty() && _totalBytes + incoming > Quota)
    {
        _totalBytes -= _images.front()->ByteSize();
        RemoveVirtualPlacements(_images.front()->id, 0);
        _images.pop_front();
    }
}
