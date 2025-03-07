#include "pch.h"

#include <til/spsc.h>
#include <til/unicode.h>
#include <til/u8u16convert.h>

#include "Blackbox.h"

#pragma warning(push)
#pragma warning(disable : 26446)

namespace
{
    std::string u16json8(std::wstring_view s)
    {
        std::string o{};
        o.reserve(s.size() * 3);
        for (const auto& v : til::utf16_iterator{ s })
        {
            char cu[6];
            int i = 0;
            char32_t ch = v[0];

            if (v.size() > 1) [[unlikely]]
            {
                ch = til::combine_surrogates(v[0], v[1]);
            }

            if (ch < 0x20 || ch == '"' || ch == '\\') [[unlikely]]
            {
                cu[i++] = '\\';
                switch (ch)
                {
                case '\n':
                    cu[i++] = 'n';
                    break;
                case '\r':
                    cu[i++] = 'r';
                    break;
                case '\\':
                case '\"':
                    cu[i++] = static_cast<char>(ch);
                    break;
                default:
                {
                    static constexpr char hexit[] = "0123456789abcdef";
                    cu[i++] = 'u';
                    cu[i++] = '0';
                    cu[i++] = '0';
                    cu[i++] = hexit[(ch >> 4) & 0x0f];
                    cu[i++] = hexit[(ch) & 0x0f];
                    break;
                }
                }
            }
            else if (ch <= 0x7F) [[likely]]
            {
                // Single-byte character (0xxxxxxx)
                cu[i++] = static_cast<char>(ch);
            }
            else if (ch <= 0x7FF)
            {
                // Two-byte character (110xxxxx 10xxxxxx)
                cu[i++] = static_cast<char>(0xC0 | ((ch >> 6) & 0x1F));
                cu[i++] = static_cast<char>(0x80 | (ch & 0x3F));
            }
            else if (ch <= 0xFFFF)
            {
                // Three-byte character (1110xxxx 10xxxxxx 10xxxxxx)
                cu[i++] = static_cast<char>(0xE0 | ((ch >> 12) & 0x0F));
                cu[i++] = static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
                cu[i++] = static_cast<char>(0x80 | (ch & 0x3F));
            }
            else if (ch <= 0x10FFFF)
            {
                // Four-byte character (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
                cu[i++] = static_cast<char>(0xF0 | ((ch >> 18) & 0x07));
                cu[i++] = static_cast<char>(0x80 | ((ch >> 12) & 0x3F));
                cu[i++] = static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
                cu[i++] = static_cast<char>(0x80 | (ch & 0x3F));
            }
            o.append(cu, &cu[0] + i);
        }
        return o;
    }
}

#pragma warning(pop)

void Blackbox::Thread(til::spsc::consumer<Record> rx)
{
    Record queue[16];

    wil::unique_hfile file;
    do
    {
        int i = 0;
        auto [sz, ok] = rx.pop_n(til::spsc::block_initially, queue, std::extent_v<decltype(queue)>);

        while (sz--)
        {
            Record rec = std::move(queue[i++]);
            auto timeDelta = (rec.time - _start).count() / 1e9f;
            UINT32 length{ 0 };
            auto buf = WindowsGetStringRawBuffer(rec.string, &length);
            auto jsonLine{ fmt::format(FMT_COMPILE(R"-([{}, "{}", "{}"])-"
                                                   "\n"),
                                       timeDelta,
                                       (char)rec.typecode,
                                       u16json8(std::wstring_view{ buf, length })) };
            WriteFile(_file.get(), jsonLine.data(), (DWORD)jsonLine.size(), nullptr, nullptr);
        }
        if (!ok)
        {
            break; // FINISH IT OFF DAVE
        }
    } while (true);
    _file.reset();
}

ConnectionRecorder::ConnectionRecorder() :
    _blackbox{ std::make_shared<Blackbox>() }
{
}

ConnectionRecorder::~ConnectionRecorder() noexcept
{
    _connectionEvents = {}; // disconnect all event handlers
    _connection = { nullptr }; // release connection handle
    _blackbox->Close();
}

void ConnectionRecorder::Connection(const winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection& connection)
{
    _connectionEvents.output = connection.TerminalOutput(winrt::auto_revoke, [this](const winrt::hstring& output) {
        this->_blackbox->Log(output);
    });
    _connectionEvents.stateChanged = connection.StateChanged(winrt::auto_revoke, [this](auto&&, auto&&) {

    });
    _connection = connection;
}

void ConnectionRecorder::Path(std::wstring_view path)
{
    _filePath = path;
}

void ConnectionRecorder::Start()
{
    if (!std::exchange(_started, true))
    {
        _blackbox->Start(_filePath);
    }
}

void ConnectionRecorder::Stop()
{
    _blackbox->Close();
}
