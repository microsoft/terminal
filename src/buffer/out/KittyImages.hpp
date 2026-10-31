/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- KittyImages.hpp

Abstract:
- This stores the images transmitted via the Kitty Graphics Protocol and the
  placements that reference them, and composes the pixels of visible
  placements into the per-row ImageSlices the renderer already paints for
  Sixel images. One instance is attached to each TextBuffer.
- Placements are kept as records in buffer coordinates instead of baking
  pixels into rows, so they can be deleted, replaced or moved later. The
  composed row pixels are cached and only recomputed when the placement
  data, the font cell size or the underlying text changes.
--*/

#pragma once

#include "til.h"
#include <unordered_map>
#include <utility>
#include <vector>

class ROW;
class TextBuffer;

class KittyImages
{
public:
    static constexpr size_t MaxTotalImageBytes = 256ull * 1024 * 1024;
    static constexpr til::CoordType MaxDimension = 10000;
    static constexpr uint32_t AutoPlacementId = 0;

    struct Image
    {
        uint32_t id = 0;
        uint32_t number = 0; // the client supplied I= key, 0 if none
        til::size size; // in pixels
        std::vector<RGBQUAD> pixels; // row-major, BGRA, straight alpha
        uint64_t gen = 0; // bumped when the pixel data is replaced
    };

    struct Placement
    {
        uint32_t imageId = 0;
        uint32_t placementId = 0; // 0 = unnamed
        // Source rectangle, in pixels of the image.
        til::rect srcRect;
        // Destination, in buffer cells. For virtual placements (U=1) only
        // the width/height of the rectangle are meaningful; x/y are unused.
        til::CoordType x = 0;
        til::CoordType y = 0;
        til::CoordType cols = 0;
        til::CoordType rows = 0;
        til::CoordType offsetX = 0; // X= pixel offset within the cell
        til::CoordType offsetY = 0; // Y= pixel offset within the cell
        int32_t z = 0;
        bool virtual_ = false;
    };

    // A cheap test the renderer uses to skip all per-row work.
    bool HasContent() const noexcept;
    // Bumped whenever anything that affects composed output changes.
    uint64_t Generation() const noexcept;

    const Image* FindImage(uint32_t id) const noexcept;
    const Image* FindNewestImageByNumber(uint32_t number) const noexcept;
    // Stores a new image. Returns the image, with id filled in when the
    // caller passed 0 (the terminal assigns an unused id).
    // Re-transmitting an existing id replaces the image and deletes its
    // placements, per the spec.
    const Image* StoreImage(uint32_t id, uint32_t number, til::size size, std::vector<RGBQUAD>&& pixels);
    bool HasPlacements(uint32_t imageId) const noexcept;
    // Frees the data of every image without placements. Returns how many.
    size_t DeleteUnreferencedImages();
    // Frees the image data. Returns false if the id is unknown.
    bool DeleteImage(uint32_t id);

    Placement* FindPlacement(uint32_t imageId, uint32_t placementId) noexcept;
    const std::vector<Placement>& Placements() const noexcept;
    // Adds a placement, or replaces the existing one with the same
    // (imageId, placementId) pair when placementId is non-zero.
    void AddPlacement(Placement placement);
    // Deletes placements matching the predicate. Returns how many.
    template<typename F>
    size_t DeletePlacements(F&& filter)
    {
        const auto oldSize = _placements.size();
        std::erase_if(_placements, std::forward<F>(filter));
        const auto removed = oldSize - _placements.size();
        if (removed)
        {
            _Bump();
        }
        return removed;
    }

    // Scroll bookkeeping. Placements live in buffer coordinates, so they
    // must ride along whenever the buffer itself scrolls.
    void OnCircularIncrement() noexcept;
    void OnScrollRows(til::CoordType firstRow, til::CoordType size, til::CoordType delta) noexcept;
    // Called when the buffer height changes (e.g. on resize/reflow), after
    // the state was moved into the resized buffer. Clamps placement
    // coordinates into range.
    void OnResize(til::CoordType height) noexcept;

    // Composes the pixels of everything visible on the given row into the
    // row's ImageSlice (creating it if needed). Cheap when nothing changed
    // since the previous call. rowY is the row's offset within the buffer,
    // cellSize the current font's cell size in pixels.
    void ComposeRow(const TextBuffer& buffer, ROW& row, til::CoordType rowY, til::size cellSize);

    void Clear() noexcept;

private:
    struct StripKey
    {
        uint64_t imageGen;
        uint32_t imageId;
        uint32_t placementId;
        til::CoordType row;
        til::CoordType cellW;
        til::CoordType cellH;
    };

    friend bool operator==(const StripKey& a, const StripKey& b) noexcept;

    void _Bump() noexcept;
    // Returns the scaled pixels of one cell-row of a placement: a strip of
    // width cols*cellW and height cellH, taken from the placement's source
    // rectangle scaled into its destination rectangle, letterboxed.
    const std::vector<RGBQUAD>& _Strip(const Placement& placement, const Image& image, til::CoordType row, til::size cellSize);

    std::vector<Image> _images;
    std::vector<Placement> _placements;
    // Cache of composed strips, keyed per (image, placement, row, cell).
    // Strips are flushed whenever the generation changes or the cache
    // grows past a soft cap.
    std::vector<std::pair<StripKey, std::vector<RGBQUAD>>> _strips;
    size_t _stripBytes = 0;
    // The generation and cell size a row was last composed with, used to
    // skip redundant recompositions. Rows with no entry were never
    // composed by us and need no cleanup.
    std::unordered_map<til::CoordType, uint64_t> _composedRows;
    uint64_t _generation = 1;
    uint64_t _nextImageGen = 1;
    bool _hasVirtualPlacements = false; // gates the per-row text scan
};
