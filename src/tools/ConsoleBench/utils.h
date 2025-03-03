#pragma once

// clang-format off
#ifdef NDEBUG
#define debugAssert(cond) if constexpr (false) { if (!(cond)); }
#else
#define debugAssert(cond)  if (!(cond)) __debugbreak()
#endif
// clang-format on

namespace mem
{
    struct Arena;
}

struct BufferedWriter
{
    BufferedWriter(HANDLE out, std::span<char> buffer);
    ~BufferedWriter();

    void flush();
    void write(std::string_view str);

private:
    void _write(const void* data, size_t bytes) const;

    HANDLE m_out;
    std::span<char> m_buffer;
    size_t m_buffer_usage = 0;
};

inline int64_t query_perf_counter()
{
    LARGE_INTEGER i;
    QueryPerformanceCounter(&i);
    return i.QuadPart;
}

inline int64_t query_perf_freq()
{
    LARGE_INTEGER i;
    QueryPerformanceFrequency(&i);
    return i.QuadPart;
}

std::string_view get_file_version(mem::Arena& arena, const wchar_t* path);
void set_clipboard(HWND hwnd, std::wstring_view contents);
