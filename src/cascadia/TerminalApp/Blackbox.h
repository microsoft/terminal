#include <til/spsc.h>

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

    Blackbox() { }

    ~Blackbox()
    {
        Close();
    }

    void Start(wil::zwstring_view filePath)
    {
        _start = std::chrono::high_resolution_clock::now();
        auto [tx, rx] = til::spsc::channel<Record>(1024);
        _chan = std::move(tx);

        _file.reset(CreateFileW(filePath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
        THROW_LAST_ERROR_IF(!_file);

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
            if (_thread.get_id() != std::this_thread::get_id() && _thread.joinable())
            {
                _thread.join(); // flush
            }
        }
    }

    void Thread(til::spsc::consumer<Record> rx);

private:
    std::thread _thread;
    std::chrono::high_resolution_clock::time_point _start;
    til::spsc::producer<Record> _chan{ nullptr };
    bool _closed{ false };
    wil::unique_hfile _file;
};

struct ConnectionRecorder : public winrt::implements<ConnectionRecorder, winrt::Windows::Foundation::IInspectable>
{
    ConnectionRecorder();
    ~ConnectionRecorder() noexcept;
    void Connection(const winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection&);
    void Path(std::wstring_view path);
    void Start();
    void Stop();

private:
    struct
    {
        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection::StateChanged_revoker stateChanged;
        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection::TerminalOutput_revoker output;
    } _connectionEvents;

    winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection _connection{ nullptr };

    bool _started{ false };
    std::shared_ptr<Blackbox> _blackbox;
    std::wstring _filePath;
};
