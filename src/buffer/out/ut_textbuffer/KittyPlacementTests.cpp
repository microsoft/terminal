// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "../../inc/consoletaeftemplates.hpp"

#include "../textBuffer.hpp"
#include "../KittyImages.hpp"
#include "../../renderer/inc/DummyRenderer.hpp"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

// Tests for how Kitty Graphics placements track the buffer as it scrolls
// and reflows. The placements live in buffer coordinates, so every buffer
// movement has to carry them along.

namespace
{
    std::unique_ptr<KittyImages::Placement> MakePlacement(const uint32_t imageId, const til::CoordType x, const til::CoordType y, const til::CoordType cols, const til::CoordType rows)
    {
        auto placement = std::make_unique<KittyImages::Placement>();
        placement->imageId = imageId;
        placement->placementId = 1;
        placement->srcRect = { 0, 0, 10, 10 };
        placement->x = x;
        placement->y = y;
        placement->cols = cols;
        placement->rows = rows;
        return placement;
    }

    void StoreImage(TextBuffer& buffer, const uint32_t id)
    {
        std::vector<RGBQUAD> pixels(10 * 10, RGBQUAD{ 0, 0, 0, 255 });
        buffer.GetKittyImages().StoreImage(id, 0, { 10, 10 }, std::move(pixels));
    }
}

class KittyPlacementTests
{
    TEST_CLASS(KittyPlacementTests);

    TEST_METHOD(ScrollRowsShiftsPlacements)
    {
        DummyRenderer renderer;
        TextBuffer buffer{ til::size{ 10, 5 }, TextAttribute{ 0x7 }, 0, false, &renderer };
        StoreImage(buffer, 1);
        buffer.GetKittyImages().AddPlacement(*MakePlacement(1, 0, 2, 2, 2));

        // Rows 0..5 move down by one; the placement rides along.
        buffer.ScrollRows(0, 5, 1);
        const auto& placements = buffer.GetKittyImages().Placements();
        VERIFY_ARE_EQUAL(1u, placements.size());
        VERIFY_ARE_EQUAL(3, placements[0].y);
    }

    TEST_METHOD(CircularIncrementShiftsPlacements)
    {
        DummyRenderer renderer;
        TextBuffer buffer{ til::size{ 10, 5 }, TextAttribute{ 0x7 }, 0, false, &renderer };
        StoreImage(buffer, 1);
        buffer.GetKittyImages().AddPlacement(*MakePlacement(1, 0, 0, 2, 2));

        // Rotating the buffer drops the top row; a placement there is
        // gone, and everything below moves up.
        buffer.GetKittyImages().AddPlacement(*MakePlacement(1, 4, 1, 1, 1));
        buffer.IncrementCircularBuffer(TextAttribute{ 0x7 });
        const auto& placements = buffer.GetKittyImages().Placements();
        VERIFY_ARE_EQUAL(1u, placements.size());
        VERIFY_ARE_EQUAL(0, placements[0].y);
        VERIFY_ARE_EQUAL(4, placements[0].x);
    }

    TEST_METHOD(ReflowCarriesStorageToNewBuffer)
    {
        DummyRenderer renderer;
        TextBuffer original{ til::size{ 10, 5 }, TextAttribute{ 0x7 }, 0, false, &renderer };
        StoreImage(original, 7);
        original.GetKittyImages().AddPlacement(*MakePlacement(7, 1, 1, 2, 2));

        auto resized = std::make_unique<TextBuffer>(til::size{ 8, 4 }, TextAttribute{ 0x7 }, 0, false, &renderer);
        TextBuffer::Reflow(original, *resized);

        // The image and the placement must travel to the resized buffer.
        VERIFY_IS_NOT_NULL(resized->GetKittyImages().FindImage(7));
        VERIFY_ARE_EQUAL(1u, resized->GetKittyImages().Placements().size());
        const auto& placement = resized->GetKittyImages().Placements()[0];
        VERIFY_ARE_EQUAL(1, placement.y);
        // And the original buffer no longer holds them.
        VERIFY_IS_NULL(original.GetKittyImages().FindImage(7));
    }

    TEST_METHOD(ReflowClampsPlacementsToNewHeight)
    {
        DummyRenderer renderer;
        TextBuffer original{ til::size{ 10, 10 }, TextAttribute{ 0x7 }, 0, false, &renderer };
        StoreImage(original, 7);
        original.GetKittyImages().AddPlacement(*MakePlacement(7, 0, 8, 2, 2));

        auto resized = std::make_unique<TextBuffer>(til::size{ 10, 4 }, TextAttribute{ 0x7 }, 0, false, &renderer);
        TextBuffer::Reflow(original, *resized);

        const auto& placements = resized->GetKittyImages().Placements();
        VERIFY_ARE_EQUAL(1u, placements.size());
        // A 2-row placement in a 4-row buffer starting at 8 is clamped to
        // the last possible position.
        VERIFY_ARE_EQUAL(2, placements[0].y);
    }

    TEST_METHOD(DeleteUnreferencedImagesFreesData)
    {
        DummyRenderer renderer;
        TextBuffer buffer{ til::size{ 10, 5 }, TextAttribute{ 0x7 }, 0, false, &renderer };
        StoreImage(buffer, 1);
        StoreImage(buffer, 2);
        buffer.GetKittyImages().AddPlacement(*MakePlacement(1, 0, 0, 1, 1));

        // Image 2 has no placements, so it is freed; image 1 stays.
        VERIFY_ARE_EQUAL(1u, buffer.GetKittyImages().DeleteUnreferencedImages());
        VERIFY_IS_NOT_NULL(buffer.GetKittyImages().FindImage(1));
        VERIFY_IS_NULL(buffer.GetKittyImages().FindImage(2));
    }

    TEST_METHOD(RetransmissionReplacesPlacements)
    {
        DummyRenderer renderer;
        TextBuffer buffer{ til::size{ 10, 5 }, TextAttribute{ 0x7 }, 0, false, &renderer };
        StoreImage(buffer, 1);
        buffer.GetKittyImages().AddPlacement(*MakePlacement(1, 0, 0, 1, 1));

        std::vector<RGBQUAD> pixels(10 * 10, RGBQUAD{ 0, 255, 0, 255 });
        buffer.GetKittyImages().StoreImage(1, 0, { 10, 10 }, std::move(pixels));

        VERIFY_ARE_EQUAL(0u, buffer.GetKittyImages().Placements().size());
        const auto image = buffer.GetKittyImages().FindImage(1);
        VERIFY_IS_NOT_NULL(image);
        VerifyPixel(RGBQUAD{ 0, 255, 0, 255 }, image->pixels[0]);
    }

    TEST_METHOD(NamedPlacementReplacesSameId)
    {
        DummyRenderer renderer;
        TextBuffer buffer{ til::size{ 10, 5 }, TextAttribute{ 0x7 }, 0, false, &renderer };
        StoreImage(buffer, 1);

        auto first = MakePlacement(1, 0, 0, 1, 1);
        first->placementId = 5;
        buffer.GetKittyImages().AddPlacement(std::move(*first));

        auto second = MakePlacement(1, 3, 3, 2, 2);
        second->placementId = 5;
        buffer.GetKittyImages().AddPlacement(std::move(*second));

        const auto& placements = buffer.GetKittyImages().Placements();
        VERIFY_ARE_EQUAL(1u, placements.size());
        VERIFY_ARE_EQUAL(3, placements[0].x);
        VERIFY_ARE_EQUAL(3, placements[0].y);

        // An unnamed placement never replaces anything.
        buffer.GetKittyImages().AddPlacement(*MakePlacement(1, 1, 1, 1, 1));
        VERIFY_ARE_EQUAL(2u, buffer.GetKittyImages().Placements().size());
    }

    static void VerifyPixel(const RGBQUAD& expected, const RGBQUAD& actual)
    {
        VERIFY_ARE_EQUAL(expected.rgbBlue, actual.rgbBlue);
        VERIFY_ARE_EQUAL(expected.rgbGreen, actual.rgbGreen);
        VERIFY_ARE_EQUAL(expected.rgbRed, actual.rgbRed);
        VERIFY_ARE_EQUAL(expected.rgbReserved, actual.rgbReserved);
    }
};
