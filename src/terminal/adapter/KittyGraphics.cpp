// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "KittyGraphics.hpp"
#include "SixelParser.hpp"
#include "adaptDispatch.hpp"
#include "../parser/ascii.hpp"

#include <unordered_set>
#include <wincodec.h>
#include <zlib.h>

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::VirtualTerminal;

namespace
{
    constexpr size_t MaxDataSize = GraphicsStore::Quota;
    constexpr int64_t MaxImageDimension = 10000;
    constexpr int64_t MaxCells = 1000;
    constexpr std::wstring_view TempFileMarker = L"tty-graphics-protocol";

    int base64Value(const wchar_t ch) noexcept
    {
        if (ch >= L'A' && ch <= L'Z')
        {
            return ch - L'A';
        }
        if (ch >= L'a' && ch <= L'z')
        {
            return ch - L'a' + 26;
        }
        if (ch >= L'0' && ch <= L'9')
        {
            return ch - L'0' + 52;
        }
        if (ch == L'+')
        {
            return 62;
        }
        if (ch == L'/')
        {
            return 63;
        }
        return -1;
    }

    std::wstring finalPathOf(const HANDLE handle)
    {
        std::wstring path(MAX_PATH, L'\0');
        for (;;)
        {
            const auto len = GetFinalPathNameByHandleW(handle, path.data(), gsl::narrow_cast<DWORD>(path.size()), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
            if (len == 0)
            {
                return {};
            }
            if (len < path.size())
            {
                path.resize(len);
                return path;
            }
            path.resize(len);
        }
    }

    // Paths come from potentially untrusted sources, such as programs running on
    // a remote machine. Only local drive paths are accepted, because merely
    // opening a network path can leak credentials to the remote host.
    bool isLocalDrivePath(const std::wstring_view path) noexcept
    {
        if (path.size() < 3 || !iswalpha(path[0]) || path[1] != L':' || (path[2] != L'\\' && path[2] != L'/'))
        {
            return false;
        }
        if (path.find(L'\0') != std::wstring_view::npos)
        {
            return false;
        }
        const wchar_t root[]{ path[0], L':', L'\\', L'\0' };
        switch (GetDriveTypeW(&root[0]))
        {
        case DRIVE_FIXED:
        case DRIVE_REMOVABLE:
        case DRIVE_CDROM:
        case DRIVE_RAMDISK:
            return true;
        default:
            return false;
        }
    }

    std::wstring lowerCase(std::wstring text)
    {
        if (!text.empty())
        {
            LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, text.data(), gsl::narrow_cast<int>(text.size()), text.data(), gsl::narrow_cast<int>(text.size()), nullptr, nullptr, 0);
        }
        return text;
    }

    std::wstring stripFinalPathPrefix(std::wstring path)
    {
        if (path.starts_with(L"\\\\?\\"))
        {
            path.erase(0, 4);
        }
        return path;
    }

    bool readFile(const std::wstring& requestedPath, const uint64_t offset, const uint64_t size, std::vector<uint8_t>& out, std::wstring& finalPath)
    {
        if (!isLocalDrivePath(requestedPath))
        {
            return false;
        }
        std::wstring path(MAX_PATH, L'\0');
        auto len = GetFullPathNameW(requestedPath.c_str(), gsl::narrow_cast<DWORD>(path.size()), path.data(), nullptr);
        if (len >= path.size())
        {
            path.resize(len);
            len = GetFullPathNameW(requestedPath.c_str(), gsl::narrow_cast<DWORD>(path.size()), path.data(), nullptr);
        }
        if (len == 0 || len >= path.size())
        {
            return false;
        }
        path.resize(len);
        if (!isLocalDrivePath(path))
        {
            return false;
        }

        wil::unique_hfile file{ CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr) };
        if (!file || GetFileType(file.get()) != FILE_TYPE_DISK)
        {
            return false;
        }
        BY_HANDLE_FILE_INFORMATION info{};
        if (!GetFileInformationByHandle(file.get(), &info) || WI_IsFlagSet(info.dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY))
        {
            return false;
        }
        finalPath = stripFinalPathPrefix(finalPathOf(file.get()));
        if (!isLocalDrivePath(finalPath))
        {
            return false;
        }

        const auto fileSize = (static_cast<uint64_t>(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
        if (offset > fileSize)
        {
            return false;
        }
        const auto available = fileSize - offset;
        const auto toRead = size ? size : available;
        if (toRead > available || toRead > MaxDataSize)
        {
            return false;
        }

        LARGE_INTEGER distance{};
        distance.QuadPart = gsl::narrow_cast<LONGLONG>(offset);
        if (!SetFilePointerEx(file.get(), distance, nullptr, FILE_BEGIN))
        {
            return false;
        }
        out.resize(gsl::narrow_cast<size_t>(toRead));
        size_t total = 0;
        while (total < out.size())
        {
            DWORD read = 0;
            const auto chunk = gsl::narrow_cast<DWORD>(std::min<size_t>(out.size() - total, 1 << 30));
            if (!ReadFile(file.get(), out.data() + total, chunk, &read, nullptr) || read == 0)
            {
                return false;
            }
            total += read;
        }
        return true;
    }

    bool readSharedMemory(const std::wstring& name, const uint64_t offset, const uint64_t size, std::vector<uint8_t>& out)
    {
        if (name.empty() || name.find(L'\0') != std::wstring::npos)
        {
            return false;
        }
        wil::unique_handle mapping{ OpenFileMappingW(FILE_MAP_READ, FALSE, name.c_str()) };
        if (!mapping)
        {
            return false;
        }
        wil::unique_mapview_ptr<uint8_t> view{ static_cast<uint8_t*>(MapViewOfFile(mapping.get(), FILE_MAP_READ, 0, 0, 0)) };
        if (!view)
        {
            return false;
        }
        // The region only covers pages with the same state, so this also
        // ensures that every page we read from is committed.
        MEMORY_BASIC_INFORMATION memoryInfo{};
        if (!VirtualQuery(view.get(), &memoryInfo, sizeof(memoryInfo)) || memoryInfo.State != MEM_COMMIT)
        {
            return false;
        }
        const uint64_t mappedSize = memoryInfo.RegionSize;
        if (offset > mappedSize)
        {
            return false;
        }
        const auto available = mappedSize - offset;
        const auto toRead = size ? size : available;
        if (toRead > available || toRead > MaxDataSize)
        {
            return false;
        }
        out.assign(view.get() + offset, view.get() + offset + toRead);
        return true;
    }

    bool isDeletableTempFile(const std::wstring& finalPath)
    {
        if (lowerCase(finalPath).find(TempFileMarker) == std::wstring::npos)
        {
            return false;
        }
        std::wstring tempPath(MAX_PATH + 1, L'\0');
        const auto len = GetTempPathW(gsl::narrow_cast<DWORD>(tempPath.size()), tempPath.data());
        if (len == 0 || len >= tempPath.size())
        {
            return false;
        }
        tempPath.resize(len);
        wil::unique_hfile directory{ CreateFileW(tempPath.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr) };
        if (!directory)
        {
            return false;
        }
        auto tempDirectory = stripFinalPathPrefix(finalPathOf(directory.get()));
        if (tempDirectory.empty())
        {
            return false;
        }
        if (!tempDirectory.ends_with(L'\\'))
        {
            tempDirectory.push_back(L'\\');
        }
        return lowerCase(finalPath).starts_with(lowerCase(tempDirectory));
    }

    bool inflateData(const std::span<const uint8_t> input, const size_t expectedSize, std::vector<uint8_t>& output)
    {
        z_stream stream{};
        if (inflateInit(&stream) != Z_OK)
        {
            return false;
        }
        auto cleanup = wil::scope_exit([&]() noexcept { inflateEnd(&stream); });

        output.resize(expectedSize ? expectedSize : std::max<size_t>(input.size() * 4, 4096));
        stream.next_in = const_cast<Bytef*>(input.data());
        stream.avail_in = gsl::narrow_cast<uInt>(input.size());
        size_t produced = 0;
        for (;;)
        {
            stream.next_out = output.data() + produced;
            stream.avail_out = gsl::narrow_cast<uInt>(std::min<size_t>(output.size() - produced, UINT_MAX));
            const auto before = stream.avail_out;
            const auto result = inflate(&stream, Z_NO_FLUSH);
            produced += before - stream.avail_out;
            if (result == Z_STREAM_END)
            {
                output.resize(produced);
                return true;
            }
            if (result != Z_OK && result != Z_BUF_ERROR)
            {
                return false;
            }
            if (stream.avail_out == 0)
            {
                if (output.size() >= MaxDataSize)
                {
                    return false;
                }
                output.resize(std::min(output.size() * 2, MaxDataSize));
            }
            else if (stream.avail_in == 0)
            {
                // The input was truncated.
                return false;
            }
        }
    }

    bool decodePng(const std::span<const uint8_t> data, til::size& size, std::vector<uint32_t>& pixels)
    {
        // WIC requires COM. If the thread is already initialized in a different
        // apartment, that's fine too, since the factory is free threaded.
        const auto hrInit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        auto uninitialize = wil::scope_exit([&]() noexcept {
            if (SUCCEEDED(hrInit))
            {
                CoUninitialize();
            }
        });

        const auto factory = wil::CoCreateInstanceNoThrow<IWICImagingFactory>(CLSID_WICImagingFactory);
        if (!factory)
        {
            return false;
        }
        wil::com_ptr<IWICStream> stream;
        if (FAILED(factory->CreateStream(stream.addressof())) ||
            FAILED(stream->InitializeFromMemory(const_cast<BYTE*>(data.data()), gsl::narrow_cast<DWORD>(data.size()))))
        {
            return false;
        }
        wil::com_ptr<IWICBitmapDecoder> decoder;
        if (FAILED(factory->CreateDecoder(GUID_ContainerFormatPng, nullptr, decoder.addressof())) ||
            FAILED(decoder->Initialize(stream.get(), WICDecodeMetadataCacheOnDemand)))
        {
            return false;
        }
        wil::com_ptr<IWICBitmapFrameDecode> frame;
        UINT width = 0;
        UINT height = 0;
        if (FAILED(decoder->GetFrame(0, frame.addressof())) || FAILED(frame->GetSize(&width, &height)))
        {
            return false;
        }
        if (width == 0 || height == 0 || width > MaxImageDimension || height > MaxImageDimension)
        {
            return false;
        }
        wil::com_ptr<IWICFormatConverter> converter;
        if (FAILED(factory->CreateFormatConverter(converter.addressof())) ||
            FAILED(converter->Initialize(frame.get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom)))
        {
            return false;
        }
        pixels.resize(static_cast<size_t>(width) * height);
        if (FAILED(converter->CopyPixels(nullptr, width * 4, gsl::narrow_cast<UINT>(pixels.size() * 4), reinterpret_cast<BYTE*>(pixels.data()))))
        {
            return false;
        }
        size = { gsl::narrow_cast<til::CoordType>(width), gsl::narrow_cast<til::CoordType>(height) };
        return true;
    }

    uint32_t premultiply(const uint32_t r, const uint32_t g, const uint32_t b, const uint32_t a) noexcept
    {
        const auto pr = (r * a + 127) / 255;
        const auto pg = (g * a + 127) / 255;
        const auto pb = (b * a + 127) / 255;
        return (a << 24) | (pr << 16) | (pg << 8) | pb;
    }

    til::CoordType ceilDiv(const double value, const til::CoordType divisor) noexcept
    {
        return gsl::narrow_cast<til::CoordType>(std::ceil(value / std::max(1, divisor)));
    }
}

bool KittyGraphics::Control::Has(const char key) const noexcept
{
    return present.test(static_cast<size_t>(key));
}

int64_t KittyGraphics::Control::Get(const char key, const int64_t defaultValue) const noexcept
{
    return Has(key) ? til::at(values, static_cast<size_t>(key)) : defaultValue;
}

uint32_t KittyGraphics::Control::GetUnsigned(const char key) const noexcept
{
    return gsl::narrow_cast<uint32_t>(std::clamp<int64_t>(Get(key), 0, UINT32_MAX));
}

int32_t KittyGraphics::Control::GetSigned(const char key) const noexcept
{
    return gsl::narrow_cast<int32_t>(std::clamp<int64_t>(Get(key), INT32_MIN, INT32_MAX));
}

wchar_t KittyGraphics::Control::GetChar(const char key, const wchar_t defaultValue) const noexcept
{
    return Has(key) && isChar.test(static_cast<size_t>(key)) ? gsl::narrow_cast<wchar_t>(Get(key)) : defaultValue;
}

KittyGraphics::KittyGraphics(AdaptDispatch& dispatcher) noexcept :
    _dispatcher{ dispatcher }
{
}

KittyGraphics::StringHandler KittyGraphics::Command()
{
    _state = ParseState::Key;
    _control = {};
    _key = 0;
    _value.clear();
    _base64Accumulator = 0;
    _base64Bits = 0;
    _payload.clear();
    _payloadTooLarge = false;
    return [this](const wchar_t ch) {
        try
        {
            return _parse(ch);
        }
        catch (...)
        {
            // An exception (for instance out of memory) aborts the command,
            // and the rest of the string is ignored.
            LOG_CAUGHT_EXCEPTION();
            _abortLoading();
            _payload = {};
            _state = ParseState::Invalid;
            return false;
        }
    };
}

void KittyGraphics::Reset() noexcept
{
    _abortLoading();
}

bool KittyGraphics::_parse(const wchar_t ch)
{
    if (ch == AsciiChars::ESC)
    {
        if (_state == ParseState::Value)
        {
            _commitValue();
        }
        if (_state != ParseState::Invalid)
        {
            _execute();
        }
        return false;
    }

    switch (_state)
    {
    case ParseState::Key:
    case ParseState::Value:
        if (!_parseControl(ch))
        {
            _state = ParseState::Invalid;
        }
        return true;
    case ParseState::Payload:
        _decodeBase64(ch);
        return true;
    default:
        return true;
    }
}

bool KittyGraphics::_parseControl(const wchar_t ch) noexcept
{
    if (_state == ParseState::Key)
    {
        if (ch == L';')
        {
            _state = ParseState::Payload;
            return true;
        }
        if (ch == L',')
        {
            return true;
        }
        if (_key == 0)
        {
            if (ch < 0x21 || ch >= 0x7f || ch == L'=')
            {
                return false;
            }
            _key = static_cast<char>(ch);
            return true;
        }
        if (ch == L'=')
        {
            _state = ParseState::Value;
            _value.clear();
            return true;
        }
        return false;
    }

    if (ch == L',' || ch == L';')
    {
        if (!_commitValue())
        {
            return false;
        }
        _state = ch == L';' ? ParseState::Payload : ParseState::Key;
        return true;
    }
    if (_value.size() >= 16)
    {
        return false;
    }
    _value.push_back(ch);
    return true;
}

bool KittyGraphics::_commitValue() noexcept
{
    const auto index = static_cast<size_t>(_key);
    _key = 0;
    _state = ParseState::Key;
    if (_value.empty())
    {
        return false;
    }

    const auto first = _value.front();
    if (first == L'-' || (first >= L'0' && first <= L'9'))
    {
        int64_t number = 0;
        const auto negative = first == L'-';
        for (size_t i = negative ? 1 : 0; i < _value.size(); i++)
        {
            const auto digit = _value[i];
            if (digit < L'0' || digit > L'9')
            {
                return false;
            }
            number = std::min<int64_t>(number * 10 + (digit - L'0'), UINT32_MAX);
        }
        til::at(_control.values, index) = negative ? -number : number;
        _control.isChar.reset(index);
    }
    else if (_value.size() == 1)
    {
        til::at(_control.values, index) = first;
        _control.isChar.set(index);
    }
    else
    {
        return false;
    }
    _control.present.set(index);
    return true;
}

void KittyGraphics::_decodeBase64(const wchar_t ch)
{
    const auto value = base64Value(ch);
    if (value < 0 || _payloadTooLarge)
    {
        return;
    }
    _base64Accumulator = (_base64Accumulator << 6) | gsl::narrow_cast<uint32_t>(value);
    _base64Bits += 6;
    if (_base64Bits >= 8)
    {
        _base64Bits -= 8;
        if (_payload.size() >= MaxDataSize)
        {
            _payloadTooLarge = true;
            return;
        }
        _payload.push_back(gsl::narrow_cast<uint8_t>(_base64Accumulator >> _base64Bits));
    }
}

void KittyGraphics::_abortLoading() noexcept
{
    _loading = false;
    _loadingControl = {};
    _loadingData = {};
}

void KittyGraphics::_execute()
{
    const auto& control = _control;
    auto data = std::move(_payload);
    _payload = {};
    const auto action = control.GetChar('a', 0);

    if (control.GetUnsigned('i') && control.GetUnsigned('I'))
    {
        const Error error{ L"EINVAL", L"Must not specify both image id and image number" };
        _respond(control, control.GetUnsigned('i'), &error);
        return;
    }

    const auto isTransmission = action == 0 || action == L't' || action == L'T' || action == L'q';
    const auto isContinuation = _loading && isTransmission && !control.GetUnsigned('i') && !control.GetUnsigned('I');

    if (_payloadTooLarge)
    {
        const Error error{ L"EFBIG", L"Image data is too large" };
        const auto& origin = isContinuation ? _loadingControl : control;
        _respond(origin, origin.GetUnsigned('i'), &error);
        _abortLoading();
        return;
    }

    if (isContinuation)
    {
        if (_loadingData.size() + data.size() > MaxDataSize)
        {
            const Error error{ L"EFBIG", L"Image data is too large" };
            _respond(_loadingControl, _loadingControl.GetUnsigned('i'), &error);
            _abortLoading();
            return;
        }
        _loadingData.insert(_loadingData.end(), data.begin(), data.end());
        if (control.Has('q'))
        {
            til::at(_loadingControl.values, 'q') = control.Get('q');
            _loadingControl.present.set('q');
        }
        if (control.Get('m') == 1)
        {
            return;
        }
        auto loadingControl = std::move(_loadingControl);
        auto loadingData = std::move(_loadingData);
        _abortLoading();
        _transmit(loadingControl, std::move(loadingData));
        return;
    }

    // Any other command aborts a chunked transmission that is in progress.
    _abortLoading();

    switch (action)
    {
    case 0:
    case L't':
    case L'T':
    case L'q':
        if (action == L'q' && control.Has('e'))
        {
            _featureQuery(control);
        }
        else if (control.Get('m') == 1)
        {
            _loading = true;
            _loadingControl = control;
            _loadingData = std::move(data);
        }
        else
        {
            _transmit(control, std::move(data));
        }
        break;
    case L'p':
        _put(control);
        break;
    case L'd':
        _delete(control);
        break;
    default:
    {
        const Error error{ L"EINVAL", L"Unsupported graphics command action" };
        _respond(control, control.GetUnsigned('i'), &error);
        break;
    }
    }
}

void KittyGraphics::_transmit(const Control& control, std::vector<uint8_t> data)
{
    const auto action = control.GetChar('a', L't');
    const auto isQuery = action == L'q';
    const auto requestedId = control.GetUnsigned('i');
    const auto number = control.GetUnsigned('I');

    if (isQuery && !requestedId)
    {
        return;
    }

    Error error{};
    auto image = _load(control, std::move(data), error);
    if (!image)
    {
        _respond(control, requestedId, &error);
        return;
    }
    if (isQuery)
    {
        _respond(control, requestedId, nullptr);
        return;
    }

    auto& store = _store();
    image->id = number ? store.NextFreeId() : requestedId;
    image->number = number;

    auto& buffer = _dispatcher._pages.ActivePage().Buffer();
    if (image->id)
    {
        // Re-transmitting an image deletes the existing image and all its placements.
        const auto id = image->id;
        _erase(buffer, [=](const GraphicsRowRef& ref) { return ref.placement->image->id == id; }, false);
        if (store.FindVirtualPlacement(id, 0))
        {
            buffer.TriggerRedrawAll();
        }
    }

    // Images without an id are stored as well, so that they count towards the
    // quota. Images evicted by the quota are removed from the screen too.
    const auto evicted = store.Add(image);
    if (!evicted.empty())
    {
        _erase(buffer, [&](const GraphicsRowRef& ref) { return std::ranges::any_of(evicted, [&](const auto& e) { return e == ref.placement->image; }); }, false);
        buffer.TriggerRedrawAll();
    }

    if (action == L'T')
    {
        if (control.GetUnsigned('U') == 1)
        {
            _placeVirtual(control, image);
        }
        else if (!_place(control, image, error))
        {
            _respond(control, image->id, &error);
            return;
        }
    }
    _respond(control, image->id, nullptr);
}

void KittyGraphics::_put(const Control& control)
{
    const auto image = _findImage(control);
    if (!image)
    {
        const Error error{ L"ENOENT", L"Put command refers to a nonexistent image" };
        _respond(control, control.GetUnsigned('i'), &error);
        return;
    }
    _store().Touch(image.get());
    if (control.GetUnsigned('U') == 1)
    {
        _placeVirtual(control, image);
    }
    else
    {
        Error error{};
        if (!_place(control, image, error))
        {
            _respond(control, image->id, &error);
            return;
        }
    }
    _respond(control, image->id, nullptr);
}

void KittyGraphics::_delete(const Control& control)
{
    const auto target = control.GetChar('d', L'a');
    const auto freeImages = target >= L'A' && target <= L'Z';
    auto page = _dispatcher._pages.ActivePage();
    auto& buffer = page.Buffer();
    auto& store = _store();

    // Placements identified by their image, which are deleted wherever they are.
    const auto deleteById = [&](const uint32_t id, const uint32_t placementId) {
        if (id == 0)
        {
            return;
        }
        _erase(buffer, [=](const GraphicsRowRef& ref) { return ref.placement->image->id == id && (placementId == 0 || ref.placement->placementId == placementId); }, freeImages);
        if (store.FindVirtualPlacement(id, placementId))
        {
            store.RemoveVirtualPlacements(id, placementId);
            buffer.TriggerRedrawAll();
        }
        if (freeImages && id && !buffer.AnyGraphics([=](const GraphicsRowRef& ref) { return ref.placement->image->id == id; }))
        {
            store.Remove(id);
        }
    };

    // Placements that intersect a cell of the page, which are deleted entirely.
    const auto deleteVisible = [&](const std::function<bool(const GraphicsRowRef&, til::point)>& predicate) {
        std::unordered_set<const GraphicsPlacement*> matches;
        for (auto y = page.Top(); y < page.Bottom(); y++)
        {
            for (const auto& ref : buffer.GetRowByOffset(y).GetGraphics())
            {
                const auto& p = *ref.placement;
                for (auto x = std::max(0, p.column); x < std::min(page.Width(), p.column + p.columns); x++)
                {
                    if (predicate(ref, { x, y - page.Top() }))
                    {
                        matches.insert(ref.placement.get());
                        break;
                    }
                }
            }
        }
        if (!matches.empty())
        {
            _erase(buffer, [&](const GraphicsRowRef& ref) { return matches.contains(ref.placement.get()); }, freeImages);
        }
    };

    const auto cellX = gsl::narrow_cast<til::CoordType>(control.Get('x')) - 1;
    const auto cellY = gsl::narrow_cast<til::CoordType>(control.Get('y')) - 1;
    const auto z = control.GetSigned('z');

    switch (target)
    {
    case L'a':
    case L'A':
        deleteVisible([](auto&, auto) { return true; });
        break;
    case L'i':
    case L'I':
        deleteById(control.GetUnsigned('i'), control.GetUnsigned('p'));
        break;
    case L'n':
    case L'N':
        if (const auto image = store.FindByNumber(control.GetUnsigned('I')))
        {
            deleteById(image->id, control.GetUnsigned('p'));
        }
        break;
    case L'c':
    case L'C':
    {
        const auto cursor = page.Cursor().GetPosition();
        deleteVisible([&](auto&, const til::point cell) { return cell == til::point{ cursor.x, cursor.y - page.Top() }; });
        break;
    }
    case L'p':
    case L'P':
        deleteVisible([&](auto&, const til::point cell) { return cell == til::point{ cellX, cellY }; });
        break;
    case L'q':
    case L'Q':
        deleteVisible([&](const GraphicsRowRef& ref, const til::point cell) { return cell == til::point{ cellX, cellY } && ref.placement->z == z; });
        break;
    case L'x':
    case L'X':
        deleteVisible([&](auto&, const til::point cell) { return cell.x == cellX; });
        break;
    case L'y':
    case L'Y':
        deleteVisible([&](auto&, const til::point cell) { return cell.y == cellY; });
        break;
    case L'z':
    case L'Z':
        _erase(buffer, [=](const GraphicsRowRef& ref) { return ref.placement->z == z; }, freeImages);
        break;
    case L'r':
    case L'R':
    {
        const auto first = control.GetUnsigned('x');
        const auto last = control.GetUnsigned('y');
        if (first <= last)
        {
            std::vector<uint32_t> ids;
            buffer.AnyGraphics([&](const GraphicsRowRef& ref) {
                const auto id = ref.placement->image->id;
                if (id >= first && id <= last)
                {
                    ids.push_back(id);
                }
                return false;
            });
            for (const auto id : store.Ids())
            {
                if (id >= first && id <= last)
                {
                    ids.push_back(id);
                }
            }
            std::ranges::sort(ids);
            ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
            for (const auto id : ids)
            {
                deleteById(id, 0);
            }
        }
        break;
    }
    default:
        break;
    }
}

void KittyGraphics::_featureQuery(const Control& control) const
{
    const auto action = control.GetChar('e', 0);
    std::wstring keys;
    const auto echo = [&](const char key) {
        keys += L',';
        keys += static_cast<wchar_t>(key);
        keys += L'=';
        if (control.isChar.test(static_cast<size_t>(key)))
        {
            keys += control.GetChar(key, 0);
        }
        else
        {
            keys += std::to_wstring(control.Get(key));
        }
    };
    const auto echoIf = [&](const char key, const bool supported) {
        if (control.Has(key) && control.Get(key) != 0 && supported)
        {
            echo(key);
        }
    };
    const auto echoTransmitKeys = [&]() {
        const auto format = control.Get('f', 32);
        echoIf('f', !control.isChar.test('f') && (format == 24 || format == 32 || format == 100));
        const auto medium = control.GetChar('t', L'd');
        echoIf('t', medium == L'f' || medium == L't' || medium == L's');
        echoIf('o', control.GetChar('o', 0) == L'z');
        for (const auto key : { 's', 'v', 'S', 'O', 'p', 'N' })
        {
            echoIf(key, !control.isChar.test(static_cast<size_t>(key)));
        }
    };
    const auto echoDisplayKeys = [&]() {
        for (const auto key : { 'x', 'y', 'w', 'h', 'X', 'Y', 'c', 'r', 'z', 'p' })
        {
            if (keys.find(std::wstring{ L',' } + static_cast<wchar_t>(key) + L'=') == std::wstring::npos)
            {
                echoIf(key, !control.isChar.test(static_cast<size_t>(key)));
            }
        }
        echoIf('C', control.Get('C') == 1);
        echoIf('U', control.Get('U') == 1);
    };

    switch (action)
    {
    case L't':
    case L'q':
        keys = L",e=";
        keys += action;
        echoTransmitKeys();
        break;
    case L'T':
        keys = L",e=T";
        echoTransmitKeys();
        echoDisplayKeys();
        break;
    case L'p':
        keys = L",e=p";
        echoDisplayKeys();
        break;
    case L'd':
    {
        keys = L",e=d";
        constexpr std::wstring_view supportedTargets = L"aAiInNcCpPqQrRxXyYzZ";
        const auto target = control.GetChar('d', 0);
        echoIf('d', target && supportedTargets.find(target) != std::wstring_view::npos);
        for (const auto key : { 'x', 'y', 'z', 'p' })
        {
            echoIf(key, !control.isChar.test(static_cast<size_t>(key)));
        }
        break;
    }
    default:
        break;
    }

    Control reply;
    til::at(reply.values, 'q') = control.Get('q');
    reply.present.set('q', control.Has('q'));
    _respond(reply, control.GetUnsigned('i'), nullptr, keys);
}

std::shared_ptr<GraphicsImage> KittyGraphics::_load(const Control& control, std::vector<uint8_t> data, Error& error) const
{
    std::vector<uint8_t> bytes;
    switch (control.GetChar('t', L'd'))
    {
    case L'd':
        bytes = std::move(data);
        break;
    case L'f':
    case L't':
    {
        const auto path = til::u8u16({ reinterpret_cast<const char*>(data.data()), data.size() });
        std::wstring finalPath;
        if (!readFile(path, control.Get('O'), control.Get('S'), bytes, finalPath))
        {
            error = { L"EBADF", L"Failed to read image file" };
            return nullptr;
        }
        // The terminal is responsible for deleting temporary files. When we're
        // running as ConPTY, the file is still needed by the attached terminal.
        if (control.GetChar('t', L'd') == L't' && !_dispatcher._api.IsConPTY() && isDeletableTempFile(finalPath))
        {
            DeleteFileW(finalPath.c_str());
        }
        break;
    }
    case L's':
    {
        const auto name = til::u8u16({ reinterpret_cast<const char*>(data.data()), data.size() });
        if (!readSharedMemory(name, control.Get('O'), control.Get('S'), bytes))
        {
            error = { L"EBADF", L"Failed to read shared memory" };
            return nullptr;
        }
        break;
    }
    default:
        error = { L"EINVAL", L"Unknown transmission medium" };
        return nullptr;
    }

    const auto format = control.Get('f', 32);
    const auto width = control.Get('s');
    const auto height = control.Get('v');
    const auto rawFormat = format == 24 || format == 32;
    const auto bytesPerPixel = format == 24 ? 3 : 4;
    const auto validSize = width > 0 && height > 0 && width <= MaxImageDimension && height <= MaxImageDimension;

    if (control.Has('o'))
    {
        if (control.GetChar('o', 0) != L'z')
        {
            error = { L"EINVAL", L"Unknown compression" };
            return nullptr;
        }
        const auto expectedSize = rawFormat && validSize ? gsl::narrow_cast<size_t>(width * height * bytesPerPixel) : 0;
        std::vector<uint8_t> inflated;
        if (!inflateData(bytes, expectedSize, inflated))
        {
            error = { L"EINVAL", L"Failed to inflate image data" };
            return nullptr;
        }
        bytes = std::move(inflated);
    }

    auto image = std::make_shared<GraphicsImage>();
    if (rawFormat)
    {
        if (!validSize)
        {
            error = { L"EINVAL", L"Invalid image dimensions" };
            return nullptr;
        }
        const auto pixelCount = gsl::narrow_cast<size_t>(width * height);
        if (bytes.size() < pixelCount * bytesPerPixel)
        {
            error = { L"ENODATA", L"Insufficient image data" };
            return nullptr;
        }
        image->size = { gsl::narrow_cast<til::CoordType>(width), gsl::narrow_cast<til::CoordType>(height) };
        image->pixels.resize(pixelCount);
        auto src = bytes.data();
        for (auto& pixel : image->pixels)
        {
            const uint32_t alpha = bytesPerPixel == 4 ? src[3] : 255;
            pixel = premultiply(src[0], src[1], src[2], alpha);
            src += bytesPerPixel;
        }
    }
    else if (format == 100)
    {
        if (!decodePng(bytes, image->size, image->pixels))
        {
            error = { L"EBADPNG", L"Failed to decode PNG data" };
            return nullptr;
        }
    }
    else
    {
        error = { L"EINVAL", L"Unsupported image format" };
        return nullptr;
    }

    // ConPTY only needs the dimensions of the image to compute the cell footprint,
    // because the attached terminal is responsible for displaying it.
    if (_dispatcher._api.IsConPTY())
    {
        image->pixels = {};
    }

    image->revision = GraphicsImage::NextRevision();
    return image;
}

bool KittyGraphics::_place(const Control& control, const std::shared_ptr<const GraphicsImage>& image, Error& error)
{
    auto page = _dispatcher._pages.ActivePage();
    auto& buffer = page.Buffer();
    auto& cursor = page.Cursor();
    const auto cell = _cellSize();
    const auto imageWidth = image->size.width;
    const auto imageHeight = image->size.height;

    const auto srcX = std::min<int64_t>(control.Get('x'), imageWidth);
    const auto srcY = std::min<int64_t>(control.Get('y'), imageHeight);
    const auto srcWidth = std::min<int64_t>(control.Get('w') > 0 ? control.Get('w') : imageWidth, imageWidth - srcX);
    const auto srcHeight = std::min<int64_t>(control.Get('h') > 0 ? control.Get('h') : imageHeight, imageHeight - srcY);
    if (srcX < 0 || srcY < 0 || srcWidth <= 0 || srcHeight <= 0)
    {
        error = { L"EINVAL", L"The source rectangle is empty" };
        return false;
    }

    auto placement = std::make_shared<GraphicsPlacement>();
    placement->image = image;
    placement->placementId = image->id ? control.GetUnsigned('p') : 0;
    placement->z = control.GetSigned('z');
    placement->revision = GraphicsImage::NextRevision();

    auto& geometry = placement->geometry;
    geometry.source = til::rect{ til::point{ gsl::narrow_cast<til::CoordType>(srcX), gsl::narrow_cast<til::CoordType>(srcY) }, til::size{ gsl::narrow_cast<til::CoordType>(srcWidth), gsl::narrow_cast<til::CoordType>(srcHeight) } };
    geometry.offset = { std::min<til::CoordType>(gsl::narrow_cast<til::CoordType>(std::max<int64_t>(control.Get('X'), 0)), cell.width - 1),
                        std::min<til::CoordType>(gsl::narrow_cast<til::CoordType>(std::max<int64_t>(control.Get('Y'), 0)), cell.height - 1) };
    geometry.columns = gsl::narrow_cast<til::CoordType>(std::clamp<int64_t>(control.Get('c'), 0, MaxCells));
    geometry.rows = gsl::narrow_cast<til::CoordType>(std::clamp<int64_t>(control.Get('r'), 0, MaxCells));
    geometry.virtualCellSize = cell;

    // The cell footprint is computed with the virtual cell size, so that the
    // cursor movement is the same in ConPTY and in the attached terminal.
    auto columns = geometry.columns;
    auto rows = geometry.rows;
    const auto w = static_cast<double>(srcWidth);
    const auto h = static_cast<double>(srcHeight);
    if (!columns)
    {
        columns = rows ? ceilDiv((cell.height * rows + geometry.offset.y) * w / h, cell.width) : ceilDiv(w + geometry.offset.x, cell.width);
    }
    if (!rows)
    {
        rows = geometry.columns ? ceilDiv((cell.width * columns + geometry.offset.x) * h / w, cell.height) : ceilDiv(h + geometry.offset.y, cell.height);
    }
    placement->columns = std::clamp<til::CoordType>(columns, 1, MaxCells);
    placement->rows = std::clamp<til::CoordType>(rows, 1, MaxCells);

    // Placing an image with the same image and placement id replaces it.
    if (placement->placementId)
    {
        const auto id = image->id;
        const auto placementId = placement->placementId;
        _erase(buffer, [=](const GraphicsRowRef& ref) { return ref.placement->image->id == id && ref.placement->placementId == placementId; }, false);
    }

    const auto origin = cursor.GetPosition();
    placement->column = origin.x;
    const auto moveCursor = control.GetUnsigned('C') != 1;
    const auto [topMargin, bottomMargin] = _dispatcher._GetVerticalMargins(page, true);
    const auto inMargins = origin.y >= topMargin && origin.y <= bottomMargin;
    const auto fullPageMargins = topMargin == page.Top();
    auto anchorY = origin.y;
    auto minimumY = fullPageMargins ? 0 : topMargin;

    // When the cursor is moved below the image, the content is first scrolled
    // up so that the entire image fits within the margins.
    const auto overflow = origin.y + placement->rows - 1 - bottomMargin;
    if (moveCursor && inMargins && overflow > 0)
    {
        cursor.SetPosition({ origin.x, bottomMargin });
        auto panAmount = 0;
        for (auto i = 0; i < overflow; i++)
        {
            if (_dispatcher._DoLineFeed(page, false, false))
            {
                page.MoveViewportDown();
                panAmount++;
            }
        }
        anchorY = origin.y + panAmount - overflow;
        minimumY += fullPageMargins ? 0 : panAmount;
    }

    auto firstRow = til::CoordTypeMax;
    auto lastRow = til::CoordTypeMin;
    for (auto i = 0; i < placement->rows; i++)
    {
        const auto y = anchorY + i;
        if (y >= minimumY && y < page.Bottom())
        {
            buffer.GetMutableRowByOffset(y).GetMutableGraphics().push_back({ placement, i });
            firstRow = std::min(firstRow, y);
            lastRow = std::max(lastRow, y);
        }
    }
    if (firstRow <= lastRow)
    {
        buffer.TriggerRedraw(Viewport::FromExclusive({ 0, firstRow, page.Width(), lastRow + 1 }));
    }

    if (moveCursor)
    {
        const auto x = origin.x + placement->columns;
        const auto y = std::clamp(anchorY + placement->rows - 1, page.Top(), page.Bottom() - 1);
        if (x >= page.Width())
        {
            cursor.SetPosition({ page.Width() - 1, y });
            if (_dispatcher._DoLineFeed(page, true, false))
            {
                page.MoveViewportDown();
            }
        }
        else
        {
            cursor.SetPosition({ x, y });
        }
        cursor.ResetDelayEOLWrap();
    }
    return true;
}

void KittyGraphics::_placeVirtual(const Control& control, const std::shared_ptr<const GraphicsImage>& image)
{
    if (!image->id)
    {
        return;
    }
    const auto cell = _cellSize();
    GraphicsVirtualPlacement placement;
    placement.imageId = image->id;
    placement.placementId = control.GetUnsigned('p');
    placement.columns = gsl::narrow_cast<til::CoordType>(std::clamp<int64_t>(control.Get('c'), 0, MaxCells));
    placement.rows = gsl::narrow_cast<til::CoordType>(std::clamp<int64_t>(control.Get('r'), 0, MaxCells));
    if (!placement.columns)
    {
        placement.columns = ceilDiv(image->size.width, cell.width);
    }
    if (!placement.rows)
    {
        placement.rows = ceilDiv(image->size.height, cell.height);
    }
    _store().SetVirtualPlacement(placement);
    _dispatcher._pages.ActivePage().Buffer().TriggerRedrawAll();
}

bool KittyGraphics::_erase(TextBuffer& buffer, const std::function<bool(const GraphicsRowRef&)>& predicate, const bool freeImages)
{
    std::vector<uint32_t> ids;
    const auto erased = buffer.EraseGraphics([&](const GraphicsRowRef& ref) {
        if (!predicate(ref))
        {
            return false;
        }
        if (freeImages && ref.placement->image->id)
        {
            ids.push_back(ref.placement->image->id);
        }
        return true;
    });
    if (freeImages)
    {
        std::ranges::sort(ids);
        ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
        auto& store = _store();
        for (const auto id : ids)
        {
            if (!buffer.AnyGraphics([=](const GraphicsRowRef& ref) { return ref.placement->image->id == id; }))
            {
                store.Remove(id);
            }
        }
    }
    if (erased)
    {
        buffer.TriggerRedrawAll();
    }
    return erased;
}

std::shared_ptr<const GraphicsImage> KittyGraphics::_findImage(const Control& control) const
{
    auto& store = _store();
    if (const auto number = control.GetUnsigned('I'))
    {
        return store.FindByNumber(number);
    }
    if (const auto id = control.GetUnsigned('i'))
    {
        return store.Find(id);
    }
    return nullptr;
}

GraphicsStore& KittyGraphics::_store() const
{
    return _dispatcher._pages.ActivePage().Buffer().GetGraphicsStore();
}

til::size KittyGraphics::_cellSize() const noexcept
{
    return SixelParser::CellSizeForLevel();
}

void KittyGraphics::_respond(const Control& control, const uint32_t id, const Error* error, const std::wstring_view extraKeys) const
{
    const auto quiet = control.GetUnsigned('q');
    if ((!error && quiet >= 1) || (error && quiet >= 2))
    {
        return;
    }
    const auto number = control.GetUnsigned('I');
    if (!id && !number)
    {
        return;
    }

    std::wstring response{ L"\x1b_G" };
    if (id)
    {
        response += L"i=" + std::to_wstring(id);
    }
    if (number)
    {
        response += id ? L"," : L"";
        response += L"I=" + std::to_wstring(number);
    }
    if (const auto placementId = control.GetUnsigned('p'))
    {
        response += L",p=" + std::to_wstring(placementId);
    }
    if (!error)
    {
        response += extraKeys;
    }
    response += L';';
    if (error)
    {
        response += error->code;
        response += L':';
        response += error->message;
    }
    else
    {
        response += L"OK";
    }
    response += L"\x1b\\";
    _dispatcher._api.ReturnResponse(response);
}
