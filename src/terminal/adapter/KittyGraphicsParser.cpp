// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "KittyGraphicsParser.hpp"
#include "Zlib.hpp"

#include "adaptDispatch.hpp"
#include "PageManager.hpp"
#include "../parser/stateMachine.hpp"
#include "../parser/ascii.hpp"
#include "../../buffer/out/ImageSlice.hpp"
#include "../../types/inc/Viewport.hpp"

using Microsoft::Console::Types::Viewport;
#include "../../buffer/out/KittyImages.hpp"
#include "../../buffer/out/KittyPlaceholder.hpp"

#include <wincodec.h>

using namespace Microsoft::Console::VirtualTerminal;

namespace
{
    // A defensive bound on the base64 payload of a single transmission,
    // accumulated across all of its chunks.
    constexpr size_t MaxBase64Length = 64ull * 1024 * 1024;

    // Client-supplied numbers are uint32 keys; every one that narrows to a
    // signed coordinate must pass this first, or a value above INT32_MAX
    // wraps negative and slips past the "> limit" checks that follow. A
    // zero passes: the callers decide whether zero is meaningful.
    bool CheckedCoord(const std::optional<uint32_t>& value, const uint32_t limit, til::CoordType& out)
    {
        out = 0;
        if (!value)
        {
            return true;
        }
        if (*value > limit)
        {
            return false;
        }
        out = static_cast<til::CoordType>(*value);
        return true;
    }

    std::optional<std::vector<uint8_t>> Base64Decode(const std::string_view text)
    {
        static constexpr auto invalid = static_cast<uint8_t>(0xFF);
        static const auto table = [] {
            std::array<uint8_t, 256> t{};
            t.fill(invalid);
            constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            for (size_t i = 0; i < alphabet.size(); ++i)
            {
                t[static_cast<uint8_t>(alphabet[i])] = static_cast<uint8_t>(i);
            }
            return t;
        }();

        std::vector<uint8_t> out;
        out.reserve(text.size() / 4 * 3 + 3);

        uint32_t accumulator = 0;
        int bits = 0;
        auto padding = false;
        for (const auto c : text)
        {
            if (c == '=')
            {
                padding = true;
                continue;
            }
            if (padding)
            {
                return std::nullopt;
            }
            const auto value = table[static_cast<uint8_t>(c)];
            if (value == invalid)
            {
                continue; // skip whitespace and other non-alphabet bytes
            }
            accumulator = (accumulator << 6) | value;
            bits += 6;
            if (bits >= 8)
            {
                bits -= 8;
                out.push_back(static_cast<uint8_t>((accumulator >> bits) & 0xFF));
            }
        }
        return out;
    }

    // Decodes a PNG payload into BGRA pixels. WIC is initialized and the
    // imaging factory cached per thread, since the parser runs on the
    // connection's reader thread.
    bool DecodePng(const std::vector<uint8_t>& bytes, til::size& outSize, std::vector<RGBQUAD>& outPixels)
    {
        try
        {
            struct ThreadWic
            {
                decltype(wil::CoInitializeEx(COINIT_APARTMENTTHREADED)) co;
                wil::com_ptr<IWICImagingFactory> factory;

                ThreadWic() :
                    co{ wil::CoInitializeEx(COINIT_APARTMENTTHREADED) }
                {
                    THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.put())));
                }
            };
            static thread_local ThreadWic wic;

            wil::com_ptr<IStream> stream;
            THROW_IF_FAILED(CreateStreamOnHGlobal(nullptr, TRUE, stream.put()));
            if (!bytes.empty())
            {
                THROW_IF_FAILED(stream->Write(bytes.data(), gsl::narrow_cast<ULONG>(bytes.size()), nullptr));
            }
            LARGE_INTEGER zero{};
            THROW_IF_FAILED(stream->Seek(zero, STREAM_SEEK_SET, nullptr));

            wil::com_ptr<IWICBitmapDecoder> decoder;
            THROW_IF_FAILED(wic.factory->CreateDecoderFromStream(stream.get(), nullptr, WICDecodeMetadataCacheOnDemand, decoder.put()));
            wil::com_ptr<IWICBitmapFrameDecode> frame;
            THROW_IF_FAILED(decoder->GetFrame(0, frame.put()));

            UINT width = 0, height = 0;
            THROW_IF_FAILED(frame->GetSize(&width, &height));
            if (width == 0 || height == 0 || width > static_cast<UINT>(KittyImages::MaxDimension) || height > static_cast<UINT>(KittyImages::MaxDimension))
            {
                return false;
            }
            // The decoded pixel allocation itself must fit the image memory
            // budget before it is made.
            if (static_cast<uint64_t>(width) * height * sizeof(RGBQUAD) > KittyImages::MaxTotalImageBytes)
            {
                return false;
            }

            wil::com_ptr<IWICFormatConverter> converter;
            THROW_IF_FAILED(wic.factory->CreateFormatConverter(converter.put()));
            THROW_IF_FAILED(converter->Initialize(frame.get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom));

            std::vector<RGBQUAD> pixels(static_cast<size_t>(width) * height);
            const auto stride = gsl::narrow_cast<UINT>(width * sizeof(RGBQUAD));
            THROW_IF_FAILED(converter->CopyPixels(nullptr, stride, gsl::narrow_cast<UINT>(pixels.size() * sizeof(RGBQUAD)), reinterpret_cast<BYTE*>(pixels.data())));

            outSize = { gsl::narrow_cast<til::CoordType>(width), gsl::narrow_cast<til::CoordType>(height) };
            outPixels = std::move(pixels);
            return true;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return false;
        }
    }
}

KittyGraphicsParser::KittyGraphicsParser(AdaptDispatch& dispatcher, const StateMachine& stateMachine) noexcept :
    _dispatcher{ dispatcher },
    _stateMachine{ stateMachine }
{
}

KittyGraphicsParser::StringHandler KittyGraphicsParser::BeginSequence()
{
    _state = ParseState::ExpectG;
    _currentKey = 0;
    _currentKeyIsNegative = false;
    _currentValue.clear();
    _cmd = Command{};
    _assignedId.reset();
    return [this](const wchar_t ch) { return _handleChar(ch); };
}

bool KittyGraphicsParser::_handleChar(const wchar_t ch)
{
    if (ch == AsciiChars::ESC)
    {
        if (_state != ParseState::Rejected)
        {
            _finalizeCommand();
        }
        return true;
    }

    switch (_state)
    {
    case ParseState::ExpectG:
        _state = ch == L'G' ? ParseState::Key : ParseState::Rejected;
        return _state != ParseState::Rejected;

    case ParseState::Key:
        switch (ch)
        {
        case L'a':
        case L'd':
        case L't':
        case L'o':
        case L'e':
        case L'f':
        case L'm':
        case L'i':
        case L'I':
        case L'p':
        case L'q':
        case L'w':
        case L'h':
        case L'x':
        case L'y':
        case L'v':
        case L's':
        case L'S':
        case L'O':
        case L'c':
        case L'r':
        case L'X':
        case L'Y':
        case L'C':
        case L'U':
        case L'N':
        case L'z':
        case L'H':
        case L'V':
        case L'b':
            _currentKey = ch;
            _state = ParseState::Equal;
            return true;
        default:
            // Unknown keys are ignored, value and all: the protocol is
            // forward-compatible, and rejecting the whole command would
            // hang a client waiting for its reply.
            _currentKey = 0;
            _state = ParseState::Equal;
            return true;
        }

    case ParseState::Equal:
        if (ch != L'=')
        {
            _state = ParseState::Rejected;
            return false;
        }
        _currentKeyIsNegative = false;
        _currentValue.clear();
        _state = (_currentKey == L'a' || _currentKey == L'd' || _currentKey == L't' || _currentKey == L'o' || _currentKey == L'e') ? ParseState::FlagValue : ParseState::Value;
        return true;

    case ParseState::FlagValue:
        switch (_currentKey)
        {
        case L'a':
            _cmd.action = ch;
            break;
        case L'd':
            _cmd.deleteAction = ch;
            break;
        case L't':
            _cmd.transmissionType = ch;
            break;
        case L'o':
            _cmd.compressed = ch;
            break;
        case L'e':
            _cmd.reportSupported = ch;
            break;
        default:
            break;
        }
        _state = ParseState::Value; // a ',' or ';' must follow
        return true;

    case ParseState::Value:
        if (ch == L',')
        {
            if (!_foldKeyValue())
            {
                return false;
            }
            _state = ParseState::Key;
            return true;
        }
        if (ch == L';')
        {
            if (!_foldKeyValue())
            {
                return false;
            }
            _state = ParseState::Payload;
            return true;
        }
        if (ch == L'-' && _currentValue.empty() && (_currentKey == L'z' || _currentKey == L'H' || _currentKey == L'V'))
        {
            _currentKeyIsNegative = true;
            return true;
        }
        if (ch >= L'0' && ch <= L'9')
        {
            if (_currentValue.size() >= 10)
            {
                _state = ParseState::Rejected;
                return false;
            }
            _currentValue.push_back(ch);
            return true;
        }
        _state = ParseState::Rejected;
        return false;

    case ParseState::Payload:
        if (_cmd.payloadBase64.size() >= MaxBase64Length || ch > 0x7F)
        {
            _state = ParseState::Rejected;
            return false;
        }
        _cmd.payloadBase64.push_back(static_cast<char>(ch));
        return true;

    case ParseState::Rejected:
    default:
        return false;
    }
}

void KittyGraphicsParser::_finalizeCommand()
{
    // A trailing value that was not followed by ',' or ';' yet.
    if (!_currentValue.empty())
    {
        _foldKeyValue();
    }
    if (_state != ParseState::Rejected)
    {
        _applyCommand();
    }
    // A large payload would otherwise stay alive until the next command
    // replaces the command struct.
    _cmd.payloadBase64.clear();
    _cmd.payloadBase64.shrink_to_fit();
}

bool KittyGraphicsParser::_foldKeyValue()
{
    if (_currentValue.empty())
    {
        return true;
    }
    const auto value = std::stoull(_currentValue);
    _currentValue.clear();
    if (value > UINT32_MAX)
    {
        _state = ParseState::Rejected;
        return false;
    }
    const auto u32 = static_cast<uint32_t>(value);
    // Negate and clamp in 64 bits: -INT32_MIN from a u32 is signed overflow.
    const auto i64 = _currentKeyIsNegative ? -static_cast<int64_t>(u32) : static_cast<int64_t>(u32);
    const auto i32 = static_cast<int32_t>(std::clamp<int64_t>(i64, INT32_MIN, INT32_MAX));
    switch (_currentKey)
    {
    case L'f': _cmd.format = u32; break;
    case L'm': _cmd.more = u32; break;
    case L'i': _cmd.id = u32; break;
    case L'I': _cmd.imageNumber = u32; break;
    case L'p': _cmd.placementId = u32; break;
    case L'q': _cmd.quiet = u32; break;
    case L'w': _cmd.width = u32; break;
    case L'h': _cmd.height = u32; break;
    case L'x': _cmd.xOffset = u32; break;
    case L'y': _cmd.yOffset = u32; break;
    case L'v': _cmd.dataHeight = u32; break;
    case L's': _cmd.dataWidth = u32; break;
    case L'c': _cmd.numCells = u32; break;
    case L'r': _cmd.numLines = u32; break;
    case L'X': _cmd.cellXOffset = u32; break;
    case L'Y': _cmd.cellYOffset = u32; break;
    case L'z': _cmd.zIndex = i32; break;
    case L'b': _cmd.frameNumber = u32; break;
    case L'C': _cmd.cursorMovement = u32; break;
    case L'U': _cmd.unicodePlacement = u32; break;
    case L'N': _cmd.usageHints = u32; break;
    default: break; // S, O, H, V belong to unsupported features
    }
    return true;
}

KittyImages& KittyGraphicsParser::_images() const
{
    auto page = _dispatcher._pages.ActivePage();
    return page.Buffer().GetKittyImages();
}

void KittyGraphicsParser::_applyCommand()
{
    // Any other command while a chunked transmission is in flight abandons
    // it; the client violated the chunking contract. Continuation chunks
    // (no action key) and frame-data chunks are of course exempt.
    if (_pendingValid && _cmd.action != 0 && _cmd.action != L't' && _cmd.action != L'T' && _cmd.action != L'f')
    {
        _pendingValid = false;
    }
    if (_framePendingValid && _cmd.action != 0 && _cmd.action != L'f')
    {
        _framePendingValid = false;
    }

    switch (_cmd.action)
    {
    case 0:
        // No action key: meaningful only as a continuation chunk of a
        // transmission already in flight.
        if (_pendingValid)
        {
            _transmit();
        }
        break;
    case L't':
        _transmit();
        break;
    case L'T':
        _transmit();
        break;
    case L'p':
        _place();
        break;
    case L'q':
        if (_cmd.reportSupported)
        {
            _queryFeatures();
        }
        else
        {
            _query();
        }
        break;
    case L'd':
        _delete();
        break;
    case L'f':
        _frame();
        break;
    case L'a':
        _animate();
        break;
    case L'c':
        _compose();
        break;
    default:
        break;
    }
}

bool KittyGraphicsParser::_quietSuppress(const bool ok) const noexcept
{
    const auto quiet = _cmd.quiet.value_or(0);
    return quiet >= 2 || (quiet >= 1 && ok);
}

uint32_t KittyGraphicsParser::_assignImageId() const
{
    const auto& images = _images();
    uint32_t id = 1;
    while (images.FindImage(id))
    {
        ++id;
    }
    return id;
}

std::wstring KittyGraphicsParser::_responsePrefix() const
{
    std::wstring prefix;
    if (_cmd.id)
    {
        prefix = fmt::format(FMT_COMPILE(L"i={}"), *_cmd.id);
    }
    else if (_assignedId)
    {
        prefix = fmt::format(FMT_COMPILE(L"i={}"), *_assignedId);
    }
    if (_cmd.imageNumber)
    {
        if (!prefix.empty())
        {
            prefix += L",";
        }
        prefix += fmt::format(FMT_COMPILE(L"I={}"), *_cmd.imageNumber);
    }
    if (_cmd.placementId && (_cmd.id || _assignedId))
    {
        prefix += fmt::format(FMT_COMPILE(L",p={}"), *_cmd.placementId);
    }
    return prefix;
}

void KittyGraphicsParser::_respond(const bool ok, const wchar_t* message) const
{
    if (_quietSuppress(ok))
    {
        return;
    }
    auto out = fmt::format(FMT_COMPILE(L"G{};{}"), _responsePrefix(), message);
    _dispatcher._ReturnApcResponse(out);
}

// Validates everything about a transmission that can be validated before
// the payload is decoded. Returns an error message or nullptr.
const wchar_t* KittyGraphicsParser::_validateTransmit()
{
    if (_cmd.id && _cmd.imageNumber)
    {
        return L"EINVAL:the i and I keys are mutually exclusive";
    }
    if (_cmd.transmissionType != L'd')
    {
        return L"ENOSYS:only direct transmission is supported";
    }
    if (_cmd.compressed && _cmd.compressed != L'z')
    {
        return L"ENOSYS:only zlib compression is supported";
    }
    const auto format = _cmd.format.value_or(32);
    if (format == 24 || format == 32 || format == 100)
    {
        return nullptr;
    }
    if (format == 8 || format == 16)
    {
        return L"ENOSYS:indexed color formats are not supported";
    }
    return L"EINVAL:unknown format";
}

void KittyGraphicsParser::_transmit()
{
    // A continuation chunk carries only the m (and optionally q) keys. A
    // command carrying full parameters starts a new transmission; if one
    // was still waiting for chunks, the client abandoned it mid-stream and
    // replacing it drops the partial payload.
    const auto isContinuation = _pendingValid && _pending.more.value_or(0) &&
                                !_cmd.format && !_cmd.dataWidth && !_cmd.dataHeight && !_cmd.id && !_cmd.imageNumber;

    if (!isContinuation)
    {
        if (const auto error = _validateTransmit())
        {
            _pendingValid = false;
            _respond(false, error);
            return;
        }
        _pending = _cmd;
        _pending.payloadBase64.clear();
        _pendingBase64Bytes = 0;
        _pendingValid = true;
    }

    if (_pendingBase64Bytes + _cmd.payloadBase64.size() > MaxBase64Length)
    {
        _pendingValid = false;
        _respond(false, L"EINVAL:payload too large");
        return;
    }
    _pendingBase64Bytes += _cmd.payloadBase64.size();
    _pending.payloadBase64 += _cmd.payloadBase64;
    _pending.more = _cmd.more;
    // A quiet level travels with the chunk that carries it; a continuation
    // without q= leaves the previous one in place.
    if (_cmd.quiet)
    {
        _pending.quiet = _cmd.quiet;
    }

    if (_pending.more.value_or(0))
    {
        return;
    }
    _finishTransmission();
}

void KittyGraphicsParser::_finishTransmission()
{
    auto cmd = std::move(_pending);
    _pendingValid = false;

    // The responses must echo the first chunk's id keys and quiet level,
    // not those of the (possibly id-less) final chunk.
    _cmd.id = cmd.id;
    _cmd.imageNumber = cmd.imageNumber;
    _cmd.placementId = cmd.placementId;
    _cmd.quiet = cmd.quiet;

    const auto format = cmd.format.value_or(32);
    til::size size{};
    std::vector<RGBQUAD> pixels;

    const auto decoded = Base64Decode(cmd.payloadBase64);
    if (!decoded)
    {
        _respond(false, L"EINVAL:corrupt base64 payload");
        return;
    }
    const uint8_t* bytes = decoded->data();
    size_t byteCount = decoded->size();
    std::vector<uint8_t> inflated;
    if (format == 100 && cmd.compressed == L'z')
    {
        // A PNG payload has no declared size; a decompressed one may not
        // exceed the image memory budget.
        inflated = Zlib::Inflate(std::span{ bytes, byteCount }, KittyImages::MaxTotalImageBytes).value_or(std::vector<uint8_t>{});
        if (inflated.empty())
        {
            _respond(false, L"EINVAL:failed to decompress payload");
            return;
        }
        bytes = inflated.data();
        byteCount = inflated.size();
    }

    if (format == 24 || format == 32)
    {
        til::CoordType width = 0;
        til::CoordType height = 0;
        if (!CheckedCoord(cmd.dataWidth, KittyImages::MaxDimension, width) ||
            !CheckedCoord(cmd.dataHeight, KittyImages::MaxDimension, height))
        {
            _respond(false, L"EINVAL:invalid dimensions");
            return;
        }
        if (width == 0 || height == 0)
        {
            _respond(false, L"EINVAL:invalid dimensions");
            return;
        }
        // A single image must not claim more than the entire memory budget.
        if (static_cast<size_t>(width) * height * sizeof(RGBQUAD) > KittyImages::MaxTotalImageBytes)
        {
            _respond(false, L"EINVAL:image too large");
            return;
        }
        const auto bytesPerPixel = format == 32 ? 4 : 3;
        const auto expected = static_cast<size_t>(width) * height * bytesPerPixel;
        if (cmd.compressed == L'z')
        {
            // A raw payload can only ever inflate to its declared size;
            // capping the decompression there stops zlib bombs short of
            // the budget.
            inflated = Zlib::Inflate(std::span{ bytes, byteCount }, expected).value_or(std::vector<uint8_t>{});
            if (inflated.empty())
            {
                _respond(false, L"EINVAL:failed to decompress payload");
                return;
            }
            bytes = inflated.data();
            byteCount = inflated.size();
        }
        if (byteCount != expected)
        {
            _respond(false, L"EINVAL:payload size does not match dimensions");
            return;
        }
        size = { static_cast<til::CoordType>(width), static_cast<til::CoordType>(height) };
        pixels.resize(static_cast<size_t>(width) * height);
        const auto* src = bytes;
        for (size_t i = 0; i < pixels.size(); ++i)
        {
            const auto r = src[i * bytesPerPixel + 0];
            const auto g = src[i * bytesPerPixel + 1];
            const auto b = src[i * bytesPerPixel + 2];
            const auto a = bytesPerPixel == 4 ? src[i * bytesPerPixel + 3] : uint8_t{ 255 };
            pixels[i] = RGBQUAD{ b, g, r, a };
        }
    }
    else // PNG
    {
        if (byteCount == 0 || !DecodePng(std::vector<uint8_t>(bytes, bytes + byteCount), size, pixels))
        {
            _respond(false, L"EINVAL:failed to decode PNG payload");
            return;
        }
    }

    auto id = cmd.id.value_or(0);
    if (id == 0)
    {
        id = _assignImageId();
        _assignedId = id;
    }
    _images().StoreImage(id, cmd.imageNumber.value_or(0), size, std::move(pixels), format, static_cast<char>(cmd.compressed));

    if (cmd.unicodePlacement.value_or(0))
    {
        // _placeVirtual has already answered the client, error included.
        if (!_placeVirtual(cmd))
        {
            return;
        }
    }
    else if (cmd.action == L'T')
    {
        // Same for _placeDirect.
        if (!_placeDirect(cmd, id))
        {
            return;
        }
    }

    _respond(true);
}

void KittyGraphicsParser::_place()
{
    if (_cmd.id && _cmd.imageNumber)
    {
        _respond(false, L"EINVAL:the i and I keys are mutually exclusive");
        return;
    }

    const auto& images = _images();
    const KittyImages::Image* image = nullptr;
    if (_cmd.id)
    {
        image = images.FindImage(*_cmd.id);
    }
    else if (_cmd.imageNumber)
    {
        image = images.FindNewestImageByNumber(*_cmd.imageNumber);
    }
    else
    {
        _respond(false, L"EINVAL:an image id or number is required");
        return;
    }
    if (!image)
    {
        _respond(false, L"ENOENT:no image with the specified id");
        return;
    }

    if (_cmd.unicodePlacement.value_or(0))
    {
        // _placeVirtual answers the client itself, error included.
        _placeVirtual(_cmd);
        return;
    }

    if (_placeDirect(_cmd, image->id))
    {
        _respond(true);
    }
}

bool KittyGraphicsParser::_placeVirtual(const Command& cmd)
{
    til::CoordType cols = 0;
    til::CoordType rows = 0;
    if (!CheckedCoord(cmd.numCells, KittyImages::MaxDimension, cols) ||
        !CheckedCoord(cmd.numLines, KittyImages::MaxDimension, rows))
    {
        _respond(false, L"EINVAL:invalid virtual placement dimensions");
        return false;
    }
    if (cols == 0 || rows == 0)
    {
        _respond(false, L"EINVAL:invalid virtual placement dimensions");
        return false;
    }

    const auto imageId = cmd.id.value_or(_assignedId.value_or(0));
    const auto image = _images().FindImage(imageId);
    if (!image)
    {
        _respond(false, L"ENOENT:no image with the specified id");
        return false;
    }

    auto placement = KittyImages::Placement{};
    placement.imageId = imageId;
    placement.placementId = imageId ? cmd.placementId.value_or(0) : 0;
    placement.srcRect = { 0, 0, image->size.width, image->size.height };
    placement.cols = cols;
    placement.rows = rows;
    placement.virtual_ = true;
    _images().AddPlacement(std::move(placement));
    return true;
}

bool KittyGraphicsParser::_placeDirect(const Command& cmd, const uint32_t imageId)
{
    const auto image = _images().FindImage(imageId);
    if (!image)
    {
        _respond(false, L"ENOENT:no image with the specified id");
        return false;
    }

    const auto cellSize = _dispatcher._api.GetFontCellSize();
    if (cellSize.width <= 0 || cellSize.height <= 0)
    {
        _respond(false, L"EINVAL:no font metrics available yet");
        return false;
    }

    // Source rectangle, clamped into the image. The offsets and sizes all
    // narrow from client u32 keys, so they are range-checked first: a
    // value above INT32_MAX would wrap negative and read outside the image.
    til::CoordType srcX = 0;
    til::CoordType srcY = 0;
    til::CoordType srcW = 0;
    til::CoordType srcH = 0;
    const auto maxDim = static_cast<uint32_t>(std::max<til::CoordType>(image->size.width, image->size.height));
    if (!CheckedCoord(cmd.xOffset, maxDim, srcX) || !CheckedCoord(cmd.yOffset, maxDim, srcY) ||
        !CheckedCoord(cmd.width, maxDim, srcW) || !CheckedCoord(cmd.height, maxDim, srcH))
    {
        _respond(false, L"EINVAL:invalid source rectangle");
        return false;
    }
    const auto left = std::min<til::CoordType>(srcX, image->size.width);
    const auto top = std::min<til::CoordType>(srcY, image->size.height);
    const auto right = cmd.width ? std::min<til::CoordType>(static_cast<int64_t>(left) + srcW, image->size.width) : image->size.width;
    const auto bottom = cmd.height ? std::min<til::CoordType>(static_cast<int64_t>(top) + srcH, image->size.height) : image->size.height;
    auto srcRect = til::rect{ left, top, right, bottom };
    if (srcRect.left >= srcRect.right || srcRect.top >= srcRect.bottom)
    {
        _respond(false, L"EINVAL:empty source rectangle");
        return false;
    }

    // Destination rectangle, in cells. With only one of c/r given, the
    // other follows the source aspect ratio.
    const auto sourceW = static_cast<double>(srcRect.right - srcRect.left);
    const auto sourceH = static_cast<double>(srcRect.bottom - srcRect.top);
    til::CoordType cols = 0;
    til::CoordType rows = 0;
    // Both counts narrow from client u32 keys; validate before any use so
    // a value above INT32_MAX cannot wrap negative past the later checks.
    if (!CheckedCoord(cmd.numCells, KittyImages::MaxDimension, cols) ||
        !CheckedCoord(cmd.numLines, KittyImages::MaxDimension, rows))
    {
        _respond(false, L"EINVAL:invalid placement dimensions");
        return false;
    }
    if (cols == 0 && rows == 0)
    {
        cols = static_cast<til::CoordType>((sourceW + cellSize.width - 1) / cellSize.width);
        rows = static_cast<til::CoordType>((sourceH + cellSize.height - 1) / cellSize.height);
    }
    else if (rows == 0)
    {
        rows = std::max<til::CoordType>(1, lround(cols * sourceH / sourceW));
    }
    else if (cols == 0)
    {
        cols = std::max<til::CoordType>(1, lround(rows * sourceW / sourceH));
    }
    if (cols == 0 || rows == 0 || cols > KittyImages::MaxDimension || rows > KittyImages::MaxDimension)
    {
        _respond(false, L"EINVAL:invalid placement dimensions");
        return false;
    }

    til::CoordType cellX = 0;
    til::CoordType cellY = 0;
    if (!CheckedCoord(cmd.cellXOffset, cellSize.width ? cellSize.width - 1 : 0, cellX) ||
        !CheckedCoord(cmd.cellYOffset, cellSize.height ? cellSize.height - 1 : 0, cellY))
    {
        _respond(false, L"EINVAL:cell offsets must be smaller than the cell size");
        return false;
    }
    if (cellX >= cellSize.width || cellY >= cellSize.height)
    {
        _respond(false, L"EINVAL:cell offsets must be smaller than the cell size");
        return false;
    }

    auto page = _dispatcher._pages.ActivePage();
    const auto origin = page.Cursor().GetPosition();

    auto placement = KittyImages::Placement{};
    placement.imageId = imageId;
    placement.placementId = imageId ? cmd.placementId.value_or(0) : 0;
    placement.srcRect = srcRect;
    placement.x = origin.x;
    placement.y = origin.y;
    placement.cols = cols;
    placement.rows = rows;
    placement.offsetX = cellX;
    placement.offsetY = cellY;
    placement.z = cmd.zIndex.value_or(0);
    _images().AddPlacement(std::move(placement));

    // The covered rows are often blank text; without this they stay clean
    // in the renderer's damage tracking and never repaint, so the composed
    // pixels would not reach the screen.
    {
        const auto page2 = _dispatcher._pages.ActivePage();
        const auto rect = til::inclusive_rect{ origin.x, origin.y, origin.x + cols - 1, origin.y + rows - 1 };
        page2.Buffer().TriggerRedraw(Viewport::FromInclusive(rect));
    }

    // Cursor movement policy. The default moves the cursor right and down
    // by the placement size, C=1 leaves it alone, C=2 puts it at the start
    // of the line below the image.
    const auto policy = cmd.cursorMovement.value_or(0);
    if (policy != 1)
    {
        auto position = origin;
        position.y = std::min<til::CoordType>(origin.y + rows, page.BufferHeight() - 1);
        position.x = policy == 2 ? 0 : std::min<til::CoordType>(origin.x + cols, page.Width() - 1);
        page.Cursor().SetPosition(position);
    }

    page.Buffer().NotifyPaintFrame();
    return true;
}

void KittyGraphicsParser::_query()
{
    // The detection query transmits a dummy image: validate as usual, but
    // neither store nor display anything.
    if (const auto error = _validateTransmit())
    {
        _respond(false, error);
        return;
    }
    const auto format = _cmd.format.value_or(32);
    if (format != 100)
    {
        const auto width = static_cast<til::CoordType>(_cmd.dataWidth.value_or(0));
        const auto height = static_cast<til::CoordType>(_cmd.dataHeight.value_or(0));
        const auto bytesPerPixel = format == 32 ? 4 : 3;
        const auto decoded = Base64Decode(_cmd.payloadBase64);
        if (width == 0 || height == 0 || !decoded || decoded->size() != static_cast<size_t>(width) * height * bytesPerPixel)
        {
            _respond(false, L"EINVAL:payload size does not match dimensions");
            return;
        }
    }
    _respond(true);
}

void KittyGraphicsParser::_queryFeatures()
{
    // Feature queries must carry an image id, like any query action.
    if (!_cmd.id)
    {
        _respond(false, L"EINVAL:feature queries require an image id");
        return;
    }

    const auto action = _cmd.reportSupported;
    switch (action)
    {
    case L't':
    case L'T':
    case L'p':
    case L'd':
    case L'q':
        break;
    default:
        // Unsupported action: answer the query without the e key, which
        // tells the client the action is not supported.
        _respond(true);
        return;
    }

    std::wstring reply = fmt::format(FMT_COMPILE(L"e={}"), static_cast<wchar_t>(action));

    const auto echo = [&reply](const wchar_t key, const std::optional<uint32_t>& value) {
        if (value)
        {
            reply += fmt::format(FMT_COMPILE(L",{}={}"), key, *value);
        }
    };

    if (action == L't' || action == L'T')
    {
        // Only non-default formats are echoed; f=32 is the default.
        if (_cmd.format == 24u || _cmd.format == 100u)
        {
            echo(L'f', _cmd.format);
        }
        echo(L's', _cmd.dataWidth);
        echo(L'v', _cmd.dataHeight);
    }
    if (action == L't' || action == L'T' || action == L'p')
    {
        echo(L'c', _cmd.numCells);
        echo(L'r', _cmd.numLines);
        echo(L'x', _cmd.xOffset);
        echo(L'y', _cmd.yOffset);
        echo(L'w', _cmd.width);
        echo(L'h', _cmd.height);
        echo(L'X', _cmd.cellXOffset);
        echo(L'Y', _cmd.cellYOffset);
        if (_cmd.unicodePlacement == 1u)
        {
            echo(L'U', _cmd.unicodePlacement);
        }
        if (_cmd.cursorMovement == 1u || _cmd.cursorMovement == 2u)
        {
            echo(L'C', _cmd.cursorMovement);
        }
    }

    if (_quietSuppress(true))
    {
        return;
    }
    auto prefix = _responsePrefix();
    if (!prefix.empty())
    {
        prefix += L",";
    }
    _dispatcher._ReturnApcResponse(fmt::format(FMT_COMPILE(L"G{}{};OK"), prefix, reply));
}

void KittyGraphicsParser::_delete()
{
    auto& images = _images();
    auto page = _dispatcher._pages.ActivePage();
    const auto viewport = page.Viewport();
    const auto what = _cmd.deleteAction ? _cmd.deleteAction : L'a';
    const auto freeData = what >= L'A' && what <= L'Z';

    // Deletes are addressed through the client's i= or I= key.
    const KittyImages::Image* target = nullptr;
    if (_cmd.id)
    {
        target = images.FindImage(*_cmd.id);
    }
    else if (_cmd.imageNumber)
    {
        target = images.FindNewestImageByNumber(*_cmd.imageNumber);
    }

    switch (what)
    {
    case L'a':
    case L'A':
        // All placements visible on screen.
        images.DeletePlacements([&](const KittyImages::Placement& p) {
            return !p.virtual_ && p.y < viewport.bottom && p.y + p.rows > viewport.top;
        });
        break;
    case L'i':
    case L'I':
    case L'n':
    case L'N':
        if (target)
        {
            if ((what == L'i' || what == L'I') && _cmd.placementId)
            {
                images.DeletePlacements([&](const KittyImages::Placement& p) {
                    return p.imageId == target->id && p.placementId == *_cmd.placementId;
                });
            }
            else
            {
                // Id-based deletion also removes the image's virtual
                // placements, per the spec.
                images.DeletePlacements([&](const KittyImages::Placement& p) {
                    return p.imageId == target->id;
                });
            }
        }
        break;
    case L'c':
    case L'C':
    {
        const auto cursor = page.Cursor().GetPosition();
        images.DeletePlacements([&](const KittyImages::Placement& p) {
            return !p.virtual_ && cursor.x >= p.x && cursor.x < p.x + p.cols && cursor.y >= p.y && cursor.y < p.y + p.rows;
        });
        break;
    }
    case L'p':
    case L'P':
    {
        const auto bx = viewport.left + static_cast<til::CoordType>(_cmd.xOffset.value_or(1)) - 1;
        const auto by = viewport.top + static_cast<til::CoordType>(_cmd.yOffset.value_or(1)) - 1;
        images.DeletePlacements([&](const KittyImages::Placement& p) {
            return !p.virtual_ && bx >= p.x && bx < p.x + p.cols && by >= p.y && by < p.y + p.rows;
        });
        break;
    }
    case L'x':
    case L'X':
    {
        const auto bx = viewport.left + static_cast<til::CoordType>(_cmd.xOffset.value_or(1)) - 1;
        images.DeletePlacements([&](const KittyImages::Placement& p) {
            return !p.virtual_ && bx >= p.x && bx < p.x + p.cols;
        });
        break;
    }
    case L'y':
    case L'Y':
    {
        const auto by = viewport.top + static_cast<til::CoordType>(_cmd.yOffset.value_or(1)) - 1;
        images.DeletePlacements([&](const KittyImages::Placement& p) {
            return !p.virtual_ && by >= p.y && by < p.y + p.rows;
        });
        break;
    }
    case L'z':
    case L'Z':
        images.DeletePlacements([&](const KittyImages::Placement& p) {
            return !p.virtual_ && p.z == _cmd.zIndex.value_or(0);
        });
        break;
    case L'r':
    case L'R':
        images.DeletePlacements([&](const KittyImages::Placement& p) {
            return p.imageId >= _cmd.xOffset.value_or(0) && p.imageId <= _cmd.yOffset.value_or(0);
        });
        break;
    case L'f':
    case L'F':
        // Delete the animation frames; the root image data remains.
        if (target)
        {
            images.DeleteFrames(target->id);
        }
        else
        {
            for (const auto& entry : images.Images())
            {
                images.DeleteFrames(entry.id);
            }
        }
        break;
    case L'q':
    case L'Q':
    {
        // The intersection of the p and z selectors, both 1-based and
        // viewport-relative like d=p.
        til::CoordType bx = 0;
        til::CoordType by = 0;
        if (!CheckedCoord(_cmd.xOffset, KittyImages::MaxDimension, bx) || !CheckedCoord(_cmd.yOffset, KittyImages::MaxDimension, by))
        {
            _respond(false, L"EINVAL:invalid delete position");
            return;
        }
        if (!_cmd.xOffset || !_cmd.yOffset)
        {
            _respond(false, L"EINVAL:the x and y keys are required");
            return;
        }
        const auto cellX = viewport.left + bx - 1;
        const auto cellY = viewport.top + by - 1;
        const auto z = _cmd.zIndex.value_or(0);
        images.DeletePlacements([&](const KittyImages::Placement& p) {
            return !p.virtual_ && cellX >= p.x && cellX < p.x + p.cols && cellY >= p.y && cellY < p.y + p.rows && p.z == z;
        });
        break;
    }
    default:
        _respond(false, L"EINVAL:unknown delete action");
        return;
    }

    // Redraw everything: deletions can affect rows anywhere on screen.
    _dispatcher._pages.VisiblePage().Buffer().TriggerRedrawAll();

    // The capital variants also free the data of images that lost their
    // last placement.
    if (freeData)
    {
        images.DeleteUnreferencedImages();
    }

    _respond(true);
}

namespace
{
    // Locates the image a command addresses through i= or I=.
    const KittyImages::Image* FindTargetImage(const KittyImages& images, const std::optional<uint32_t>& id, const std::optional<uint32_t>& imageNumber)
    {
        if (id)
        {
            return images.FindImage(*id);
        }
        if (imageNumber)
        {
            return images.FindNewestImageByNumber(*imageNumber);
        }
        return nullptr;
    }
}

void KittyGraphicsParser::_frame()
{
    // A frame-data chunk carrying the full parameter set starts a new
    // frame; a chunk with only m= and payload continues the current one.
    const auto isContinuation = _framePendingValid && _framePending.more.value_or(0) &&
                                !_cmd.id && !_cmd.imageNumber && !_cmd.frameNumber &&
                                !_cmd.xOffset && !_cmd.yOffset && !_cmd.dataWidth && !_cmd.dataHeight && !_cmd.numCells && !_cmd.numLines && !_cmd.zIndex;
    if (!isContinuation)
    {
        _framePending = _cmd;
        _framePending.payloadBase64.clear();
        _frameBase64Bytes = 0;
        _framePendingValid = true;
    }

    if (_frameBase64Bytes + _cmd.payloadBase64.size() > MaxBase64Length)
    {
        _framePendingValid = false;
        _respond(false, L"EINVAL:payload too large");
        return;
    }
    _frameBase64Bytes += _cmd.payloadBase64.size();
    _framePending.payloadBase64 += _cmd.payloadBase64;
    _framePending.more = _cmd.more;
    // A quiet level travels with the chunk that carries it.
    if (_cmd.quiet)
    {
        _framePending.quiet = _cmd.quiet;
    }

    if (_framePending.more.value_or(0))
    {
        return;
    }
    _framePendingValid = false;

    const auto cmd = std::move(_framePending);
    _cmd.id = cmd.id;
    _cmd.imageNumber = cmd.imageNumber;
    _cmd.quiet = cmd.quiet;

    const auto image = FindTargetImage(_images(), cmd.id, cmd.imageNumber);
    if (!image)
    {
        _respond(false, L"ENOENT:no image with the specified id");
        return;
    }
    const auto format = cmd.format ? *cmd.format : image->format;
    if (format == 100)
    {
        // PNG images have no raw frame data; animation frames arrive as
        // raw pixels over f=24/f=32 transmissions.
        _respond(false, L"EINVAL:frames require a raw pixel format");
        return;
    }

    // Per the spec the updated rectangle is x, y, s, v; everything narrows
    // from client u32 keys, so validate in 64 bits before touching pixels.
    const auto maxDim = static_cast<uint32_t>(std::max<til::CoordType>(image->size.width, image->size.height));
    til::CoordType x = 0;
    til::CoordType y = 0;
    til::CoordType width = 0;
    til::CoordType height = 0;
    if (!CheckedCoord(cmd.xOffset, maxDim, x) || !CheckedCoord(cmd.yOffset, maxDim, y) ||
        !CheckedCoord(cmd.dataWidth, maxDim, width) || !CheckedCoord(cmd.dataHeight, maxDim, height))
    {
        _respond(false, L"EINVAL:invalid frame rectangle");
        return;
    }
    const auto full = width == 0 && height == 0;
    const auto rectWidth = full ? image->size.width : width;
    const auto rectHeight = full ? image->size.height : height;
    if (rectWidth <= 0 || rectHeight <= 0 ||
        static_cast<uint64_t>(x) + static_cast<uint64_t>(rectWidth) > static_cast<uint64_t>(image->size.width) ||
        static_cast<uint64_t>(y) + static_cast<uint64_t>(rectHeight) > static_cast<uint64_t>(image->size.height))
    {
        _respond(false, L"EINVAL:invalid frame rectangle");
        return;
    }

    const auto decoded = Base64Decode(cmd.payloadBase64);
    if (!decoded)
    {
        _respond(false, L"EINVAL:corrupt base64 payload");
        return;
    }
    const uint8_t* bytes = decoded->data();
    size_t byteCount = decoded->size();
    std::vector<uint8_t> inflated;
    if (cmd.compressed == L'z')
    {
        // A frame payload can only ever inflate to its declared size.
        const auto expected = static_cast<size_t>(rectWidth) * rectHeight * (format == 24 ? 3 : 4);
        inflated = Zlib::Inflate(std::span{ bytes, byteCount }, expected).value_or(std::vector<uint8_t>{});
        if (inflated.empty())
        {
            _respond(false, L"EINVAL:failed to decompress payload");
            return;
        }
        bytes = inflated.data();
        byteCount = inflated.size();
    }

    const auto bytesPerPixel = format == 24 ? 3 : 4;
    if (byteCount != static_cast<size_t>(rectWidth) * rectHeight * bytesPerPixel)
    {
        _respond(false, L"EINVAL:payload size does not match the rectangle");
        return;
    }

    // The new frame starts as the frame the c key copies, or transparent
    // (or the Y background color) when it names none. An r key edits the
    // frame it names instead of appending a new one.
    std::vector<RGBQUAD> frame;
    uint32_t target = 0;
    if (cmd.numLines)
    {
        target = *cmd.numLines;
        if (target > KittyImages::MaxFrames || !_images().EnsureFrames(image->id, target))
        {
            _respond(false, L"EINVAL:invalid frame number");
            return;
        }
        const auto* stored = _images().FindImage(image->id);
        if (stored->frames.size() >= target)
        {
            frame = stored->frames[target - 1].pixels;
        }
    }
    if (frame.empty())
    {
        frame.resize(image->pixels.size());
        if (cmd.cellYOffset)
        {
            // Y is a 32-bit RGBA color for the pixels the frame does not
            // carry; the default is transparent black.
            const auto rgba = *cmd.cellYOffset;
            const auto background = RGBQUAD{
                static_cast<BYTE>((rgba >> 8) & 0xFF),
                static_cast<BYTE>((rgba >> 16) & 0xFF),
                static_cast<BYTE>((rgba >> 24) & 0xFF),
                static_cast<BYTE>(rgba & 0xFF),
            };
            std::fill(frame.begin(), frame.end(), background);
        }
        if (cmd.numCells)
        {
            // c names the frame whose pixels serve as the base canvas.
            const auto base = *cmd.numCells;
            if (base > KittyImages::MaxFrames || !_images().EnsureFrames(image->id, base))
            {
                _respond(false, L"EINVAL:invalid frame number");
                return;
            }
            const auto* stored = _images().FindImage(image->id);
            if (stored->frames.size() >= base)
            {
                frame = stored->frames[base - 1].pixels;
            }
        }
    }

    // The composition mode defaults to full alpha blending; X=1 replaces.
    const auto replace = cmd.cellXOffset.value_or(0) == 1;
    for (til::CoordType row = 0; row < rectHeight; ++row)
    {
        const auto* src = bytes + static_cast<size_t>(row) * rectWidth * bytesPerPixel;
        auto* dst = frame.data() + static_cast<size_t>(y + row) * image->size.width + x;
        for (til::CoordType col = 0; col < rectWidth; ++col)
        {
            const auto r = src[col * bytesPerPixel + 0];
            const auto g = src[col * bytesPerPixel + 1];
            const auto b = src[col * bytesPerPixel + 2];
            const auto a = bytesPerPixel == 4 ? src[col * bytesPerPixel + 3] : uint8_t{ 255 };
            const auto srcPixel = RGBQUAD{ b, g, r, a };
            if (replace || a == 255)
            {
                dst[col] = srcPixel;
            }
            else if (a != 0)
            {
                // Straight-alpha "over": source atop destination.
                const auto& dstPixel = dst[col];
                const uint32_t sa = a;
                const uint32_t da = dstPixel.rgbReserved;
                const uint32_t inv = 255 - sa;
                const uint32_t outA = sa + da * inv / 255;
                if (outA == 0)
                {
                    dst[col] = srcPixel;
                    continue;
                }
                const auto mix = [&](const BYTE sc, const BYTE dc) {
                    return static_cast<BYTE>((static_cast<uint32_t>(sc) * sa + static_cast<uint32_t>(dc) * (da * inv / 255)) / outA);
                };
                dst[col] = RGBQUAD{
                    mix(srcPixel.rgbBlue, dstPixel.rgbBlue),
                    mix(srcPixel.rgbGreen, dstPixel.rgbGreen),
                    mix(srcPixel.rgbRed, dstPixel.rgbRed),
                    static_cast<BYTE>(outA),
                };
            }
        }
    }

    // b= stores the frame under an explicit number (0 appends after the
    // last); an r= edit without b= replaces r.
    auto explicitTarget = cmd.frameNumber.value_or(0);
    if (explicitTarget == 0)
    {
        explicitTarget = target;
    }
    if (!_images().StoreFrame(image->id, explicitTarget, std::move(frame), cmd.zIndex.value_or(0)))
    {
        _respond(false, L"EINVAL:invalid frame number");
        return;
    }
    _respond(true);
}

void KittyGraphicsParser::_compose()
{
    const auto image = FindTargetImage(_images(), _cmd.id, _cmd.imageNumber);
    if (!image)
    {
        _respond(false, L"ENOENT:no image with the specified id");
        return;
    }
    const auto source = _cmd.numLines.value_or(1);
    const auto destination = _cmd.numCells.value_or(1);
    if (source == 0 || destination == 0 || source > KittyImages::MaxFrames || destination > KittyImages::MaxFrames)
    {
        _respond(false, L"EINVAL:invalid frame number");
        return;
    }

    // The rectangle, source offset (X, Y) and destination offset (x, y) all
    // narrow from client u32 keys; validate in 64 bits or a value above
    // INT32_MAX wraps negative into the composition loops.
    const auto maxDim = static_cast<uint32_t>(std::max<til::CoordType>(image->size.width, image->size.height));
    til::CoordType sx = 0;
    til::CoordType sy = 0;
    til::CoordType w = 0;
    til::CoordType h = 0;
    til::CoordType dx = 0;
    til::CoordType dy = 0;
    if (!CheckedCoord(_cmd.cellXOffset, maxDim, sx) || !CheckedCoord(_cmd.cellYOffset, maxDim, sy) ||
        !CheckedCoord(_cmd.width, maxDim, w) || !CheckedCoord(_cmd.height, maxDim, h) ||
        !CheckedCoord(_cmd.xOffset, maxDim, dx) || !CheckedCoord(_cmd.yOffset, maxDim, dy))
    {
        _respond(false, L"EINVAL:invalid composition rectangle");
        return;
    }
    const auto rectWidth = w ? w : image->size.width;
    const auto rectHeight = h ? h : image->size.height;
    if (static_cast<uint64_t>(sx) + static_cast<uint64_t>(rectWidth) > static_cast<uint64_t>(image->size.width) ||
        static_cast<uint64_t>(sy) + static_cast<uint64_t>(rectHeight) > static_cast<uint64_t>(image->size.height) ||
        static_cast<uint64_t>(dx) + static_cast<uint64_t>(rectWidth) > static_cast<uint64_t>(image->size.width) ||
        static_cast<uint64_t>(dy) + static_cast<uint64_t>(rectHeight) > static_cast<uint64_t>(image->size.height))
    {
        _respond(false, L"EINVAL:invalid composition rectangle");
        return;
    }
    const auto srcRect = til::rect{ sx, sy, sx + rectWidth, sy + rectHeight };
    const auto destinationOffset = til::point{ dx, dy };
    const auto replace = _cmd.cursorMovement.value_or(0) == 1;

    if (source == destination &&
        srcRect.left < destinationOffset.x + rectWidth && destinationOffset.x < srcRect.right &&
        srcRect.top < destinationOffset.y + rectHeight && destinationOffset.y < srcRect.bottom)
    {
        // Composing a frame onto itself needs disjoint rectangles.
        _respond(false, L"EINVAL:overlapping source and destination");
        return;
    }

    if (!_images().ComposeFrames(image->id, source, destination, srcRect, destinationOffset, replace))
    {
        _respond(false, L"EINVAL:invalid composition rectangle");
        return;
    }
    _redrawImagePlacements(image->id);
    _respond(true);
}

void KittyGraphicsParser::_animate()
{
    const auto image = FindTargetImage(_images(), _cmd.id, _cmd.imageNumber);
    if (!image)
    {
        _respond(false, L"ENOENT:no image with the specified id");
        return;
    }
    const auto imageId = image->id;
    auto ok = true;
    auto visible = false;
    // c= makes a frame the displayed one.
    if (_cmd.numCells)
    {
        ok = _images().SelectFrame(imageId, *_cmd.numCells);
        visible = ok;
    }
    // r= with z= sets the gap of that frame.
    if (_cmd.numLines && _cmd.zIndex)
    {
        ok = _images().SetFrameGap(imageId, *_cmd.numLines, *_cmd.zIndex) && ok;
    }
    // s= and v= record the requested animation state. Terminal-driven
    // playback is not implemented; the keys are kept for the client.
    if (_cmd.dataWidth || _cmd.dataHeight)
    {
        ok = _images().SetAnimationState(imageId, _cmd.dataWidth.value_or(0), _cmd.dataHeight.value_or(0)) && ok;
    }
    if (!ok)
    {
        _respond(false, L"EINVAL:invalid animation command");
        return;
    }
    if (visible)
    {
        _redrawImagePlacements(imageId);
    }
    _respond(true);
}

void KittyGraphicsParser::_redrawImagePlacements(const uint32_t imageId)
{
    auto page = _dispatcher._pages.ActivePage();
    for (const auto& placement : _images().Placements())
    {
        if (placement.imageId != imageId)
        {
            continue;
        }
        const auto rect = til::inclusive_rect{ placement.x, placement.y, placement.x + placement.cols - 1, placement.y + placement.rows - 1 };
        page.Buffer().TriggerRedraw(Viewport::FromInclusive(rect));
    }
    page.Buffer().NotifyPaintFrame();
}
