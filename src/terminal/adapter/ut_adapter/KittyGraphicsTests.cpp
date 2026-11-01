// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include <consoletaeftemplates.hpp>
#include <WexTestClass.h>

#include "../adaptDispatch.hpp"
#include "../../parser/stateMachine.hpp"
#include "../../parser/OutputStateMachineEngine.hpp"
#include "../../../buffer/out/textBuffer.hpp"
#include "../../../buffer/out/ImageSlice.hpp"
#include "../../../buffer/out/KittyImages.hpp"
#include "../../../buffer/out/KittyPlaceholder.hpp"
#include "../../../renderer/inc/DummyRenderer.hpp"

using namespace WEX::TestExecution;
using namespace WEX::Logging;
using namespace WEX::Common;
using namespace Microsoft::Console::VirtualTerminal;

// Tests for the Kitty Graphics Protocol APC dispatch path, end to end:
// raw VT bytes -> StateMachine -> AdaptDispatch -> KittyGraphicsParser ->
// KittyImages, plus the row composition that feeds the renderer.
// A small local ITerminalApi mock keeps these tests independent of the
// larger TestGetSet harness in adapterTest.cpp.

namespace
{
    void VerifyPixel(const RGBQUAD& expected, const RGBQUAD& actual)
    {
        VERIFY_ARE_EQUAL(expected.rgbBlue, actual.rgbBlue);
        VERIFY_ARE_EQUAL(expected.rgbGreen, actual.rgbGreen);
        VERIFY_ARE_EQUAL(expected.rgbRed, actual.rgbRed);
        VERIFY_ARE_EQUAL(expected.rgbReserved, actual.rgbReserved);
    }
}

namespace Microsoft::Console::VirtualTerminal
{
    class KittyGraphicsTests
    {
        TEST_CLASS(KittyGraphicsTests);

        class KittyTestApi final : public ITerminalApi
        {
        public:
            std::wstring response;
            bool retainResponse = false;
            TextBuffer* buffer = nullptr;
            StateMachine* stateMachine = nullptr;
            til::rect viewport{ 0, 0, 80, 25 };
            bool enabled = true;
            til::size cellSize{ 10, 20 };

            void UnknownSequence() noexcept override {}
            void ReturnResponse(const std::wstring_view r) override
            {
                if (retainResponse)
                {
                    response += r;
                }
                else
                {
                    response = r;
                }
            }
            bool IsConPTY() const noexcept override { return false; }
            StateMachine& GetStateMachine() override { return *stateMachine; }
            BufferState GetBufferAndViewport() override
            {
                return { *buffer, viewport, true };
            }
            void SetViewportPosition(const til::point) override {}
            bool IsVtInputEnabled() const override { return false; }
            bool IsKittyGraphicsProtocolEnabled() const noexcept override { return enabled; }
            til::size GetFontCellSize() const override { return cellSize; }
            void SetSystemMode(const Mode, const bool) override {}
            bool GetSystemMode(const Mode) const override { return false; }
            void ReturnAnswerback() override {}
            void WarningBell() override {}
            void SetWindowTitle(const std::wstring_view) override {}
            void UseAlternateScreenBuffer(const TextAttribute&) override {}
            void UseMainScreenBuffer() override {}
            CursorType GetUserDefaultCursorStyle() const override { return CursorType::Legacy; }
            void ShowWindow(bool) override {}
            void SetCodePage(const unsigned int) override {}
            void ResetCodePage() override {}
            unsigned int GetOutputCodePage() const override { return CP_UTF8; }
            unsigned int GetInputCodePage() const override { return CP_UTF8; }
            void CopyToClipboard(const wil::zwstring_view) override {}
            void SetTaskbarProgress(const DispatchTypes::TaskbarState, const size_t) override {}
            void SetWorkingDirectory(const std::wstring_view) override {}
            void PlayMidiNote(const int, const int, const std::chrono::microseconds) override {}
            bool ResizeWindow(const til::CoordType, const til::CoordType) override { return false; }
            void NotifyBufferRotation(const int) override {}
            void NotifyShellIntegrationMark() override {}
            void InvokeCompletions(std::wstring_view, unsigned int) override {}
            void SearchMissingCommand(const std::wstring_view) override {}
            void ShowNotification(const std::wstring_view, const std::wstring_view) override {}
        };

        std::unique_ptr<TerminalInput> _terminalInput;
        std::unique_ptr<StateMachine> _stateMachine;
        std::unique_ptr<KittyTestApi> _api;
        std::unique_ptr<TextBuffer> _buffer;
        DummyRenderer _renderer;

        TEST_METHOD_SETUP(Setup)
        {
            _api = std::make_unique<KittyTestApi>();
            _terminalInput = std::make_unique<TerminalInput>();
            _buffer = std::make_unique<TextBuffer>(til::size{ 80, 25 }, TextAttribute{}, 0, false, nullptr);
            _api->buffer = _buffer.get();

            auto adapter = std::make_unique<AdaptDispatch>(*_api, &_renderer, _renderer._renderSettings, *_terminalInput);
            auto engine = std::make_unique<OutputStateMachineEngine>(std::move(adapter));
            _stateMachine = std::make_unique<StateMachine>(std::move(engine));
            _api->stateMachine = _stateMachine.get();
            return true;
        }

        void Send(const std::wstring_view sequence)
        {
            _api->response.clear();
            _stateMachine->ProcessString(sequence);
        }

        void SendApc(const std::wstring_view body)
        {
            Send(fmt::format(FMT_COMPILE(L"\x1b_G{}\x1b\\"), body));
        }

        TEST_METHOD(DisabledCapabilityIgnoresSequences)
        {
            _api->enabled = false;
            SendApc(L"a=t,f=24,s=1,v=1,i=7;AAAA");
            VERIFY_IS_NULL(_buffer->GetKittyImages().FindImage(7));
            VERIFY_IS_TRUE(_api->response.empty());
        }

        TEST_METHOD(TransmitRgbStoresImage)
        {
            // 2x1 RGB: red, green.
            SendApc(L"a=t,f=24,s=2,v=1,i=7;/wAAAP8A");
            const auto image = _buffer->GetKittyImages().FindImage(7);
            VERIFY_IS_NOT_NULL(image);
            VERIFY_ARE_EQUAL(2, image->size.width);
            VERIFY_ARE_EQUAL(1, image->size.height);
            VerifyPixel(RGBQUAD{ 0, 0, 255, 255 }, image->pixels[0]);
            VerifyPixel(RGBQUAD{ 0, 255, 0, 255 }, image->pixels[1]);
            VERIFY_ARE_EQUAL(L"\x1b_Gi=7;OK\x1b\\", _api->response);
        }

        TEST_METHOD(TransmitRgbaStoresImage)
        {
            // 1x1 RGBA: translucent blue.
            SendApc(L"a=t,f=32,s=1,v=1,i=8;AAD/tA==");
            const auto image = _buffer->GetKittyImages().FindImage(8);
            VERIFY_IS_NOT_NULL(image);
            VerifyPixel(RGBQUAD{ 255, 0, 0, 180 }, image->pixels[0]);
        }

        TEST_METHOD(TransmitPngStoresImage)
        {
            // A 3x2 PNG: red, green, blue on the first row, white,
            // translucent dark blue and black on the second.
            SendApc(L"a=t,f=100,i=9;iVBORw0KGgoAAAANSUhEUgAAAAMAAAACCAYAAACddGYaAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsMAAA7DAcdvqGQAAAAeSURBVBhXY/jPwPAfDBn+/2f4////fy4RuQYGoCAAoI0LsjnxEtIAAAAASUVORK5CYII=");
            const auto image = _buffer->GetKittyImages().FindImage(9);
            VERIFY_IS_NOT_NULL(image);
            VERIFY_ARE_EQUAL(3, image->size.width);
            VERIFY_ARE_EQUAL(2, image->size.height);
            VerifyPixel(RGBQUAD{ 0, 0, 255, 255 }, image->pixels[0]);
            VerifyPixel(RGBQUAD{ 0, 255, 0, 255 }, image->pixels[1]);
            VerifyPixel(RGBQUAD{ 255, 0, 0, 255 }, image->pixels[2]);
            VerifyPixel(RGBQUAD{ 255, 255, 255, 255 }, image->pixels[3]);
        }

        TEST_METHOD(TransmitWithAssignedIdEchoesIt)
        {
            SendApc(L"a=t,f=24,s=1,v=1;AAAA");
            VERIFY_IS_TRUE(_api->response.starts_with(L"\x1b_Gi="));
            VERIFY_IS_TRUE(_api->response.ends_with(L";OK\x1b\\"));
        }

        TEST_METHOD(TransmitWithNumberAssignsId)
        {
            SendApc(L"a=t,f=24,s=1,v=1,I=13;AAAA");
            VERIFY_IS_TRUE(_api->response.find(L"I=13") != std::wstring::npos);
            VERIFY_IS_TRUE(_api->response.find(L";OK") != std::wstring::npos);
            const auto image = _buffer->GetKittyImages().FindNewestImageByNumber(13);
            VERIFY_IS_NOT_NULL(image);
        }

        TEST_METHOD(MutuallyExclusiveIdsRejected)
        {
            SendApc(L"a=t,f=24,s=1,v=1,i=1,I=2;AAAA");
            VERIFY_IS_TRUE(_api->response.find(L"EINVAL") != std::wstring::npos);
            VERIFY_IS_NULL(_buffer->GetKittyImages().FindImage(1));
        }

        TEST_METHOD(SizeMismatchRejected)
        {
            SendApc(L"a=t,f=24,s=2,v=2,i=5;AAAA");
            VERIFY_IS_TRUE(_api->response.find(L"EINVAL") != std::wstring::npos);
            VERIFY_IS_NULL(_buffer->GetKittyImages().FindImage(5));
        }

        TEST_METHOD(UnsupportedMediumAndFormatRejected)
        {
            SendApc(L"a=t,t=f,i=5;AAAA");
            VERIFY_IS_TRUE(_api->response.find(L"ENOSYS") != std::wstring::npos);
            SendApc(L"a=t,o=q,i=5;AAAA");
            VERIFY_IS_TRUE(_api->response.find(L"ENOSYS") != std::wstring::npos);
            SendApc(L"a=t,f=8,s=1,v=1,i=5;AAAA");
            VERIFY_IS_TRUE(_api->response.find(L"ENOSYS") != std::wstring::npos);
        }

        TEST_METHOD(ChunkedTransmission)
        {
            // 2x1 RGB split across two chunks: red + green.
            SendApc(L"a=t,f=24,s=2,v=1,i=7,m=1;/wAA");
            // No image yet, and no response yet.
            VERIFY_IS_NULL(_buffer->GetKittyImages().FindImage(7));
            VERIFY_IS_TRUE(_api->response.empty());
            SendApc(L"m=0;AP8A");
            const auto image = _buffer->GetKittyImages().FindImage(7);
            VERIFY_IS_NOT_NULL(image);
            VerifyPixel(RGBQUAD{ 0, 255, 0, 255 }, image->pixels[1]);
            VERIFY_ARE_EQUAL(L"\x1b_Gi=7;OK\x1b\\", _api->response);
        }

        TEST_METHOD(DirectPlacementComposesPixels)
        {
            // 10x20 px image, 1x1 cells with the 10x20 mock cell size.
            // RGB, so 600 zero bytes, which is exactly 800 base64 chars.
            const auto payload = std::wstring(800, L'A');
            SendApc(fmt::format(FMT_COMPILE(L"a=T,f=24,s=10,v=20,i=3,p=1;{}"), payload));
            const auto& placements = _buffer->GetKittyImages().Placements();
            VERIFY_ARE_EQUAL(1u, placements.size());
            VERIFY_ARE_EQUAL(3u, placements[0].imageId);
            VERIFY_ARE_EQUAL(1u, placements[0].placementId);
            VERIFY_ARE_EQUAL(1, placements[0].cols);
            VERIFY_ARE_EQUAL(1, placements[0].rows);

            auto& row = _buffer->GetMutableRowByOffset(0);
            _buffer->GetKittyImages().ComposeRow(*_buffer, row, 0, { 10, 20 });
            const auto slice = row.GetImageSlice();
            VERIFY_IS_NOT_NULL(slice);
            VerifyPixel(RGBQUAD{ 0, 0, 0, 255 }, slice->Pixels()[0]);
            VerifyPixel(RGBQUAD{ 0, 0, 0, 255 }, slice->Pixels()[199]);
        }

        TEST_METHOD(PlacementScalesToCells)
        {
            // 20x40 px image scaled into 2x2 cells (10x20 px each).
            // RGB, so 2400 zero bytes, which is exactly 3200 base64 chars.
            const auto payload = std::wstring(3200, L'A');
            SendApc(fmt::format(FMT_COMPILE(L"a=T,f=24,s=20,v=40,i=4,c=2,r=2;{}"), payload));
            const auto& placements = _buffer->GetKittyImages().Placements();
            VERIFY_ARE_EQUAL(1u, placements.size());
            VERIFY_ARE_EQUAL(2, placements[0].cols);
            VERIFY_ARE_EQUAL(2, placements[0].rows);

            auto& row = _buffer->GetMutableRowByOffset(0);
            _buffer->GetKittyImages().ComposeRow(*_buffer, row, 0, { 10, 20 });
            VERIFY_IS_NOT_NULL(row.GetImageSlice());
        }

        TEST_METHOD(NegativeZIndexComposesBelowText)
        {
            // The same 10x20 px image in a 1x1 cell, placed with z=-1:
            // the pixels must land in the below-text layer, not the
            // default above-text one.
            const auto payload = std::wstring(800, L'A');
            SendApc(fmt::format(FMT_COMPILE(L"a=T,f=24,s=10,v=20,i=5,z=-1,p=1;{}"), payload));
            auto& row = _buffer->GetMutableRowByOffset(0);
            _buffer->GetKittyImages().ComposeRow(*_buffer, row, 0, { 10, 20 });
            VERIFY_IS_NULL(row.GetImageSlice(ImageSliceLayer::AboveText));
            const auto below = row.GetImageSlice(ImageSliceLayer::BelowText);
            VERIFY_IS_NOT_NULL(below);
            VerifyPixel(RGBQUAD{ 0, 0, 0, 255 }, below->Pixels()[0]);

            // Re-placing the same placement with z=0 moves the pixels to
            // the above-text layer. The cursor has advanced past the first
            // placement, so the re-placement sits on row 1, and row 0 must
            // be back to no image content on either layer.
            SendApc(L"a=p,i=5,p=1,z=0,C=1");
            auto& rowOne = _buffer->GetMutableRowByOffset(1);
            _buffer->GetKittyImages().ComposeRow(*_buffer, rowOne, 1, { 10, 20 });
            VERIFY_IS_NOT_NULL(rowOne.GetImageSlice(ImageSliceLayer::AboveText));
            VERIFY_IS_NULL(rowOne.GetImageSlice(ImageSliceLayer::BelowText));
            _buffer->GetKittyImages().ComposeRow(*_buffer, row, 0, { 10, 20 });
            VERIFY_IS_NULL(row.GetImageSlice(ImageSliceLayer::AboveText));
            VERIFY_IS_NULL(row.GetImageSlice(ImageSliceLayer::BelowText));
        }

        TEST_METHOD(PlacementAckIncludesPlacementId)
        {
            SendApc(L"a=t,f=24,s=1,v=1,i=7,q=1;AAAA");
            SendApc(L"a=p,i=7,p=3");
            VERIFY_ARE_EQUAL(std::wstring(L"\x1b_Gi=7,p=3;OK\x1b\\"), _api->response);
        }

        TEST_METHOD(PlacementOfUnknownImageFails)
        {
            SendApc(L"a=p,i=99");
            VERIFY_IS_TRUE(_api->response.find(L"ENOENT") != std::wstring::npos);
        }

        TEST_METHOD(DeleteAbortsChunkedUpload)
        {
            SendApc(L"a=t,f=24,s=2,v=1,i=7,m=1;/wAA");
            VERIFY_IS_NULL(_buffer->GetKittyImages().FindImage(7));
            // A delete in the middle of a chunked upload aborts it.
            SendApc(L"a=d,d=i,i=7");
            SendApc(L"m=0;AP8A");
            VERIFY_IS_NULL(_buffer->GetKittyImages().FindImage(7));
        }

        TEST_METHOD(TransmitCompressedRgbaStoresImage)
        {
            // A 64x1 RGBA gradient, zlib-compressed by .NET. Exercises the
            // dynamic-Huffman path of the inflater end to end.
            const auto payload = L"eJwFwRG0gwAAQNFgEAyCQTAIBsEgGASDYBAMgkEwCAbBIBgEg2AQDIJBMAgGwSAYBINgEAyCQTAIBsEg+BDsnPfvFQSBgThElGSG8gRJ0Ripc2TNZKyvUAyHiblFtXym9gHNiZi5Z3QvZe7fMIKCRfjEjGqW8Qcr6VilP+xMZJ2PcAqFTTnFrXS29QKvsdi1a/zOZd/vCISAg3gklGKO8oVIyTipd2Kt5Ky/SIyGi/kltXqu9oDMkbi5Y3JP5e7PKAKDR7ikjGye8YYq8Xile+os5J2faIqET3mlrXK+9YOuqfhr3/Rdy6//4x9LS53B";
            SendApc(fmt::format(FMT_COMPILE(L"a=t,f=32,o=z,s=64,v=1,i=7;{}"), payload));
            const auto image = _buffer->GetKittyImages().FindImage(7);
            VERIFY_IS_NOT_NULL(image);
            VERIFY_ARE_EQUAL(64, image->size.width);
            VERIFY_ARE_EQUAL(1, image->size.height);
            VerifyPixel(RGBQUAD{ 0, 0, 0, 255 }, image->pixels[0]);
            VerifyPixel(RGBQUAD{ 24, 16, 8, 255 }, image->pixels[2]);
        }

        TEST_METHOD(TransmitZlibStoredBlockStoresImage)
        {
            // A zlib stream with a stored (uncompressed) deflate block,
            // assembled by hand: header, final+stored flag, LEN/NLEN, one
            // red RGB pixel, Adler-32 of { 0xff, 0, 0 }.
            const auto payload = L"eJz7z8AAAAMAAQA=";
            SendApc(fmt::format(FMT_COMPILE(L"a=t,f=24,o=z,s=1,v=1,i=7;{}"), payload));
            const auto image = _buffer->GetKittyImages().FindImage(7);
            VERIFY_IS_NOT_NULL(image);
            VerifyPixel(RGBQUAD{ 0, 0, 255, 255 }, image->pixels[0]);
        }

        TEST_METHOD(CorruptZlibRejected)
        {
            SendApc(L"a=t,f=32,o=z,s=1,v=1,i=7;AAAA");
            VERIFY_IS_TRUE(_api->response.find(L"EINVAL") != std::wstring::npos);
            VERIFY_IS_NULL(_buffer->GetKittyImages().FindImage(7));
        }

        TEST_METHOD(QueryActionDetectsSupport)
        {
            SendApc(L"i=31,s=1,v=1,a=q,t=d,f=24;AAAA");
            VERIFY_ARE_EQUAL(L"\x1b_Gi=31;OK\x1b\\", _api->response);
        }

        TEST_METHOD(FeatureQueryReportsSupport)
        {
            SendApc(L"a=q,i=31,e=T,U=1,C=1");
            VERIFY_ARE_EQUAL(L"\x1b_Gi=31,e=T,U=1,C=1;OK\x1b\\", _api->response);
        }

        TEST_METHOD(FeatureQueryHidesUnsupportedAction)
        {
            SendApc(L"a=q,i=31,e=f");
            VERIFY_ARE_EQUAL(L"\x1b_Gi=31;OK\x1b\\", _api->response);
        }

        TEST_METHOD(QuietSuppressesResponses)
        {
            SendApc(L"a=t,f=24,s=1,v=1,i=7,q=1;AAAA");
            VERIFY_IS_TRUE(_api->response.empty());
            SendApc(L"a=t,f=24,s=1,v=1,i=7,q=2;AAAA");
            VERIFY_IS_TRUE(_api->response.empty());
        }

        TEST_METHOD(DeleteByIdRemovesPlacements)
        {
            SendApc(L"a=T,f=24,s=1,v=1,i=7;AAAA");
            VERIFY_ARE_EQUAL(1u, _buffer->GetKittyImages().Placements().size());
            SendApc(L"a=d,d=i,i=7");
            VERIFY_ARE_EQUAL(0u, _buffer->GetKittyImages().Placements().size());
            // The lowercase variant keeps the image data.
            VERIFY_IS_NOT_NULL(_buffer->GetKittyImages().FindImage(7));
        }

        TEST_METHOD(DeleteFreesUnreferencedImages)
        {
            SendApc(L"a=t,f=24,s=1,v=1,i=7;AAAA");
            VERIFY_IS_NOT_NULL(_buffer->GetKittyImages().FindImage(7));
            SendApc(L"a=d,d=I,i=7");
            VERIFY_IS_NULL(_buffer->GetKittyImages().FindImage(7));
        }

        TEST_METHOD(RetransmissionReplacesPlacements)
        {
            SendApc(L"a=T,f=24,s=1,v=1,i=7;AAAA");
            VERIFY_ARE_EQUAL(1u, _buffer->GetKittyImages().Placements().size());
            SendApc(L"a=t,f=24,s=1,v=1,i=7;AAAA");
            // Re-transmitting deletes the placements of the old image.
            VERIFY_ARE_EQUAL(0u, _buffer->GetKittyImages().Placements().size());
            VERIFY_IS_NOT_NULL(_buffer->GetKittyImages().FindImage(7));
        }

        TEST_METHOD(VirtualPlacementAndPlaceholders)
        {
            // Transmit quietly, then create a 2x2 virtual placement.
            // 10x10 px RGB = 300 zero bytes = exactly 400 base64 chars.
            const auto payload = std::wstring(400, L'A');
            SendApc(fmt::format(FMT_COMPILE(L"a=t,f=24,s=10,v=10,i=42,q=2;{}"), payload));
            VERIFY_IS_TRUE(_api->response.empty());
            SendApc(L"a=p,U=1,i=42,c=2,r=2");
            VERIFY_IS_FALSE(_buffer->GetKittyImages().Placements().empty());

            // Print placeholder text for image 42 (fg = 38;5;42):
            // row 0: (0,0) and (0,1); row 1: (1,0) and (1,1).
            // U+0305 encodes 0 and U+030D encodes 1.
            const auto line0 = L"\x1b[38;5;42m\xDB3B\xDEEE\x0305\x0305\xDB3B\xDEEE\x0305\x030D\x1b[39m";
            const auto line1 = L"\x1b[38;5;42m\xDB3B\xDEEE\x030D\x0305\xDB3B\xDEEE\x030D\x030D\x1b[39m";
            // The same grid printed again two rows further down: its grid
            // rows no longer match their screen rows.
            const auto line2 = L"\x1b[38;5;42m\xDB3B\xDEEE\x0305\x0305\xDB3B\xDEEE\x0305\x030D\x1b[39m";
            Send(std::wstring(line0) + L"\r\n" + line1 + L"\r\n\r\n" + line2);

            // Both rows must compose an image slice for the placement, and
            // the misaligned rows must carry pixels too: a placeholder draws
            // into the very row that holds its text, sampling the strip of
            // the grid row it encodes. The 10x10 source letterboxed into the
            // 2x2 cells of 10x20 px leaves the top half of each strip
            // transparent, so sample below the middle.
            for (auto rowY : { 0, 1, 3 })
            {
                auto& row = _buffer->GetMutableRowByOffset(rowY);
                _buffer->GetKittyImages().ComposeRow(*_buffer, row, rowY, { 10, 20 });
                const auto slice = row.GetImageSlice();
                VERIFY_IS_NOT_NULL(slice);
                const auto width = slice->PixelWidth();
                VERIFY_ARE_EQUAL(20, width);
                const auto& pixels = slice->Pixels();
                // The letterboxed image covers the vertical middle of the
                // 2-row grid, so the lower half of grid row 0 and the upper
                // half of grid row 1 are opaque. Every composed row must
                // carry the pixels of the grid row its placeholders encode.
                const auto expectOpaqueAtTop = rowY == 1;
                VERIFY_ARE_EQUAL(expectOpaqueAtTop ? 255 : 0, static_cast<int>(pixels[5 * width + 5].rgbReserved));
                VERIFY_ARE_EQUAL(expectOpaqueAtTop ? 0 : 255, static_cast<int>(pixels[15 * width + 5].rgbReserved));
            }
        }

        TEST_METHOD(PlaceholderDiacriticsBeyondSeven)
        {
            // A 10x180 px image in a 1x9 cell grid fills all nine strip
            // rows, so the placeholder for grid row 8 - encoded with the
            // ninth diacritic, U+0346 - must carry pixels.
            // RGB, so 5400 zero bytes, which is exactly 7200 base64 chars.
            const auto payload = std::wstring(7200, L'A');
            SendApc(fmt::format(FMT_COMPILE(L"a=t,f=24,s=10,v=180,i=9,q=2;{}"), payload));
            SendApc(L"a=p,U=1,i=9,c=1,r=9");
            Send(L"\x1b[38;5;9m\xDB3B\xDEEE\x0346\x0305\x1b[39m\r\n");
            auto& row = _buffer->GetMutableRowByOffset(0);
            _buffer->GetKittyImages().ComposeRow(*_buffer, row, 0, { 10, 20 });
            const auto slice = row.GetImageSlice();
            VERIFY_IS_NOT_NULL(slice);
            VERIFY_ARE_EQUAL(255, static_cast<int>(slice->Pixels()[10].rgbReserved));
        }

        TEST_METHOD(PlaceholdersStrippedFromPlainText)        {
            const auto text = std::wstring(L"before \xDB3B\xDEEE\x0305\x030D after");
            const auto stripped = KittyPlaceholder::StripPlaceholders(text);
            VERIFY_ARE_EQUAL(std::wstring(L"before  after"), stripped);
        }

        TEST_METHOD(GarbageSweepDoesNotCrash)
        {
            // A deterministic pseudo-random sweep of APC payloads. Nothing
            // here is expected to do anything except leave the parser in a
            // working state.
            uint32_t state = 0x12345678;
            for (auto i = 0; i < 200; ++i)
            {
                state = state * 1664525 + 1013904223;
                std::wstring garbage = L"a=";
                for (auto j = 0; j < 16; ++j)
                {
                    state = state * 1664525 + 1013904223;
                    garbage += static_cast<wchar_t>(L'!' + (state >> 24) % 90);
                }
                SendApc(garbage);
                VERIFY_IS_TRUE(_api->response.empty() || _api->response.size() < 200);
            }
            // The parser must still work afterwards.
            SendApc(L"a=t,f=24,s=1,v=1,i=7;AAAA");
            VERIFY_IS_NOT_NULL(_buffer->GetKittyImages().FindImage(7));
        }

        TEST_METHOD(OversizedImageRejected)
        {
            // A single image claiming more than the total memory budget
            // is rejected outright.
            SendApc(L"a=t,f=32,s=10000,v=10000,i=5;AAAA");
            VERIFY_IS_TRUE(_api->response.find(L"EINVAL") != std::wstring::npos);
            VERIFY_IS_NULL(_buffer->GetKittyImages().FindImage(5));
        }

        TEST_METHOD(ForeignApcSequencesIgnored)
        {
            // APC content that is not a Kitty graphics command must be
            // dropped without a response or a crash, including partial
            // and malformed ones.
            Send(L"\x1b_other terminal's business\x1b\\");
            VERIFY_IS_TRUE(_api->response.empty());
            Send(L"\x1b_G");
            VERIFY_IS_TRUE(_api->response.empty());
            SendApc(L"a=9,zz=%%;@@@@");
            VERIFY_IS_TRUE(_api->response.empty());
            // A value that does not fit aborts the sequence silently.
            SendApc(L"a=t,f=24,s=9999999999,v=1;AAAA");
            VERIFY_IS_TRUE(_api->response.empty());
        }

        TEST_METHOD(CursorPolicyDefaultMovesCursor)
        {
            _buffer->GetCursor().SetPosition({ 0, 0 });
            // 10x20 px -> 1x1 cells. RGB, 600 bytes = 800 base64 chars.
            const auto payload = std::wstring(800, L'A');
            SendApc(fmt::format(FMT_COMPILE(L"a=T,f=24,s=10,v=20,i=3;{}"), payload));
            const auto position = _buffer->GetCursor().GetPosition();
            VERIFY_ARE_EQUAL(1, position.x);
            VERIFY_ARE_EQUAL(1, position.y);
        }

        TEST_METHOD(CursorPolicyOneKeepsCursor)
        {
            _buffer->GetCursor().SetPosition({ 2, 3 });
            const auto payload = std::wstring(800, L'A');
            SendApc(fmt::format(FMT_COMPILE(L"a=T,f=24,s=10,v=20,i=3,C=1;{}"), payload));
            const auto position = _buffer->GetCursor().GetPosition();
            VERIFY_ARE_EQUAL(2, position.x);
            VERIFY_ARE_EQUAL(3, position.y);
        }
    };
}
