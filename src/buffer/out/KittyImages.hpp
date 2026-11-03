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

    // One animation frame of an image. Frame numbers are 1-based in the
    // protocol; frame 1 is the root data transmitted with the image itself.
    // The extra frames start at frame 2.
    struct Frame
    {
        std::vector<RGBQUAD> pixels; // full image size, straight alpha
        int32_t gap = 0; // ms to wait before the next frame; < 0 is gapless
    };

    struct Image
    {
        uint32_t id = 0;
        uint32_t number = 0; // the client supplied I= key, 0 if none
        til::size size; // in pixels
        std::vector<RGBQUAD> pixels; // row-major, BGRA, straight alpha; the currently displayed frame
        uint32_t format = 32; // 24, 32 or 100, kept for animation frame data
        char compressed = 0; // 'z' when the payload arrived zlib compressed
        uint32_t currentFrame = 1; // 1-based frame number shown in pixels
        uint32_t animState = 0; // last s= value of a=a, kept for the client
        uint32_t loops = 0; // last v= value of a=a, kept for the client
        // Root frame first (a copy of the pixels as transmitted), the extra
        // animation frames after it; frames[k] is frame number k + 1.
        std::vector<Frame> frames;
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
    // placements, per the spec. The format and compression are kept so
    // later animation frame data can be decoded the same way.
    const Image* StoreImage(uint32_t id, uint32_t number, til::size size, std::vector<RGBQUAD>&& pixels, uint32_t format = 32, char compressed = 0);
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

    // Animation frames. Every frame of an image is a full-size pixel
    // buffer; frame 1 is the root data. StoredFrameGap returns the frame
    // gap of a frame, 0 when it has none.
    static constexpr uint32_t MaxFrames = 1024;
    // Returns the image with its frames ensured to hold `frame` entries,
    // padding with transparent frames, or nullptr when the frame number is
    // out of range.
    Image* EnsureFrames(uint32_t imageId, uint32_t frame);
    // Stores a full frame buffer as the given frame (0 = append after the
    // last). Returns false when the image or frame number is invalid.
    bool StoreFrame(uint32_t imageId, uint32_t frame, std::vector<RGBQUAD>&& pixels, int32_t gap);
    // Copies the given frame into the image's displayed pixels. Returns
    // false when the image or frame number is invalid.
    bool SelectFrame(uint32_t imageId, uint32_t frame);
    // Sets just a frame's gap. Returns false on an invalid image or frame.
    bool SetFrameGap(uint32_t imageId, uint32_t frame, int32_t gap);
    // Copies a rectangle of one frame over another, alpha blended or
    // replaced depending on `replace`. Returns false when the image, frame
    // numbers or rectangle are invalid.
    bool ComposeFrames(uint32_t imageId, uint32_t source, uint32_t destination, const til::rect& sourceRect, const til::point& destinationOffset, bool replace);
    // Records the last animation-control state keys. Returns false on an
    // unknown image.
    bool SetAnimationState(uint32_t imageId, uint32_t state, uint32_t loops);
    // Drops an image's animation frames; the root image data remains.
    bool DeleteFrames(uint32_t imageId);
    // All stored images, in arrival order.
    const std::vector<Image>& Images() const noexcept;
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

    // Drops a row's cached composition so the next paint recomposes it.
    // Called when text is written over a row that carries image pixels.
    void InvalidateRow(til::CoordType rowY) noexcept;

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
        // Two placements of one image can share every field above — two
        // crops sent without a p= both carry placement id 0 — so the
        // geometry a strip is built from is part of its identity.
        til::rect srcRect;
        til::CoordType cols;
        til::CoordType rows;
        til::CoordType offsetX;
        til::CoordType offsetY;
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
