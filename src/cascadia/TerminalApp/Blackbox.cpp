#include <til/spsc.h>
#include <til/unicode.h>
#include <til/u8u16convert.h>
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

struct Blackbox : public std::enable_shared_from_this<Blackbox>
{
    struct Record
    {
        Record() :
            time{}, typecode{ 0 }, string{ nullptr } {}
        Record(char type, HSTRING f) :
            time{ std::chrono::high_resolution_clock::now() }, typecode{ type } { WindowsDuplicateString(f, &string); }
        Record(HSTRING f) :
            Record('o', f) {}
        Record(Record&& r) :
            time{ r.time },
            typecode{ r.typecode },
            string{ r.string }
        {
            r.string = nullptr;
        }
        Record& operator=(Record&& r)
        {
            time = r.time;
            typecode = r.typecode;
            string = r.string;
            r.string = nullptr;
            return *this;
        }
        ~Record()
        {
            if (string)
            {
                WindowsDeleteString(string);
            }
        }
        std::chrono::high_resolution_clock::time_point time;
        char typecode{ 0 };
        HSTRING string{ nullptr };
    };

    Blackbox(std::wstring_view filePath) :
        _filePath{ filePath }
    {
    }

    ~Blackbox()
    {
        Close();
    }

    void Start()
    {
        _file.reset(CreateFileW(_filePath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
        THROW_LAST_ERROR_IF(!_file);

        _start = std::chrono::high_resolution_clock::now();
        auto [tx, rx] = til::spsc::channel<Record>(1024);
        _chan = std::move(tx);

        auto s{ fmt::format(R"-({{"version": 2, "width": 120, "height": 30}})-"
                            "\n") };
        WriteFile(_file.get(), s.data(), (DWORD)s.size(), nullptr, nullptr);

        _thread = std::thread([this, strong = shared_from_this(), rx = std::move(rx)]() mutable {
            Thread(std::move(rx));
        });
    }

    void Log(HSTRING h)
    {
        if (!_closed)
        {
            _chan.emplace(h);
        }
    }

    void Log(const winrt::hstring& h) { Log((HSTRING)winrt::get_abi(h)); }

    void LogResize(til::CoordType columns, til::CoordType rows)
    {
        if (!_closed)
        {
            //? TODO(DH) determine whether we should pass the size along as a string or just make Record a tagged union w/ typecode
            winrt::hstring newSizeRecord{ fmt::format(FMT_COMPILE(L"{0}x{1}"), columns, rows) };
            _chan.emplace('r', (HSTRING)winrt::get_abi(newSizeRecord));
        }
    }

    void Close()
    {
        if (!std::exchange(_closed, true))
        {
            {
                auto _ = std::move(_chan);
                // the tx side of the channel closes at the end of this scope
            }

            // we may be getting destructed on the Thread thread.
            if (_thread.get_id() != std::this_thread::get_id())
            {
                _thread.join(); // flush
            }
        }
    }

    void Thread(til::spsc::consumer<Record> rx)
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

private:
    std::thread _thread;
    std::chrono::high_resolution_clock::time_point _start;
    til::spsc::producer<Record> _chan{ nullptr };
    bool _closed{ false };
    std::wstring _filePath;
    wil::unique_hfile _file;
};

#pragma warning(pop)
