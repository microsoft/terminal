#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <wil/com.h>

#include <atomic>

#include <conpty.h>

static std::string visualize_control_codes(std::string_view str) noexcept
{
    std::string out;
    out.reserve(str.size() * 3);
    for (size_t i = 0; i < str.size();)
    {
        const auto ch = static_cast<unsigned char>(str[i]);
        if (ch < 0x20 || ch == 0x7f)
        {
            // Map C0 controls to U+2400..U+241F and DEL to U+2421.
            const char32_t cp = ch == 0x7f ? 0x2421 : 0x2400 + ch;
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
        else if (ch == 0x20)
        {
            // Replace space with ␣ (U+2423).
            out += "\xE2\x90\xA3";
        }
        else
        {
            out += static_cast<char>(ch);
        }
        ++i;
    }
    return out;
}

static std::string u16u8(std::wstring_view str) noexcept
{
    std::string out;
    out.resize(str.size() * 3); // worst case
    const auto len = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), out.data(), static_cast<int>(out.size()), nullptr, nullptr);
    out.resize(std::max(0, len));
    return out;
}

static void write_stdout(std::string_view str) noexcept
{
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), str.data(), static_cast<DWORD>(str.size()), nullptr, nullptr);
}

struct Host : IPtyHost
{
    HRESULT QueryInterface(const IID& riid, void** ppvObject)
    {
        if (ppvObject == nullptr)
        {
            return E_POINTER;
        }

        if (riid == __uuidof(IPtyHost) || riid == __uuidof(IUnknown))
        {
            *ppvObject = static_cast<IPtyHost*>(this);
            AddRef();
            return S_OK;
        }

        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }

    ULONG AddRef()
    {
        return m_refCount.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    ULONG Release()
    {
        const auto count = m_refCount.fetch_sub(1, std::memory_order_relaxed) - 1;
        if (count == 0)
        {
            delete this;
        }
        return count;
    }

    void HandleUTF8Output(LPCSTR data, SIZE_T length) override
    {
        write_stdout(visualize_control_codes({ data, length }));
    }

    void HandleUTF16Output(LPCWSTR data, SIZE_T length) override
    {
        write_stdout(visualize_control_codes(u16u8({ data, length })));
    }

private:
    std::atomic<ULONG> m_refCount{ 1 };
};

int main()
{
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    wil::com_ptr<IPtyServer> server;
    THROW_IF_FAILED(PtyCreateServer(IID_PPV_ARGS(server.addressof())));
    THROW_IF_FAILED(server->SetHost(new Host()));

    wil::unique_process_information pi;
    THROW_IF_FAILED(server->CreateProcessW(
        nullptr,
        _wcsdup(L"C:\\Windows\\System32\\edit.exe"),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        nullptr,
        pi.addressof()));

    THROW_IF_FAILED(server->Run());
    return 0;
}
