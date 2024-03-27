#include <til/spsc.h>
#include <til/unicode.h>
#include <til/u8u16convert.h>
#pragma warning(push)
#pragma warning(disable : 26446)

namespace
{
    std::string accumulateEscaped16(std::wstring_view s)
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

struct Blackbox
{
    struct Record
    {
        Record() :
            time{}, typecode{ 0 }, string{ nullptr } {}
        Record(HSTRING f) :
            time{ std::chrono::high_resolution_clock::now() }, typecode{ 'o' } { WindowsDuplicateString(f, &string); }
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

    Blackbox() :
        _start{ std::chrono::high_resolution_clock::now() },
        _chan{ til::spsc::channel<Record>(1024) }
    {
        _file = L"C:\\users\\dustin\\desktop\\foo.cast";
        _t = std::thread([this]() {
            Thread();
        });
    }

    ~Blackbox()
    {
        Close();
    }

    void Log(HSTRING h)
    {
        if (!_closed)
        {
            _chan.first.emplace(h);
        }
    }

    void Log(const winrt::hstring& h) { Log((HSTRING)winrt::get_abi(h)); }

    void Close()
    {
        if (!std::exchange(_closed, true))
        {
            {
                auto _ = std::move(_chan.first);
                // destruct
            }

            if (_t.get_id() != std::this_thread::get_id())
            {
                _t.join(); // flush
            }
        }
    }

    void Thread()
    {
        Record queue[16];
        auto& rx = _chan.second;

        wil::unique_hfile file;
        do
        {
            int i = 0;
            auto [sz, ok] = rx.pop_n(til::spsc::block_initially, queue, std::extent_v<decltype(queue)>);

            if (!file)
            {
                file.reset(CreateFileW(_file.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
                auto s{ fmt::format(R"-({{"version": 2, "width": 120, "height": 30}})-"
                                    "\n") };
                WriteFile(file.get(), s.data(), (DWORD)s.size(), nullptr, nullptr);
            }
            if (!file)
            {
                // TODO(DH)
                std::terminate();
            }

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
                                           accumulateEscaped16(std::wstring_view{ buf, length })) };
                WriteFile(file.get(), jsonLine.data(), (DWORD)jsonLine.size(), nullptr, nullptr);
            }
            if (!ok)
            {
                break; // FINISH IT OFF DAVE
            }
        } while (true);
    }

private:
    std::wstring _file;
    std::thread _t;
    std::chrono::high_resolution_clock::time_point _start;
    std::pair<til::spsc::producer<Record>, til::spsc::consumer<Record>> _chan;
    bool _closed{ false };
};

#pragma warning(pop)
